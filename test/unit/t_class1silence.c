/*
 * t_class1silence.c -- differential test of `_put_silence`.
 *
 * `.text` 0x092b70, 28 bytes: one loop that zeroes `count` elements of
 * `buf`.  The interesting property is the RETURN VALUE -- the object leaves
 * the loop counter in `%eax` rather than reloading `count`, so it answers
 * `count` on a normal exit but 0 (not a negative `count`) when the loop
 * never ran -- and that is what this test is built to catch: a naive
 * `return count;` would match on every positive input and diverge only on
 * zero or negative ones.
 */

#include <stddef.h>

#include "harness.h"
#include "dsplib/class1.h"

extern int ref__put_silence(short *buf, int count);

#define BUF_MAX		64
#define GUARD		16
#define POISON		((short)0x5a5a)

static void
poison(short *b, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = POISON;
}

static void
run_case(int count, long input)
{
	short a[BUF_MAX + GUARD];
	short b[BUF_MAX + GUARD];
	int ra, rb;
	size_t i;
	int tail_a = 0, tail_b = 0;

	poison(a, BUF_MAX + GUARD);
	poison(b, BUF_MAX + GUARD);

	ra = _put_silence(a, count);
	rb = ref__put_silence(b, count);

	diff_eq_int("_put_silence return, input %ld", ra, rb, input);

	/* The written region, both sides -- must be zero as far as it goes. */
	for (i = 0; i < (size_t)(count > 0 ? count : 0) && i < BUF_MAX; i++) {
		diff_eq_int("buf[%ld] == 0 (ours), input %ld",
			    a[i] == 0, 1, input);
		diff_eq_int("buf[%ld] == 0 (blob), input %ld",
			    b[i] == 0, 1, input);
	}

	/* Nothing past the written region moved, on EITHER side. */
	for (i = (size_t)(count > 0 ? count : 0); i < BUF_MAX + GUARD; i++) {
		if (a[i] != POISON)
			tail_a++;
		if (b[i] != POISON)
			tail_b++;
	}
	diff_eq_int("bytes written past count, ours, input %ld", tail_a, 0,
		    input);
	diff_eq_int("bytes written past count, blob, input %ld", tail_b, 0,
		    input);
}

static int
test_put_silence(void)
{
	static const int counts[] = { 0, 1, 2, 5, 20, 63, 64, -1, -5, -100 };
	size_t i;

	diff_begin("_put_silence");

	for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
		run_case(counts[i], (long)counts[i]);

	return diff_end();
}

int
main(void)
{
	return test_put_silence();
}
