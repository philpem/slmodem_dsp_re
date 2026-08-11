/*
 * V90SpectralVerifier.cpp -- clearing the spectrum accumulator.
 *
 * Reconstructed from dsplibs.o.  One of the class's twelve members:
 * `reset()`, which is the one `v34handshak` reaches.
 * `include/dsplib/V90SpectralVerifier.h` carries the object map.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE DIAGNOSTIC IS NOT GATED.  Unlike `V90ConstellationDesigner`'s two, this
 * one is a bare `call edprintf` with no `dsplibs_debug_level` test in front
 * of it -- because `edprintf` does its own gating, and does it AFTER
 * formatting and encoding, so the call has an effect at every level (see
 * src/core/encode.c).  It comes first in the object and it comes first here.
 *
 * THE THREE STORES ARE INDEPENDENT.  The object emits them +0x24, +0x20,
 * +0x28; they are three zeroes into three distinct words of the same object,
 * so the order is the scheduler's and not the source's, and no order of the
 * three is distinguishable by any observer.  Written low-to-high here.
 */

#include <stddef.h>

#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
#include "dsplib/Psd.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90SpectralVerifier.h"

/*
 * `Psd::Psd` AND `Psd::~Psd` BY THEIR MANGLED NAMES, for the reason
 * V90Demodulator.cpp gives for `V90Resampler::reset`, plus one this class
 * has on its own: the object allocates the `Psd` with `sysdep_malloc` and
 * then runs its constructor over that storage, and C++ has no syntax for
 * that without `<new>`, which this tree builds `-nostdinc++` without.  The
 * symbols are the ones the object calls, `C1` and `D1`, and both take `this`
 * as their first stack argument like everything else here (finding 215).
 */
extern void psd_construct(void *self, unsigned int length, WindowType window,
			  unsigned int overlap)
	asm("_ZN3PsdC1Ej10WindowTypej");
extern void psd_destruct(void *self) asm("_ZN3PsdD1Ev");

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SV_OFF(field, off, tag) \
	typedef char v90sv_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralVerifier, field) == (off)) \
	    ? 1 : -1]

V90SV_OFF(params,       0x00, params);
V90SV_OFF(psd,          0x04, psd);
V90SV_OFF(sampleFreq,   0x08, samplefreq);
V90SV_OFF(fftLength,    0x0c, fftlength);
V90SV_OFF(psdLength,    0x10, psdlength);
V90SV_OFF(binWidth,     0x14, binwidth);
V90SV_OFF(buf_18,       0x18, buf18);
V90SV_OFF(spectrum,     0x1c, spectrum);
V90SV_OFF(accumCount,   0x20, accumcount);
V90SV_OFF(accumulating, 0x24, accumulating);
V90SV_OFF(word_28,      0x28, word28);
typedef char v90sv_size[(sizeof(V90SpectralVerifier) == 0x2c) ? 1 : -1];

/* The parameter slots the constructor reads, held to the map (finding 230). */
#define V90SV_POFF(field, off, tag) \
	typedef char v90sv_poff_##tag[ \
	    ((int)__builtin_offsetof(V90Parameters, field) == (off)) ? 1 : -1]

V90SV_POFF(SPECTRAL_VERIFIER_SAMPLE_FREQ,      0x2ac, freq);
V90SV_POFF(SPECTRAL_VERIFIER_FFT_LEN,          0x2b0, fftlen);
V90SV_POFF(SPECTRAL_VERIFIER_FFT_WINDOW,       0x2b4, window);
V90SV_POFF(SPECTRAL_VERIFIER_PSD_LEN,          0x2b8, psdlen);
V90SV_POFF(SPECTRAL_VERIFIER_PSD_OVERLAP_LEN,  0x2bc, overlap);
#endif

/*
 * THREE ALLOCATIONS, NONE CHECKED, AND ONE OF THEM CONSTRUCTED.  The two
 * float buffers are left exactly as the allocator returned them -- nothing
 * here clears either -- and the third holds a `Psd` built from three
 * parameter slots.
 *
 * THE PARAMETER BLOCK IS RE-READ THROUGH THE OBJECT.  `SPECTRAL_VERIFIER_-
 * FFT_WINDOW` and `..._PSD_OVERLAP_LEN` are loaded via `(%ebx)`, the pointer
 * just stored at +0x00, and not via the incoming argument still live in a
 * register -- so the source names the member, and it is written that way.
 *
 * +0x20 AND +0x24 ARE NOT INITIALISED, which `reset()` clears and this does
 * not.  It is the object's, not an omission here; docs/deviations.md.
 */
V90SpectralVerifier::V90SpectralVerifier(V90Parameters *p)
{
	params = p;

	fftLength = p->SPECTRAL_VERIFIER_FFT_LEN;
	sampleFreq = p->SPECTRAL_VERIFIER_SAMPLE_FREQ;
	binWidth = sampleFreq / fftLength;
	psdLength = p->SPECTRAL_VERIFIER_PSD_LEN;

	buf_18 = (float *)sysdep_malloc(psdLength * sizeof(float));
	spectrum = (float *)sysdep_malloc((fftLength / 2) * sizeof(float));

	psd = (Psd *)sysdep_malloc(sizeof(Psd));
	psd_construct(psd, fftLength,
		      (WindowType)params->SPECTRAL_VERIFIER_FFT_WINDOW,
		      params->SPECTRAL_VERIFIER_PSD_OVERLAP_LEN);

	word_28 = 0;
}

/*
 * THE ORDER IS +0x18, +0x1c, +0x04, and the third is the only one that is
 * more than a free: the `Psd` is destroyed and then its storage released,
 * two calls on the same address.  Each pointer is tested first and none is
 * nulled afterwards.
 */
V90SpectralVerifier::~V90SpectralVerifier()
{
	if (buf_18 != 0)
		sysdep_free(buf_18);

	if (spectrum != 0)
		sysdep_free(spectrum);

	if (psd != 0) {
		psd_destruct(psd);
		sysdep_free(psd);
	}
}

void
V90SpectralVerifier::reset()
{
	edprintf("V90SpectralVerifier: Reset\r\n");

	accumCount = 0;
	accumulating = 0;
	word_28 = 0;
}
