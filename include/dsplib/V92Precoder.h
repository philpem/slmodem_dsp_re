/*
 * V92Precoder.h -- the V.92 transmit precoder: two FloatFIRs it owns, over a
 * block of mapping parameters it copies out of `V92MappingParams`.
 *
 * Reconstructed from dsplibs.o.  Six members in the blob, 1,410 bytes; this
 * tree defines the constructor and the destructor and declares the other
 * four, because a declaration here is a specification (the signatures are the
 * mangling's) and a definition would be a claim.
 *
 * NOT POLYMORPHIC, AND THE ARGUMENT IS NOT THE USUAL ONE.  `tools/cppstruct.py`
 * lists the destructor as D1,D2 with no deleting `D0`, which settles only that
 * the DESTRUCTOR is not virtual -- a class can have virtual functions and a
 * non-virtual destructor.  What settles the rest is the constructor: it never
 * writes offset 0, and the constructor of a polymorphic class always stores
 * the vptr there.  So there is no vptr and +0x00 is a real member.
 *
 * THE OBJECT IS 0x80 BYTES, AND THAT IS MEASURED RATHER THAN BOUNDED.
 * `V92Transmitter::V92Transmitter` builds one with
 *
 *     53bfa:  c7 04 24 80 00 00 00   movl $0x80,(%esp)
 *     53c01:  e8 ..                  call sysdep_malloc
 *     53c08:  b8 40 01 00 00         mov  $0x140,%eax
 *     53c14:  e8 ..                  call V92Precoder::V92Precoder(unsigned)
 *
 * -- so 0x80 is the size the original's `sizeof` produced, not the largest
 * displacement anybody happened to use.  The two agree: the furthest any of
 * the six members reaches is +0x7c, a four-byte store in `setCoefficients`.
 * 0x140 is also the only tap count the object ever asks for (finding 1245).
 *
 * WHAT PROVES WHICH POINTER IS OWNED.  `reset(V92MappingParams *)` refills the
 * object from the parameter block a word at a time, and it writes +0x04
 * through +0x64 and +0x70, +0x74 -- skipping EXACTLY +0x68 and +0x6c.  Those
 * two are the FloatFIRs the constructor allocated and the destructor frees,
 * and a reset that overwrote them would leak both filters and leave the
 * object pointing at parameter words.  The skip is the map's best evidence.
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
	/*
	 * Written.  The constructor allocates both filters and does nothing
	 * else -- it leaves every other field of the object alone, which is
	 * why `reset(V92MappingParams *)` exists and why the test seeds the
	 * storage and checks that all but eight bytes survive.
	 */
	V92Precoder(unsigned int nTaps);
	~V92Precoder();

	/*
	 * The signatures are the mangling's; a return type is never mangled,
	 * so all four are unknown and spelled `void` where nothing in the
	 * caller uses a result.  `reset()` is the one member of this class
	 * this tree has not written.
	 */
	void reset();
	void reset(V92MappingParams *params);
	void setCoefficients(float *coefFir1, float *coefFir2,
			     unsigned int taps1, unsigned int taps2);
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

	/*
	 * +0x00  NOT REFERENCED BY ANY OF THE SIX MEMBERS, and not written by
	 * the constructor.  It is a member rather than a vptr for the reason
	 * in the header comment; what it holds is not recoverable from this
	 * class alone.  V92PreFilter has the same unreferenced word at +0x00
	 * (finding 1246).
	 */
	unsigned int word_00;

	/*
	 * +0x04  `lea 0x9c(%ebx),%edx` in `reset(V92MappingParams *)`: a
	 * pointer INTO the parameter block, not a copy out of it.  It lands
	 * on `struct V92ParamsInfo`'s +0x9c -- the 0x18 bytes between +0x9c
	 * and the end of the 0xb4 block, which is TWENTY-FOUR and was written
	 * "the eighteen bytes" here until somebody read the hex twice.  That
	 * header recorded them as reached by nothing; `process` reaches them
	 * as SIX INTS indexed `(i + 4 * a) % 6`, and the unpacker fills them
	 * with a real indexed loop, which is what named them `indexConstel`.
	 * Each one selects a constellation and a modulus below.
	 */
	int *paramsAt9c;

	/*
	 * +0x08 .. +0x1c  Six words copied from the parameters' +0x84..+0x98,
	 * which is `struct V92ParamsInfo::constellations` -- so these are the
	 * six constellation ARRAYS and not six scalars.  `process`
	 * dereferences the one `paramsAt9c` selects, and does it with
	 * `push $0; push value; fildll`, the sequence for an UNSIGNED 32-bit
	 * to floating conversion; a signed element would be one `fildl`.
	 */
	unsigned int *head[6];

	/*
	 * +0x20 .. +0x4c  Twelve words from the parameters' +0x1c..+0x48.
	 * `process` indexes this one -- `mov 0x20(%ebx,%ebp,4),%esi` -- which
	 * is what makes it an array rather than twelve fields.  The index is
	 * `i + 4 * a` over four symbols, so `a` is a frame number in 0..2 and
	 * twelve is three frames of four.  It is the step between
	 * constellation points: every candidate is `k * tableA[n] + in[n]`.
	 */
	int tableA[12];

	/*
	 * +0x50 .. +0x64  Six words from the parameters' +0x6c..+0x80, also
	 * indexed by `process` (`mov 0x50(%edi,%edx,4),%esi`) -- through
	 * `paramsAt9c`'s selector rather than through `n`.  Twice the entry
	 * is the modulus the search interval is derived from.
	 */
	int tableB[6];

	/*
	 * +0x68, +0x6c  The two owned filters.  Both are built
	 * `FloatFIR(nTaps, 0, 99)` from the constructor's one argument, so
	 * they are identical in every field; nothing in the final state of
	 * the object distinguishes them and only the order they are allocated
	 * in does.  See t_v92precoder.cpp.
	 */
	FloatFIR *fir1;
	FloatFIR *fir2;

	/*
	 * +0x70, +0x74  Floats: `reset(V92MappingParams *)` zeroes both with
	 * an integer store and `process` reads them with `flds` and writes
	 * them back with `fstps`.  Two samples of carried state: both are
	 * added to every candidate point, and each is replaced by one
	 * filter's output at the end of a symbol -- +0x70 from `fir1` fed the
	 * chosen point, +0x74 from `fir2` fed the sum.
	 */
	float state0;
	float state1;

	/*
	 * +0x78, +0x7c  `setCoefficients`' third and fourth arguments, stored
	 * unchanged.  Their type is the mangling's `j`, unsigned int.  They
	 * are the tap counts the caller asked for -- NOT the counts the
	 * filters ended up with, which FloatFIR rounds down to a multiple of
	 * four and keeps in its own `taps`.
	 */
	unsigned int taps1;
	unsigned int taps2;
};

#endif /* DSPLIB_V92PRECODER_H */
