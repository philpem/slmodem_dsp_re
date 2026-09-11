/*
 * period_compat.h -- teach GCC 3.4.2 the constructs OUR APPARATUS uses.
 *
 * Force-included (`-include`) by the period build.  Never included by the
 * modern build, and never referenced from src/.
 *
 * THE LINE THIS FILE DRAWS, and it is the whole point of the period build.
 *
 * The reconstruction exists to say what the author wrote.  The author wrote
 * it for GCC 3.4.2, so anything in `src/` that GCC 3.4.2 rejects is something
 * the author cannot have written -- a defect in the reconstruction, to be
 * fixed in `src/`.
 *
 * But this tree also carries VERIFICATION APPARATUS the author never had:
 * compile-time offset assertions pinning every struct to the object's map
 * (finding F230, tools/offcheck.py).  Those are ours.  They may use whatever
 * the modern compiler offers, and where the period compiler lacks it, the
 * shim belongs HERE -- outside the reconstruction -- rather than in the
 * source being reconstructed.
 *
 * So: a shim for the apparatus goes in this file.  A shim for the code goes
 * nowhere, and the thing it was hiding gets fixed instead.
 */

#ifndef DSPLIB_PERIOD_COMPAT_H
#define DSPLIB_PERIOD_COMPAT_H

/*
 * __builtin_offsetof arrived in GCC 4.0.  127 sites use it, all of them
 * assertions of the form
 *
 *	typedef char x_off_field[ ((int)__builtin_offsetof(T, field) == N) ? 1 : -1 ];
 *
 * The classic null-pointer expansion is what <stddef.h> itself used before
 * the builtin existed, and it is a constant expression to GCC 3.4 in exactly
 * this position -- verified on both a POD and a polymorphic class, where it
 * correctly reports the vptr's four bytes.
 *
 * Using a non-zero sentinel base (0xDEADBEEF) avoids GCC 3.4.2's C++ warning
 * "invalid access to non-static data member of NULL object" while producing
 * the same compile-time offset.
 *
 * TWO ARGUMENTS OR THREE, because one site is
 *
 *	__builtin_offsetof(Scrambler<unsigned char, int>, field)
 *
 * and the preprocessor does not know that the comma is inside `<>` -- it
 * counts three arguments and a fixed-arity macro rejects the call.  The
 * builtin has no such trouble because it is parsed, not expanded.
 *
 * Splitting on argument count works because macro arguments are token
 * sequences: `Ta` receives `Scrambler<unsigned char`, `Tb` receives `int>`,
 * and pasting `Ta , Tb` reassembles exactly the tokens that were written.
 * That is the whole trick, and it is why the three-argument form spells the
 * type `Ta, Tb` rather than trying to rejoin it any more cleverly.
 */
#define DSPLIB_OFF2(T, m) \
	((unsigned long)&(((T *)0xDEADBEEF)->m) - 0xDEADBEEF)
#define DSPLIB_OFF3(Ta, Tb, m) \
	((unsigned long)&(((Ta, Tb *)0xDEADBEEF)->m) - 0xDEADBEEF)
#define DSPLIB_OFF_PICK(_1, _2, _3, NAME, ...)	NAME
#define __builtin_offsetof(...) \
	DSPLIB_OFF_PICK(__VA_ARGS__, DSPLIB_OFF3, DSPLIB_OFF2, )(__VA_ARGS__)

#endif /* DSPLIB_PERIOD_COMPAT_H */
