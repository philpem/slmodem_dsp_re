/*
 * Decode the V.8 negotiation out of a recorded call, using SpanDSP.
 *
 * WHY SPANDSP.  Two implementations disagree: the blob transmits a CM and two
 * unrelated hardware modems decline to answer it with a JM.  Neither side of
 * that argument can settle it, and our own reconstruction is derived from one
 * of them, so it cannot be the referee either.  SpanDSP's V.8 is a third,
 * independent implementation written from the recommendation -- if it parses
 * the blob's CM and reports sensible modulations, the message is well formed
 * and the fault is elsewhere; if it rejects it, we have the defect.
 *
 * Input is raw 16-bit little-endian mono at 8000 Hz -- i.e. the *_8k.raw files
 * the bench records, which are the SIP audio exactly as carried.
 *
 *   v8analyse <file.raw> caller|answerer
 *
 * "answerer" decodes what an ANSWERING modem would see, so point it at a
 * recording of the originator (our modem_tx_8k.raw) to read the CM.
 * "caller" decodes what a CALLING modem would see, so point it at
 * modem_rx_8k.raw to read the far end's ANSam and JM.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spandsp.h>

static int results;

static void report(void *user_data, v8_parms_t *r)
{
    (void) user_data;
    results++;
    printf("\n  --- V.8 result %d ---\n", results);
    printf("    status              %d (%s)\n", r->status,
           v8_status_to_str(r->status));
    printf("    modem connect tone  %d\n", r->modem_connect_tone);
    printf("    CI sent             %d\n", r->send_ci);
    printf("    V.92                %d\n", r->v92);
    printf("    call function       %d (%s)\n", r->jm_cm.call_function,
           v8_call_function_to_str(r->jm_cm.call_function));
    printf("    modulations         0x%08x\n", r->jm_cm.modulations);
    /* Name them, because a bare bitmap is the thing this tool exists to
       translate. */
    struct { int bit; const char *name; } mods[] = {
        {V8_MOD_V17, "V.17"},   {V8_MOD_V21, "V.21"},   {V8_MOD_V22, "V.22"},
        {V8_MOD_V23HDX, "V.23 hdx"}, {V8_MOD_V23, "V.23"},
        {V8_MOD_V26BIS, "V.26bis"}, {V8_MOD_V26TER, "V.26ter"},
        {V8_MOD_V27TER, "V.27ter"}, {V8_MOD_V29, "V.29"},
        {V8_MOD_V32, "V.32/V.32bis"}, {V8_MOD_V34HDX, "V.34 hdx"},
        {V8_MOD_V34, "V.34"},   {V8_MOD_V90, "V.90"},   {V8_MOD_V92, "V.92"},
        {0, NULL}
    };
    printf("                        ");
    int any = 0;
    for (int i = 0; mods[i].name; i++) {
        if (r->jm_cm.modulations & mods[i].bit) {
            printf("%s%s", any++ ? ", " : "", mods[i].name);
        }
    }
    printf("%s\n", any ? "" : "(none)");
    printf("    protocol            %d\n", r->jm_cm.protocols);
    printf("    PCM modem avail     %d\n", r->jm_cm.pcm_modem_availability);
    printf("    PSTN access         %d\n", r->jm_cm.pstn_access);
    printf("    NSF                 %d\n", r->jm_cm.nsf);
    printf("    T.66                %d\n", r->jm_cm.t66);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <file.raw> caller|answerer\n", argv[0]);
        return 2;
    }
    bool calling = (strcmp(argv[2], "caller") == 0);

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }

    v8_parms_t parms;
    memset(&parms, 0, sizeof(parms));
    parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    parms.jm_cm.call_function = V8_CALL_V_SERIES;
    parms.jm_cm.modulations = V8_MOD_V21 | V8_MOD_V22 | V8_MOD_V23 | V8_MOD_V32
                            | V8_MOD_V34 | V8_MOD_V90;
    parms.jm_cm.protocols = V8_PROTOCOL_LAPM_V42;
    parms.jm_cm.pcm_modem_availability = 0;
    parms.jm_cm.pstn_access = 0;

    v8_state_t *v8 = v8_init(NULL, calling, &parms, report, NULL);
    if (!v8) {
        fprintf(stderr, "v8_init failed\n");
        return 1;
    }
    /* Everything SpanDSP notices, not just the final answer -- the interesting
       case here is a negotiation that never completes, so the log is the
       output that matters. */
    logging_state_t *log = v8_get_logging_state(v8);
    span_log_set_level(log, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL
                            | SPAN_LOG_FLOW);
    span_log_set_tag(log, calling ? "v8-caller" : "v8-answerer");

    int16_t buf[160];
    size_t n;
    long total = 0;
    while ((n = fread(buf, sizeof(int16_t), 160, f)) > 0) {
        v8_rx(v8, buf, (int) n);
        total += (long) n;
    }
    fclose(f);

    printf("\n  fed %ld samples (%.1f s at 8 kHz), %d result callback(s)\n",
           total, total / 8000.0, results);
    if (results == 0)
        printf("  NO V.8 RESULT: SpanDSP did not complete a negotiation from this audio\n");
    v8_free(v8);
    return 0;
}
