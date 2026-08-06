/*
 * ResamplerTimingOffset.h -- the resampler's timing-offset control.
 *
 * Reconstructed from dsplibs.o.  Seven members, 298 bytes; `setTimingOffset`
 * is the one written here and the one `v34handshak` reaches.
 *
 * THIS CLASS IS POLYMORPHIC AND OFFSET 0 IS ITS VPTR.  It is one of finding
 * 228's four, and a twenty-one-byte setter is exactly where nobody expects a
 * layout trap: a struct that is right in size and wrong by four in every
 * offset passes a size check and fails everything after it.  Three
 * independent measurements, none of them a guess:
 *
 *   - `tools/cppstruct.py ResamplerTimingOffset` lists the destructor with a
 *     `D0` variant.  `D0` is the DELETING destructor, which GCC emits only
 *     for a virtual one.  (Every other class in this batch is listed with
 *     `D1` and `D2` alone.)
 *   - `nm -C` has `V vtable for ResamplerTimingOffset`.
 *   - The constructor and destructor both do
 *
 *         movl $0x8,(%eax)   <== R_386_32 _ZTV21ResamplerTimingOffset
 *
 *     -- the vtable's address plus eight, stored at offset ZERO.  Eight is
 *     the Itanium ABI's two leading words (offset-to-top and the type info),
 *     so this is a vptr and not a small integer that happens to be 8.
 *
 * IT IS MODELLED AS A LEADING POINTER-SIZED FIELD AND NOT AS `virtual`.
 * Declaring a real virtual member would make GCC emit a vtable, which needs
 * every virtual method defined or a key function present, and that re-opens
 * the link closure for a twenty-one-byte setter -- docs/v90cpp.md.  Nothing
 * here dispatches through `vptr`; it exists to put the other two fields where
 * the blob puts them.  The class derives from `Resampler`: `reset()` and the
 * destructor both call `_ZN9Resampler*`, and the vptr is the base's.
 *
 * THE OBJECT IS 76 BYTES.  The largest `this`-relative displacement across
 * all eleven defined members is +0x48, four bytes wide, so the object ends at
 * 0x4c.
 *
 * THE ARGUMENT IS IN PARTS PER MILLION, which is read off the whole family
 * rather than off `setTimingOffset` alone.  The setter is
 *
 *     timingOffset = ppmScale * arg * 1e-6f
 *
 * and `getTimingOffsetPPM() const` is its inverse, `1e6f * timingOffset /
 * ppmScale`, with 1e6f loaded from .rodata.cst4 by that method and 1e-6f
 * (0x358637bd) by this one.  So the argument and the accessor's result are in
 * the same units, the accessor names those units PPM, and +0x2c is whatever
 * factor converts one to the other -- which is what `ppmScale` says and all
 * it says.  Data member names are invented and descriptive; the mangling
 * preserves method and type names but never a data member's (finding 226).
 *
 * The regions between are `pad_`.  One measurement worth keeping out of them:
 * `timingCorrection(float)` does `fldl 0xc(%eax)` / `fadds 0x48(%eax)` /
 * `fstpl 0xc(%eax)`, so +0x0c is an EIGHT-byte accumulator that
 * `timingOffset` is added into -- the only `double` anywhere in this class,
 * and the reason its alignment is worth checking if +0x04..+0x2b is ever
 * modelled.  On i386 `__alignof__(double)` is 4, which is what lets a
 * `double` sit at +0x0c at all.
 */

#ifndef DSPLIB_RESAMPLERTIMINGOFFSET_H
#define DSPLIB_RESAMPLERTIMINGOFFSET_H

class ResamplerTimingOffset {
public:
	/* Defined in src/pump/v90/ResamplerTimingOffset.cpp. */
	void setTimingOffset(float);

	/* Public for offsetof; see V90ConstellationDesigner.h. */

	/*
	 * The vptr.  A plain pointer-sized field, NOT a `virtual` member --
	 * see the file comment.  Nothing reads it here.
	 */
	void *vptr;			/* +0x00 &_ZTV21ResamplerTimingOffset[2] */

	unsigned char pad_04[0x28];	/* +0x04 incl. the double at +0x0c  */
	float ppmScale;			/* +0x2c PPM <-> internal units     */
	unsigned char pad_30[0x18];	/* +0x30                            */
	float timingOffset;		/* +0x48                            */
};

#endif /* DSPLIB_RESAMPLERTIMINGOFFSET_H */
