/**
 * @file V92Precoder.h
 * @brief The V.92 transmit precoder: two `FloatFIR`s it owns, over a block
 *        of mapping parameters it copies out of `V92MappingParams`.
 *
 * Six members in the blob, 1,410 bytes; this tree defines the constructor
 * and destructor and declares the other four, since a declaration here is
 * a specification (the signatures are the mangling's) and a definition
 * would be a claim.
 *
 * Not polymorphic, and +0x00 is a real member rather than a vptr: the
 * constructor never writes offset 0, and a polymorphic class's constructor
 * always stores the vptr there (`tools/cppstruct.py` listing the ordinary
 * D1/D2 destructor pair with no deleting-destructor variant only settles
 * that the *destructor* isn't virtual, which a class can have alongside a
 * non-virtual one).
 *
 * The object is 0x80 bytes, measured from `V92Transmitter::V92Transmitter`'s
 * `sysdep_malloc(0x80)` immediately preceding this constructor's call
 * (with `0x140` as its tap-count argument -- the only tap count the object
 * ever asks for, finding F1245) -- agreeing with the furthest member access,
 * +0x7c in `setCoefficients`.
 *
 * What proves which pointer is owned: `reset(V92MappingParams *)` refills
 * the object from the parameter block a word at a time, writing +0x04
 * through +0x64 and +0x70, +0x74 -- skipping exactly +0x68 and +0x6c. Those
 * two are the `FloatFIR`s the constructor allocated and the destructor
 * frees; a reset that overwrote them would leak both filters and leave the
 * object pointing at parameter words. The skip is the map's best evidence.
 */

#ifndef DSPLIB_V92PRECODER_H
#define DSPLIB_V92PRECODER_H

#include "dsplib/FloatFIR.h"

/*
 * NOT MODELLED HERE.  The name is the original's, out of the mangling of
 * `reset` and of eight other classes' members; what is inside it is another
 * batch's work.  Only a pointer to it is needed.
 */
class V92MappingParams;

/*
 * The block size both filters are built with: `movl $0x63,0xc(%esp)` before
 * each `FloatFIR::FloatFIR`, so 99 samples of slack above the tap count.  It
 * is the FIR's compaction interval and not a tap count -- see FloatFIR.h.
 */
#define V92PRECODER_BLOCK 99

class V92Precoder {
public:
	/**
	 * @brief Construct, allocating both owned filters. Leaves every
	 *        other field of the object untouched -- which is why
	 *        reset(V92MappingParams*) exists.
	 * @param nTaps  Tap count passed through to both `FloatFIR`s.
	 */
	V92Precoder(unsigned int nTaps);
	/** @brief Destroy, freeing both owned filters. */
	~V92Precoder();

	/** @brief No-argument reset. Declared for the record; this tree has
	 *  not written it (its signature is the mangling's, its behavior
	 *  is not). */
	void reset();
	/**
	 * @brief Refill the object from a mapping-parameter block, without
	 *        touching the two owned filter pointers.
	 * @param params  Source block; see the field comments below for
	 *                which words are copied where.
	 */
	void reset(V92MappingParams *params);
	/**
	 * @brief Store new FIR coefficients and tap counts for both owned
	 *        filters.
	 * @param coefFir1  Coefficients for `fir1`.
	 * @param coefFir2  Coefficients for `fir2`.
	 * @param taps1     Tap count requested for `fir1` (stored verbatim
	 *                  in `taps1`; `FloatFIR` itself rounds down to a
	 *                  multiple of four).
	 * @param taps2     Tap count requested for `fir2`, same caveat.
	 */
	void setCoefficients(float *coefFir1, float *coefFir2,
			     unsigned int taps1, unsigned int taps2);
	/**
	 * @brief Precode one symbol's candidate points.
	 * @param in    Input vector.
	 * @param a     Frame selector (0..2) into `tableA`/`tableB`.
	 * @param b     Coset-constraint bit, folded into the search index.
	 * @param out   Integer candidate outputs.
	 * @param outf  Floating-point candidate outputs.
	 */
	void process(unsigned int *in, int a, int b, int *out, float *outf);

	/*
	 * Data members are public because the original's access specifiers
	 * are not recoverable from the mangling, and because one access
	 * section keeps the class standard-layout so `__builtin_offsetof` in
	 * the .cpp is well defined rather than merely supported.  The names
	 * are invented; the mangling never carries a data member's name.
	 *
	 * A 32-bit `mov` establishes a field's WIDTH and nothing about its
	 * type, so the integer spellings below are the honest ones only where
	 * the object gives no more.  Where it does -- `fstps`/`flds` at +0x70
	 * and +0x74, and the `jj` in `setCoefficients`' mangling for +0x78
	 * and +0x7c -- the type is evidence.
	 */

	/* +0x00  Not referenced by any of the six members, and not written by
	 * the constructor -- a real member rather than a vptr (see file
	 * comment); its contents are not recoverable from this class alone.
	 * `V92PreFilter` has the same unreferenced word at +0x00 (F1246). */
	unsigned int word_00;

	/* +0x04  A pointer into the parameter block (not a copy out of it),
	 * landing on `V92ParamsInfo`'s +0x9c..+0xb3 -- six ints, each
	 * selecting a constellation (`head`) and a modulus (`tableB`) below
	 * (finding F1372). */
	int *paramsAt9c;

	/* +0x08 .. +0x1c  Six constellation array pointers, copied from the
	 * parameters' `constellations[6]`; `process` dereferences the one
	 * `paramsAt9c` selects, as unsigned 32-bit values (finding F1372). */
	unsigned int *head[6];

	/* +0x20 .. +0x4c  Twelve words from the parameters' +0x1c..+0x48,
	 * indexed by `process` as `i + 4*a` over four symbols and a frame
	 * number `a` in 0..2 -- the step between constellation points, in
	 * `k * tableA[n] + in[n]` (finding F1372). */
	int tableA[12];

	/* +0x50 .. +0x64  Six words from the parameters' +0x6c..+0x80,
	 * indexed by `process` through `paramsAt9c`'s selector. The modulus
	 * the search interval is derived from (finding F1372). */
	int tableB[6];

	/* +0x68, +0x6c  The two owned filters, both built
	 * `FloatFIR(nTaps, 0, 99)` from the constructor's one argument --
	 * identical in every field, distinguished only by allocation order
	 * (see t_v92precoder.cpp). */
	FloatFIR *fir1;
	FloatFIR *fir2;

	/* +0x70, +0x74  Two floats of carried state, added to every
	 * candidate point and each replaced by one filter's output at the
	 * end of a symbol: +0x70 from `fir1` feeds the chosen point, +0x74
	 * from `fir2` feeds the sum. */
	float state0;
	float state1;

	/* +0x78, +0x7c  setCoefficients()'s third and fourth arguments,
	 * stored unchanged -- the tap counts the caller asked for, not the
	 * counts the filters ended up with (`FloatFIR` rounds down to a
	 * multiple of four and keeps its own `taps`). */
	unsigned int taps1;
	unsigned int taps2;
};

#endif /* DSPLIB_V92PRECODER_H */
