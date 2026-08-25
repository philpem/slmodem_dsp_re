/*
 * V92Transmitter.h -- the V.92 upstream transmit chain: six owned pieces.
 *
 * Reconstructed from dsplibs.o.  ALL SIX of the class's symbols are now
 * written in src/pump/v90/V92Transmitter.cpp -- the constructor (C1 at
 * .text+0x53b90 and C2 at +0x53c50, 180 bytes each), the destructor (D2 at
 * +0x53a30 and D1 at +0x53ae0, 173 bytes each), `reset(V92MappingParams *)`
 * (+0x53d10, 2,161 bytes) and `process(unsigned char *, unsigned int,
 * short *, unsigned int &)` (+0x54590, 355 bytes).  `process` came in with
 * the V92BitsToSymbol batch, which is its only caller.
 *
 * `process` IS WHAT MADE THIS CLASS A DATAPUMP RATHER THAN A CONSTRUCTOR.
 * It buffers input bits until it holds `K` of them, and then runs one frame:
 * the modulus encoder over the buffer, three precoder/convolution-encoder
 * steps of four symbols each, the pre-filter over all twelve, and twelve
 * `(short)(x * gain)` stores into the caller's output.  Four of this
 * header's names below are its doing.
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
 * That is the ORIGINAL COMPILER'S OWN `sizeof` (finding F1249's oracle), not a
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
 * Data member names are invented and descriptive (finding F226); the mangling
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

/* The bit buffer at +0x08: `movl $0x50,(%esp)` at .text+0x53b99. */
#define V92TX_BUF08_BYTES	0x50

/*
 * Twelve samples come out of every frame, and the object says so three times
 * over: `process` runs its scaling loop `cmp $0xb,%ecx; jbe`, adds a literal
 * 12 to the output count (`addl $0xc,0x28(%esp)`), and V92PreFilter.h already
 * records that `V92PreFilter::process` moves twelve floats whatever path it
 * takes.  It is also 3 precoder calls x 4 symbols each.
 */
#define V92TX_FRAME_SYMBOLS	12

/*
 * The precoder runs three times per frame, four symbols at a time:
 * `cmp $0x2,%edi; jbe` at .text+0x54653 with the counter incremented before
 * the test, so the values it is called with are 0, 1 and 2.  UNSIGNED is
 * forced -- a signed counter would have compared with `jle`.
 */
#define V92TX_PRECODER_STEPS	3
#define V92TX_PRECODER_SYMBOLS	4

class V92Transmitter {
public:
	V92Transmitter();
	~V92Transmitter();

	/*
	 * The argument types are the mangling's and exact; the return types
	 * are not mangled and `void` here means "not established" rather than
	 * "measured" -- `reset` leaves whatever the last call left in %eax,
	 * and `process` leaves whatever its last comparison did.  Neither
	 * body ever sets %eax deliberately, which is what separates them from
	 * V92BitsToSymbol's three `process` overloads.
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
	 * whatever this is, it is not one of the classes above.
	 *
	 * `process` IS WHAT NAMES IT, and the element type is forced twice
	 * over: `process` fills it one byte at a time from its own `bits`
	 * argument -- `movzbl (%ebx,%ebp,1),%eax; mov %al,(%ecx,%edi,1)` at
	 * .text+0x545dc -- and then hands it to `V92ModulusEncoder::progress`,
	 * whose mangling `EPhPj` types the first argument `unsigned char *`.
	 * It was `void *` until that reader was written.
	 */
	unsigned char *bitBuffer;

	/*
	 * +0x0c  How many bits `bitBuffer` currently holds.  Cleared by the
	 * constructor and by `reset`; `process` uses it as the store index,
	 * increments it once per input bit, compares it against `K` and
	 * subtracts `K` from it whenever a frame comes out.  It is never
	 * reset to zero at that point, so a partial frame's tail carries.
	 * The name is `process`'s arithmetic and nothing else.
	 */
	unsigned int bitsBuffered;

	/*
	 * +0x10 .. +0x3f  TWELVE WORDS, and `process` is what turned them from
	 * forty-eight bytes of pad into an array.  Neither the constructor nor
	 * `reset` touches them; `process` passes `this + 0x10` as the second
	 * argument of `V92ModulusEncoder::progress(unsigned char *,
	 * unsigned int *)` -- so the element type is the mangling's -- and
	 * then as the first argument of `V92Precoder::process(unsigned int *,
	 * int, int, int *, float *)`, which indexes it `i + 4 * a` for `i` in
	 * 0..3 over `a` = 0, 1, 2.  Twelve is therefore the largest index the
	 * only reader can form, and it is also exactly the forty-eight bytes
	 * the region has: the two agree, which is why this is an array rather
	 * than a bound.
	 *
	 * WHAT THE WORDS MEAN IS THE PRECODER'S BUSINESS and not established
	 * here: it uses each as the `x` a constellation search is centred on.
	 * The name says who fills it, which is what the object states.
	 */
	unsigned int modulusOut[V92TX_FRAME_SYMBOLS];

	/*
	 * +0x40  The convolution encoder's last output, carried from one 4D
	 * symbol to the next.  Zeroed by `reset` at every one of its three
	 * exits; `process` is its only other user and does exactly two things
	 * with it -- passes it as `V92Precoder::process`'s third argument (the
	 * `b` that enters the fourth symbol's parity) and then overwrites it
	 * with `V92ConvolutionEncoder::process`'s return value, three times
	 * per frame.  So it is read one iteration BEFORE the value that
	 * replaces it is computed, and the first read of a frame sees the last
	 * write of the one before.
	 *
	 * `int` rather than `unsigned int`, and that is the two neighbours'
	 * doing: `V92ConvolutionEncoder::process` returns `int` and
	 * `V92Precoder::process` takes `int`.  The width never changes and no
	 * comparison forces a sign, so the retype cannot move code generation.
	 */
	int convEncoderOutput;

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
