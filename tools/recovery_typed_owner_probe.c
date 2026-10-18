/*
 * Period-layout replay for historical V32 typed-owner access audits.
 * Includes complete v32seq/v32fpctl and V32rxhdx ledgers; other samples
 * remain in their separate probes.
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
    P(v32_fp, agc); P(v32_fp, encoders[0]); P(v32_fp, imap); P(v32_fp, qmap);
    S(v32_fp, tx_rate_index); S(v32_fp, rx_rate_index);
    S(v32_fp, encoder_sel); S(v32_fp, decision_error); S(v32_fp, rate_fallback);
    S(v32_fp, short_2c); S(v32_fp, short_2e);
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
    P(v32_hdx, agc);
    S(v32_hdx, state); S(v32_hdx, mode); S(v32_hdx, block_charge);
    S(v32_hdx, sample_len); S(v32_hdx, loss_blocks); S(v32_hdx, int_88);
    P(v32_hdx, tx_state); P(v32_hdx, rx_state);
    S(v32_hdx, gen_index); S(v32_hdx, gen_index_mask); S(v32_hdx, gen_width);
    S(v32_hdx, gen_mask); S(v32_hdx, gen_pattern); S(v32_hdx, det_width);
    S(v32_hdx, det_out_mask); S(v32_hdx, det_target); S(v32_hdx, det_mask);
    S(v32_hdx, det_reg); S(v32_hdx, det_match); S(v32_hdx, short_9c);
    S(v32_hdx, regs[0]); S(v32_hdx, regs[4]);
    S(v32_hdx, timer); S(v32_hdx, limit); S(v32_hdx, state_left);
    S(v32_hdx, short_48);
    S(v32_hdx, short_aa); S(v32_hdx, short_a8); S(v32_hdx, int_90);
    S(v32_hdx, rtd); S(v32_hdx, short_ac);
    S(v32_hdx, symbol_len); S(v32_hdx, turnaround); S(v32_hdx, short_98);
    S(v32_hdx, short_9a); S(v32_hdx, short_46);
    S(v32_modem, params.protocol); S(v32_modem, params.options);
    S(v32_modem, status_word);
    P(v32_modem, diag_out_i); P(v32_modem, diag_out_q);
    P(v32_modem, diag_n_out); P(v32_modem, diag_icoeff); P(v32_modem, diag_qcoeff);
    S(v32_modem, diag_fse_taps); P(v32_modem, diag_near_i);
    P(v32_modem, diag_near_q); S(v32_modem, diag_near_n);
    P(v32_modem, diag_far_i); P(v32_modem, diag_far_q); S(v32_modem, diag_far_n);
    S(v32_fp, rx_len);
    S(v32_sdm, group); S(v32_sdm, tap1); S(v32_sdm, tap2);
    S(v32_smc, mode); S(v32_smc, pad02); S(v32_smc, shift);
    S(v32_smc, state[0]); S(v32_smc, quad); S(v32_smc, trellis_diff_state);
    S(v32_smc, trellis_state); S(v32_smc, pad12); S(v32_smc, uncoded_bits);
    S(v32_dec, int_68); S(v32_dec, short_06); S(v32_dec, sym[0]);
    S(v32_dec, scram_tap);
    printf("sizeof.v32_modem size=%lu\n", (unsigned long)sizeof(struct v32_modem));
    printf("sizeof.v32_fp size=%lu\n", (unsigned long)sizeof(struct v32_fp));
    printf("sizeof.v32_hdx size=%lu\n", (unsigned long)sizeof(struct v32_hdx));
    printf("sizeof.v32_smc size=%lu\n", (unsigned long)sizeof(struct v32_smc));
    printf("sizeof.v32_sdm size=%lu\n", (unsigned long)sizeof(struct v32_sdm));
    P(v32_dec, vtb); S(v32_dec, short_04); S(v32_dec, short_74);
    S(v32_sdm, tap1_pos); S(v32_sdm, tap2_pos);
    return 0;
}
