/*
 * vce.h -- the `vce_*` / `STRM_VCE_*` corner of the voice service, and the
 * four-function API the host calls it through.
 *
 * Eight functions from the `voice.c#3` span (0x600..0x2b60), all of which the
 * object exports or keeps file-local next to the ring detector:
 *
 *   vce_hook_on                          .text 0x000600     40 bytes  LOCAL
 *   vce_hook_off                         .text 0x000630     40 bytes  LOCAL
 *   vce_get_sreg                         .text 0x000660    188 bytes  LOCAL
 *   VOICE_create                         .text 0x000720    493 bytes
 *   VOICE_delete                         .text 0x000910    133 bytes
 *   VOICE_command                        .text 0x0009a0    548 bytes
 *   VOICE_process                        .text 0x000bd0   2016 bytes
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

/**
 * @brief Off-hook notification (diagnostic only).
 *
 * The whole body is a debug-level gate and a printf of @p p with `%p`; there
 * is no other observable effect at any debug level.
 *
 * @param p Opaque pointer, printed and otherwise untouched.
 */
void vce_hook_on(void *p);

/** @brief On-hook notification. Same shape as vce_hook_on(), diagnostic only. */
void vce_hook_off(void *p);

/**
 * @brief The voice service's own S-register reader.
 *
 * Not a call into slmodemd's `modem_get_sreg`: it fetches `struct voice_info`
 * via `MDMPRM_VOICEINFO` and answers seven register numbers out of that
 * block and three built-in constants (S24, S72, S73). Any other register
 * number returns 0, including ones the host itself has a value for. The
 * block is fetched unconditionally before the switch, even for the three
 * constant answers, so the `modem_get_param` call is always made.
 *
 * @param modem The host handle, passed through to `modem_get_param`.
 * @param num   The S-register number (`SREG_*`).
 * @return The register's value, or 0 for any register this function does
 *         not know.
 */
int vce_get_sreg(void *modem, unsigned int num);

/**
 * @brief Report the two FDSP echo delays, in samples.
 *
 * Both are hard-coded constants (`STRM_VCE_FAR_ECHO_DELAY`,
 * `STRM_VCE_NEAR_ECHO_DELAY`); the incoming values are only ever printed
 * (the object's "old:"/"new:" debug lines), never read, so a caller cannot
 * influence the answer.
 *
 * @param psFarEchoDelay  Out: the far-echo delay, samples.
 * @param psNearEchoDelay Out: the near-echo delay, samples.
 */
void STRM_VCE_GetFDSPEnvironmentalParams(short *psFarEchoDelay,
					 short *psNearEchoDelay);

/* ------------------------------------------------------------------ *
 * The VOICE service object: what `VOICE_create` allocates and the
 * other three operate on.
 * ------------------------------------------------------------------ */

struct voice_ctx;	/* voice.h -- the service core this layer wraps */
struct rc;		/* fixedrc.h -- a fixed-ratio rate converter    */

/*
 * THE FOUR PROTOTYPES ARE THE HOST'S OWN, transcribed from the copy of
 * slmodemd this tree vendors: `ref/slmodemd/modem.c` declares all four at
 * lines 92-95, and `modem_voice_process` / `modem_voice_command` are the call
 * sites.  So the `void *` in and out buffers, the `unsigned` sample rate and
 * the `int` count are the interface's, not an inference -- which matters
 * because the object's own use of `in` and `out` is byte-wise (see D1020).
 *
 * `VOICE_command`'s second parameter is `enum VOICE_CMD` there.  It is spelled
 * `unsigned int` here because that is what the object compares (`cmp $0x7,%edx;
 * ja` at 0x9b6, and the refusal prints `%u`), and because a GCC 3.4 enum whose
 * enumerators are all non-negative IS unsigned -- the two spellings agree, and
 * this one does not need the enum defined in two trees.
 */
/**
 * @brief Build the VOICE service object.
 *
 * Allocates the 0x1484-byte `struct vce`, creates the line-rate converter
 * pair the requested rate needs (none at 8000, since that is the pump's own
 * rate), and builds the voice service core underneath it. An unsupported
 * rate leaves both converters NULL and still succeeds; there is no failure
 * return reachable from a test, since the only failure path is a
 * `sysdep_malloc` that cannot be made to fail here.
 *
 * @param modem The host handle.
 * @param rate  Line sample rate: `VCE_RATE_8000`, `VCE_RATE_9600` or
 *              `VCE_RATE_48000`; anything else takes the pump's own rate.
 * @return The new service object.
 */
void *VOICE_create(void *modem, unsigned int rate);

/**
 * @brief Tear the VOICE service down: the voice core, both rate converters
 * (if created), and the object itself.
 */
void VOICE_delete(void *obj);

/**
 * @brief Translate a host `enum VOICE_CMD` opcode into the four
 * `voice_command` opcodes underneath, fetching any tone parameters out of
 * `struct voice_info` on the way.
 *
 * @param obj The VOICE service object.
 * @param cmd One of the eight `VOICE_CMD_*` values.
 * @return -1 for a NULL object or an opcode outside 0..`VOICE_CMD_MAX`;
 *         otherwise whatever `voice_command` answered.
 */
int VOICE_command(void *obj, unsigned int cmd);

/**
 * @brief Run one buffer of line audio through the voice service.
 *
 * Resamples in, scales to float, exchanges host-side bytes, drives
 * `voice_modem` and resamples back out, a block at a time.
 *
 * @param obj   The VOICE service object.
 * @param in    Line-rate input samples.
 * @param out   Line-rate output samples.
 * @param count Samples in @p in and @p out.
 * @return 0 in every case this tree has observed; the object never sets it
 *         otherwise.
 */
int VOICE_process(void *obj, void *in, void *out, int count);

/*
 * `enum VOICE_CMD`, as eight numbers.
 *
 * THE NAMES ARE THE HOST'S AND THE OBJECT'S, AND THEY AGREE ARM FOR ARM --
 * which is as strong as naming evidence gets here.  `ref/slmodemd/modem_defs.h`
 * declares the enum in this order, and seven of `VOICE_command`'s eight arms
 * print a string that names the same thing:
 *
 *	0  STATE_COMMAND  "voice: VCE: VOICE_CMD_SET_MODE: COMMAND"
 *	1  STATE_RX       "voice: VCE: VOICE_CMD_SET_MODE: RX"
 *	2  STATE_TX       "voice: VCE: VOICE_CMD_SET_MODE: TX"
 *	3  STATE_DUPLEX   "voice: VCE: Unknown command %u"   <-- see D1021
 *	4  STATE_SPEAKER  "voice: VCE: VOICE_CMD_SET_MODE: SPEAKER"
 *	5  BEEP           "voice: VCE: VOICE_CMD_BEEP, %d %d %d"
 *	6  DTMF           "voice: VCE: VOICE_CMD_DTMF, %d %d"
 *	7  ABORT          "voice: VCE: VOICE_CMD_ABORT"
 *
 * The host's spelling is kept verbatim so a grep across the two trees matches,
 * exactly as SREG_HANDSET_GANE above is kept.  Finding F8830.
 */
#define VOICE_CMD_STATE_COMMAND		0
#define VOICE_CMD_STATE_RX		1
#define VOICE_CMD_STATE_TX		2
#define VOICE_CMD_STATE_DUPLEX		3
#define VOICE_CMD_STATE_SPEAKER		4
#define VOICE_CMD_BEEP			5
#define VOICE_CMD_DTMF			6
#define VOICE_CMD_ABORT			7

/* The largest command the switch accepts; `cmp $0x7,%edx; ja` at 0x9b6. */
#define VOICE_CMD_MAX			7

/*
 * The service state at `struct vce` +0x0c, and the status `VOICE_process`
 * returns.  Both sets are the host's own, from the same header, and the object
 * corroborates every value:
 *
 *   - the four states are written by the message arms of `VOICE_process`
 *     (0 by VOICE_OK / START_ONLINE / ERROR, 2 by START_TX, 1 by START_RX,
 *     3 by START_DUPLEX) and read back by the host-I/O block, where TX takes
 *     the "read `host_count` bytes from the host" arm and RX takes the "read
 *     160 and look for <DLE>!" one.  That is the right way round for a
 *     playback state and a record state.
 *   - the three statuses are the values `VOICE_process` leaves in its return:
 *     1 from the two "online" messages, 2 from ERROR and from an unknown
 *     message, 3 from the three "start" messages.  `modem_voice_process` in
 *     modem.c switches on exactly these three names.
 *
 * VOICE_STATE_DUPLEX is `TX|RX` in the host's header, which is why 3 and not 4
 * is the duplex state.  VOICE_STATE_SPEAKER (8) is never written by this
 * object.  Finding F8831.
 */
#define VOICE_STATE_COMMAND		0x00
#define VOICE_STATE_RX			0x01
#define VOICE_STATE_TX			0x02
#define VOICE_STATE_DUPLEX		(VOICE_STATE_TX | VOICE_STATE_RX)
#define VOICE_STATE_SPEAKER		0x08

#define VOICE_STATUS_OK			1
#define VOICE_STATUS_ERROR		2
#define VOICE_STATUS_CONNECT		3

/*
 * The fourteen messages `voice_modem` can answer, which is what
 * `VOICE_process`'s jump table at `.rodata` 0x28 dispatches on.
 *
 * TEN OF THEM WEAR THE AUTHOR'S OWN IDENTIFIER.  Arms 0..9 each print a string
 * that is "voice: STRM_VCE" followed by the constant's name, so the spelling
 * below is transcribed rather than invented (evidence class 1).  Arms 10..13
 * print English rather than an identifier -- "BUSY", "DIALTONE", "FAX Tone",
 * "Underrun" -- so those four keep the author's WORD and not his spelling.
 *
 * The last four are the call-progress reports, and they close a chain that
 * runs the whole depth of the service: `detector_progress` names its two
 * cadence results `cadence_busy` and `cadence_dial` (F8800), `_handle_status`
 * maps its codes 1, 2 and 4 to 10, 11 and 12, and those three arms here send
 * <DLE>'b', <DLE>'d' and <DLE>'c' to the host -- the V.253 shielded codes for
 * busy, dial tone and fax calling tone.  Nothing was assumed at any hop.
 * Finding F8832.
 */
#define VOICE_NO_MESSAGE			0
#define VOICE_OK				1
#define VOICE_START_ONLINE			2
#define VOICE_START_TX				3
#define VOICE_START_RX				4
#define VOICE_START_DUPLEX			5
#define VOICE_PURGE				6
#define VOICE_ERROR				7
#define VOICE_START_ONLINE_AFTER_ABORT		8
#define VOICE_CANCEL				9
#define VOICE_BUSY				10	/* "BUSY"      */
#define VOICE_DIALTONE				11	/* "DIALTONE"  */
#define VOICE_FAX_TONE				12	/* "FAX Tone"  */
#define VOICE_UNDERRUN				13	/* "Underrun"  */

/* The largest message the switch has an arm for; `cmp $0xd,%ecx; ja` at 0xe54. */
#define VOICE_MESSAGE_MAX			13

/*
 * The four shielded report characters, from the object's own immediates
 * (`movb $0x62`, `$0x64`, `$0x63`, `$0x75`) and from V.253's table of them.
 * Each is sent as <DLE> then the letter -- see VOICE_DLE in voice.h.
 */
#define VOICE_REPORT_BUSY		'b'
#define VOICE_REPORT_DIALTONE		'd'
#define VOICE_REPORT_FAX_TONE		'c'
#define VOICE_REPORT_UNDERRUN		'u'

/*
 * The three sample rates `VOICE_create` knows, and the `RcFixed` conversion
 * modes it picks for the two that are not the pump's own 8 kHz.
 *
 * The mode numbers are the object's immediates; what they MEAN comes from
 * this tree's own `fixedRc_UpFact` / `fixedRc_DownFact` tables in
 * src/core/fixedrc.c, and every one of them checks out as the conversion the
 * rate needs: mode 3 is 5/6 (9600 -> 8000), mode 2 is 6/5 (8000 -> 9600),
 * mode 5 is 1/6 (48000 -> 8000) and mode 4 is 6/1 (8000 -> 48000).  So the
 * two members are unambiguously the line-in and line-out converters.
 *
 * 8000 needs neither and gets neither: the object jumps over both creations,
 * leaving the two pointers NULL from the memset -- see D1022 for what
 * `RcFixed_Resample` then does with them.
 */
#define VCE_RATE_8000			8000
#define VCE_RATE_9600			9600
#define VCE_RATE_48000			48000

#define VCE_RC_MODE_9600_IN		3
#define VCE_RC_MODE_9600_OUT		2
#define VCE_RC_MODE_48000_IN		5
#define VCE_RC_MODE_48000_OUT		4

/*
 * The block size, in samples: `rate * 160 / 8000`.
 *
 * THE DIVISOR IS 8000 AND NOT 1000, and it takes the magic number to see it:
 * the object computes `rate * 5 * 32` and then `mul $0x10624dd3` / `shr $9`
 * (0x7d5-0x801).  0x10624dd3 is 274877907, and 2^41 / 274877907 is 8000 to
 * eleven digits, so the pair is a reciprocal divide by 8000 -- not the divide
 * by 1000 the same constant performs at a shift of 6.  It gives 160 at 8 kHz,
 * 192 at 9.6 kHz and 960 at 48 kHz, and 160 is what the object then hardcodes
 * as the resampler's output limit.  Finding F8833.
 */
#define VCE_BLOCK_REF_RATE		8000
#define VCE_BLOCK_REF_SAMPLES		160

/*
 * One of the two rate-conversion rings, 0x310 bytes.
 *
 * It is a DOUBLE BUFFER dressed as a ring: `blk` is 0 or `block` and selects
 * which half `RcFixed_Resample` is working on, while `wr` or `rd` walks the
 * whole 2*block window a memcpy at a time.  Each ring uses only one of the two
 * cursors -- the input ring is filled through `wr` and the output ring drained
 * through `rd` -- which is why both are here and why either one is dead in
 * half the object's uses of this type.
 *
 * `data` HOLDS 384 SAMPLES AND THE CURSORS WRAP MODULO `2 * block`, so the
 * type is only self-consistent while `block <= 192`.  That is true at 8000
 * (160) and exactly true at 9600 (192), and false at 48000 (960).  The size
 * is not an inference: 0xe64 + 0x310 is 0x1174, the second ring's offset, and
 * 0x1174 + 0x310 is 0x1484, the allocation exactly.  Deviation D1023.
 */
#define VCE_RING_SAMPLES		384

struct vce_ring {
	int		count;	/* +0x00 samples in the ring            */
	unsigned int	wr;	/* +0x04 fill cursor, samples           */
	unsigned int	rd;	/* +0x08 drain cursor, samples          */
	unsigned int	blk;	/* +0x0c 0 or `block`: the half in use  */
	short		data[VCE_RING_SAMPLES];		/* +0x10       */
};

/*
 * The VOICE service object.  0x1484 bytes, which is not an inference either:
 * `VOICE_create` passes that to `sysdep_malloc` at 0x73b and to
 * `sysdep_memset` at 0x758.
 *
 * THE DIRECTION NAMES ARE THE OBJECT'S, VERIFIED THROUGH `voice_modem`.
 * `from_line` is filled from the input ring and handed to `voice_modem` in the
 * slot voice.h calls `tx_flt`; `to_line` is what comes back in `rx_flt` and is
 * converted and resampled out to the line.  Those two names look crossed and
 * are not: voice.h's rx/tx are named from the DSP's side of the HOST link, so
 * `rx_lin`/`rx_flt` is what the DSP received from the host and `tx_lin`/
 * `tx_flt` is what it transmits to the host.  `host_in` lands in `rx_lin` and
 * `host_out` comes back through `tx_lin`, which settles it both ways round.
 * Finding F8834 -- and nothing in voice.h needs renaming.
 *
 * `lin` is shared by both directions, one after the other in the same block:
 * the input resampler writes it, the float conversion reads it, and then the
 * reverse conversion overwrites it for the output resampler.
 */
struct vce {
	void			*modem;		/* +0x0000 host handle    */
	struct voice_ctx	*voice;		/* +0x0004 voice_create   */
	struct voice_info	*info;		/* +0x0008 MDMPRM_VOICEINFO */
	int			state;		/* +0x000c VOICE_STATE_*  */
	int			last_message;	/* +0x0010 last VOICE_*   */
	int			host_count;	/* +0x0014 bytes the host owes */
	struct rc		*rc_in;		/* +0x0018 line -> 8 kHz  */
	struct rc		*rc_out;	/* +0x001c 8 kHz -> line  */
	unsigned char		host_in[0x400];	 /* +0x0020               */
	unsigned char		host_out[0x400]; /* +0x0420               */
	short			lin[160];	/* +0x0820 both ways      */
	float			from_line[160];	/* +0x0960 voice_modem tx_flt */
	float			to_line[160];	/* +0x0be0 voice_modem rx_flt */
	unsigned int		block;		/* +0x0e60 samples/block  */
	struct vce_ring		out_ring;	/* +0x0e64 to the line    */
	struct vce_ring		in_ring;	/* +0x1174 from the line  */
	/* 0x1484 bytes in total -- VOICE_create's allocation size. */
};

/*
 * The two float scale factors, and they are NOT reciprocals of each other in
 * type.  Coming in the object multiplies a `short` by the DOUBLE 6.25e-05
 * (`.rodata.cst8` 0x0, `fldl` hoisted out of the loop) and narrows the product
 * to `float`; going out it multiplies a `float` by the FLOAT 16000.0f
 * (`.rodata.cst4` 0x0, `flds` hoisted) and converts to `short`.  Both
 * spellings are forced by the operand size in the instruction, and swapping
 * either one moves the x87 sequence.
 *
 * 16000 rather than 32767 means everything this path carries is 6 dB below
 * full scale, which is the same scale `detector_create`'s cadence arm uses
 * (F8801) -- so the two agree rather than one being wrong.
 */
#define VCE_LINE_IN_SCALE		6.25e-05	/* double, 1/16000 */
#define VCE_LINE_OUT_SCALE		16000.0f	/* float           */

/*
 * The two-byte host command `VOICE_process` watches for while recording:
 * <DLE>'!', read as one little-endian 16-bit word (`cmpw $0x2110,0x20(%edi)`
 * at 0x1014).  Seeing it aborts the recording through VOICE_CMD_ABORT.
 */
#define VCE_HOST_ABORT_WORD		0x2110

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VCE_H */
