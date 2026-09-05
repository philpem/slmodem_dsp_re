/**
 * @file V92Transmitter.h
 * @brief ITU-T V.92 upstream transmit chain: `V92Transmitter`, which buffers
 *        incoming bits into `K`-bit frames and runs each frame through the
 *        modulus encoder, precoder, convolution encoder and pre-filter to
 *        produce output samples. Owns all four of those sub-objects plus two
 *        raw buffers -- six allocations in total.
 *
 * `process()` is what makes this class a datapump rather than a constructor:
 * it buffers input bits until `K` of them have arrived, then runs one frame
 * (the modulus encoder over the buffer, three precoder/convolution-encoder
 * steps of four symbols each, the pre-filter over all twelve, and twelve
 * `(short)(x * gain)` stores into the caller's output). It came in with the
 * V92BitsToSymbol batch, which is its only caller.
 *
 * `reset()` is where three of this class's names come from: it prints its
 * own `K` and `gain` fields by those names, and prints the parameter block
 * field by field, which is what named most of `V92ParamsInfo.h`. What it
 * does to the object itself is small -- two words loaded from the parameter
 * block, one byte cleared, two words zeroed -- and pushes the rest of the
 * work into the four sub-objects.
 *
 * `sizeof == 0x60` is the allocation `V92BitsToSymbol::V92BitsToSymbol`
 * makes before calling this constructor (finding F1249's oracle, the
 * original compiler's own `sizeof`, not a displacement): the furthest field
 * the constructor writes ends at 0x5c, and the last four bytes are untouched
 * by anything written here.
 *
 * ---------------------------------------------------------------------------
 * What the constructor owns, and how the six allocations differ
 *
 * The object treats its six allocations in three different ways, and the
 * difference is the constructor's and is reproduced rather than tidied:
 *
 *   +0x08  raw buffer, no constructor at all
 *   +0x48  V92ModulusEncoder, constructed
 *   +0x58  raw one-byte buffer, cleared in place
 *   +0x54  V92ConvolutionEncoder, constructed
 *   +0x4c  V92Precoder(0x140), constructed
 *   +0x50  V92PreFilter(0x140), constructed
 *
 * The destructor frees all six (each behind a null test) but calls a
 * destructor for only three of them -- the precoder, the pre-filter and the
 * convolution encoder. The modulus encoder at +0x48 is freed without one,
 * and that is not a leak: it has no user-declared destructor for GCC to have
 * called, so a plain free is exactly what `delete p` emits for a
 * trivially-destructible `p`; the two raw buffers are the same story with no
 * class involved.
 *
 * The destruction order is not the construction order -- the last three
 * allocations are permuted, which is what a hand-written destructor looks
 * like and not what member destruction would give -- and only the precoder's
 * pointer is nulled after its free; the other five are left dangling.
 * Reproduced; see docs/deviations.md D210.
 *
 * Data member names are invented and descriptive (finding F226); the
 * mangling never carries a data member's name. Where a field's role is not
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
	/**
	 * @brief Construct the transmitter: allocate the raw bit buffer
	 *        (`bitBuffer`), the one-byte `byte_58` buffer, and the four
	 *        owned sub-object filters/encoders (modulus encoder,
	 *        precoder, pre-filter, convolution encoder, the latter two
	 *        built with #V92TX_FILTER_TAPS taps). Leaves `K`/`gain`/the
	 *        modulus-output array unset -- reset() fills them from the
	 *        negotiated parameters.
	 */
	V92Transmitter();

	/**
	 * @brief Destroy the transmitter: free all six owned allocations
	 *        (destructing the precoder, pre-filter and convolution
	 *        encoder; the modulus encoder and the two raw buffers have no
	 *        destructor to call). Nulls the precoder pointer after
	 *        freeing it; the other five pointers are left dangling
	 *        (reproduced -- see the file comment, docs/deviations.md
	 *        D210).
	 */
	~V92Transmitter();

	/**
	 * @brief Reinitialize the transmitter from negotiated V.92 mapping
	 *        parameters: load `gain` and `K`, reset and configure the
	 *        modulus encoder, precoder, pre-filter and convolution
	 *        encoder from the parameter block, and zero the carried
	 *        convolution-encoder output and the bit-buffer fill count.
	 *        Also emits the parameter block field by field as debug
	 *        output when debugging is on.
	 * @param params  Negotiated V.92 mapping parameters (actually a
	 *                `V92ParamsInfo *`; see V92ParamsInfo.h).
	 */
	void reset(V92MappingParams *params);

	/**
	 * @brief Buffer incoming bits and, each time `K` of them have
	 *        accumulated, run one frame: the modulus encoder over the
	 *        buffer, three precoder/convolution-encoder steps of four
	 *        symbols each (#V92TX_PRECODER_STEPS x
	 *        #V92TX_PRECODER_SYMBOLS), the pre-filter over all twelve
	 *        results, and a store of `gain`-scaled samples (rounded
	 *        toward zero) into the caller's output for each frame
	 *        produced.
	 * @param bits   Input bits, one per byte, unmasked.
	 * @param nbits  Number of bits in `bits`.
	 * @param out    Output sample buffer; #V92TX_FRAME_SYMBOLS samples
	 *               are appended per completed frame.
	 * @param nout   Set to the number of samples written to `out`
	 *               (0 if no frame completed).
	 */
	void process(unsigned char *bits, unsigned int nbits, short *out,
		     unsigned int &nout);

	/*
	 * Public for `offsetof`, which needs standard layout, and for the
	 * same reason as V90Jd's and V92Precoder's: one access section.
	 */

	/*
	 * +0x00  Not written by the constructor, and nothing here names it.
	 * Four bytes that arrive in whatever state the allocation left.
	 */
	unsigned char pad_00[4];

	/*
	 * +0x04  Cleared by the constructor and refilled by `reset` from the
	 * parameter block's +0x00. The author's own name for it, printed off
	 * this field as "K = %d". What K counts is not established; the
	 * unpacker builds it as twice (drn + 17) and that is all the object
	 * says. See V92ParamsInfo.h.
	 */
	int K;

	/*
	 * +0x08  A raw buffer -- `sysdep_malloc(0x50)` with no constructor
	 * call after it, so it is not one of the classes above. `process`
	 * names it and forces its element type: it fills the buffer one byte
	 * at a time from its own `bits` argument, then hands it to
	 * `V92ModulusEncoder::progress`, whose mangling types the first
	 * argument `unsigned char *` (it was `void *` until that reader was
	 * written).
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
	 * +0x10 .. +0x3f  Twelve words; `process` is what turned them from
	 * forty-eight bytes of pad into an array (neither the constructor nor
	 * `reset` touches them). `process` passes `this + 0x10` as
	 * `V92ModulusEncoder::progress`'s output array, then as
	 * `V92Precoder::process`'s input, which indexes it `i + 4 * a` for
	 * `i` in 0..3 over `a` = 0, 1, 2 -- twelve is the largest index the
	 * only reader can form, and exactly the region's forty-eight bytes,
	 * which is why this is an array rather than a bound. What the words
	 * mean is the precoder's business, not established here: it uses each
	 * as the `x` a constellation search is centred on.
	 */
	unsigned int modulusOut[V92TX_FRAME_SYMBOLS];

	/*
	 * +0x40  The convolution encoder's last output, carried from one 4D
	 * symbol to the next. Zeroed by `reset` at every one of its three
	 * exits; `process` is its only other user, and reads it as
	 * `V92Precoder::process`'s third argument (the `b` that enters the
	 * fourth symbol's parity) one iteration before overwriting it with
	 * `V92ConvolutionEncoder::process`'s return value, three times per
	 * frame -- so the first read of a frame sees the last write of the
	 * one before. `int` rather than `unsigned int` because both
	 * neighbouring functions use `int`; the width never changes and no
	 * comparison forces a sign, so the retype cannot move code generation.
	 */
	int convEncoderOutput;

	/*
	 * +0x44  The constellation gain, copied by `reset` from the parameter
	 * block -- the first thing reset does, before any of the six calls.
	 * Printed from this field as "Gain = %c%d.%07d", so both the name and
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
