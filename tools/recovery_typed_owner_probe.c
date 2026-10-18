/*
 * Period-layout replay for the complete historical v32seq/v32fpctl ledgers.
 * The RXHDX-40 and V32.c-40 sample batches remain in their separate probes.
 * Compiled by recovery_typed_owner_probe.py with the period flags from
 * build/tc_repro/.build-config and DSPLIB_REPRODUCE_BUGS appended last.
 */
#include <stdio.h>
#include <stddef.h>
#include "dsplib/v32struct.h"
#define S(T, x) printf(#T "." #x " off=%lu size=%lu signed=%d\n", (unsigned long)__builtin_offsetof(struct T, x), (unsigned long)sizeof(((struct T *)0)->x), (((__typeof__(((struct T *)0)->x))(-1)) < 0))
#define P(T, x) printf(#T "." #x " off=%lu size=%lu signed=n/a\n", (unsigned long)__builtin_offsetof(struct T, x), (unsigned long)sizeof(((struct T *)0)->x))
int main(void) {
    P(v32_modem, params); P(v32_modem, hdx); P(v32_modem, fp);
    S(v32_modem, params.rx_rate); S(v32_modem, params.trellis);
    S(v32_modem, params.ec_near_delay); S(v32_modem, params.symlen_sel);
    S(v32_modem, status); S(v32_modem, flags);
    P(v32_fp, scrambler); P(v32_fp, descrambler); P(v32_fp, tx_smc);
    P(v32_fp, pps); P(v32_fp, symout); P(v32_fp, ecc); P(v32_fp, fse);
    S(v32_fp, tx_rate_index); S(v32_fp, rx_rate_index);
    S(v32_fp, encoder_sel); S(v32_fp, short_2c); S(v32_fp, short_2e);
    S(v32_fp, int_00); S(v32_fp, int_04); S(v32_fp, int_08); S(v32_fp, int_0c);
    S(v32_fp, eq_adapt); S(v32_fp, int_14); S(v32_fp, int_18);
    P(v32_fp, decoder); P(v32_fp, decoder.vtb); P(v32_fp, decoder.vtb.paths);
    S(v32_fp, decoder.short_04); S(v32_fp, decoder.short_74);
    P(v32_fp, sre); P(v32_fp, mrf); P(v32_fp, rx_buf); P(v32_fp, clean_buf);
    S(v32_fp, clean_n); S(v32_fp, rx_smc.shift); S(v32_fp, rx_smc.mode);
    S(v32_fp, ecc.pwr_out); S(v32_fp, ecc.adapt_near);
    S(v32_fp, mrf.need); S(v32_fp, mrf.phase);
    P(v32_hdx, regs); P(v32_hdx, mtd); P(v32_hdx, tone0);
    P(v32_hdx, tone1); P(v32_hdx, tone2); P(v32_hdx, buffer);
    S(v32_hdx, gen_index); S(v32_hdx, gen_index_mask); S(v32_hdx, gen_width);
    S(v32_hdx, gen_mask); S(v32_hdx, gen_pattern); S(v32_hdx, det_width);
    S(v32_hdx, det_out_mask); S(v32_hdx, det_target); S(v32_hdx, det_mask);
    S(v32_hdx, det_reg); S(v32_hdx, det_match); S(v32_hdx, short_9c);
    S(v32_hdx, regs[0]);
    S(v32_hdx, symbol_len); S(v32_hdx, turnaround); S(v32_hdx, short_98);
    S(v32_hdx, short_9a); S(v32_hdx, short_46);
    P(v32_dec, vtb); S(v32_dec, short_04); S(v32_dec, short_74);
    S(v32_sdm, tap1_pos); S(v32_sdm, tap2_pos);
    return 0;
}
