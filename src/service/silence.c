/*
 * silence.c -- the silence detector's create/delete/query, and `_status`.
 *
 * Emission order follows the object: _status 0xb02e0,
 * silence_is_more_then 0xb0360, silence_create 0xb03b0, silence_delete
 * 0xb0410.  silence_progress (0xb0420) belongs between the last two and is
 * not written yet -- it reads the blob's LOCAL `silence_level_table`.
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
silence_is_more_then(struct silence *s, float ms)
{
	return s->count > (int)(10.0f * ms);
}

/*
 * Initialise, allocating when handed NULL.  The allocation IS checked here,
 * where FIFO8_create's is not -- both are the object's own shape.
 */
struct silence *
silence_create(struct silence *s, int a, int b)
{
	if (s == 0) {
		s = (struct silence *)sysdep_malloc(sizeof(*s));
		if (s == 0)
			return 0;
	}
	s->int_00 = a;
	s->int_04 = b;
	s->count = 0;
	s->short_0a = 0;
	s->short_0c = 0;
	s->int_10 = 0;
	return s;
}

/* A tail call and nothing else -- five bytes in the object. */
void
silence_delete(struct silence *s)
{
	sysdep_free(s);
}
