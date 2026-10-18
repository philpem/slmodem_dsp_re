/*
 * Bounded period-layout probe for the 45 selected typed-owner audit rows.
 * Compiled by recovery_typed_owner_probe.py with the period flags from
 * build/tc_repro/.build-config and DSPLIB_REPRODUCE_BUGS appended last.
 */
#include <stdio.h>
#include <stddef.h>
#include "dsplib/v32struct.h"
#define S(T, x) printf(#T "." #x " off=%lu size=%lu signed=%d\n", (unsigned long)__builtin_offsetof(struct T, x), (unsigned long)sizeof(((struct T *)0)->x), (((__typeof__(((struct T *)0)->x))(-1)) < 0))
#define P(T, x) printf(#T "." #x " off=%lu size=%lu signed=n/a\n", (unsigned long)__builtin_offsetof(struct T, x), (unsigned long)sizeof(((struct T *)0)->x))
int main(void) {
    S(v32_hdx, gen_index); S(v32_hdx, gen_index_mask); S(v32_hdx, gen_width);
    S(v32_hdx, gen_mask); S(v32_hdx, gen_pattern); S(v32_hdx, det_width);
    S(v32_hdx, det_out_mask); S(v32_hdx, det_target); S(v32_hdx, det_mask);
    S(v32_hdx, det_reg); S(v32_hdx, det_match);
    S(v32_hdx, short_46); S(v32_fp, ecc.pwr_out); S(v32_fp, ecc.adapt_near);
    S(v32_fp, mrf.need); S(v32_fp, mrf.phase);
    P(v32_hdx, mtd); P(v32_hdx, tone2); P(v32_hdx, tone1); P(v32_hdx, tone0);
    P(v32_fp, decoder.vtb.paths); P(v32_fp, sre); P(v32_fp, mrf);
    P(v32_fp, rx_buf); P(v32_fp, clean_buf); P(v32_hdx, buffer);
    S(v32_fp, clean_n); S(v32_fp, rx_smc.shift); S(v32_fp, rx_smc.mode);
    S(v32_dec, short_04); S(v32_fp, short_2e);
    S(v32_modem, status); S(v32_modem, flags);
    S(v32_sdm, tap1_pos); S(v32_sdm, tap2_pos);
    return 0;
}
