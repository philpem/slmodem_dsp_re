/*
 * dialer.h -- Dialler: turning a dial string into tones and pulses.
 *
 * The dialler holds the string it is working through, a copy of the country's
 * dialling rules, and where it has got to.  It is not allocated: it lives
 * inside the call-progress supervisor, which is why `DialerCreate` takes an
 * object rather than returning one.
 */

#ifndef DSPLIB_DIALER_H
#define DSPLIB_DIALER_H

#include "dsplib/dialercfg.h"

/*
 * The dial string lives at the very start of the object and the configuration
 * begins immediately after it, at +0x64.  That is not a coincidence: it is
 * why `AnalyseDialString` rejects anything longer than 100 characters.
 */
#define DIALER_MAX_STRING	100

/*
 * How `AnalyseDialString` grades a string.  The names are the object's own --
 * it carries them at .rodata+0x613c for its debug output -- and
 * `IsDialStringInvalid` is exactly `grade <= DIALER_INVALID`.
 */
#define DIALER_FATAL		0	/* too long to store             */
#define DIALER_INVALID		1
#define DIALER_TOLERABLE	2	/* oddities, but dial it anyway  */
#define DIALER_VALID		3

struct dialer {
	char	string[DIALER_MAX_STRING];	/* +0x00 */
	struct dialer_cfg cfg;			/* +0x64 */

	/*
	 * Index of the last character that would actually send something --
	 * a digit rather than a pause or a modifier.  -2 when there is none,
	 * and only written when `AnalyseDialString` is asked to store it.
	 */
	int	last_digit;			/* +0xa0 */

	int	f_a4;				/* +0xa4 */
	int	state;				/* +0xa8 */
	int	pos;				/* +0xac */
	int	f_b0, f_b4, f_b8, f_bc;		/* +0xb0 */
	int	f_c0, f_c4, f_c8, f_cc, f_d0;	/* +0xc0 */
	void	*modem;				/* +0xd4 */
};

/*
 * Grade a dial string.
 *
 * NOT the C calling convention: the original takes `d` in `eax` and `s` in
 * `edx`, with `store` on the stack -- GCC's regparm(2), used for calls that
 * never leave Dialer.c.  Anything declaring this for the differential test
 * must say `__attribute__((regparm(2)))` or it will pass arguments the callee
 * never reads.  See finding 51.
 *
 * `store` asks for `d->last_digit` to be updated; the grade is returned
 * either way.
 */
int AnalyseDialString(struct dialer *d, const char *s, int store);

/* True when the string is too poor to dial: `AnalyseDialString(...) <= 1`. */
int IsDialStringInvalid(struct dialer *d, const char *s);

#endif /* DSPLIB_DIALER_H */
