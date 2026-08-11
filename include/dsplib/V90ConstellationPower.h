/*
 * V90ConstellationPower.h -- the V.90 downstream constellation power model.
 *
 * Reconstructed from dsplibs.o.  Five members and one static data member;
 * ONE pair is written here, the constructor and the destructor, and both are
 * a single `ret`.
 *
 * WHY A CLASS WHOSE CONSTRUCTOR DOES NOTHING IS STILL DECLARED HERE.  The
 * blob has four symbols for it -- `C1`, `C2`, `D1`, `D2` at 0x3dd40, 0x3dd50,
 * 0x3dd60 and 0x3dd70, one byte each -- and GCC emits an out-of-line
 * constructor or destructor symbol only for a USER-DECLARED one.  An implicit
 * trivial destructor produces no symbol at all.  So the four bytes are
 * themselves the evidence that the original declared both and left both
 * bodies empty, and reproducing that means declaring them and leaving them
 * empty here.  `test/unit/t_v90designers.cpp` runs all four against the blob
 * and asserts that they write nothing, so "does nothing" is measured rather
 * than assumed.
 *
 * NOT POLYMORPHIC: the destructor appears with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one, so
 * offset 0 is a real member and there is no vptr.
 *
 * THE OBJECT IS 144 BYTES (0x90), and the constructor is not what proves it --
 * it initialises nothing.  Two other members do, and they agree:
 *
 *   - `calcModulusParameters` (0x3dd80) takes `this` in %edi from the first
 *     stack argument and writes every four-byte slot from +0x08 to +0x5f, then
 *     six eight-byte `fstl`/`fstpl` at +0x60, +0x68, +0x70, +0x78, +0x80 and
 *     +0x88.  The last is eight bytes wide at +0x88, so the object ends at
 *     0x90.  A displacement is not a size (finding 215); the width of what
 *     sits at the bound is what turns one into the other.
 *   - `getPower` (0x3e0e0) takes `this` in %esi -- unambiguously, because it
 *     hands %esi straight to `calcModulusParameters` as that call's first
 *     argument -- and indexes the same three regions with the same element
 *     widths: `0x48(%esi,%edi,4)`, `0x18(%esi,%edi,8)` and
 *     `0x60(%esi,%edi,8)`.  Six elements of eight bytes from +0x60 is again
 *     0x90.
 *
 * SO THE INTERIOR IS `pad_`, and deliberately.  `getPower` alone shows a
 * pointer and an int at +0x00 and +0x04 (`mov %eax,(%esi)` and
 * `mov %eax,0x4(%esi)`), a 64-bit integer at +0x08 (`fildll 0x8(%esi)`), a run
 * of 64-bit integers from +0x10, six 32-bit values from +0x48 and six doubles
 * from +0x60 -- but nothing reconstructed here reads or writes any of them, so
 * giving them names and types would be a guess dressed as a measurement.  The
 * size is the claim; the contents are not.
 *
 * NOT DECLARED HERE: the static data member `averagePowerLimits`, 0x8c bytes
 * of `.data` at 0xb60 that `getPowerIndexForPower` indexes.  It is a real
 * member of this class, it is not reconstructed, and declaring it without
 * defining it would put an undefined symbol in every test binary that
 * happened to reference it.
 */

#ifndef DSPLIB_V90CONSTELLATIONPOWER_H
#define DSPLIB_V90CONSTELLATIONPOWER_H

class V90ConstellationPower {
public:
	/*
	 * Both one byte, both `ret`.  See the file comment for why they are
	 * declared at all.
	 */
	V90ConstellationPower();
	~V90ConstellationPower();

	/*
	 * Public for `offsetof`, and because the original's access specifiers
	 * are not recoverable from the mangling.  See V90ConstellationDesigner.h.
	 */

	/*
	 * The whole object.  144 bytes, none of it modelled as fields: see the
	 * file comment.  An offset landing here is the answer, not a gap in
	 * the answer.
	 */
	unsigned char pad_00[0x90];
};

#endif /* DSPLIB_V90CONSTELLATIONPOWER_H */
