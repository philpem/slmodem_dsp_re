/*
 * silence.c -- the voice path's silence detector, and `_status`.
 *
 * Emission order follows the object: _status 0xb02e0,
 * silence_is_more_then 0xb0360, silence_create 0xb03b0, silence_delete
 * 0xb0410, silence_progress 0xb0420.
 *
 * `_status` is defined first because silence_progress needs it INLINED --
 * the object has two copies of its three stores inside silence_progress's
 * two escape arms and no call, which is what a static-ish helper earlier in
 * the same translation unit gives at -O3.
 */

#include "dsplib/silence.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/*
 * Two bytes appended to a stream that escapes them: DLE (0x10) and a code.
 * `*len` is advanced by two, not set -- the caller owns the cursor.
 *
 * The debug line runs BEFORE the stores in the object, in both arms of the
 * gate (the tail is duplicated there, which is the compiler's doing).
 */
void
_status(unsigned char *out, unsigned short *len, char code)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DLE %d\n", code);
	*len += 2;
	out[0] = 0x10;
	out[1] = code;
}

/*
 * The truncation is a C cast, not a rounding: the object saves the x87
 * control word, forces round-toward-zero, does one `fistpl`, and restores
 * it.  The comparison that follows is signed and 32-bit, on a `count` that
 * is loaded zero-extended.
 */
int
silence_is_more_then(struct silence *s, float t)
{
	return s->count > (int)(10.0f * t);
}

/*
 * Initialise, allocating when handed NULL.  The allocation IS checked here,
 * where FIFO8_create's is not -- both are the object's own shape.
 */
struct silence *
silence_create(struct silence *s, void *obj,
	       unsigned int (*query)(void *obj, int what))
{
	if (s == 0) {
		s = (struct silence *)sysdep_malloc(sizeof(*s));
		if (s == 0)
			return 0;
	}
	s->obj = obj;
	s->query = query;
	s->count = 0;
	s->nsamp = 0;
	s->saw_signal = 0;
	s->energy = 0.0f;
	return s;
}

/* A tail call and nothing else -- five bytes in the object. */
void
silence_delete(struct silence *s)
{
	sysdep_free(s);
}

/*
 * The four energy thresholds, indexed by the answer to SILENCE_PARAM_LEVEL.
 *
 * A LOCAL object in `.data` at 0x84d4 in the blob, sixteen bytes, and the
 * name is the blob's own.  Row 0 is unreachable -- silence_progress returns
 * before the table when the setting is 0 -- and its -1.0f reads as a
 * sentinel.  The three live rows are the exact values of the object's
 * words, spelled to the last digit for the reason F8750 gives: a constant
 * read as a 32-bit word is exact evidence and the decimal you would guess
 * for it is not.
 *
 * They are energies per sample of a signal scaled to +-1.0, so
 * SILENCE_FULLSCALE2 turns them back into the counts^2 the debug line
 * prints: 2700, 7500 and 24300.
 */
static float silence_level_table[4] = {
	-1.0f,
	0.000002514724428692716173827648162841796875f,
	0.0000069853458626312203705310821533203125f,
	0.000022632519176113419234752655029296875f
};

/* Samples per analysis block: 800, which is 100 ms at 8 kHz. */
#define SILENCE_BLOCK		800.0f

/* 1/800, and the object holds it as a DOUBLE (`fmull`). */
#define SILENCE_BLOCK_RECIP	0.00125

/*
 * Full scale squared.  The object's word is 0x4e7ffc00, which is
 * fl(32767 * 32767) -- 1073676289 rounded down to the nearest float -- so
 * the debug line reports the energy in counts^2 against a 15-bit full scale.
 */
#define SILENCE_FULLSCALE2	(32767.0f * 32767.0f)

/*
 * One buffer of samples through the detector.
 *
 * The energy accumulator lives in an x87 register across the whole loop and
 * is only NARROWED where it is stored: `s->energy` gets a float copy every
 * sample, but the running sum the next sample adds to is the register's
 * extended value.  That is the object's shape (one `flds 0x10(%ebx)` before
 * the loop and stores inside it, never a reload) and it is a real numerical
 * property, not a scheduling detail -- a `-ffloat-store` build would answer
 * differently on a long block.
 *
 * The two settings come from the owner through `s->query`.  The first is
 * read once per call and disables the detector outright when it is 0 or
 * above 3; the second is read again on EVERY silent block, so an owner that
 * changes it mid-buffer is obeyed immediately.
 */
void
silence_progress(struct silence *s, float *buf, short n,
		 unsigned char *out, unsigned short *len)
{
	unsigned short lvl;
	unsigned short i;
	float acc;

	lvl = (unsigned short)s->query(s->obj, SILENCE_PARAM_LEVEL);
	if (lvl > 3 || lvl == 0)
		return;

	acc = s->energy;
	for (i = 0; i < n; i++) {
		int level;

		s->nsamp++;
		acc = acc + buf[i] * buf[i];
		if (s->nsamp < SILENCE_BLOCK) {
			s->energy = acc;
			continue;
		}

		acc = acc * SILENCE_BLOCK_RECIP;
		s->energy = acc;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "energy = %d, threshold = %d\n",
			    (int)(acc * SILENCE_FULLSCALE2),
			    (int)(silence_level_table[lvl]
				  * SILENCE_FULLSCALE2));

		if (acc > silence_level_table[lvl]) {
			s->count = 0;
			s->saw_signal = 1;
		} else {
			/*
			 * The object converts the answer as UNSIGNED and back
			 * through the x87 (`fildll` on a zero-extended
			 * doubleword, then a truncating `fistpl`), which is
			 * what this cast chain spells; a plain `(int)` would
			 * emit neither instruction.
			 */
			level = (int)(double)s->query(s->obj,
						      SILENCE_PARAM_TIME);
			s->count++;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "silence counter %d, level %d\n",
				    s->count, level);
			if (s->count > level) {
				s->count = 0;
				if (s->saw_signal)
					_status(out, len, 0x71);
				else
					_status(out, len, 0x73);
			}
		}
		s->nsamp = 0;
		acc = 0.0f;
		s->energy = acc;
	}
}
