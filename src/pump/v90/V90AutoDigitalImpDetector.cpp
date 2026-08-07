/*
 * V90AutoDigitalImpDetector.cpp -- the V.90 digital-impairment detector's two
 * reset methods.
 *
 * Two of the class's thirty-two members: `reset(unsigned char, PcmType,
 * short)` and `resetLinearMapping()`, which are the two that are leaves --
 * between them they call only `alaw2linear` and `ulaw2linear`.
 * include/dsplib/V90AutoDigitalImpDetector.h carries the object map and the
 * measurement the 43,440-byte size comes from.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x20(%esp),%ebx` after four pushes and a twelve-byte frame
 * -- not %ecx, so these are not thiscall and nothing here needs an attribute
 * (finding 215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals, no allocation, no static data members, so
 * the test binaries still link with $(CC).
 */

#include <stddef.h>

extern "C" {
#include "dsplib/pcm.h"
}

#include "dsplib/V90AutoDigitalImpDetector.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset, which is
 * the failure docs/v90cpp.md warns about for the classes that DO have a vptr.
 *
 * GUARDED ON THE POINTER WIDTH, because the class holds one -- `params` at
 * +0x2814 -- and `make check64` compiles this file `-fsyntax-only` for the
 * native target, where that pointer is eight bytes and every offset after it
 * moves.  The claim is about the 32-bit layout the blob has, so it is only
 * asserted where the compiler is laying that layout out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define ADID_OFF(field, off, tag) \
	typedef char adid_off_##tag[ \
	    ((int)__builtin_offsetof(V90AutoDigitalImpDetector, field) \
	     == (off)) ? 1 : -1]

ADID_OFF(linMapp,      0x0000, linmapp);
ADID_OFF(linMappAlt,   0x0600, linmappalt);
ADID_OFF(pad_0c00,     0x0c00, pad0c00);
ADID_OFF(byte_0d00,    0x0d00, byte0d00);
ADID_OFF(int_1000,     0x1000, int1000);
ADID_OFF(int_1c00,     0x1c00, int1c00);
ADID_OFF(short_2800,   0x2800, short2800);
ADID_OFF(byte_280c,    0x280c, byte280c);
ADID_OFF(params,       0x2814, params);
ADID_OFF(short_8b00,   0x8b00, short8b00);
ADID_OFF(int_9100,     0x9100, int9100);
ADID_OFF(float_9118,   0x9118, float9118);
ADID_OFF(float_9d18,   0x9d18, float9d18);
ADID_OFF(int_9d30,     0x9d30, int9d30);
ADID_OFF(float_9d48,   0x9d48, float9d48);
ADID_OFF(short_a948,   0xa948, shorta948);
ADID_OFF(float_a94c,   0xa94c, floata94c);
ADID_OFF(pcmType,      0xa95c, pcmtype);
ADID_OFF(ucode,        0xa96b, ucode);
ADID_OFF(ucodeLevel,   0xa96c, ucodelevel);
ADID_OFF(short_a96e,   0xa96e, shorta96e);
ADID_OFF(float_a970,   0xa970, floata970);
ADID_OFF(float_a974,   0xa974, floata974);
ADID_OFF(short_a978,   0xa978, shorta978);
ADID_OFF(short_a97a,   0xa97a, shorta97a);
ADID_OFF(float_a97c,   0xa97c, floata97c);
ADID_OFF(float_a980,   0xa980, floata980);
typedef char adid_size[(sizeof(V90AutoDigitalImpDetector) == 0xa9b0) ? 1 : -1];

#endif /* 32-bit */

/*
 * V90Parameters is not modelled -- include/dsplib/V90PreFilter.h declares it
 * as an unnamed word block for exactly this reason -- and `reset` reads
 * exactly one thing out of it: the `short` at +0x0c, which it compares
 * against 2.  Reading it through a byte pointer keeps the offset numeric,
 * which is the honest spelling while the block's own layout is unknown.
 */
#define V90PARAMETERS_CONNECTION_TYPE 0x0c

static short
paramShort(const V90Parameters *p, unsigned int off)
{
	return *(const short *)((const unsigned char *)p + off);
}

/*
 * Clear the per-phase measurement state and install the session's reference
 * code, its companding law and its alternate-RBS flag.
 *
 * The five arrays cleared per (phase, code) and the five scalars cleared per
 * phase are the object's whole measurement state; `linMapp` and `linMappAlt`
 * are NOT among them -- `resetLinearMapping` below is a separate call, and
 * `reset` does not make it.
 *
 * The code-to-level conversion is the blob's, and it is the same one
 * V90Phase3Modulator's DIL expansion uses: the argument is a seven-bit
 * magnitude, and the sign/company bits are supplied here -- `(code & 0x7f) ^
 * 0xd5` for A-law and `(code & 0x7f) ^ 0xff` for mu-law, which is `~` of the
 * masked code.  Both produce a code in 0x80..0xff.
 *
 * The four thresholds at the end come from the parameter block's +0x0c: two
 * sets of (short, short, float, float) selected by whether it is 2.  The
 * float constants are the object's own bit patterns -- 0x40a00000, 0x3fc00000,
 * 0x3e800000, 0x3eb33333, 0x3fe00000 -- read out of the immediate operands,
 * not out of a decompilation.
 */
void
V90AutoDigitalImpDetector::reset(unsigned char code, PcmType law, short altRbs)
{
	short phase;

	pcmType = law;
	ucode = code;

	if (law != PCM_TYPE_MU_LAW)
		ucodeLevel = (short)alaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xd5));
	else
		ucodeLevel = (short)ulaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xff));

	short_a948 = 0;
	float_a94c = 1.0f;
	short_a96e = altRbs;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		short ci;

		for (ci = 0; ci < V90ADID_CODES; ci++) {
			int_1c00[phase][ci] = 0;
			int_1000[phase][ci] = 0;
			float_9118[phase][ci] = 0.0f;
			float_9d48[phase][ci] = 0.0f;
			short_8b00[phase][ci] = 0;
			byte_0d00[phase][ci] = 1;
		}

		int_9100[phase] = 0;
		short_2800[phase] = 0;
		int_9d30[phase] = 0;
		byte_280c[phase] = 0;
		float_9d18[phase] = 0.0f;
	}

	float_a970 = short_a96e != 0 ? 5.0f : 1.5f;
	float_a974 = 5.0f;

	if (paramShort(params, V90PARAMETERS_CONNECTION_TYPE) == 2) {
		short_a978 = 1;
		short_a97a = 88;
		float_a97c = 0.35f;
		float_a980 = 1.75f;
	} else {
		short_a978 = 0;
		short_a97a = 80;
		float_a97c = 0.25f;
		float_a980 = 1.5f;
	}
}

/*
 * Clear both linear-mapping tables and seed each phase's entry for the
 * reference code with the reference level.
 *
 * The two tables are written identically at every step -- the same 128 zeros
 * per phase, then the same `ucodeLevel` at the same `ucode` -- which is what
 * makes the pair legible as a value and its alternate-RBS hypothesis rather
 * than as two unrelated arrays.
 *
 * `ucode` is a byte and the tables are 128 wide, so a reference code of 128
 * or more would write past the end of a row.  The object does not mask it
 * here, and neither does this: `reset` is what puts a value there and it
 * stores its argument unmasked.
 */
void
V90AutoDigitalImpDetector::resetLinearMapping()
{
	short level = (short)ucodeLevel;
	unsigned char at = ucode;
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		short ci;

		for (ci = 0; ci < V90ADID_CODES; ci++) {
			linMapp[phase][ci] = 0;
			linMappAlt[phase][ci] = 0;
		}

		linMapp[phase][at] = level;
		linMappAlt[phase][at] = level;
	}
}
