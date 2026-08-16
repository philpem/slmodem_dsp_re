/*
 * V92Transmitter.h -- the V.92 upstream transmit chain: six owned pieces.
 *
 * Reconstructed from dsplibs.o.  Five of the class's six symbols are written
 * in src/pump/v90/V92Transmitter.cpp -- the constructor (C1 at .text+0x53b90
 * and C2 at +0x53c50, 180 bytes each), the destructor (D2 at +0x53a30 and
 * D1 at +0x53ae0, 173 bytes each) and `reset(V92MappingParams *)` (+0x53d10,
 * 2,161 bytes).  `process(unsigned char *, unsigned int, short *, unsigned
 * int &)` (+0x54590, 355 bytes) is declared here and deliberately left
 * undefined: defining a member whose callers are not written re-opens the
 * link closure for the whole suite, and it has not been read.
 *
 * `reset` IS WHERE THREE OF THIS CLASS'S NAMES COME FROM.  It prints its own
 * +0x04 as "K" and its own +0x44 as "Gain", and it prints the parameter block
 * field by field -- which is what named most of
 * include/dsplib/V92ParamsInfo.h.  What it does to the object itself is small:
 * two words in from the parameter block, one byte cleared through +0x58, two
 * words zeroed on the way out, and six calls that push the rest of the work
 * into the five sub-objects.
 *
 * THE OBJECT IS 0x60 BYTES, AND IT IS MEASURED RATHER THAN BOUNDED.
 * `V92BitsToSymbol::V92BitsToSymbol` allocates it and hands the block
 * straight to this constructor:
 *
 *     4deee:  c7 04 24 60 00 00 00   movl $0x60,(%esp)
 *     4def5:  e8 ..                  call sysdep_malloc
 *     4deff:  e8 ..                  call V92Transmitter::V92Transmitter()
 *
 * That is the ORIGINAL COMPILER'S OWN `sizeof` (finding 1249's oracle), not a
 * displacement: the furthest field the constructor writes is the four bytes
 * at +0x58, which end at 0x5c, and the last four bytes are never touched by
 * anything written here.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE CONSTRUCTOR OWNS, AND HOW THE SIX DIFFER
 *
 * Six allocations, and the object treats them in three different ways -- the
 * difference is the constructor's and it is reproduced rather than tidied:
 *
 *   +0x08  sysdep_malloc(0x50)          raw, no constructor at all
 *   +0x48  sysdep_malloc(0x54)          V92ModulusEncoder, constructed
 *   +0x58  sysdep_malloc(1), *p = 0      raw, one byte, cleared in place
 *   +0x54  sysdep_malloc(0x2008)        V92ConvolutionEncoder, constructed
 *   +0x4c  sysdep_malloc(0x80)          V92Precoder(0x140), constructed
 *   +0x50  sysdep_malloc(0x14)          V92PreFilter(0x140), constructed
 *
 * and the destructor frees all six with a null test each, but calls a
 * destructor for only THREE of them: the precoder, the pre-filter and the
 * convolution encoder.  **The modulus encoder at +0x48 is freed without one,
 * and that is not a leak the object has.**  `readelf -sW` carries no
 * `_ZN17V92ModulusEncoderD1Ev` or `D2Ev` of any kind, so the class has no
 * user-declared destructor for GCC to have called; a plain `sysdep_free` is
 * exactly what `delete p` emits for a trivially-destructible `p`.  The raw
 * buffers at +0x08 and +0x58 are the same story with no class involved.
 *
 * THE ORDER IS NOT THE CONSTRUCTOR'S.  Construction runs +0x08, +0x48, +0x58,
 * +0x54, +0x4c, +0x50; destruction runs +0x08, +0x48, +0x58, +0x4c, +0x50,
 * +0x54.  The last three are permuted, which is what a hand-written
 * destructor looks like and not what member destruction would give.
 *
 * ONE POINTER IS NULLED AFTER ITS FREE AND FIVE ARE NOT.  `movl $0x0,0x4c`
 * follows the precoder's release at .text+0x53aa6 and nothing follows the
 * other five, so a second destruction double-frees five buffers and not the
 * sixth.  Reproduced; see docs/deviations.md D210.
 *
 * Data member names are invented and descriptive (finding 226); the mangling
 * never carries a data member's name.  Where a field's ROLE is not
 * established, the name says so rather than guessing.
 */

#ifndef DSPLIB_V92TRANSMITTER_H
#define DSPLIB_V92TRANSMITTER_H

class V92MappingParams;
class V92ModulusEncoder;
class V92ConvolutionEncoder;
class V92Precoder;
class V92PreFilter;

/*
 * The tap count both filters are built with, `mov $0x140,%ecx` at
 * .text+0x53c2d and `mov $0x140,%edx` at +0x53c0d.  Both get the same number
 * and the object spells it once per call site.
 */
#define V92TX_FILTER_TAPS	0x140

/* The raw buffer at +0x08: `movl $0x50,(%esp)` at .text+0x53b99. */
#define V92TX_BUF08_BYTES	0x50

class V92Transmitter {
public:
	V92Transmitter();
	~V92Transmitter();

	/*
	 * The argument types are the mangling's and exact; the return types
	 * are not mangled and `void` here means "not established" rather than
	 * "measured" -- `reset` leaves whatever the last call left in %eax.
	 *
	 * `process` is declared, not defined.
	 */
	void reset(V92MappingParams *params);
	void process(unsigned char *bits, unsigned int nbits, short *out,
		     unsigned int &nout);

	/*
	 * Public for `offsetof`, which needs standard layout, and for the
	 * same reason as V90Jd's and V92Precoder's: one access section.
	 */

	/*
	 * +0x00  NOT WRITTEN by the constructor, and nothing here names it.
	 * Four bytes that arrive in whatever state the allocation left.
	 */
	unsigned char pad_00[4];

	/*
	 * +0x04  Cleared by the constructor and refilled by `reset` from the
	 * parameter block's +0x00.  "K = %d" (.rodata.str1.1:0x26b6) is the
	 * author's name for it, printed off THIS field -- `mov 0x4(%edi),%eax`
	 * at .text+0x54279 -- which makes it one of the few names in this
	 * class that is not an inference.  What K counts is not established;
	 * the unpacker builds it as twice (drn + 17) and that is all the
	 * object says.  See include/dsplib/V92ParamsInfo.h.
	 */
	int K;

	/*
	 * +0x08  `sysdep_malloc(0x50)` with no constructor call after it, so
	 * whatever this is, it is not one of the classes above.  The element
	 * type is not established either; `void *` is what the object shows.
	 */
	void *buf_08;

	/* +0x0c  Cleared by the constructor, immediately after +0x08 is
	 * stored and before +0x04 is cleared.  Role not established. */
	unsigned int word_0c;

	/*
	 * +0x10 .. +0x3f  Forty-eight bytes neither the constructor nor
	 * `reset` touches.  `process` is still not written, so nothing here
	 * can say what lives in them.  This region was 56 bytes until `reset`
	 * was read; the last two words of it are below.
	 */
	unsigned char pad_10[0x30];

	/*
	 * +0x40  Zeroed by `reset`, in the same pair of stores as +0x0c and at
	 * every one of its three exits.  Nothing reads it in anything written
	 * here and no string names it, so its role is not established and the
	 * neutral name stays.  Four bytes, stored as a word.
	 */
	unsigned int word_40;

	/*
	 * +0x44  The constellation gain, copied by `reset` from the parameter
	 * block's +0x18 -- the FIRST thing reset does, before any of the six
	 * calls.  "Gain = %c%d.%07d" (.rodata.str1.1:0x26a3) is printed from
	 * this field, `flds 0x44(%edi)` at .text+0x541e6, so both the name and
	 * `float` are the author's rather than inferred.
	 */
	float gain;

	/* +0x48  V92ModulusEncoder, 0x54 bytes, constructed and freed with no
	 * destructor call -- see the file comment. */
	V92ModulusEncoder *modulusEncoder;

	/* +0x4c  V92Precoder(0x140), 0x80 bytes.  The one pointer the
	 * destructor nulls after releasing it. */
	V92Precoder *precoder;

	/* +0x50  V92PreFilter(0x140), 0x14 bytes. */
	V92PreFilter *preFilter;

	/* +0x54  V92ConvolutionEncoder, 0x2008 bytes. */
	V92ConvolutionEncoder *convolutionEncoder;

	/*
	 * +0x58  One byte, allocated and cleared through the returned pointer
	 * -- `movb $0x0,(%eax)` at .text+0x53bdb, BEFORE the pointer is
	 * stored.  A one-element buffer, not a member the class holds by
	 * value.
	 */
	unsigned char *byte_58;

	/*
	 * +0x5c  The last four bytes of the 0x60 the caller allocates.  The
	 * constructor never writes them and no member written here reads
	 * them; they are what makes `sizeof` 0x60 rather than 0x5c, and the
	 * allocation is the only evidence they exist.
	 */
	unsigned char pad_5c[4];
};

#endif /* DSPLIB_V92TRANSMITTER_H */
