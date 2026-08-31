/*
 * vce.h -- the `vce_*` / `STRM_VCE_*` corner of the voice service.
 *
 * Four small functions from the `voice.c#3` span (0x600..0x2b60), all of
 * which the object exports or keeps file-local next to the ring detector:
 *
 *   vce_hook_on                          .text 0x000600     40 bytes  LOCAL
 *   vce_hook_off                         .text 0x000630     40 bytes  LOCAL
 *   vce_get_sreg                         .text 0x000660    188 bytes  LOCAL
 *   STRM_VCE_GetFDSPEnvironmentalParams  .text 0x0013b0    152 bytes
 *
 * THREE OF THEM ARE `t` IN THE OBJECT AND OUR COPIES ARE EXTERNAL, which is
 * the `GetGain` precedent (finding F8462) and the `AnalyseDialString` one
 * before it: a static we cannot name is a static we cannot test.
 *
 * WHAT IS NOT INHERITED WITH IT.  F8462's three carry GCC 3.4's static-
 * function `regparm(2)`, so their tests declare the reference side
 * `__attribute__((regparm(2)))`.  THESE THREE DO NOT -- every one of them
 * reads its arguments off the stack in the object (`mov 0x10(%esp),%eax` in
 * `vce_hook_on`, `mov 0x10(%esp)`/`0x14(%esp)` in `vce_get_sreg`), so they
 * are ordinary cdecl and their `ref_` aliases are declared plainly.  Being
 * LOCAL is not on its own enough to predict the convention; the body is.
 * Finding F8770.
 */

#ifndef DSPLIB_VCE_H
#define DSPLIB_VCE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `struct voice_info` -- what MDMPRM_VOICEINFO answers with.
 *
 * The MEMBER NAMES AND ORDER are slmodemd's own, from the copy of
 * `modem_defs.h` this tree vendors verbatim under `ref/slmodemd/`, where
 * `struct modem` embeds one and `modem_param.c` hands its address back for
 * MDMPRM_VOICEINFO.  It is declared here rather than included from there for
 * the same reason `struct dsp_info` is declared in modem_params.h: `ref/` is
 * the reference material, not a header this build compiles against.
 *
 * The OFFSETS ARE THIS OBJECT'S, and they corroborate the host's layout
 * exactly: `vce_get_sreg` reads +0x08, +0x24 and +0x28, which under eleven
 * four-byte members are `rx_gain`, `silence_detect_sensitivity` and
 * `silence_detect_period` -- and those are precisely the three S-registers it
 * answers (#VGR-class gain, #VSS and #VSP).  The members are `unsigned` in
 * the host's header, and the object agrees: the sensitivity is shifted with
 * `shr` and compared with `jbe`, both unsigned.  Finding F8771.
 */
struct voice_info {
	unsigned int	comp_method;			/* +0x00 */
	unsigned int	sample_rate;			/* +0x04 */
	unsigned int	rx_gain;			/* +0x08 read */
	unsigned int	tx_gain;			/* +0x0c */
	unsigned int	dtmf_symbol;			/* +0x10 */
	unsigned int	tone1_freq;			/* +0x14 */
	unsigned int	tone2_freq;			/* +0x18 */
	unsigned int	tone_duration;			/* +0x1c */
	unsigned int	inactivity_timer;		/* +0x20 */
	unsigned int	silence_detect_sensitivity;	/* +0x24 read */
	unsigned int	silence_detect_period;		/* +0x28 read */
};

/*
 * The seven S-register numbers `vce_get_sreg` knows, with slmodemd's own
 * names -- including its spelling of 72, which is `SREG_HANDSET_GANE` in
 * `ref/slmodemd/modem_defs.h` and is kept so a grep across the two trees
 * matches.  73 has two names there for one number; the voice one is used.
 */
#define SREG_FLASH_TIMER			24
#define SREG_HANDSET_GANE			72
#define SREG_VOICE_DIALTONE_DETECT_DELAY	73	/* seconds */
#define SREG_SILENCE_DETECT_SENSITIVITY		82	/* #VSS */
#define SREG_SILENCE_DETECT_DURATION		83	/* #VSP */
#define SREG_MIC_GAIN				138
#define SREG_LINE_RECORD_GAIN			139

/*
 * The three constants `vce_get_sreg` answers without consulting anything.
 * They are the object's, and it holds no storage for them at all -- setting
 * S24, S72 or S73 cannot move what this function returns.
 */
#define VCE_FLASH_TIMER			20
#define VCE_HANDSET_GAIN		19
#define VCE_DIALTONE_DETECT_DELAY	3

/*
 * The silence-detect sensitivity is reported as a 0..3 level, and the mapping
 * is not a plain shift: `>> 6` would send every raw value under 64 to level
 * 0, and the object instead reports level 1 for any NON-ZERO value below 64.
 * So zero means zero and everything else is at least one step of sensitivity.
 */
#define VCE_SILENCE_LEVEL_SHIFT		6
#define VCE_SILENCE_LEVEL_MAX		3

/*
 * The two delays `STRM_VCE_GetFDSPEnvironmentalParams` reports, in samples.
 * They are hard-coded in the object; the names are the author's, off the
 * format string it prints them with.
 */
#define STRM_VCE_FAR_ECHO_DELAY		51
#define STRM_VCE_NEAR_ECHO_DELAY	369

void vce_hook_on(void *p);
void vce_hook_off(void *p);
int vce_get_sreg(void *modem, unsigned int num);
void STRM_VCE_GetFDSPEnvironmentalParams(short *psFarEchoDelay,
					 short *psNearEchoDelay);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VCE_H */
