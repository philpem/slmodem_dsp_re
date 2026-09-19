#include "dsplib/period_byte_layout.h"
/* v32struct.h -- owning period-i386 layouts of the V.32 allocations. */
#ifndef DSPLIB_V32STRUCT_H
#define DSPLIB_V32STRUCT_H

#include "dsplib/v32fp.h"
#include "dsplib/v32scram.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32dec.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_fse.h"

struct fpm_tone;
struct fpm_mtd;
struct v32_modem;
#define V32_SEQ_REGS 5

/**
 * @brief Run the active half-duplex transmit state.
 * @param modem Owning v32_modem, not its v32_hdx.
 * @param data Input-word buffer; unused by some states.
 * @param[out] out Shaped output samples.
 * @param[in,out] left Remaining symbols, initially hdx->symbol_len.
 * @return Samples written, not symbols consumed.
 */
typedef short (*v32_txhdx_fn)(struct v32_modem *modem, short *data,
			    short *out, unsigned short *left);
/**
 * @brief Run the active half-duplex receive state.
 * @param modem Owning v32_modem, not its v32_hdx.
 * @param in Input samples.
 * @param[out] out Decoded data words.
 * @param[in,out] count Input sample count; the data state replaces it with
 *                     its output-word count, not unconsumed input samples.
 */
typedef void (*v32_rxhdx_fn)(struct v32_modem *modem, short *in,
			   unsigned short *out, unsigned short *count);
/**
 * @brief Append absolute, differential or trellis-coded symbols to a ring.
 * @param[in,out] smc Coder state.
 * @param[in,out] out Destination symbol ring, including its write cursor.
 * @param[in,out] in Input words; the trellis encoder masks them in place.
 * @param count Number of input words to encode.
 */
typedef void (*v32_encoder_fn)(struct v32_smc *smc, struct v32_symout *out,
			      short *in, unsigned short count);

struct v32_hdx {
	struct fpm_agc agc;
	struct fpm_tone *tone0, *tone1, *tone2;
	struct fpm_mtd *mtd;
	short regs[V32_SEQ_REGS];
	short short_46, short_48;
	unsigned short gen_index, gen_index_mask, gen_width, gen_mask, gen_pattern;
	unsigned short det_width;
	unsigned char pad_56[2];
	int det_out_mask, det_target, det_mask, det_reg, det_match;
	v32_txhdx_fn tx_state;
	v32_rxhdx_fn rx_state;
	short state, mode;
	int state_left;
	unsigned int timer, limit;
	short block_charge;
	unsigned char pad_86[2];
	int int_88;
	unsigned char pad_8c[4];
	int int_90;
	short turnaround, rtd, short_98, short_9a, short_9c;
	short symbol_len, sample_len;
	unsigned char pad_a2[2];
	short *buffer;
	short short_a8, short_aa, short_ac, loss_blocks;
};

struct v32_fp {
	int int_00, int_04, int_08, int_0c, eq_adapt;
	int int_14, int_18, int_1c, int_20, int_24;
	short tx_rate_index, rx_rate_index, short_2c, short_2e;
	struct v32_sdm scrambler;
	struct v32_smc tx_smc;
	unsigned char pad_5e[2];
	struct fpm_pps pps;
	v32_encoder_fn encoders[3];
	short encoder_sel;
	unsigned char pad_a6[2];
	const short *imap, *qmap;
	struct v32_symout symout;
	struct fpm_mrf mrf;
	struct fpm_ecc ecc;
	struct fpm_sre sre;
	struct fpm_agc agc;
	struct fpm_fse fse;
	unsigned char pad_501c[4];
	struct v32_dec decoder;
	struct v32_smc rx_smc;
	unsigned char pad_50ae[2];
	struct v32_sdm descrambler;
	unsigned short rx_len;
	unsigned char pad_50ca[2];
	short *rx_buf, *clean_buf;
	unsigned short clean_n;
	short decision_error;
	unsigned short rate_fallback;
	unsigned char pad_50da[2];
};

struct v32_modem {
	struct v32fp_params params;
	union {
		struct {
			unsigned char status, flags, byte_32, byte_33;
		};
		unsigned int status_word;
	};
	short *diag_out_i, *diag_out_q;
	unsigned short *diag_n_out;
	short *diag_icoeff, *diag_qcoeff;
	short diag_fse_taps;
	unsigned char pad_4a[2];
	short *diag_near_i, *diag_near_q;
	unsigned short diag_near_n;
	unsigned char pad_56[2];
	short *diag_far_i, *diag_far_q;
	unsigned short diag_far_n;
	unsigned char pad_62[2];
	struct v32_hdx *hdx;
	struct v32_fp *fp;
};

/* GCC 3.4.2 lacks __SIZEOF_POINTER__, so its assertions remain active. */
#if !defined(__SIZEOF_POINTER__) || __SIZEOF_POINTER__ == 4
#define V32_SA(n, e) typedef char n[(e) ? 1 : -1]
#define V32_SO(t, m) ((unsigned long)__builtin_offsetof(t, m))
V32_SA(v32_hdx_regs_3c, V32_SO(struct v32_hdx, regs) == 0x3c);
V32_SA(v32_hdx_tx_6c, V32_SO(struct v32_hdx, tx_state) == 0x6c);
V32_SA(v32_hdx_stateleft_78,
	V32_SO(struct v32_hdx, state_left) == 0x78);
V32_SA(v32_hdx_timer_7c, V32_SO(struct v32_hdx, timer) == 0x7c);
V32_SA(v32_hdx_limit_80, V32_SO(struct v32_hdx, limit) == 0x80);
V32_SA(v32_hdx_blockcharge_84,
	V32_SO(struct v32_hdx, block_charge) == 0x84);
V32_SA(v32_hdx_buf_a4, V32_SO(struct v32_hdx, buffer) == 0xa4);
V32_SA(v32_hdx_size_b0, sizeof(struct v32_hdx) == 0xb0);
V32_SA(v32_fp_rate_2a, V32_SO(struct v32_fp, rx_rate_index) == 0x2a);
V32_SA(v32_fp_fse_204, V32_SO(struct v32_fp, fse) == 0x204);
V32_SA(v32_fp_dec_5020, V32_SO(struct v32_fp, decoder) == 0x5020);
V32_SA(v32_fp_rxsmc_5098, V32_SO(struct v32_fp, rx_smc) == 0x5098);
V32_SA(v32_fp_clean_50d0, V32_SO(struct v32_fp, clean_buf) == 0x50d0);
V32_SA(v32_fp_size_50dc, sizeof(struct v32_fp) == 0x50dc);
V32_SA(v32_modem_hdx_64, V32_SO(struct v32_modem, hdx) == 0x64);
V32_SA(v32_modem_status_30, V32_SO(struct v32_modem, status_word) == 0x30);
V32_SA(v32_modem_fp_68, V32_SO(struct v32_modem, fp) == 0x68);
V32_SA(v32_modem_size_6c, sizeof(struct v32_modem) == 0x6c);
#undef V32_SO
#undef V32_SA
#endif
#endif
