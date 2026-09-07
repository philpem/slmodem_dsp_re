/*
 * detector.h -- the voice service's tone detector: what it listens for, and
 * what it reports.
 *
 * The object is 0x38 bytes and its whole surface is now reconstructed:
 * `detector_create` and `detector_progress` in src/service/detector.c, the
 * three setters and `detector_delete` in src/service/beepgen.c.
 *
 * WHAT IT IS.  One `detector` watches an 8 kHz receive stream for six things
 * at once, and `enable` is a mask of which:
 *
 *	0x01	DTMF, through the `struct dtmf` receiver at +0x04
 *	0x02	1300 Hz	 the four `TONE_detect` resonators at +0x14,
 *	0x04	1100 Hz	 each an `fdsp_tone` built from TONEamode_CFG
 *	0x08	2100 Hz	 with its own frequency substituted
 *	0x10	2225 Hz
 *	0x20	busy and dial tone, through the two `cadence` detectors
 *
 * `detector_create` sets all six.
 *
 * HOW IT REPORTS, and it is one of two ways -- see `output_mode`.  In stream
 * mode every event is two bytes appended to the caller's buffer, DLE (0x10)
 * then a letter; in status mode the event becomes the function's return code
 * instead, and only the LAST event of a block survives.  The letters and the
 * codes are the object's own tables (`tone_char`, `status`, and the two 0x62
 * / 0x64 literals in the cadence arm):
 *
 *	event		letter	status code
 *	busy		'b'	1
 *	dial tone	'd'	2
 *	1300 Hz		'e'	3
 *	1100 Hz		'c'	4
 *	2100 Hz		'a'	5
 *	2225 Hz		'f'	6
 *	a DTMF digit	the digit, and NEVER a status code
 *
 * The DTMF arm is the odd one out twice over: it writes its two bytes inline
 * rather than through `_status`, it ADVANCES the caller's cursor where every
 * other arm does not, and it has no status-mode form at all.  See
 * src/service/detector.c and finding F8805.
 */

#ifndef DSPLIB_DETECTOR_H
#define DSPLIB_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Values of struct detector's `output_mode`. */
#define DETECTOR_OUTPUT_STATUS		0
#define DETECTOR_OUTPUT_IN_STREAM	1

/*
 * Bits of `enable`.
 *
 * DTMF and CADENCE are immediates in `detector_progress` (`test $0x1,%dl` at
 * 0xad6fd, `test $0x20,%dl` at 0xad736).  The four tone bits are the object's
 * own `enable` table at .rodata 0xeecc, indexed by the same subscript as
 * `tone[]`, so the frequency each bit selects is the frequency at that index
 * of the `tone` table -- not an inference.
 */
#define DETECTOR_ENABLE_DTMF		0x01
#define DETECTOR_ENABLE_1300		0x02
#define DETECTOR_ENABLE_1100		0x04
#define DETECTOR_ENABLE_2100		0x08
#define DETECTOR_ENABLE_2225		0x10
#define DETECTOR_ENABLE_CADENCE		0x20
/* What detector_create installs: `movw $0x3f,0x0(%ebp)` at 0xad54a. */
#define DETECTOR_ENABLE_ALL		0x3f

/* How many resonators, and therefore how long `tone[]` and `counter[]` are. */
#define DETECTOR_TONES			4

struct cadence;
struct dtmf;
struct fdsp_tone;

/*
 * The host callback `detector_create` takes.
 *
 * It is called ONCE, as `f(modem, 73)`, and 73 is
 * SREG_VOICE_DIALTONE_DETECT_DELAY (see dsplib/vce.h) -- so this is the
 * S-register getter, not an opaque hook.  `voice_create` passes the
 * `struct beepgen_config` callback it holds, which `VOICE_create` fills with
 * `vce_get_sreg`; that chain is what types the argument and the result, and
 * it is the same shape `silence_create` already takes.
 *
 * The RESULT is unsigned: the object divides `r * 50` by four with `shr`
 * (0xad531), and a signed quotient would have to be `sar`.
 */
typedef unsigned int (*detector_sreg_fn)(void *modem, int num);

struct detector {
	/*
	 * Which of the six detectors run.  Read `movzwl` at every one of the
	 * four sites in `detector_progress`, and re-read from memory after
	 * every call -- so `detector_progress` consults the live field rather
	 * than a copy, and a callee that changed it would be obeyed.
	 */
	unsigned short enable;		/* +0x00 DETECTOR_ENABLE_*           */
	/*
	 * +0x0002 was `pad_0002[2]` -- REMOVED (finding F10151): `enable`
	 * ends at +0x02 and the struct's own 4-byte alignment (forced by
	 * `dtmf` and every pointer/int after it) leaves exactly this gap
	 * ahead of `dtmf`. `dis.py` over `detector_create`/`detector_delete`/
	 * `detector_progress`/`detector_set_enable` finds no genuine access
	 * to offset 0x02/0x03 -- one `lea 0x2(%edx),%eax` in
	 * `detector_progress` is `dtmf_progress`'s return value plus 2, an
	 * unrelated arithmetic computation, not a field read (the same
	 * function's `mov 0x4(%esi),%edx` three instructions earlier is the
	 * genuine `dtmf` field access, corroborating +0x04's own offset).
	 */

	/*
	 * The DTMF receiver.  Typed by its two users: `create_dtmf` returns
	 * it into this field (0xad4bd) and `dtmf_progress` is handed it
	 * (0xad8b1).  `detector_progress` also reads `->held` (+0x90)
	 * directly at 0xad709 to hold the tone arm off while a digit is
	 * being reported.
	 */
	struct dtmf *dtmf;		/* +0x04 sysdep_free'd unconditionally */

	/*
	 * Three cadence slots, of which `detector_create` builds two.
	 *
	 * `cadence_busy` and `cadence_dial` are named from the object's own
	 * format strings -- "busy detected by cadence\n" (.rodata.str1.1
	 * 0x5105) is printed for +0x0c and "dial detected by cadence\n"
	 * (0x511f) for +0x10 -- and confirmed independently by the
	 * `cadence_setup.tone` each is created with, CADENCE_TONE_BUSY (0)
	 * and CADENCE_TONE_DIAL (1).
	 *
	 * +0x08 stays neutral: `detector_create` only zeroes it on the
	 * allocating path, nothing ever builds it, and `detector_delete`'s
	 * NULL guard is the only code that looks at it.
	 */
	struct cadence *cadence_0008;	/* +0x08 never built                 */
	struct cadence *cadence_busy;	/* +0x0c */
	struct cadence *cadence_dial;	/* +0x10 */

	/* One resonator per frequency; see DETECTOR_ENABLE_* above. */
	struct fdsp_tone *tone[DETECTOR_TONES];
					/* +0x14 +0x18 +0x1c +0x20 */

	/*
	 * Consecutive blocks in which `TONE_detect` returned 0 for this
	 * tone -- reset to zero by any non-zero verdict, and reported once
	 * it passes the object's `tone_integration_threshold`.  Named from
	 * that symbol, which is the author's.  `unsigned short`: loaded
	 * `movzwl` at 0xad85d and compared unsigned (`jae`) at 0xad870.
	 */
	unsigned short tone_integration[DETECTOR_TONES];
					/* +0x24 +0x26 +0x28 +0x2a */

	/*
	 * Written 0 by `detector_create` (0xad537) and read by nothing in
	 * the whole object.  Modelled, not named.
	 */
	int	int_002c;		/* +0x2c */

	/*
	 * SREG_VOICE_DIALTONE_DETECT_DELAY, in seconds, times 50/4.
	 *
	 * The scale is exactly what the object computes -- two `lea`s for
	 * *5, an `add` for *2 and `shr $2` -- and 12.5 per second is one
	 * per 640 samples at 8 kHz, which is the order of the dial-tone
	 * cadence's own verdict interval.  That last step is arithmetic and
	 * not evidence: NOTHING in the 1.2 MB reads this field back, so the
	 * unit is derived from the S-register's and not confirmed by a use.
	 */
	int	dialtone_detect_delay;	/* +0x30 */

	int	output_mode;		/* +0x34 DETECTOR_OUTPUT_*           */
	/* 0x38 bytes in total -- detector_create's allocation size. */
};

/**
 * @brief Build a tone/DTMF/cadence detector.
 *
 * @param d        NULL allocates 0x38 bytes and returns NULL if that
 *                 fails; anything else is re-initialised in place and
 *                 the existing dtmf, cadence and tone objects are reused
 *                 rather than replaced.
 * @param modem    The host's modem object, passed to @p get_sreg.
 * @param get_sreg May be NULL, in which case `dialtone_detect_delay` is
 *                 0 and the detector is otherwise identical.
 * @return The detector (@p d, or the newly allocated one), or NULL on
 *         allocation failure.
 */
struct detector *detector_create(struct detector *d, void *modem,
				 detector_sreg_fn get_sreg);

/**
 * @brief Set which of the six detectors run.
 * @param d       The detector.
 * @param enable  A mask of `DETECTOR_ENABLE_*` bits. Whole-word store;
 *                nothing here reads the value back.
 */
void detector_set_enable(struct detector *d, unsigned short enable);

/**
 * @brief Tear a detector down.
 *
 * Frees the +0x04 block, then all four tones, then whichever of the
 * three cadences are non-NULL, then the detector itself. The tone
 * pointers are NOT guarded -- `TONE_delete` is handed all four whatever
 * they hold -- and neither is the +0x04 block.
 *
 * @param d  The detector to free.
 */
void detector_delete(struct detector *d);

/** @brief Switch to status-code reporting; see the file comment. */
void detector_set_output_status(struct detector *d);
/** @brief Switch to in-stream (DLE-escaped) reporting; see the file comment. */
void detector_set_output_in_stream(struct detector *d);

/**
 * @brief Run one block of samples through whichever detectors are enabled.
 *
 * @param d        The detector, updated in place.
 * @param samples  @p count samples, in the -1..+1 float domain the voice
 *                 path uses.
 * @param count    Number of samples.
 * @param out      The caller's DLE-escaped event stream. Touched only in
 *                 #DETECTOR_OUTPUT_IN_STREAM mode.
 * @param outlen   Count of bytes already in @p out; advanced by however
 *                 many are appended. Touched only in
 *                 #DETECTOR_OUTPUT_IN_STREAM mode.
 * @return The last status code the block produced, or 0 -- in stream
 *         mode this is always 0, because every arm that would set it
 *         takes the other branch.
 */
int detector_progress(struct detector *d, float *samples, short count,
		      unsigned char *out, unsigned short *outlen);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_DETECTOR_H */
