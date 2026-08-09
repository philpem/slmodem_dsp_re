/*
 * Psd.cpp -- reconstructed from dsplibs.o.  FIVE of the six members;
 * `include/dsplib/Psd.h` carries the object map, the `OutputOption` values
 * and the two reasons `Psd::process` is not here (finding 876).
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

extern "C" {
void *sysdep_malloc(unsigned size);
void sysdep_free(void *ptr);
}

/*
 * Hold the compiler to the map in the header (finding 230); tools/offcheck.py
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
 * The enumerator values are read from `process`, which this file does not
 * define; assert them here so a later batch cannot renumber the enum and
 * leave the compare chain it was read from behind.
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
	if (m_window != 0)
		sysdep_free(m_window);
	if (m_fft != 0)
		sysdep_free(m_fft);
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
 * st(0), so 1.0 / length and not length / 1.0 (finding 245, in the direction
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
