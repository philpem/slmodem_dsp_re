/*
 * V90RDetector.h -- the V.90 R / Rf sequence detector.
 *
 * Reconstructed from dsplibs.o.  Seven members; ONE pair is written here, the
 * constructor (0x3da20, 12 bytes) and the destructor (0x3da40, one byte).
 *
 * NOT POLYMORPHIC: the destructor appears with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one, so
 * offset 0 is a real member and there is no vptr.
 *
 * THE OBJECT IS 44 BYTES (0x2c), and all seven members agree on it.  The
 * largest `this`-relative displacement any of them uses is +0x28 and it is a
 * four-byte store -- `mov %edx,0x28(%eax)`, the constructor's only
 * instruction that touches the object -- so the object ends at 0x2c.  A
 * displacement is not a size (finding F215); the width of what sits at the
 * bound is what turns one into the other.  Nothing reaches past +0x28: the
 * four detectors and `reset` stop at +0x24.
 *
 * THE CONSTRUCTOR IS ONE STORE.  Twelve bytes, no frame:
 *
 *     mov 0x8(%esp),%edx      ; argument 1
 *     mov 0x4(%esp),%eax      ; this
 *     mov %edx,0x28(%eax)
 *
 * and `_ZN12V90RDetectorC1EP13V90Parameters` is what types it a
 * `V90Parameters *`.  Nothing reconstructed here dereferences it, so the type
 * comes from the mangling and not from a load -- which is the strongest
 * evidence a store-only constructor can offer.  Note what the constructor
 * does NOT do: it leaves every other field of the object exactly as it found
 * it, so a `V90RDetector` is unusable until `reset` has run.
 *
 * THE OTHER FIELDS ARE NAMED FROM THEIR STORE WIDTHS, not from their meaning.
 * `reset` (0x3da60) and the four detectors are not reconstructed here; the
 * widths below are read off their encodings, which is a measurement, and the
 * names are offsets, because nothing here knows what any of them counts:
 *
 *   +0x00 +0x14 +0x18 +0x1c   `movl $0x0` -- four bytes each.  +0x00 is the
 *                             one every detector increments and compares
 *                             against 6 or 12.
 *   +0x04 +0x08 +0x0c +0x10   `mov %reg,` -- four bytes.  `reset` fills them
 *                             from its two unsigned arguments by the
 *                             `mul $0xaaaaaaab` reciprocal idiom, and the
 *                             detectors compare +0x14/+0x18/+0x1c against
 *                             them.
 *   +0x20                     `movw $0x0` -- TWO bytes, and UNSIGNED: every
 *                             detector loads it with `movzwl 0x20(%ecx),%eax`
 *                             and uses the 32-bit result, which is the
 *                             signedness the compiler was FORCED to encode
 *                             (CLAUDE.md's rule; finding F613 is the case that
 *                             found a real defect).
 *   +0x24                     `movl $0x1` and `movl $0xffffffff` -- four
 *                             bytes and SIGNED, because -1 is one of the two
 *                             values stored and `detectRNot` branches on
 *                             `test %edx,%edx; jle`.
 *
 * WHAT THE FIVE UNWRITTEN MEMBERS TURNED OUT TO DO WITH THEM.  The names are
 * left as offsets -- they are what the existing test and mutation set name --
 * but the meanings are no longer unknown, and this is the record of them:
 *
 *   +0x00  the number of samples taken so far in the current GROUP.  Every
 *          detector increments it, compares it against 6 (R) or 12 (Rf), and
 *          returns 0 without doing anything else until it matches.
 *   +0x20  a shift register of SIGN BITS, one per sample, newest in bit 0:
 *          `bits = bits * 2; if (sample > 0) bits |= 1`.  Truncated to 16
 *          bits by the store, which is what bounds the patterns below.
 *   +0x04  what +0x14 and +0x18 are compared against, and it is `reset`'s
 *          FIRST argument rounded down to a multiple of 6.
 *   +0x08  the same for +0x1c, from `reset`'s SECOND argument.
 *   +0x0c  +0x04's twelve-sample counterpart: argument 1 rounded down to a
 *          multiple of 12, and what `detectRf` compares against.
 *   +0x10  argument 2 rounded down to a multiple of 12, for `detectRfNot`.
 *   +0x14  a run length in SAMPLES of the pattern that starts positive
 *          (0x38 for R, 0xccc for Rf), incremented by the group size.
 *   +0x18  the same for the pattern that starts negative (0x07, 0x333).
 *   +0x1c  the run length the two `Not` detectors keep, against whichever of
 *          the two patterns +0x24 selects.
 *   +0x24  which pattern is the live one: +1 selects the positive-first
 *          pattern and -1 the other.  `reset` starts it at +1, and the two
 *          plain detectors set it when their run reaches the limit.
 */

#ifndef DSPLIB_V90RDETECTOR_H
#define DSPLIB_V90RDETECTOR_H

/*
 * A POINTER ONLY, so a forward declaration is what belongs here.  Two
 * different definitions of `V90Parameters` exist in this tree and no
 * translation unit may include both (finding F1112); declaring the class keeps
 * this header compatible with either.
 */
class V90Parameters;

class V90RDetector {
public:
	/*
	 * THE DESTRUCTOR IS ONE BYTE, a bare `ret`.  It is declared because
	 * the blob has the symbol: GCC emits an out-of-line destructor only
	 * for a user-declared one, so a class with an implicit destructor
	 * contributes no `D1`/`D2` at all.  The test drives it and asserts it
	 * writes nothing.
	 */
	V90RDetector(V90Parameters *params);
	~V90RDetector();

	/*
	 * `reset` takes two sample counts and rounds each DOWN to a multiple
	 * of 6 and of 12; the four detectors take one sample each and answer
	 * 0 until a whole group of 6 or 12 has arrived.  The source has the
	 * shape and the sign patterns.
	 */
	void reset(unsigned int rSamples, unsigned int rNotSamples);
	int detectR(short sample);
	int detectRNot(short sample);
	int detectRf(short sample);
	int detectRfNot(short sample);

	/*
	 * Public for `offsetof`; the original's access specifiers are not
	 * recoverable, and one access section is what keeps `offsetof`
	 * meaningful.  See V90ConstellationDesigner.h.
	 */

	int int_00;			/* +0x00 */
	int int_04;			/* +0x04 */
	int int_08;			/* +0x08 */
	int int_0c;			/* +0x0c */
	int int_10;			/* +0x10 */
	int int_14;			/* +0x14 */
	int int_18;			/* +0x18 */
	int int_1c;			/* +0x1c */
	unsigned short ushort_20;	/* +0x20 */
	/*
	 * +0x22 was `pad_22[2]` -- REMOVED (finding F10145).  Already
	 * correctly described as alignment before +0x24; proved mechanically
	 * by the existing `RD_OFF(ushort_20, 0x20, ...)`/`RD_OFF(int_24,
	 * 0x24, ...)` and by `dis.py` over all nine `V90RDetector` methods
	 * finding no access to offset 0x22/0x23.
	 */
	int int_24;			/* +0x24 */
	V90Parameters *params;		/* +0x28 = constructor argument 1  */
};

#endif /* DSPLIB_V90RDETECTOR_H */
