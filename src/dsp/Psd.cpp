/*
 * Psd.cpp -- reconstructed from dsplibs.o.  All six members;
 * `include/dsplib/Psd.h` carries the object map, the `OutputOption` values
 * and the two reasons `Psd::process` used to be missing (finding F876).
 *
 * THE CALLING CONVENTION IS PLAIN CDECL, as everywhere else in this tree:
 * `this` is the first stack argument.
 *
 * `designWindow<float>` IS OURS, not the blob's.  It is already reconstructed
 * in src/dsp/DspMath.cpp and already compared against the blob by
 * test/unit/t_dspmath.cpp, so the constructor test below is a comparison of
 * two independent implementations and not a hybrid.
 */

#include "dsplib/Psd.h"
#include "dsplib/fft.h"		/* process() calls realfft (finding F1325) */

extern "C" {
void *sysdep_malloc(unsigned size);
void sysdep_free(void *ptr);
}

inline void operator delete[](void *p) { sysdep_free(p); }

/*
 * Hold the compiler to the map in the header (finding F230); tools/offcheck.py
 * cannot read a class.
 */
#define PSD_OFF(field, off, tag) \
	typedef char psd_off_##tag[ \
	    ((int)__builtin_offsetof(Psd, field) == (off)) ? 1 : -1]

#if __SIZEOF_POINTER__ == 4
PSD_OFF(m_length,  0x00, m_length);
PSD_OFF(m_overlap, 0x04, m_overlap);
PSD_OFF(m_window,  0x08, m_window);
PSD_OFF(m_fft,     0x0c, m_fft);
typedef char psd_size[(sizeof(Psd) == 0x10) ? 1 : -1];
#endif

/*
 * The enumerator values are read from `process`'s compare chain; assert them
 * here so nothing can renumber the enum and leave that chain behind.
 */
typedef char psd_opt_db[(Psd::OUTPUT_DB == 0) ? 1 : -1];
typedef char psd_opt_db_peak[(Psd::OUTPUT_DB_PEAK == 1) ? 1 : -1];
typedef char psd_opt_linear[(Psd::OUTPUT_LINEAR == 2) ? 1 : -1];

/*
 * Two allocations and no checks.  The first is sized from the ARGUMENT and
 * the second from `m_length` read back out of the object -- the same value,
 * and the object really does re-read it.  Neither buffer is cleared: only
 * `m_window` is written, by `designWindow`, so `m_fft` carries allocator
 * garbage out of the constructor.  `m_overlap` is stored last, after the
 * window is designed.
 */
Psd::Psd(unsigned int length, WindowType window, unsigned int overlap)
{
	m_length = length;
	m_window = (float *)sysdep_malloc(length * sizeof(float));
	m_fft = (float *)sysdep_malloc((m_length + 1) * sizeof(float));
	designWindow(window, m_window, m_length);
	m_overlap = overlap;
}

/* Window first, spectrum second; neither pointer is nulled. */
Psd::~Psd()
{
	delete[] m_window;
	delete[] m_fft;
}

void
Psd::setOverlapLength(unsigned int overlap)
{
	m_overlap = overlap;
}

/*
 * Redesign the window in place.  The type is NOT stored anywhere, so this is
 * the only trace it leaves; the object holds no field the two sides could be
 * compared on except the window itself.
 */
void
Psd::setWindowType(WindowType window)
{
	designWindow(window, m_window, m_length);
}

/*
 * The bin centre frequencies, for the `length / 2` bins `process` fills.
 *
 * The reciprocal is computed once, in an x87 register: `fld1`, `fildll` of
 * the length as a 64-bit integer, then a popping divide.  That divide is
 * `de f9`, which objdump prints as `fdivrp` and which IS `FDIVP` -- st(1) /
 * st(0), so 1.0 / length and not length / 1.0 (finding F245, in the direction
 * the trap is usually met from).  `i` reaches the multiply through `fildll`
 * too, and the product is formed as (i * sampleRate) * (1 / length): three
 * extended-precision steps and one rounding, at the store.
 */
void
Psd::getFrequencies(float *freq, float sampleRate) const
{
	unsigned int i;
	unsigned int bins = m_length >> 1;

	for (i = 0; i < bins; i++)
		freq[i] = (float)((long double)i * sampleRate *
				  (1.0L / m_length));
}

/*
 * log10() on the coprocessor, as the object computes it: `fldlg2` pushes
 * log10(2) at the register's full 64-bit mantissa and `fyl2x` computes
 * st(1) * log2(st(0)) and pops, so the pair takes one value and leaves one.
 *
 * GCC EMITS THAT SEQUENCE FOR `log10()` ONLY UNDER
 * `-funsafe-math-optimizations` (finding F876), which this tree does not build
 * with and must not: the flag changes every other floating-point expression
 * in the translation unit as well.  A call to libm's `log10` is not the same
 * function -- it is correctly rounded where `fyl2x` is not -- and the
 * difference lands in the last place of the result, which is exactly what the
 * arms below then round to a float and store.
 *
 * THE COPY IS DELIBERATE AND IT IS THE THIRD.  `V90Equalizer.cpp` and
 * `VPcmFloModem.cpp` each carry the same eight lines.  A shared header would
 * be better and is a separate concern: a NEW C++ header has to be added to
 * `offcheck.py`'s SKIP_HEADERS or the `offsets` gate breaks tree-wide naming
 * files nobody touched, and that is not a change to make from inside one
 * class's batch.
 */
static inline long double
psd_x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * Welch's method: overlapping windowed frames, each transformed, the squared
 * magnitudes summed across frames, and then one of three scalings.
 *
 * THE FRAME COUNT IS AN UNSIGNED DIVISION AND IT IS NOT GUARDED.
 * `(count - m_overlap) / (m_length - m_overlap)` with no test of either
 * operand: a length equal to the overlap divides by zero, and a count below
 * the overlap wraps to something enormous and walks off the input.  Both are
 * the object's, which computes the quotient before it looks at anything else.
 * The test stays away from both and says so.
 *
 * THE OUTPUT IS CLEARED FIRST -- `m_length / 2` words, an integer zero store
 * -- and every frame accumulates into it.  So the caller's array is written
 * even when the frame count comes out zero, and only the first `m_length / 2`
 * words of it are touched however long it is.
 *
 * `m_fft[0]` IS NEVER WRITTEN.  The window loop stores to `m_fft[1 + j]` and
 * the magnitudes are read from `m_fft[1 + 2i]` and `m_fft[2 + 2i]`: the
 * Numerical Recipes 1-based convention `realfft` is written to, and the
 * reason the constructor allocates `m_length + 1` floats for `m_length`
 * points.
 *
 * EVERYTHING BETWEEN A LOAD AND A STORE IS 64 SIGNIFICAND BITS.  The
 * accumulation rounds once per bin per frame, at the `fstps`; the scalings
 * round once at the store -- and once more, in the two logarithmic arms, at a
 * `fstps`/`flds` pair between the logarithm and the multiply by 10.  That
 * middle rounding is the object's, and it is written here as a plain `float`
 * local, which is what the original's source must have had.
 */
void
Psd::process(float *in, unsigned int count, float *out, OutputOption option)
{
	unsigned int bins = m_length >> 1;
	unsigned int frames = (count - m_overlap) / (m_length - m_overlap);
	unsigned int frame, i, j, pos;

	for (i = 0; i < bins; i++)
		out[i] = 0.0f;

	pos = 0;
	for (frame = 0; frame < frames; frame++) {
		for (j = 0; j < m_length; j++)
			m_fft[1 + j] = in[pos + j] * m_window[j];

		realfft(m_fft, m_length, 1);

		for (i = 0; i < bins; i++) {
			long double re = m_fft[1 + 2 * i];
			long double im = m_fft[2 + 2 * i];

			out[i] = (float)(re * re + im * im + out[i]);
		}

		pos += m_length - m_overlap;
	}

	/*
	 * The three scalings.  `10 * log10` is a POWER decibel and the
	 * accumulator holds squared magnitudes, so the factor suits what is
	 * in the array; 1e-25 is a floor that keeps an empty bin from
	 * reaching the logarithm as a zero, and it is a `double` constant
	 * where the 10 is a `float` one -- both as the object loads them.
	 */
	if (option == OUTPUT_DB_PEAK) {
		long double peak = 0.0L;
		long double scale;

		/*
		 * `fcom` + `ja`, so an unordered comparison does NOT replace
		 * the peak: a NaN bin leaves it where it was rather than
		 * poisoning it, and `>` is what does that in C too.
		 */
		for (i = 0; i < bins; i++)
			if (out[i] > peak)
				peak = out[i];

		scale = 1.0L / peak;
		for (i = 0; i < bins; i++) {
			float l = (float)psd_x87_log10((long double)out[i]
						       * scale + 1e-25);

			out[i] = (float)(l * 10.0f);
		}
	} else if (option == OUTPUT_LINEAR) {
		long double scale = 1.0L / (long double)frames;

		for (i = 0; i < bins; i++)
			out[i] = (float)((long double)out[i] * scale);
	} else if (option == OUTPUT_DB) {
		long double scale = 1.0L / (long double)frames;

		for (i = 0; i < bins; i++) {
			float l = (float)psd_x87_log10((long double)out[i]
						       * scale + 1e-25);

			out[i] = (float)(l * 10.0f);
		}
	}
}
