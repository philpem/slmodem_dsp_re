/*
 * V90Jd.cpp -- V.90 Jd message packing.
 *
 * Reconstructed from dsplibs.o V90Jd.cpp.  Two of the class's thirteen
 * methods: `getBitVector()` and `unPackReset()`, which are the two that are
 * leaves.  `include/dsplib/V90Jd.h` carries the object map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x4(%esp),%eax` -- not %ecx, so these are not thiscall and
 * nothing here needs an attribute (finding F215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals, no allocation, no static data members, so
 * the test binaries still link with $(CC).
 */

#include <stddef.h>

#include "dsplib/V90Jd.h"
#include "dsplib/V90Parameters.h"	/* the constructor's five fields */

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset, which is
 * the failure docs/v90cpp.md warns about for the classes that DO have a vptr.
 */
#define V90JD_OFF(field, off, tag) \
	typedef char v90jd_off_##tag[ \
	    ((int)__builtin_offsetof(V90Jd, field) == (off)) ? 1 : -1]

V90JD_OFF(unpack,     0x00, unpack);
V90JD_OFF(bits,       0x02, bits);
V90JD_OFF(crc,        0x4c, crc);
V90JD_OFF(unpackWord, 0x8c, unpackword);
typedef char v90jd_size[(sizeof(V90Jd) == 0x90) ? 1 : -1];

/*
 * ===========================================================================
 * The constructor, 0x1e790, and it is where the header's bit map comes from.
 *
 * Five fields of V90Parameters, and the author's own names for all five
 * (finding F860's extraction of `loadParams`):
 *
 *     DIGITAL_RATE_MASK             +0x2c  28 bits, spread over two groups
 *     MAX_SPECTRAL_SHAPER_LOOKAHEAD +0x30  two bits
 *     V34_PHASE4_CONSTELLATION      +0x34  one byte
 *     V34_RRN_CONSTELLATION         +0x38  one byte
 *
 * THE TWO GROUPS ARE 16 AND 12 BITS, NOT 14 AND 14.  The first loop is
 * `cmp $0xf,%ecx; jle` and the second `cmp $0xb,%edx; jle`, so bits 0..15 of
 * the mask land in `bits[18..33]` and bits 16..27 in `bits[35..46]` -- 28
 * rates, with the group boundary falling inside the mask rather than between
 * two halves of it.  V92Jd's second loop stops one earlier; see there.
 *
 * BOTH COUNTERS ARE SIGNED.  `jle`, not `jbe`, in both loops here, against
 * V92Jd's `jbe` on its phase loop.  That is the induction variable's declared
 * type showing through, which the codegen tier can see and the differential
 * tier cannot, so it is written the way the object was compiled: `int` here.
 *
 * The store order below is the object's.  GCC is free to reorder it and does
 * (CLAUDE.md's rule for reading a codegen difference); what is not free is
 * which byte gets which value.
 * ===========================================================================
 */
V90Jd::V90Jd(V90Parameters *params)
{
	int mask;
	int i;

	unpack[0] = 0;
	unpack[1] = 0;
	unpackWord = 0;

	/*
	 * The maximum lookahead, two bits.  `movzbl` then `and $1` and
	 * `shr $1; and $1` -- the low two bits of the parameter's low byte,
	 * and nothing above them can reach the message.
	 */
	bits[49] = (unsigned char)(params->MAX_SPECTRAL_SHAPER_LOOKAHEAD & 1);
	bits[50] = (unsigned char)
	    ((params->MAX_SPECTRAL_SHAPER_LOOKAHEAD >> 1) & 1);

	/*
	 * The constellation size, two bits, one parameter each -- a whole-word
	 * load and a byte store, so only the low byte of either is carried.
	 */
	bits[47] = (unsigned char)params->V34_PHASE4_CONSTELLATION;
	bits[48] = (unsigned char)params->V34_RRN_CONSTELLATION;

	mask = params->DIGITAL_RATE_MASK;
	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP1 + 1 + i] = (unsigned char)((mask >> i) & 1);
	for (i = 0; i <= 11; i++)
		bits[V90JD_GROUP2 + 1 + i] =
		    (unsigned char)((mask >> (i + 16)) & 1);
}

/*
 * One byte of `ret` in the blob, at 0x1e890 and 0x1e8a0.  It exists here
 * because the object has the symbols and a caller that destroys a V90Jd needs
 * them; see the header for why an implicit one would not do.
 */
V90Jd::~V90Jd()
{
}

/*
 * ===========================================================================
 * The three accessors, 0x1e8b0, 0x1e900 and 0x1e920 -- AND THEY DO NOT READ
 * THE LAYOUT THE CONSTRUCTOR WRITES.
 *
 * The constructor and `getBitVector` build a FRAMED message: seventeen 1 bits,
 * then a 0 at `bits[17]`, the low sixteen mask bits at `bits[18..33]`, a 0 at
 * `bits[34]`, the high twelve at `bits[35..46]`, and the two small fields at
 * `bits[47..48]` and `bits[49..50]`.  These three read
 *
 *     getRatesMask         +0x02..+0x11 and +0x12..+0x1d   bits[0..15], [16..27]
 *     getConstelationSize  +0x1e, +0x1f                    bits[28], bits[29]
 *     getMaxLookahead      +0x20, +0x21                    bits[30], bits[31]
 *
 * -- a payload-contiguous layout with no group markers in it, 28 rate bits
 * then two and two, ending at `bits[31]`.  Nothing that PACKS in this class
 * writes that.  `unPackData` DOES: it strips the framing off an incoming
 * message and fills `bits[0..47]` flat, which is why these three read where
 * they read.  The class has two layouts because it has two directions, and
 * D270 -- opened here when the accessors landed and the unpacker had not --
 * says so now.  Both are reproduced as the object has them and each is driven
 * against the blob.
 *
 * A BYTE COUNTS AS SET IF IT IS NON-ZERO, and that is not the same rule the
 * two small fields use.  `getRatesMask` tests `cmpb $0x0`, so a byte of 2
 * contributes its bit; `getMaxLookahead` does `and $0x1` on both of its, so a
 * byte of 2 contributes nothing.  The test seeds the vector with varied bytes
 * rather than with 0 and 1 so that the two rules are told apart.
 *
 * THE RETURN TYPES ARE MEASURED, not assumed -- a return type is not mangled.
 * `getRatesMask` leaves a 32-bit value in `%eax` (`mov %edx,%eax` on an
 * accumulator built with `shl`/`or`), so it returns an `int`.
 * `getMaxLookahead` ends `movzbl %dl,%eax` on a sum computed in a byte
 * register, which is an `unsigned char` result widened at the return.
 * `getConstelationSize` writes through both pointers and sets `%eax` to
 * nothing, so it returns void.
 * ===========================================================================
 */
int
V90Jd::getRatesMask()
{
	int mask = 0;
	int i;

	for (i = 0; i <= 15; i++)
		if (bits[i])
			mask |= 1 << i;

	for (i = 0; i <= 11; i++)
		if (bits[16 + i])
			mask |= 1 << (i + 16);

	return mask;
}

void
V90Jd::getConstelationSize(unsigned char *first, unsigned char *second)
{
	*first = bits[28];
	*second = bits[29];
}

/*
 * The high bit is `bits[31]` and the low one `bits[30]`, which is the order
 * `setMaxLookahead` and the constructor use for the pair they write -- at
 * `bits[50]` and `bits[49]`, nineteen bytes further on.
 */
unsigned char
V90Jd::getMaxLookahead()
{
	return (unsigned char)((bits[30] & 1) + ((bits[31] & 1) << 1));
}

/*
 * ===========================================================================
 * THE CRC-16 THE MESSAGE CARRIES, and it is written out INLINE in both bodies
 * below rather than through a shared helper.  That is a measurement, not a
 * style choice, and finding F7940 is the whole of it.
 *
 * The arithmetic first.  Sixteen ints, one per bit, shifting down toward
 * crc[0], with the feedback
 *
 *     t = <input bit> + crc[0]
 *
 * folded into positions 3, 10 and 15.  The three sums are `add` and the
 * result is masked to one bit afterwards, which is XOR spelled as addition;
 * that is why the input byte is used unmasked -- only its low bit can reach
 * the answer.  Sixteen ints to hold sixteen bits is the original's choice,
 * not a convenience here: the register lives at +0x4c as `int crc[16]` and
 * `resetCrc()` writes it there.
 *
 * NOW THE SHAPE, WHICH IS 74 OF THE 149 INSTRUCTIONS.  In the object the CRC
 * register is NOT IN MEMORY while the group loop runs.  GCC 3.4.2 loads
 * crc[0..15] into sixteen stack slots BEFORE the loop (0x1e9af..0x1ea2d),
 * runs both groups entirely on the slots, and writes all sixteen back AFTER
 * it (0x1eaec..0x1eb5a); there is not one store to `0x4c(%edi)` between.
 * That promotion is why the frame is `sub $0x48,%esp` -- sixteen ints plus
 * the two loop variables -- and it does not happen unless the source meets
 * BOTH of two conditions, each measured by removing it:
 *
 *   1. THE SHIFT IS WRITTEN OUT.  Rolled as `for (i = 0; i <= 14; i++)
 *      crc[i] = crc[i + 1];` every element sits at a VARIABLE address, no
 *      address is loop-invariant, and nothing can be promoted: the body
 *      compiles to 75 instructions and 218 bytes against 149 and 534, with
 *      `mov 0x4(%ecx,%edx,4),%eax` where the object has slot copies.
 *
 *   2. BOTH SIDES ARE MEMBER SUBSCRIPTS OF `this`, not pointers.  A helper
 *      taking `int *crc` and `const unsigned char *in` -- the obvious
 *      factoring, and the one this file used to have -- reaches the loop as
 *      two INDIRECT_REFs that GCC cannot tell apart, so the stores have to
 *      stay in case the byte read aliases them, and the promotion collapses
 *      to a per-group reload.  Written as `crc[i]` and `bits[...]`, the two
 *      are component references to different fields of one record, GCC
 *      proves they cannot overlap, and the stores sink out of the loop.
 *      Measured in four steps: helper with pointers 75 insns; shift written
 *      out 141; helper given `V90Jd *` but keeping `int *crc` 137; both
 *      sides member subscripts 149, which is the object's count exactly.
 *
 * IT IS NOT AN ALIASING PROBLEM IN THE C SENSE, and that was tested twice:
 * casting the input read to `const int *` and then to `const float *` -- the
 * latter genuinely cannot alias `int` under -fstrict-aliasing -- moved
 * nothing while the pointers stayed pointers.  What GCC 3.4.2 needs is the
 * COMPONENT REFERENCE, not a compatible alias set.
 *
 * AND THE INPUT IS SUBSCRIPTED, NOT WALKED.  `*in++` over a local pointer
 * costs the same disambiguation and gives nothing back: the object's
 * `movzbl (%ebx),%eax; inc %ebx` and its `dec %esi; jns` countdown are
 * produced by strength reduction FROM the subscript, so writing the pointer
 * by hand reaches 136 instructions and stops.  Finding F7941.
 * ===========================================================================
 */

/*
 * ===========================================================================
 * packData, 0x1e960, 534 bytes -- AND NOTHING IN THE OBJECT CALLS IT.
 *
 * `nm -S` gives `0001e960 00000216 T _ZN5V90Jd8packDataEv`, and no relocation
 * of any kind names that symbol anywhere in the 1.2 MB object.  It is not a
 * stub either: it is the whole packer, and it is reproduced here because the
 * vendor shipped it, not because anything reaches it.
 *
 * IT CALLS NOTHING, so this batch is link-closed.  There is no `call` in the
 * 534 bytes and no relocation inside them.  In particular `resetCrc` --
 * declared in the header and still deliberately undefined -- is INLINED: the
 * loop at 0x1e9a0 is byte for byte the body of `_ZN5V90Jd8resetCrcEv` at
 * 0x1e940, with `%edi` where the standalone copy uses `%edx`.  So the loop is
 * written out below rather than turned into a call, which is what the object
 * has and is also the only thing that links.
 *
 * EVERY COUNTER IS SIGNED, checked the way the constructor's two were.  `jle`
 * at 0x1e979 (group 0), 0x1e9ad (the CRC reset), 0x1eae6 (the group loop) and
 * 0x1eb6c (the CRC write-back); the innermost loop is strength-reduced to a
 * countdown, `mov $0xf,%esi` at 0x1ea35 and `dec %esi; jns` at 0x1eabd, and
 * `jns` is a signed test as well.  Five loops, five signed branches, so five
 * `int` induction variables.  There is no `jbe` in the function.
 *
 * BOTH GROUPS ARE SIXTEEN BITS HERE, AND THAT IS NOT THE 16/12 SPLIT.  The
 * inner bound `mov $0xf,%esi` sits at 0x1ea35, INSIDE the outer loop whose
 * back edge is `cmpl $0x1,0x4(%esp); jle 1ea31`, so both passes run sixteen
 * times.  16/12 is the CONSTRUCTOR's split of the 28-bit RATE MASK; group 2's
 * PAYLOAD is sixteen bits because it is those twelve rate bits plus the two
 * constellation-size bits at bits[47..48] and the two lookahead bits at
 * bits[49..50].  The CRC therefore covers all thirty-two payload bits, and a
 * reconstruction that carried the constructor's 12 across would checksum
 * twenty-eight.
 *
 * THE FEEDBACK IS `add` FOLLOWED BY `and $0x1`, never `xor`: 0x1ea50, 0x1ea74,
 * 0x1eaa9 and the three masks at 0x1ea7e, 0x1eaaf, 0x1eab2.  The bit is loaded
 * `movzbl` and goes in unmasked, so only its low bit can reach the answer --
 * the same shape the CRC block above carries for `getBitVector`.
 *
 * THE INPUT POINTER is `0x8(%esp)`, which starts at `this` and takes
 * `addl $0x11` once per group while `%ebx` is set to it plus `0x14`; `this`
 * plus 0x14 is `&bits[18]` and plus 0x25 is `&bits[35]`, which is the two
 * groups' payloads with each leading 0 skipped.
 *
 * WHAT IT DOES NOT TOUCH is as measured as what it does.  The last store in
 * the function is `mov %eax,0x88(%edi)`, which is crc[15]; +0x00, +0x01 and
 * +0x8c are never written, so packData leaves the UNPACKER's three state
 * fields exactly as it found them.  t_v90packdata seeds all three non-zero for
 * that reason, and seeds the CRC register non-zero so that the reset at
 * 0x1e9a0 is distinguishable from its absence.
 *
 * THE BODY IS `getBitVector`'s, INSTRUCTION FOR INSTRUCTION.  149 instructions
 * and 515 bytes here against 150 and 518 at 0x1ef10; the one extra instruction
 * is `lea 0x2(%edi),%eax`, the `return bits`, and every other difference is a
 * branch label at the same function-relative offset (+0x10, +0x40, +0xe0,
 * +0x200) or an %edx/%ebx swap the allocator was free to make.  The economical
 * reading is that the original wrote `getBitVector() { packData(); return
 * bits; }` -- which is exactly the shape V92Jd.cpp already has for
 * `getJdBitVector`/`packJdData`, whose 665-byte callee was NOT inlined -- and
 * that GCC inlined this 534-byte one, which would also explain why no
 * relocation names packData: its only caller was inlined away.  THAT IS
 * RECORDED AND NOT ACTED ON.  Writing the call would put a relocation on a
 * symbol the object has none for, and this batch owns packData alone, so the
 * body appears twice in this file exactly as it appears twice in the object.
 * ===========================================================================
 */
void
V90Jd::packData()
{
	int i, g;

	/* Group 0, seventeen 1 bits, and the object writes them first. */
	for (i = 0; i <= 16; i++)
		bits[i] = 1;

	/* Each later group opens with a 0, and four 0 bits close the message. */
	bits[V90JD_GROUP1] = 0;
	bits[V90JD_GROUP2] = 0;
	bits[V90JD_GROUP3] = 0;
	bits[68] = 0;
	bits[69] = 0;
	bits[70] = 0;
	bits[71] = 0;

	/* resetCrc() inlined, 0x1e9a0: the register starts all ones. */
	for (i = 0; i <= 15; i++)
		crc[i] = 1;

	/* Sixteen payload bits from each group, the leading 0 of each skipped. */
	for (g = 0; g <= 1; g++) {
		int k;

		for (k = 0; k <= 15; k++) {
			int t = bits[V90JD_GROUP1 + 1 + g * V90JD_GROUP + k]
			    + crc[0];
			int tap3 = (crc[4] + t) & 1;
			int tap10 = (crc[11] + t) & 1;

			/*
			 * The shift.  Rolled, this is
			 *
			 *	int i;
			 *	for (i = 0; i <= 14; i++)
			 *		crc[i] = crc[i + 1];
			 *
			 * and that spelling is condition 1 above: it keeps
			 * every element at a variable address and costs the
			 * promotion, 74 of the 149 instructions.
			 */
			crc[0] = crc[1];
			crc[1] = crc[2];
			crc[2] = crc[3];
			crc[3] = crc[4];
			crc[4] = crc[5];
			crc[5] = crc[6];
			crc[6] = crc[7];
			crc[7] = crc[8];
			crc[8] = crc[9];
			crc[9] = crc[10];
			crc[10] = crc[11];
			crc[11] = crc[12];
			crc[12] = crc[13];
			crc[13] = crc[14];
			crc[14] = crc[15];
			crc[3] = tap3;
			crc[10] = tap10;
			crc[15] = t & 1;
		}
	}

	/* Group 3 carries the CRC, low bit first. */
	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP3 + 1 + i] = (unsigned char)crc[i];
}

unsigned char *
V90Jd::getBitVector()
{
	int i, g;

	/* Group 0 is seventeen 1 bits; the later three each open with a 0. */
	for (i = 0; i <= 16; i++)
		bits[i] = 1;
	bits[V90JD_GROUP1] = 0;
	bits[V90JD_GROUP2] = 0;
	bits[V90JD_GROUP3] = 0;
	bits[68] = 0;
	bits[69] = 0;
	bits[70] = 0;
	bits[71] = 0;

	/* The CRC register starts all ones. */
	for (i = 0; i <= 15; i++)
		crc[i] = 1;

	/*
	 * Over groups 1 and 2, sixteen bits each, skipping the leading 0 of
	 * each.  The object walks a pointer that starts at `this + 0x14` and
	 * advances by the 17-byte stride once -- and THAT POINTER IS NOT
	 * WRITTEN HERE ON PURPOSE.  It is what strength reduction makes of the
	 * subscript below; spelling it `*in++` over a local hands GCC 3.4.2 an
	 * INDIRECT_REF it cannot tell apart from the CRC's stores, costs the
	 * promotion of the CRC register out of memory, and lands 13
	 * instructions short of the object.  Finding F7941.
	 */
	for (g = 0; g <= 1; g++) {
		int k;

		for (k = 0; k <= 15; k++) {
			int t = bits[V90JD_GROUP1 + 1 + g * V90JD_GROUP + k]
			    + crc[0];
			int tap3 = (crc[4] + t) & 1;
			int tap10 = (crc[11] + t) & 1;

			/*
			 * The shift.  Rolled, this is
			 *
			 *	int i;
			 *	for (i = 0; i <= 14; i++)
			 *		crc[i] = crc[i + 1];
			 *
			 * and that spelling is condition 1 above: it keeps
			 * every element at a variable address and costs the
			 * promotion, 74 of the 149 instructions.
			 */
			crc[0] = crc[1];
			crc[1] = crc[2];
			crc[2] = crc[3];
			crc[3] = crc[4];
			crc[4] = crc[5];
			crc[5] = crc[6];
			crc[6] = crc[7];
			crc[7] = crc[8];
			crc[8] = crc[9];
			crc[9] = crc[10];
			crc[10] = crc[11];
			crc[11] = crc[12];
			crc[12] = crc[13];
			crc[13] = crc[14];
			crc[14] = crc[15];
			crc[3] = tap3;
			crc[10] = tap10;
			crc[15] = t & 1;
		}
	}

	/* And group 3 is the CRC, low bit first. */
	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP3 + 1 + i] = (unsigned char)crc[i];

	return bits;
}

void
V90Jd::unPackReset()
{
	unpack[1] = 0;
	unpackWord = 0;
	unpack[0] = 0;
}

/*
 * ===========================================================================
 * The unpacker, 0x1eba0, 879 bytes -- AND IT IS THE PRODUCER THE THREE
 * ACCESSORS WERE WRITTEN FOR.  D270 asked whether anything writes the
 * payload-contiguous layout `getRatesMask` and friends read.  This does, and
 * nothing else in the class does:
 *
 *     bits[ 0..15]   `mov %cl,0x2(%ebx,%esi,1)` in the case at 0x1ec51
 *     bits[16..27]   the same store at 0x1ec89
 *     bits[28..31]   the same store at 0x1ecaf
 *     bits[32..47]   the same store at 0x1ecfd -- the CRC as received
 *
 * -- written strictly in arrival order, one byte per received bit, starting
 * at `bits[0]`, and no group marker is ever stored.  So the class has two
 * layouts because it has two directions: the constructor and `getBitVector`
 * build the FRAMED message for the wire, and the unpacker strips the framing
 * off an incoming one and leaves the payload flat for the accessors.  Framed
 * position 18+p is payload p for p in 0..15, 35+(p-16) for p in 16..31 and
 * 52+(p-32) for p in 32..47, which is exactly the run the two sides agree on.
 *
 * THE THREE STATE FIELDS, now that something reads them.  `unpack[0]` is the
 * length of the current run of 1 bits -- the entry sequence is `test %ecx;
 * je` over `movzbl (%ebx); inc %al`, so it is set to `unpack[0] + 1` on a
 * non-zero bit and to 0 on a zero one, as a BYTE, wrapping at 255.
 * `unpack[1]` is how many payload bytes have been stored, and `unpackWord` is
 * the state: `cmp $0x8,%eax; ja <default>` over a nine-entry jump table at
 * .rodata+0x794, so `switch` on an `int` with cases 0 through 8.
 *
 * THE RETURN TYPE IS `int`, measured: every path but one reaches `xor %edx,
 * %edx` before `mov %edx,%eax`, and the case-8 completion at 0x1ec18 jumps
 * past it with `%edx` holding 1.  A return type is not mangled, so this is
 * the only way to know it.
 *
 * The state machine walks the framing the packer writes:
 *
 *     0  hunt          `cmp $0x10,%dl; jbe` -- seventeen 1 bits, unsigned
 *     1  group 1's 0   a 1 here restarts
 *     2  16 payload    `cmp $0x10,%al`
 *     3  group 2's 0   a 1 here restarts
 *     4  12 payload    `cmp $0x1c,%al`   (V92Jd's data unpacker splits this)
 *     5  4 payload     `cmp $0x20,%al`   -- constellation size and lookahead
 *     6  group 3's 0   a 1 here restarts
 *     7  16 payload    `cmp $0x30,%al`, then the CRC is checked
 *     8  4 trailing    `cmp $0x34,%al`, then 1 is returned
 *
 * THE RESTART IS `unPackReset()`'s THREE STORES IN ITS ORDER, byte for byte,
 * at 0x1ec40: `movb $0x0,0x1(%ebx)`, then the word, then `movb $0x0,(%ebx)`.
 * That is consistent with an inlined call to it and is written as one; the
 * run length computed on the way in is discarded, so a stray 1 where a marker
 * belongs does not count toward the next seventeen.
 *
 * THE CRC IS `getBitVector`'s, over ONE run of thirty-two rather than two of
 * sixteen: `cmpl $0x1f,0x3c(%esp)` where the packer's helper stops at 15 and
 * is called twice with a 17-byte stride.  The payload is contiguous here, so
 * the two are the same thirty-two bytes in the same order and the register
 * agrees with what the packer put on the wire.  It is written inline rather
 * than through a shared helper because the object's loop bound is 31,
 * and because a helper taking pointers costs the promotion above anyway.
 *
 * THE COMPARISON IS A SUM OF ABSOLUTE DIFFERENCES, not a bitwise test:
 * `sub %edi,%edx; mov %edx,%eax; sar $0x1f,%eax; xor %eax,%edx; sub %eax,%edx`
 * accumulated over sixteen and tested for zero.  Since the received byte is
 * stored unmasked, a CRC byte of 2 fails here where `(x ^ y) & 1` would pass,
 * and the test drives exactly that.
 * ===========================================================================
 */
int
V90Jd::unPackData(int bit)
{
	unsigned char ones = 0;
	unsigned char n;
	int sum;
	int i, k;

	if (bit)
		ones = (unsigned char)(unpack[0] + 1);

	switch (unpackWord) {
	case 0:
		/* Group 0: seventeen 1 bits, counted as an unsigned byte. */
		unpack[0] = ones;
		if (ones > 16)
			unpackWord = 1;
		break;

	case 1:
		/* Group 1's leading 0. */
		if (bit) {
			unPackReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 2;
		break;

	case 2:
		/* The low sixteen rate bits. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 16)
			unpackWord = 3;
		break;

	case 3:
		/* Group 2's leading 0. */
		if (bit) {
			unPackReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 4;
		break;

	case 4:
		/* The high twelve rate bits. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 28)
			unpackWord = 5;
		break;

	case 5:
		/* The constellation size and the maximum lookahead. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 32)
			unpackWord = 6;
		break;

	case 6:
		/* Group 3's leading 0. */
		if (bit) {
			unPackReset();
			break;
		}
		unpackWord = 7;
		unpack[0] = ones;
		break;

	case 7:
		/* The sixteen CRC bits, then the check. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		n++;
		if (n != 48) {
			unpack[1] = n;
			unpack[0] = ones;
			break;
		}

		for (i = 0; i <= 15; i++)
			crc[i] = 1;

		for (k = 0; k <= 31; k++) {
			int t = bits[k] + crc[0];
			int tap3 = (crc[4] + t) & 1;
			int tap10 = (crc[11] + t) & 1;

			for (i = 0; i <= 14; i++)
				crc[i] = crc[i + 1];
			crc[3] = tap3;
			crc[10] = tap10;
			crc[15] = t & 1;
		}

		sum = 0;
		for (i = 0; i <= 15; i++) {
			int d = crc[i] - bits[32 + i];

			sum += d < 0 ? -d : d;
		}
		if (sum != 0) {
			unPackReset();
			break;
		}

		unpack[1] = 48;
		unpackWord = 8;
		unpack[0] = ones;
		break;

	case 8:
		/* The four trailing bits, whose values are not looked at. */
		n = unpack[1];
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 0x34)
			return 1;
		break;

	default:
		unpack[0] = ones;
		break;
	}

	return 0;
}
