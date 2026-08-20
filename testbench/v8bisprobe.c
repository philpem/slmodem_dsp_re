/*
 * v8bisprobe - deliberately small active V.8bis physical-layer probe.
 *
 * This is not a second V.8bis implementation.  Its first job is to make the
 * part which SpanDSP does provide -- V.21 FSK plus HDLC framing -- observable
 * and testable before we attach a state machine to a live modem.  The wire
 * format is the same one v8analyse receives.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <spandsp.h>

struct probe {
    fsk_tx_state_t *tx;
    fsk_rx_state_t *rx;
    hdlc_tx_state_t *htx;
    hdlc_rx_state_t *hrx;
    int received;
    int bad;
    int last_len;
    uint8_t last[16];
};

static int next_bit(void *user_data)
{
    struct probe *p = user_data;
    return hdlc_tx_get_bit(p->htx);
}

static void put_bit(void *user_data, int bit)
{
    struct probe *p = user_data;
    hdlc_rx_put_bit(p->hrx, bit);
}

static void got_frame(void *user_data, const uint8_t *frame, int len, int ok)
{
    struct probe *p = user_data;
    int i;

    if (!ok) {
        p->bad++;
        return;
    }
    if (len <= 0)
        return;                 /* flag-only delimiter, not a message */
    p->received++;
    p->last_len = len < (int)sizeof(p->last) ? len : (int)sizeof(p->last);
    memcpy(p->last, frame, (size_t)p->last_len);
    printf("RX valid V.8bis frame:");
    for (i = 0; i < len; i++)
        printf(" %02x", frame[i]);
    printf("\n");
}

static int init_probe(struct probe *p, int v21_channel)
{
    const fsk_spec_t *spec = &preset_fsk_specs[v21_channel];

    memset(p, 0, sizeof(*p));
    p->htx = hdlc_tx_init(NULL, false, 2, false, NULL, NULL);
    p->hrx = hdlc_rx_init(NULL, false, true, 2, got_frame, p);
    if (!p->htx || !p->hrx)
        return -1;
    p->tx = fsk_tx_init(NULL, spec, next_bit, p);
    p->rx = fsk_rx_init(NULL, spec, FSK_FRAME_MODE_SYNC, put_bit, p);
    if (!p->tx || !p->rx)
        return -1;
    fsk_tx_power(p->tx, -13.0f);
    return 0;
}

static void free_probe(struct probe *p)
{
    if (p->tx) fsk_tx_free(p->tx);
    if (p->rx) fsk_rx_free(p->rx);
    if (p->htx) hdlc_tx_free(p->htx);
    if (p->hrx) hdlc_rx_free(p->hrx);
}

static int selftest(void)
{
    /* Revision 1 ACK: the smallest valid V.8bis message, so a successful
       loop proves the V.21 channel choice, bit order, HDLC transparency and
       FCS are all correct. */
    static const uint8_t ack[] = { 0x14 };
    struct probe p;
    int16_t block[160];
    int n;

    if (init_probe(&p, FSK_V21CH1)) {
        fprintf(stderr, "cannot initialise SpanDSP V.21/HDLC\n");
        return 1;
    }
    if (hdlc_tx_flags(p.htx, 24) || hdlc_tx_frame(p.htx, ack, sizeof(ack))
        || hdlc_tx_flags(p.htx, 8)) {
        fprintf(stderr, "cannot queue HDLC ACK\n");
        free_probe(&p);
        return 1;
    }
    printf("TX V.8bis ACK: 14 (revision 1) on V.21(L)\n");
    for (n = 0; n < 80 && !p.received; n++) {
        fsk_tx(p.tx, block, (int)(sizeof(block) / sizeof(block[0])));
        fsk_rx(p.rx, block, (int)(sizeof(block) / sizeof(block[0])));
    }
    if (p.received != 1 || p.bad || p.last_len != (int)sizeof(ack)
        || memcmp(p.last, ack, sizeof(ack)) != 0) {
        fprintf(stderr, "FAIL: valid=%d bad=%d\n", p.received, p.bad);
        free_probe(&p);
        return 1;
    }
    printf("PASS: V.21/HDLC V.8bis physical layer loopback\n");
    free_probe(&p);
    return 0;
}

enum { SIP_SAMPLES = 160, SOCKET_FRAME_BYTES = 324 };

static int read_all(int fd, void *buf, size_t n)
{
    unsigned char *p = buf;
    while (n) {
        ssize_t k = read(fd, p, n);
        if (k == 0) return 0;
        if (k < 0) { if (errno == EINTR) continue; return -1; }
        p += k; n -= (size_t)k;
    }
    return 1;
}

static int write_all(int fd, const void *buf, size_t n)
{
    const unsigned char *p = buf;
    while (n) {
        ssize_t k = write(fd, p, n);
        if (k < 0) { if (errno == EINTR) continue; return -1; }
        p += k; n -= (size_t)k;
    }
    return 0;
}

/* slmodemd -e peer: socket_frame is a 32-bit type followed by a 320-byte
   union.  This starts with the 1375+2002 Hz V.8bis initiating dual-tone
   segment, then listens on both V.21 directions.  It is intentionally a
   stimulus/recorder, not yet a claim of a complete CRe state machine. */
static int slmodem_dual_tone_peer(int argc, char **argv)
{
    struct probe low, high;
    int fd, frame = 0, audio = 0;
    unsigned char in[SOCKET_FRAME_BYTES], out[SOCKET_FRAME_BYTES];
    int16_t *rx, *tx;
    double p1 = 0.0, p2 = 0.0;
    const double d1 = 2.0 * M_PI * 1375.0 / 8000.0;
    const double d2 = 2.0 * M_PI * 2002.0 / 8000.0;

    if (argc < 4) {
        fprintf(stderr, "slmodem peer expects <dial> <audio-fd> <control-fd>\n");
        return 2;
    }
    fd = atoi(argv[argc - 2]);
    if (init_probe(&low, FSK_V21CH1) || init_probe(&high, FSK_V21CH2)) {
        fprintf(stderr, "cannot initialise V.21 receivers\n");
        return 1;
    }
    fprintf(stderr, "v8bisprobe: injecting 1375+2002 Hz for 400 ms; listening for V.21 frames\n");
    while (read_all(fd, in, sizeof(in)) > 0) {
        int type;
        memcpy(&type, in, sizeof(type));
        if (type != 0) continue;
        rx = (int16_t *)(in + 4);
        fsk_rx(low.rx, rx, SIP_SAMPLES);
        fsk_rx(high.rx, rx, SIP_SAMPLES);
        memset(out, 0, sizeof(out));
        memcpy(out, &type, sizeof(type));
        tx = (int16_t *)(out + 4);
        if (audio < 20) {            /* 20 * 20 ms = 400 ms */
            for (int i = 0; i < SIP_SAMPLES; i++) {
                tx[i] = (int16_t)(5500.0 * sin(p1) + 5500.0 * sin(p2));
                p1 += d1; p2 += d2;
                if (p1 >= 2.0 * M_PI) p1 -= 2.0 * M_PI;
                if (p2 >= 2.0 * M_PI) p2 -= 2.0 * M_PI;
            }
        }
        if (write_all(fd, out, sizeof(out))) break;
        audio++; frame++;
        if (audio >= 200) break;     /* four seconds: the V.8bis response window */
    }
    fprintf(stderr, "v8bisprobe: %d audio frames; V.21(L) valid=%d bad=%d; "
                    "V.21(H) valid=%d bad=%d\n", frame,
            low.received, low.bad, high.received, high.bad);
    free_probe(&low); free_probe(&high);
    return 0;
}

/* A 16 kHz raw-block peer for hsfuser's `modem` mode.  SpanDSP V.8 is an
   8 kHz implementation, hence the deliberate 2:1 decimation/interpolation.
   It is used to get a real DCE through the ordinary V.8 CI/CM/JM exchange
   before attempting the V.8bis transaction which follows it. */
static void v8_result(void *user_data, v8_parms_t *result)
{
    (void)user_data;
    fprintf(stderr, "v8 caller result: status=%d function=%d modulations=0x%x protocol=%d\\n",
            result->status, result->jm_cm.call_function,
            result->jm_cm.modulations, result->jm_cm.protocols);
}

static int raw_v8_caller_peer(const char *fd_text)
{
    int fd = atoi(fd_text), blocks;
    int16_t in16[128], out16[128], in8[64], out8[64];
    v8_parms_t parms;
    v8_state_t *v8;
    FILE *capture;

    memset(&parms, 0, sizeof(parms));
    parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    parms.jm_cm.call_function = V8_CALL_V_SERIES;
    parms.jm_cm.modulations = V8_MOD_V21 | V8_MOD_V22 | V8_MOD_V23 | V8_MOD_V32
                            | V8_MOD_V34 | V8_MOD_V90 | V8_MOD_V92;
    parms.jm_cm.protocols = V8_PROTOCOL_LAPM_V42;
    v8 = v8_init(NULL, true, &parms, v8_result, NULL);
    if (!v8) return 1;
    capture = fopen("/tmp/hsf_v8caller_hsf_tx_8k.raw", "wb");
    fprintf(stderr, "v8bisprobe: raw V.8 caller peer active on fd %d\n", fd);
    for (blocks = 0; blocks < 4000 && read_all(fd, in16, sizeof(in16)) > 0; blocks++) {
        int n;
        for (n = 0; n < 64; n++) in8[n] = in16[n * 2];
        if (capture) fwrite(in8, sizeof(in8[0]), 64, capture);
        v8_rx(v8, in8, 64);
        n = v8_tx(v8, out8, 64);
        if (n < 0) n = 0;
        for (int i = 0; i < 64; i++) {
            int16_t s = i < n ? out8[i] : 0;
            out16[i * 2] = out16[i * 2 + 1] = s;
        }
        if (write_all(fd, out16, sizeof(out16))) break;
    }
    fprintf(stderr, "v8bisprobe: raw V.8 caller peer completed after %d blocks\n", blocks);
    if (capture) fclose(capture);
    v8_free(v8);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
        return selftest();
    if (argc >= 4 && strcmp(argv[1], "--slmodem-peer") == 0)
        return slmodem_dual_tone_peer(argc - 1, argv + 1);
    if (argc == 3 && strcmp(argv[1], "--raw-v8-caller") == 0)
        return raw_v8_caller_peer(argv[2]);
    /* slmodemd -e supplies exactly <dial> <audio-fd> <control-fd>; it cannot
       append a mode flag, so this is the deliberate default for that shape. */
    if (argc == 4)
        return slmodem_dual_tone_peer(argc, argv);
    fprintf(stderr, "usage: %s --selftest | --raw-v8-caller <audio-fd> | --slmodem-peer <dial> <audio-fd> <control-fd>\n", argv[0]);
    return 2;
}
