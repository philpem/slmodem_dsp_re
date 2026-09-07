/*
 * Scrambler.cpp -- see dsplib/Scrambler.h, which holds the bodies and the
 * whole derivation.  This file exists to EMIT the thirty-four members the
 * blob has, and to assert the layout of all five instantiations.
 *
 * INSTANTIATED MEMBER BY MEMBER, not `template class Scrambler<...>;`.
 *
 * `src/dsp/Queue.cpp` has the same note for the same reason, and here it is
 * sharper: the five instantiations do NOT have the same member set.
 * `Scrambler<unsigned char,int>` has no bulk `process`, `Scrambler<int,
 * unsigned char>` has no single-value one, `processAllOnes` and
 * `processAllZeros` exist only at `<unsigned char,unsigned char>`, and
 * `Descrambler<int,int>` has no bulk `process`.  A whole-class instantiation
 * would define six members the original does not contain -- weak symbols we
 * emit and it does not -- so each of the thirty-four is named.
 *
 * Until this file existed every one of them was inlined away at its call
 * sites, no object under `build/src` referenced any of them, and
 * `tools/closure.py --missing` reported all thirty-four missing however many
 * were written.  It now reports what is true.
 */

#include "dsplib/Scrambler.h"

/*
 * The layout, asserted for every instantiation rather than for the one
 * `V90Phase3Modulator.cpp` already covers.  Eight pointers-and-a-count at the
 * same offsets whatever `T` is, because every field but the last is a
 * pointer; the assertion is that nothing has been reordered or padded.
 *
 * Guarded on a 32-bit pointer for the reason src/v8/v8util.c gives: the 64-bit
 * builds exist to prove the CODE does not depend on 32-bit, and the blob's
 * layout is not something they can assert.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define SCR_OFF(cls, field, off, tag) \
	typedef char scrambler_off_##tag[ \
	    ((int)__builtin_offsetof(cls, field) == (off)) ? 1 : -1]

#define SCR_MAP(cls, tag) \
	SCR_OFF(cls, pLimit,	0x00, tag##_plimit); \
	SCR_OFF(cls, pInitOut,	0x04, tag##_pinitout); \
	SCR_OFF(cls, pInitTap1,	0x08, tag##_pinittap1); \
	SCR_OFF(cls, pInitTap2,	0x0c, tag##_pinittap2); \
	SCR_OFF(cls, pOut,	0x10, tag##_pout); \
	SCR_OFF(cls, pTap1,	0x14, tag##_ptap1); \
	SCR_OFF(cls, pTap2,	0x18, tag##_ptap2); \
	SCR_OFF(cls, tailLength, 0x1c, tag##_taillength); \
	typedef char scrambler_size_##tag[(sizeof(cls) == 0x20) ? 1 : -1]

typedef Scrambler<unsigned char, unsigned char> ScramblerHH;
typedef Scrambler<unsigned char, int> ScramblerHI;
typedef Scrambler<int, unsigned char> ScramblerIH;
typedef Descrambler<unsigned char, int> DescramblerHI;
typedef Descrambler<int, int> DescramblerII;

SCR_MAP(ScramblerHH, shh);
SCR_MAP(ScramblerHI, shi);
SCR_MAP(ScramblerIH, sih);
SCR_MAP(DescramblerHI, dhi);
SCR_MAP(DescramblerII, dii);

#endif /* 32-bit host */

/*
 * Scrambler<unsigned char, unsigned char> -- nine members, 696 bytes.
 *
 * THE CONSTRUCTOR IS NOT FIRST, AND THAT IS MEASURED RATHER THAN TIDY.  This
 * file holds no bodies at all, so nothing in it looks like it could move a
 * byte -- and `nm -n` cannot see the difference either, because every one of
 * the thirty-four lands in its own `.gnu.linkonce.t.*` section and they come
 * out SORTED whatever order they are written in.  The object is 34 of 34 at
 * the blob's index under every arrangement.
 *
 * What the order does reach is lever 3b's `peep2_find_free_register` cursor,
 * which is threaded in the order GCC PROCESSES functions, not the order they
 * are emitted.  With the constructor written first, `Scrambler<h,h>`'s C1
 * comes out `xor %edx,%edx` where the blob has `xor %ecx,%ecx`; with one
 * scratch-consuming instantiation ahead of it, it is byte-identical.
 *
 * Seventy-two cells, exhausted: all 9 positions of this group's constructor
 * line crossed with all 9 of its `reset` line, the other seven held in
 * their relative order.  TWO distinct emissions, and 57 of the 72 put C1 at
 * byte identity -- every arrangement except "constructor first" and
 * "constructor second with `reset` not above it".  So what is decoded is that
 * the constructor was not the first thing in this translation unit, which is
 * F0's several-preimages case; the position below is the smallest move that
 * reaches the object and no claim is made that it is the author's.
 * Finding F8144.
 */
template Scrambler<unsigned char, unsigned char>::~Scrambler();
template void Scrambler<unsigned char, unsigned char>::resetHistoryIndexes();
template Scrambler<unsigned char, unsigned char>::Scrambler(unsigned int,
							   unsigned int,
							   unsigned int);
template void Scrambler<unsigned char, unsigned char>::copyHistoryTail();
template void Scrambler<unsigned char, unsigned char>::reset(unsigned char);
template unsigned char
Scrambler<unsigned char, unsigned char>::process(unsigned char);
template void
Scrambler<unsigned char, unsigned char>::process(const unsigned char *,
						 unsigned char *, unsigned int);
template void
Scrambler<unsigned char, unsigned char>::processAllOnes(unsigned char *,
							unsigned int);
template void
Scrambler<unsigned char, unsigned char>::processAllZeros(unsigned char *,
							 unsigned int);

/* Scrambler<unsigned char, int> -- six members, 347 bytes.  No bulk form. */
template Scrambler<unsigned char, int>::Scrambler(unsigned int, unsigned int,
						  unsigned int);
template Scrambler<unsigned char, int>::~Scrambler();
template void Scrambler<unsigned char, int>::resetHistoryIndexes();
template void Scrambler<unsigned char, int>::copyHistoryTail();
template void Scrambler<unsigned char, int>::reset(unsigned char);
template int Scrambler<unsigned char, int>::process(unsigned char);

/* Scrambler<int, unsigned char> -- six members, 379 bytes.  Bulk form only. */
template Scrambler<int, unsigned char>::Scrambler(unsigned int, unsigned int,
						  unsigned int);
template Scrambler<int, unsigned char>::~Scrambler();
template void Scrambler<int, unsigned char>::resetHistoryIndexes();
template void Scrambler<int, unsigned char>::copyHistoryTail();
template void Scrambler<int, unsigned char>::reset(int);
template void Scrambler<int, unsigned char>::process(const int *,
						     unsigned char *,
						     unsigned int);

/* Descrambler<unsigned char, int> -- seven members, 467 bytes. */
template Descrambler<unsigned char, int>::Descrambler(unsigned int,
						      unsigned int,
						      unsigned int);
template Descrambler<unsigned char, int>::~Descrambler();
template void Descrambler<unsigned char, int>::resetHistoryIndexes();
template void Descrambler<unsigned char, int>::copyHistoryTail();
template void Descrambler<unsigned char, int>::reset(unsigned char);
template unsigned char Descrambler<unsigned char, int>::process(unsigned char);
template void Descrambler<unsigned char, int>::process(const unsigned char *,
						       int *, unsigned int);

/* Descrambler<int, int> -- six members, 353 bytes.  No bulk form. */
template Descrambler<int, int>::Descrambler(unsigned int, unsigned int,
					    unsigned int);
template Descrambler<int, int>::~Descrambler();
template void Descrambler<int, int>::resetHistoryIndexes();
template void Descrambler<int, int>::copyHistoryTail();
template void Descrambler<int, int>::reset(int);
template int Descrambler<int, int>::process(int);
