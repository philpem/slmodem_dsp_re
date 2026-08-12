/*
 * fft.cpp -- reconstructed from dsplibs.o, 0x536c0 (four1, 365 bytes) and
 * 0x53830 (realfft, 505 bytes).
 *
 * THIS FILE NEEDS -funsafe-math-optimizations AND THAT IS A MEASUREMENT.
 *
 * Both bodies compute their twiddle angles with the x87 `fsin` instruction
 * -- 0x5376a and 0x5378c in four1, 0x5388c and 0x53890 in realfft.  GCC
 * expands `sin()` inline to `fsin` only when `flag_unsafe_math_optimizations`
 * is set; without it the call goes to libm.  The same flag is what finding
 * 876 records `Psd::process` needing for its `fldlg2`/`fyl2x` log10 arms,
 * which is consistent: the original's build of this part of the library had
 * it on.
 *
 * THE DIFFERENTIAL TIER CANNOT SEE THAT, and saying so is the point.  Built
 * without the pragma the file contains zero `fsin`, calls libm, and STILL
 * passes all 489,322 checks (finding 833).  It is not that the two agree --
 * `fsin` returns an 80-bit result and glibc's `sin` a correctly-rounded
 * 64-bit one, so they differ around the 54th bit -- it is that the difference
 * is 2^-53 relative in a quantity whose effect is finally rounded to a
 * 24-bit float.  Contrast `tempr` below, whose rounding is 2^-24 and shows up
 * in a tenth of all compared words.  So the flag is here on the disassembly's
 * authority and not the test's, which is what tier 3 is for
 * (docs/method/tiers.md); do not remove it on the grounds that the suite
 * stays green, and do not record the suite staying green as evidence the two
 * are equivalent.  It is a sample, on finding 248's argument.
 *
 * It is a `#pragma` rather than a Makefile flag so the blast radius is this
 * translation unit.  Nothing else in this tree is built with it and nothing
 * else should start being built with it as a side effect of this file.
 *
 * AND IT IS THE WHOLE FLAG, not the one sub-option `fsin` needs.  Adding
 * `no-associative-math` back on top -- on the argument that reassociation
 * could move the twiddle recurrences -- was tried and the OBJECT CONTRADICTS
 * IT.  `-2.0*wtemp*wtemp` parses as `((-2.0)*wtemp)*wtemp`, and the object
 * computes `wtemp*wtemp` first and multiplies by -2.0 afterwards:
 * `d8 c8  fmul %st(0),%st` at 0x5376c and again at 0x5389a.  That regrouping
 * is licensed by nothing but `-fassociative-math`, so the original had it.
 * With the flag whole, GCC 13 emits the same `fmul %st(0),%st` in both
 * places; with `no-associative-math` it emits `fld %st(0)` and two separate
 * multiplies, which is the reconstruction disagreeing with the object about
 * an instruction the compiler was not free to choose.  Both spellings pass
 * the differential test -- multiplying by -2.0 is exact either way -- so this
 * one was settled on the disassembly, which is what the codegen tier is for.
 *
 * The recurrence grouping that prompted the worry is unharmed: GCC 13 still
 * emits `(wr*wpr - wi*wpi)` and adds the old `wr` last, as 0x537ff-0x53809
 * and 0x53977-0x53989 do.
 *
 * WHAT `-freciprocal-math` DOES HERE IS EXACTLY NOTHING, and that is worth
 * writing down rather than worrying about.  The flag turns `A/len` into
 * `A * (1.0/len)`, which the object visibly did: 0x5374f divides 1.0 by
 * `mmax` and 0x53753 then multiplies by 6.28318530717959, and 0x53856 /
 * 0x53858 do the same with `n >> 1` and pi.  `mmax` and `n >> 1` are powers
 * of two on every input these functions accept, `1.0/2^k` is exact, and
 * multiplying by an exact power of two is exact -- so the reciprocal form
 * and the divide form agree bit for bit.  On a non-power-of-two length they
 * would not, which is one more reason not to test one.
 *
 * THE CONSTANTS ARE THE OBJECT'S, read from .rodata:
 *   .rodata.cst8+0x168  0x401921FB54442D1C  6.28318530717959 (not 2*pi)
 *   .rodata.cst8+0x170  0x3FE0000000000000  0.5
 *   .rodata.cst8+0x178  0xC000000000000000  -2.0
 *   .rodata.cst8+0x180  0x400921FB54442D18  3.141592653589793 (pi, exactly)
 *   .rodata.cst4+0x4e4  0x3F000000          0.5f   -- c1
 *   .rodata.cst4+0x4e8  0xBF000000          -0.5f  -- c2, forward
 *   .rodata.cst4+0x4ec  0xC0000000          -2.0f
 * The two-pi constant is the truncated 15-digit literal and NOT the nearest
 * double to 2*pi -- the low word is ...2D1C where 2*pi is ...2D18 -- so it
 * has to be written out as the literal it is.  pi in realfft, by contrast,
 * IS the nearest double.  A single `M_PI` or `2*M_PI` would be wrong in one
 * of the two places.
 *
 * WHERE THE OBJECT ROUNDS, AND WHERE IT DOES NOT, IS THE HARD PART OF THIS
 * FILE.  Two deviations from the obvious spelling are here for it, and each
 * was ABLATED -- removed, rebuilt, watched fail, put back (finding 833).
 * Neither is decoration and neither may be tidied away.
 *
 * BOTH ARE GONE NOW, AND THE PERIOD COMPILER IS WHY (finding 1354).  They
 * were ablated under GCC 13 only, which could show that GCC 13 needs them
 * and could not show that the author wrote them.  `make period` can, and it
 * passes 155 of 155 with the plain source:
 *
 *   `volatile float tempr, tempi` in four1.  The object spills both to a
 *   single-precision stack slot in the middle of each butterfly --
 *   `fstps 0xc(%esp)` at 0x537a0 and 0x537b4, `flds` back at 0x537aa and
 *   0x537bb -- so the narrowing between the two halves of a butterfly is real
 *   behaviour.  GCC 3.4.2 does it FROM PLAIN SOURCE, running out of x87
 *   registers exactly as the author's compiler did, because this file's
 *   `four1` is Numerical Recipes' unchanged and so has the same shape.  The
 *   `volatile` only ever spoke to GCC 13.
 *
 *   `(double)` ON ONE OPERAND of each of realfft's four half-transform
 *   temporaries.  This one was worse than a codegen hint: it changed FLOAT
 *   arithmetic to DOUBLE, which is an alteration of what the author wrote and
 *   not merely of how it is compiled.  The object never narrows
 *   h1r/h1i/h2r/h2i -- they live on the x87 stack from 0x538e9 to 0x5394a and
 *   are never stored -- and GCC 3.4.2 keeps them there from the plain float
 *   expression.  The casts are out; `c1` and `c2` are `float` as the object
 *   has them, and so is the arithmetic.
 *
 * WHAT IT COSTS THE MODERN BUILD is 47,955 of 476,100 words, declared in
 * tools/gccdiverge.json.  A double temporary was tried for four1 --
 * `double tr = wr * data[j] - ...; tempr = (float)tr;` -- on the strength of
 * that spelling fixing src/pump/v90/Resampler.cpp, and it does not help here:
 * identical failure counts.  Under -fexcess-precision=fast GCC 13 keeps the
 * 80-bit register across the conversion, and only `volatile` moves it.
 *
 * WHAT WAS ABLATED AND TURNED OUT NOT TO MATTER is recorded too, because a
 * deviation nothing can see is worse than none.  Forcing realfft's `wi`,
 * `wpr` and `wpi` into memory with `volatile` -- which is where the object
 * keeps them, at 0x3c, 0x34 and 0x2c(%esp) -- changes nothing: GCC 13 already
 * gives them double stack slots, and the test passes identically either way.
 * The `volatile` came back out.
 *
 * The remaining widths are the object's: the twiddles wr/wi/wpr/wpi/wtemp and
 * theta are `double`, on `fstl`/`fldl` and 64-bit stack slots throughout.
 *
 * The shape is Numerical Recipes in C's `four1` and `realft` unchanged, down
 * to the one-based indexing and the SWAP macro; the disassembly was read
 * first and the correspondence noted afterwards, not the other way round.
 * See docs/findings.md 832 and 833.
 */

#pragma GCC optimize("unsafe-math-optimizations")

#include "dsplib/fft.h"

#include <math.h>

/*
 * The object swaps with three integer moves per float (0x536e4-0x536fc): a
 * plain three-way exchange through a temporary, which is what GCC emits for
 * this macro on a `float`.
 */
#define SWAP(a, b) \
	do { float swap_tmp = (a); (a) = (b); (b) = swap_tmp; } while (0)

/*
 * four1 -- 0x536c0, 365 bytes.
 *
 * Two phases with nothing between them.  The bit-reversal permutation runs
 * over odd `i` with the descending-mask loop at 0x53706, and every compare
 * in it is UNSIGNED in the object (`jae`, `jbe`, `seta`, `jb`), which is
 * what fixes `n`, `i`, `j`, `m`, `mmax` and `istep` as `unsigned long`
 * rather than `long`.  The Danielson-Lanczos stage then doubles `mmax` from
 * 2 until it reaches `n`, with `(double)mmax` produced by a 64-bit `fildll`
 * off a zero high word (0x5373e-0x53747) -- an unsigned conversion, so the
 * declared type is not `long` there either.
 */
void
four1(float *data, unsigned long nn, int isign)
{
	unsigned long n, mmax, m, j, istep, i;
	double wtemp, wr, wpr, wpi, wi, theta;
	float tempr, tempi;

	n = nn << 1;
	j = 1;
	for (i = 1; i < n; i += 2) {
		if (j > i) {
			SWAP(data[j], data[i]);
			SWAP(data[j + 1], data[i + 1]);
		}
		m = n >> 1;
		while (m >= 2 && j > m) {
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	mmax = 2;
	while (n > mmax) {
		istep = mmax << 1;
		theta = isign * (6.28318530717959 / mmax);
		wtemp = sin(0.5 * theta);
		wpr = -2.0 * wtemp * wtemp;
		wpi = sin(theta);
		wr = 1.0;
		wi = 0.0;
		for (m = 1; m < mmax; m += 2) {
			for (i = m; i <= n; i += istep) {
				j = i + mmax;
				tempr = wr * data[j] - wi * data[j + 1];
				tempi = wr * data[j + 1] + wi * data[j];
				data[j] = data[i] - tempr;
				data[j + 1] = data[i + 1] - tempi;
				data[i] += tempr;
				data[i + 1] += tempi;
			}
			wr = (wtemp = wr) * wpr - wi * wpi + wr;
			wi = wi * wpr + wtemp * wpi + wi;
		}
		mmax = istep;
	}
}

/*
 * realfft -- 0x53830, 505 bytes.  Calls four1 and nothing else.
 *
 * `theta` is negated for the inverse (0x53864) and the forward direction
 * transforms first (0x53a1b) while the inverse transforms last (0x539ee) --
 * the two `four1` call sites are the two ends of the function and each is
 * reached by exactly one of the `isign` arms.
 *
 * The separation loop walks two cursors: `i1`/`i2` upward from data[3] as an
 * index (0x538d4, `edx` starting at 4 and stepping 2) and `i3`/`i4`
 * downward from data[np3-4] as a pointer (0x538bf, stepping -8 bytes).  It
 * runs `i = 2 .. n>>2`, so for n = 4 it does not run at all.
 */
void
realfft(float *data, unsigned long n, int isign)
{
	unsigned long i, i1, i2, i3, i4, np3;
	float c1 = 0.5, c2;
	double h1r, h1i, h2r, h2i;
	double wr, wi, wpr, wpi, wtemp, theta;

	theta = 3.141592653589793 / (double)(n >> 1);
	if (isign == 1) {
		c2 = -0.5;
		four1(data, n >> 1, 1);
	} else {
		c2 = 0.5;
		theta = -theta;
	}
	wtemp = sin(0.5 * theta);
	wpr = -2.0 * wtemp * wtemp;
	wpi = sin(theta);
	wr = 1.0 + wpr;
	wi = wpi;
	np3 = n + 3;
	for (i = 2; i <= (n >> 2); i++) {
		i4 = 1 + (i3 = np3 - (i2 = 1 + (i1 = i + i - 1)));
		h1r = c1 * (data[i1] + data[i3]);
		h1i = c1 * (data[i2] - data[i4]);
		h2r = -c2 * (data[i2] + data[i4]);
		h2i = c2 * (data[i1] - data[i3]);
		data[i1] = h1r + wr * h2r - wi * h2i;
		data[i2] = h1i + wr * h2i + wi * h2r;
		data[i3] = h1r - wr * h2r + wi * h2i;
		data[i4] = -h1i + wr * h2i + wi * h2r;
		wr = (wtemp = wr) * wpr - wi * wpi + wr;
		wi = wi * wpr + wtemp * wpi + wi;
	}
	if (isign == 1) {
		data[1] = (h1r = data[1]) + data[2];
		data[2] = h1r - data[2];
	} else {
		data[1] = c1 * ((h1r = data[1]) + data[2]);
		data[2] = c1 * (h1r - data[2]);
		four1(data, n >> 1, -1);
	}
}
