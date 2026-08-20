/*
 * Decode a V.8 negotiation from an audio capture, using SpanDSP.
 *
 * Usage:
 *   v8analyse <capture.{wav,raw}> [caller|answerer|both] [raw-rate]
 *
 * WAV input must be 16-bit little-endian PCM and supplies its own rate. Raw
 * input is also 16-bit little-endian PCM; it defaults to 8000 Hz for the old
 * command line, but an explicit raw-rate is preferable. SpanDSP's V.8 front
 * end runs at 8000 Hz, so the capture must be 8 kHz (use the *_8k.wav stereo
 * recording, not the datapump-native 9600 Hz recording).
 *
 * `caller` and `answerer` name the receiver viewpoint. `both` (the default)
 * runs both on every input channel. A stereo capture is handled as two
 * independent directions: L=received and R=transmitted, as written by row.sh.
 * It deliberately does not infer which endpoint called: it labels all four
 * interpretations, so the decoded CM/JM is evidence rather than a guess.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spandsp.h>

struct capture {
    int rate;
    int channels;
    size_t frames;
    int16_t *samples;
};

struct decoder {
    const char *channel_name;
    const char *view_name;
    int channel;
    size_t sample_offset;
    v8_state_t *state;
};

/* V.8bis messages use V.21 plus HDLC framing, unlike the asynchronous V.8
   CM/JM exchange handled by v8_decode_rx above. Keep each physical direction
   and each V.21 channel separate: V.21(L) is the initiating station and
   V.21(H) is the responding station. */
struct v8bis_stream {
    const char *channel_name;
    const char *v21_name;
    int channel;
    size_t sample_offset;
    int frames;
    fsk_rx_state_t *fsk;
    hdlc_rx_state_t *hdlc;
};

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int read_exact(FILE *f, void *p, size_t n)
{
    return fread(p, 1, n, f) == n ? 0 : -1;
}

static int load_wav(FILE *f, struct capture *capture)
{
    unsigned char header[12], chunk[8], fmt[16];
    uint32_t data_bytes = 0;
    int have_fmt = 0;

    if (read_exact(f, header, sizeof(header)) || memcmp(header, "RIFF", 4)
        || memcmp(header + 8, "WAVE", 4))
        return -1;
    while (read_exact(f, chunk, sizeof(chunk)) == 0) {
        uint32_t length = le32(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            if (length < sizeof(fmt) || read_exact(f, fmt, sizeof(fmt)))
                return -1;
            if (fseek(f, (long)(length - sizeof(fmt) + (length & 1U)), SEEK_CUR))
                return -1;
            if (le16(fmt) != 1 || le16(fmt + 2) < 1 || le16(fmt + 2) > 2
                || le16(fmt + 14) != 16)
                return -1;
            capture->channels = le16(fmt + 2);
            capture->rate = (int)le32(fmt + 4);
            have_fmt = 1;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!have_fmt || length == 0)
                return -1;
            data_bytes = length;
            break;
        } else if (fseek(f, (long)(length + (length & 1U)), SEEK_CUR)) {
            return -1;
        }
    }
    if (!have_fmt || !data_bytes || data_bytes % (2U * (unsigned)capture->channels))
        return -1;
    capture->frames = data_bytes / (2U * (unsigned)capture->channels);
    capture->samples = malloc((size_t)data_bytes);
    if (!capture->samples || read_exact(f, capture->samples, data_bytes))
        return -1;
    return 0;
}

static int load_capture(const char *path, int raw_rate, struct capture *capture)
{
    unsigned char magic[4];
    FILE *f;
    long bytes;

    memset(capture, 0, sizeof(*capture));
    f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return -1;
    }
    if (read_exact(f, magic, sizeof(magic)))
        goto bad;
    rewind(f);
    if (memcmp(magic, "RIFF", 4) == 0) {
        if (load_wav(f, capture) == 0) {
            fclose(f);
            return 0;
        }
        fprintf(stderr, "%s: expected 16-bit PCM WAV with one or two channels\n", path);
        goto bad;
    }
    if (raw_rate <= 0) {
        fprintf(stderr, "%s: raw PCM needs a positive sample rate\n", path);
        goto bad;
    }
    if (fseek(f, 0, SEEK_END) || (bytes = ftell(f)) < 0 || bytes % 2)
        goto bad;
    rewind(f);
    capture->rate = raw_rate;
    capture->channels = 1;
    capture->frames = (size_t)bytes / 2U;
    capture->samples = malloc((size_t)bytes);
    if (!capture->samples || read_exact(f, capture->samples, (size_t)bytes))
        goto bad;
    fclose(f);
    return 0;
bad:
    fclose(f);
    free(capture->samples);
    memset(capture, 0, sizeof(*capture));
    return -1;
}

static const char *call_function_name(unsigned value)
{
    static const char *const names[] = {
        "TBS", "H.324", "V.18", "T.101", "T.30 fax transmit",
        "T.30 fax receive", "V-series modem data", "extension"
    };
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "unknown";
}

static void print_modulations(const unsigned char *p, int n, int *at)
{
    static const char *const first[] = { "V.90", "V.34", "V.34 half-duplex" };
    static const char *const second[] = { "V.32/V.32bis", "V.22/V.22bis", "V.17", "V.29", "V.27ter" };
    static const char *const third[] = { "V.26ter", "V.26bis", "V.23", "V.23 half-duplex", "V.21" };
    const char *comma = "";
    unsigned value;

    value = p[(*at)++];
    printf("    modulations: ");
    if (value & 0x20) { printf("%s%s", comma, first[0]); comma = ", "; }
    if (value & 0x40) { printf("%s%s", comma, first[1]); comma = ", "; }
    if (value & 0x80) { printf("%s%s", comma, first[2]); comma = ", "; }
    if (*at < n && (p[*at] & 0x38) == 0x10) {
        value = p[(*at)++];
        if (value & 0x01) { printf("%s%s", comma, second[0]); comma = ", "; }
        if (value & 0x02) { printf("%s%s", comma, second[1]); comma = ", "; }
        if (value & 0x04) { printf("%s%s", comma, second[2]); comma = ", "; }
        if (value & 0x40) { printf("%s%s", comma, second[3]); comma = ", "; }
        if (value & 0x80) { printf("%s%s", comma, second[4]); comma = ", "; }
    }
    if (*at < n && (p[*at] & 0x38) == 0x10) {
        value = p[(*at)++];
        if (value & 0x01) { printf("%s%s", comma, third[0]); comma = ", "; }
        if (value & 0x02) { printf("%s%s", comma, third[1]); comma = ", "; }
        if (value & 0x04) { printf("%s%s", comma, third[2]); comma = ", "; }
        if (value & 0x40) { printf("%s%s", comma, third[3]); comma = ", "; }
        if (value & 0x80) { printf("%s%s", comma, third[4]); comma = ", "; }
    }
    printf("%s\n", comma[0] ? "" : "(none)");
}

static void print_hex(const unsigned char *p, int n)
{
    for (int i = 0; i < n; i++)
        printf("%s%02x", i ? " " : "", p[i]);
}

static void print_t35_blocks(const unsigned char *decoded, int byte, const char *indent)
{
    int pos;

    for (pos = 0; pos < byte; ) {
        int length, country_len, provider_len, start;
        const char *country = NULL, *vendor = NULL, *model = NULL;

        length = decoded[pos++];
        start = pos;
        if (length < 2 || length > byte - pos) {
            printf("%smalformed NS block: length %d, %d octets remain\n",
                   indent, length, byte - pos);
            return;
        }
        country_len = decoded[pos] == 0xff ? 2 : 1;
        if (country_len >= length) {
            printf("%smalformed NS block: missing provider-code length\n", indent);
            return;
        }
        provider_len = decoded[pos + country_len];
        if (country_len + 1 + provider_len > length) {
            printf("%smalformed NS block: provider code length %d\n", indent, provider_len);
            return;
        }
        t35_decode(&decoded[start], length, &country, &vendor, &model);
        printf("%sT.35 country: %s (code ", indent, country ? country : "unknown");
        print_hex(&decoded[start], country_len);
        printf(")\n");
        printf("%sprovider code: ", indent);
        print_hex(&decoded[start + country_len + 1], provider_len);
        printf("%s%s%s\n", vendor ? " (" : "", vendor ? vendor : "",
               vendor ? ")" : "");
        if (vendor)
            printf("%smanufacturer: %s%s%s\n", indent, vendor,
                   model ? ", model " : "", model ? model : "");
        if (length > country_len + 1 + provider_len) {
            printf("%smanufacturer-defined data: ", indent);
            print_hex(&decoded[start + country_len + 1 + provider_len],
                      length - country_len - 1 - provider_len);
            printf("\n");
        }
        pos = start + length;
    }
}

/* V.8 carries NS in three option bits in the category octet followed by five
   option bits per extension octet. Reassemble that continuous bit stream into
   the T.35 blocks specified by V.8 clause 6.6. */
static void print_v8_nsf(const unsigned char *p, int n, int *at)
{
    unsigned char decoded[128];
    int bit_count = 0, i, byte = 0;

    memset(decoded, 0, sizeof(decoded));
    for (i = 5; i <= 7; i++) {
        decoded[bit_count / 8] |= ((p[*at] >> i) & 1U) << (bit_count & 7);
        bit_count++;
    }
    (*at)++;
    while (*at < n && (p[*at] & 0x38) == 0x10 && bit_count < 8 * (int)sizeof(decoded)) {
        static const int bits[] = { 0, 1, 2, 6, 7 };
        for (i = 0; i < (int)(sizeof(bits) / sizeof(bits[0])); i++) {
            decoded[bit_count / 8] |= ((p[*at] >> bits[i]) & 1U) << (bit_count & 7);
            bit_count++;
        }
        (*at)++;
    }
    byte = bit_count / 8;
    printf("    non-standard facilities: %d decoded octet%s\n", byte,
           byte == 1 ? "" : "s");
    print_t35_blocks(decoded, byte, "      ");
}

static void describe_message(struct decoder *decoder, const char *kind,
                             const unsigned char *bytes, int n)
{
    int at = 0;

    printf("[%8.3fs] %s / %s: %s", decoder->sample_offset / 8000.0,
           decoder->channel_name, decoder->view_name, kind);
    printf("  raw:");
    for (int i = 0; i < n; i++)
        printf(" %02x", bytes[i]);
    printf("\n");
    while (at < n) {
        unsigned byte = bytes[at];
        unsigned value = byte >> 5;
        switch (byte & 0x1f) {
        case 0x01:
            printf("    call function: %s (%u)\n", call_function_name(value), value);
            at++;
            break;
        case 0x05:
            print_modulations(bytes, n, &at);
            break;
        case 0x0a:
            printf("    protocol: %s\n", value == 1 ? "LAPM / V.42"
                   : value == 0 ? "none" : value == 7 ? "extension" : "reserved");
            at++;
            break;
        case 0x0d:
            printf("    PSTN access: %s%s%s\n", value & 1 ? "calling DCE cellular " : "",
                   value & 2 ? "answering DCE cellular " : "",
                   value & 4 ? "DCE on digital network" : value ? "" : "unknown");
            at++;
            break;
        case 0x07:
            printf("    PCM modem availability: %s%s%s\n", value & 1 ? "V.90/V.92 analogue " : "",
                   value & 2 ? "V.90/V.92 digital " : "", value & 4 ? "V.91" : value ? "" : "none");
            at++;
            break;
        case 0x0e:
            printf("    T.66: %u\n", value);
            at++;
            break;
        case 0x0f:
            print_v8_nsf(bytes, n, &at);
            break;
        default:
            printf("    extension/unknown field: 0x%02x\n", byte);
            at++;
            break;
        }
    }
}

static const char *v8bis_message_name(unsigned type)
{
    static const char *const names[] = {
        "reserved", "MS (mode select)", "CL (capabilities list)",
        "CLR (capabilities list request)", "ACK(1)", "ACK(2)",
        "reserved", "reserved", "NAK(1)", "NAK(2)", "NAK(3)", "NAK(4)"
    };
    return type < sizeof(names) / sizeof(names[0]) ? names[type] : "reserved/unknown";
}

/* Consume one V.8bis parameter block. Delimiter bit 8 applies to level 1/2
   blocks, and bit 7 to level 3 blocks. The returned bits exclude the
   delimiter. */
static int v8bis_block(const uint8_t *p, int n, int *at, unsigned delimiter,
                       unsigned *bits, unsigned *last)
{
    unsigned value = 0;

    while (*at < n) {
        unsigned octet = p[(*at)++];
        value |= octet & (delimiter - 1U);
        if (octet & delimiter) {
            if (bits)
                *bits = value;
            if (last)
                *last = octet;
            return 0;
        }
    }
    return -1;
}

static int v8bis_tree(const uint8_t *p, int n, int *at, unsigned *npar1,
                      unsigned *spar1)
{
    unsigned top, ignored, last;
    int i;

    if (v8bis_block(p, n, at, 0x80, npar1, NULL)
        || v8bis_block(p, n, at, 0x80, spar1, NULL))
        return -1;
    top = *spar1;
    for (i = 0; i < 7; i++) {
        if (!(top & (1U << i)))
            continue;
        if (v8bis_block(p, n, at, 0x80, &ignored, &last))
            return -1;
        /* Bit 7 in the final NPar(2) says that this capability has no
           SPar(2)/NPar(3) descendants. */
        if (last & 0x40)
            continue;
        if (v8bis_block(p, n, at, 0x80, &ignored, NULL))
            return -1;
        for (int j = 0; j < 6; j++) {
            if (ignored & (1U << j)) {
                if (v8bis_block(p, n, at, 0x40, NULL, NULL))
                    return -1;
            }
        }
    }
    return 0;
}

static void print_v8bis_flags(const char *label, unsigned flags,
                              const char *const names[], int count)
{
    const char *separator = "";
    int i;

    printf("    %s: ", label);
    for (i = 0; i < count; i++) {
        if (flags & (1U << i)) {
            printf("%s%s", separator, names[i]);
            separator = ", ";
        }
    }
    printf("%s\n", separator[0] ? "" : "none");
}

static void describe_v8bis_frame(void *user_data, const uint8_t *bytes, int n, int ok)
{
    struct v8bis_stream *stream = user_data;
    unsigned type, revision;
    unsigned i_npar, i_spar, s_npar, s_spar;
    int at = 1;
    int i;

    /* A V.21 receiver will occasionally see HDLC-like flags in later modem
       training. They are not V.8bis unless their FCS is valid. */
    if (!bytes || n <= 0 || !ok)
        return;
    stream->frames++;
    printf("[%8.3fs] %s / %s: V.8bis %s%s  raw:",
           stream->sample_offset / 8000.0, stream->channel_name, stream->v21_name,
           v8bis_message_name(bytes[0] & 0x0f), "");
    for (i = 0; i < n; i++)
        printf(" %02x", bytes[i]);
    printf("\n");
    type = bytes[0] & 0x0f;
    revision = (bytes[0] >> 4) & 0x0f;
    printf("    message type: %s (%u)\n", v8bis_message_name(type), type);
    printf("    V.8bis revision: %u%s\n", revision,
           revision == 1 ? " (Recommendation V.8bis revision 1)" :
           revision == 2 ? " (Recommendation V.8bis revision 2)" : "");
    if (type == 4 || type == 5 || (type >= 8 && type <= 11)) {
        printf("    no I/S/NS parameter fields in this acknowledgement\n");
        return;
    }
    if (v8bis_tree(bytes, n, &at, &i_npar, &i_spar)
        || v8bis_tree(bytes, n, &at, &s_npar, &s_spar)) {
        printf("    malformed or unsupported I/S parameter tree\n");
        return;
    }
    {
        static const char *const i_names[] = {
            "ITU-T V.8", "short V.8", "additional information available",
            "request ACK(1)", "reserved", "reserved", "non-standard field"
        };
        static const char *const i_s_names[] = {
            "network type", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved"
        };
        static const char *const s_names[] = {
            "data", "simultaneous voice/data", "H.324", "V.18", "T.30 fax",
            "analogue telephony", "T.101 videotex"
        };
        print_v8bis_flags("I field", i_npar, i_names, 7);
        print_v8bis_flags("I-field parameter groups", i_spar, i_s_names, 7);
        print_v8bis_flags("S-field parameter groups", s_spar, s_names, 7);
        if (s_npar & 0x40)
            printf("    S field: non-standard capabilities indicated\n");
    }
    if (i_npar & 0x40) {
        printf("    non-standard information (NS): %d octet%s\n", n - at,
               n - at == 1 ? "" : "s");
        print_t35_blocks(bytes + at, n - at, "      ");
    }
}

static void v8bis_put_bit(void *user_data, int bit)
{
    struct v8bis_stream *stream = user_data;
    hdlc_rx_put_bit(stream->hdlc, bit);
}

static int init_v8bis_stream(struct v8bis_stream *stream, const char *channel_name,
                             int channel, int v21_channel)
{
    const fsk_spec_t *spec = &preset_fsk_specs[v21_channel];

    memset(stream, 0, sizeof(*stream));
    stream->channel_name = channel_name;
    stream->v21_name = v21_channel == FSK_V21CH1
        ? "V.21(L), initiating station" : "V.21(H), responding station";
    stream->channel = channel;
    stream->hdlc = hdlc_rx_init(NULL, false, true, 2, describe_v8bis_frame, stream);
    if (!stream->hdlc)
        return -1;
    stream->fsk = fsk_rx_init(NULL, spec, FSK_FRAME_MODE_SYNC, v8bis_put_bit, stream);
    if (!stream->fsk) {
        hdlc_rx_free(stream->hdlc);
        stream->hdlc = NULL;
        return -1;
    }
    return 0;
}

static void feed_v8bis_stream(struct v8bis_stream *stream, const struct capture *capture,
                               size_t at, size_t n)
{
    int16_t block[160];
    size_t i;

    stream->sample_offset = at;
    for (i = 0; i < n; i++)
        block[i] = capture->samples[(at + i) * (size_t)capture->channels + stream->channel];
    fsk_rx(stream->fsk, block, (int)n);
}

static void v8_log(void *user_data, int level, const char *text)
{
    struct decoder *decoder = user_data;
    const char *kind = strstr(text, ">CM:");
    const char *cursor;
    unsigned char bytes[32];
    int n = 0, used, value;
    const char *name = "CM";

    (void) level;
    if (!kind) {
        kind = strstr(text, ">JM:");
        name = "JM";
    }
    if (!kind) {
        kind = strstr(text, ">CI:");
        name = "CI";
    }
    if (!kind) {
        kind = strstr(text, ">V.92:");
        name = "V.92 indication";
    }
    if (!kind) {
        if (strstr(text, "recognised"))
            printf("[%8.3fs] %s / %s: %s", decoder->sample_offset / 8000.0,
                   decoder->channel_name, decoder->view_name, text);
        return;
    }
    cursor = strchr(kind, ':') + 1;
    while (n < (int)(sizeof(bytes) / sizeof(bytes[0]))
           && sscanf(cursor, " %x%n", &value, &used) == 1) {
        bytes[n++] = (unsigned char)value;
        cursor += used;
    }
    describe_message(decoder, name, bytes, n);
}

static int init_decoder(struct decoder *decoder, const char *channel_name, int channel,
                        int calling)
{
    v8_parms_t parms;
    logging_state_t *log;

    memset(&parms, 0, sizeof(parms));
    parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    parms.jm_cm.call_function = V8_CALL_V_SERIES;
    parms.jm_cm.modulations = V8_MOD_V21 | V8_MOD_V22 | V8_MOD_V23 | V8_MOD_V32
                            | V8_MOD_V34 | V8_MOD_V90;
    parms.jm_cm.protocols = V8_PROTOCOL_LAPM_V42;
    decoder->channel_name = channel_name;
    decoder->channel = channel;
    decoder->view_name = calling ? "caller view" : "answerer view";
    /* v8_decode_rx is SpanDSP's passive capture reader. It decodes CM/JM
       without trying to conduct a new negotiation against the recording. */
    decoder->state = v8_init(NULL, calling, &parms, NULL, NULL);
    if (!decoder->state)
        return -1;
    log = v8_get_logging_state(decoder->state);
    span_log_set_level(log, SPAN_LOG_FLOW);
    span_log_set_tag(log, decoder->view_name);
    span_log_set_message_handler(log, v8_log, decoder);
    return 0;
}

static void feed_decoder(struct decoder *decoder, const struct capture *capture,
                         size_t at, size_t n)
{
    int16_t block[160];
    size_t i;
    decoder->sample_offset = at;
    for (i = 0; i < n; i++)
        block[i] = capture->samples[(at + i) * (size_t)capture->channels + decoder->channel];
    v8_decode_rx(decoder->state, block, (int)n);
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s <capture.{wav,raw}> [caller|answerer|both] [raw-rate]\n"
            "\n"
            "Decode the V.8 CM/JM exchange in a recorded audio capture.\n"
            "Also decode valid-FCS V.8bis MS/CL/CLR/ACK/NAK frames on V.21(L/H).\n"
            "This is a passive decoder: it reports messages seen on the wire,\n"
            "not an end-to-end success/failure verdict for the recorded call.\n"
            "\n"
            "Input:\n"
            "  WAV files must be 16-bit little-endian PCM, mono or stereo.\n"
            "  Raw files are 16-bit little-endian mono PCM; raw-rate defaults to 8000.\n"
            "  SpanDSP V.8 decoding requires 8000 Hz audio. Use *_8k.wav, not\n"
            "  the 9600 Hz datapump-native recordings.\n"
            "\n"
            "Modes:\n"
            "  caller    Run only the caller receiver view.\n"
            "  answerer  Run only the answerer receiver view.\n"
            "  both      Run both views (default).\n"
            "\n"
            "Stereo captures from row.sh are L=received and R=transmitted. In\n"
            "both mode every view is run on both directions, labelled in output.\n"
            "The final line states how many valid V.8bis frames were found.\n",
            program);
}

int main(int argc, char *argv[])
{
    struct capture capture;
    struct decoder decoders[4];
    struct v8bis_stream v8bis[4];
    const char *mode = argc > 2 ? argv[2] : "both";
    int raw_rate = argc > 3 ? atoi(argv[3]) : 8000;
    int views[2], nviews = 0, channel, ndecoders = 0, nv8bis = 0;

    if ((argc == 2 && (strcmp(argv[1], "--help") == 0
                       || strcmp(argv[1], "-h") == 0))
        || argc < 2 || argc > 4) {
        usage(argv[0]);
        return argc == 2 ? 0 : 2;
    }
    if (strcmp(mode, "caller") == 0)
        views[nviews++] = 1;
    else if (strcmp(mode, "answerer") == 0)
        views[nviews++] = 0;
    else if (strcmp(mode, "both") == 0) {
        views[nviews++] = 1;
        views[nviews++] = 0;
    } else {
        usage(argv[0]);
        return 2;
    }
    if (load_capture(argv[1], raw_rate, &capture))
        return 1;
    if (capture.rate != 8000) {
        fprintf(stderr, "%s: V.8 analysis requires an 8000 Hz capture (got %d Hz)\n",
                argv[1], capture.rate);
        free(capture.samples);
        return 1;
    }

    printf("V.8 capture: %s, %zu frames, %d Hz, %s\n", argv[1], capture.frames,
           capture.rate, capture.channels == 2 ? "stereo (L=received, R=transmitted)" : "mono");
    for (channel = 0; channel < capture.channels; channel++) {
        const char *channel_name = capture.channels == 2
            ? (channel == 0 ? "left / received" : "right / transmitted") : "mono";
        int v;
        for (v = 0; v < nviews; v++) {
            struct decoder *decoder = &decoders[ndecoders];
            memset(decoder, 0, sizeof(*decoder));
            if (init_decoder(decoder, channel_name, channel, views[v])) {
                fprintf(stderr, "v8_init failed\n");
                free(capture.samples);
                return 1;
            }
            ndecoders++;
        }
        for (v = FSK_V21CH1; v <= FSK_V21CH2; v++) {
            if (init_v8bis_stream(&v8bis[nv8bis], channel_name, channel, v)) {
                fprintf(stderr, "V.8bis FSK/HDLC initialisation failed\n");
                free(capture.samples);
                return 1;
            }
            nv8bis++;
        }
    }
    for (size_t at = 0; at < capture.frames; ) {
        size_t n = capture.frames - at;
        if (n > 160)
            n = 160;
        for (int i = 0; i < ndecoders; i++)
            feed_decoder(&decoders[i], &capture, at, n);
        for (int i = 0; i < nv8bis; i++)
            feed_v8bis_stream(&v8bis[i], &capture, at, n);
        at += n;
    }
    for (int i = 0; i < ndecoders; i++)
        v8_free(decoders[i].state);
    for (int i = 0; i < nv8bis; i++) {
        fsk_rx_free(v8bis[i].fsk);
        hdlc_rx_free(v8bis[i].hdlc);
    }
    {
        int v8bis_frames = 0;
        for (int i = 0; i < nv8bis; i++)
            v8bis_frames += v8bis[i].frames;
        printf("Transcript complete: %.1f s of audio across %d V.8 channel/view stream%s; "
               "%d valid V.8bis HDLC frame%s.\n",
               capture.frames / (double)capture.rate, ndecoders, ndecoders == 1 ? "" : "s",
               v8bis_frames, v8bis_frames == 1 ? "" : "s");
    }
    free(capture.samples);
    return 0;
}
