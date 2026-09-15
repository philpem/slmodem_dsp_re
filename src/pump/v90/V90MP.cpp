/*
 * V90MP.cpp -- the V.90 MP message: build it, read it back, check its CRC.
 *
 * Reconstructed from dsplibs.o V90MP.cpp.  TEN of the class's sixteen
 * symbols, in the order they appear below -- the constructor (0x1f410, 40
 * bytes), the destructor (0x1f130, one byte, a bare `ret`), `reset`
 * (0x1f3e0), `getBitVector` (0x1f700), `printNofRecievedMpMpNot` (0x20bc0),
 * `evaluateInfo` (0x1f720), `infoToBits` (0x1f990), `calcCRC` (0x1f170),
 * `evaluateCRC` (0x1f470) and `bitsToInfo` (0x20190).
 * `include/dsplib/V90MP.h` carries the object map and says which member
 * proved which offset.
 *
 * THIS PARAGRAPH USED TO SAY "TWO", and had said it since the file held two.
 * It is findings F6100 and F6103's class exactly -- a count in a comment with
 * no gate behind it -- and it is the first thing a reader of any member here
 * sees, so it is worth the edit every time one lands.
 *
 * THE CONSTRUCTOR AND `reset` ARE THE SAME FORTY BYTES, instruction for
 * instruction: the two symbols at 0x1f410 and 0x1f3e0 disassemble alike down
 * to the register allocation.  Both are ordinary GLOBAL symbols in `.text`
 * rather than in a linkonce section, so `reset` is not an in-class inline the
 * compiler folded into the constructor (GCC 3.4 at -O2 does not inline an
 * ordinary global function).  The original repeated the assignments, which is
 * why they are repeated here rather than written as `reset()`.  Finding F1237.
 *
 * The calling convention is plain cdecl -- `mov 0x4(%esp),%eax` -- not
 * thiscall (finding F215).
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/V90MP.h"

/* Hold the compiler to the map in the header; see V90CP.cpp for why. */
#if __SIZEOF_POINTER__ == 4
#define V90MP_OFF(field, off, tag) \
	typedef char v90mp_off_##tag[ \
	    ((int)__builtin_offsetof(V90MP, field) == (off)) ? 1 : -1]

V90MP_OFF(Type,			0x000, type0);
V90MP_OFF(Rate,			0x001, rate);
V90MP_OFF(Trellis,		0x002, trellis);
V90MP_OFF(NonLin,		0x003, nonlin);
V90MP_OFF(Shaping,		0x004, shaping);
V90MP_OFF(CPack,		0x005, cpack);
V90MP_OFF(rateMask,		0x006, ratemask);
V90MP_OFF(h1Real,		0x008, h1real);
V90MP_OFF(h1Imag,		0x00a, h1imag);
V90MP_OFF(h2Real,		0x00c, h2real);
V90MP_OFF(h2Imag,		0x00e, h2imag);
V90MP_OFF(h3Real,		0x010, h3real);
V90MP_OFF(h3Imag,		0x012, h3imag);
V90MP_OFF(rxState,		0x014, word14);
V90MP_OFF(type,			0x018, type18);
V90MP_OFF(onesRun,		0x019, byte19);
V90MP_OFF(zerosRun,		0x01a, byte1a);
V90MP_OFF(bitIndex,		0x01b, byte1b);
V90MP_OFF(bits,			0x01c, bits);
V90MP_OFF(crc,			0x102, crc);
V90MP_OFF(groupSize,		0x114, word114);
V90MP_OFF(seqLength,		0x118, byte118);
V90MP_OFF(bodyLength,		0x119, byte119);
V90MP_OFF(nofRecievedMp,	0x11c, nofmp);
V90MP_OFF(nofRecievedMpNot,	0x120, nofmpnot);
typedef char v90mp_size[(sizeof(V90MP) == 0x124) ? 1 : -1];
#endif

/*
 * THE ORDER OF THE FIRST FOUR STORES IS DECODED, NOT TRANSCRIBED, and the
 * distinction is the whole of finding F7770: the object emits
 *
 *     movb $0x12,0x1b ; movl $0x0,0x14 ; movb $0x0,0x19 ; movb $0x0,0x1a
 *
 * and writing that order does NOT produce it -- GCC 3.4.2 sinks the `0x1b`
 * store to the end of the run.  All 4! = 24 orders of these four statements
 * were compiled before any cell was read, holding the two four-byte counter
 * stores fixed (they are position-matched on both sides, before and after,
 * which is what licenses holding them), differing bytes of forty:
 *
 *     14 19 1a 1b  12     19 14 1a 1b   5     1a 14 19 1b   4     1b 14 19 1a   0  <--
 *     14 19 1b 1a  11     19 14 1b 1a   4     1a 14 1b 19   5     1b 14 1a 19   2
 *     14 1a 19 1b  11     19 1a 14 1b  12     1a 19 14 1b  12     1b 19 14 1a   8
 *     14 1a 1b 19  12     19 1a 1b 14  14     1a 19 1b 14  14     1b 19 1a 14  12
 *     14 1b 19 1a  10     19 1b 14 1a  11     1a 1b 14 19  12     1b 1a 14 19   9
 *     14 1b 1a 19  12     19 1b 1a 14  15     1a 1b 19 14  15     1b 1a 19 14  12
 *
 * Exactly one cell reaches zero, so the object's emission has a unique
 * preimage inside the family and the author's order is recovered rather than
 * fitted.  Note the near miss at 2 -- had the search stopped at "much
 * closer" it would have taken the wrong cell, which is 7779's warning.
 *
 * The claim is bounded exactly as 7770 requires: within the family of source
 * texts differing from this one only in the order of these four statements.
 */
V90MP::V90MP()
{
	bitIndex = 18;
	rxState = 0;
	onesRun = 0;
	zerosRun = 0;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;
}

/*
 * One byte in the object: `ret`.  The class allocates nothing -- unlike
 * V90CP, whose 173-byte destructor releases six buffers -- so there is
 * nothing for this to do, and the size is the evidence that it does none.
 */
V90MP::~V90MP()
{
}

/*
 * resetCRC -- 0x1f150, 32 bytes: the CRC register to all ones, sixteen BYTE
 * stores (`mov %cl`) where V90Jd's CRC keeps ints.  `jle` against 15, so the
 * counter is `int`.  The identical loop opens `calcCRC`'s callers inline;
 * this is the out-of-line copy.
 */
void
V90MP::resetCRC()
{
	int i;

	for (i = 0; i <= 15; i++)
		crc[i] = 1;
}

/*
 * resetDetector -- 0x1f3c0, 24 bytes, four stores and a `ret`.  The same
 * four `reset` and the constructor repeat inline (the block comment below
 * has the measurement and finding F1237 for why the repetition is the
 * original's and not a call).
 */
void
V90MP::resetDetector()
{
	bitIndex = 18;
	rxState = 0;
	onesRun = 0;
	zerosRun = 0;
}

/*
 * reset -- the constructor's forty bytes again, instruction for instruction.
 *
 * 0x1f3e0 and 0x1f410 differ in nothing but their address: the same six
 * stores in the same order, with the same two scratch registers zeroed ahead
 * of the pair of four-byte ones.  **That is now MEASURED and not read off the
 * listing** -- in the blob `V90MP()`, `V90MP()` (the C2 clone) and `reset` are
 * byte-identical to each other, 0 of 40, and so are ours.  So the store order
 * decoded above the constructor is this function's too, and the one edit
 * closed three symbols.  Finding F1237 is why the assignments are
 * repeated here rather than written as a call to `resetDetector` plus two
 * counters -- `resetDetector` is a separate GLOBAL symbol at 0x1f3c0 and GCC
 * 3.4 at -O2 does not inline one of those, so an original that called it
 * would have left a call behind.
 */
void
V90MP::reset()
{
	bitIndex = 18;
	rxState = 0;
	onesRun = 0;
	zerosRun = 0;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;
}

/*
 * getBitVector -- hand back the vector and its length.
 *
 * Twenty-one bytes and no branch:
 *
 *     1f708:  0f b6 88 18 01 00 00   movzbl 0x118(%eax),%ecx
 *     1f70f:  83 c0 1c               add    $0x1c,%eax
 *     1f712:  89 0a                  mov    %ecx,(%edx)
 *
 * so the length is the ONE BYTE at +0x118 widened without sign, written
 * whole into the caller's `unsigned int`, and the pointer is `this + 0x1c`
 * -- which is what fixes the bit vector's start.  `movzbl` into a register
 * whose whole 32 bits are then stored is the forced-signedness case
 * CLAUDE.md names: it is why +0x118 is `unsigned char` and not `char`.
 */
unsigned char *
V90MP::getBitVector(unsigned int &length)
{
	length = seqLength;
	return bits;
}

/*
 * printNofRecievedMpMpNot -- the two counters, by the debug string's words.
 *
 * The gate is the object's own: `cmpl $0x1,dsplibs_debug_level; ja`, which
 * is `DSPLIB_DEBUG_ON()`.  This one is NOT an `edprintf` -- the call at
 * 0x20bef relocates against `dsplibs_debug_printf` directly -- so unlike
 * every diagnostic in `V90ConnectionEvaluator` it says nothing at all below
 * the gate, and there is no encoder key to move.
 *
 * The argument order is the object's: +0x11c is the first `%d` and +0x120
 * the second, which is what names the two fields.
 */
void
V90MP::evaluateInfo()
{
	unsigned int i;
	unsigned char t = (unsigned char)type;

	Type = (char)t;

	Rate = 0;
	for (i = 0x1b; i > 0x17; i--)
		Rate = (char)((Rate << 1) | (bits[i] & 1));

	/*
	 * TRELLIS ACCUMULATES THROUGH A `char`, AND THE OBJECT SAYS SO IN ONE
	 * INSTRUCTION.  The blob narrows the first bit to eight bits and then
	 * SIGN-EXTENDS it before the shift --
	 *
	 *     and    $0x1,%al          bits[0x1e] & 1, in 8 bits
	 *     movsbl %al,%edx          <-- we emitted nothing here
	 *     add    %edx,%edx         << 1
	 *
	 * -- where a single expression of type `int` gives `and $0x1,%edx;
	 * add %edx,%edx` and no `movsbl` at all.  The extension is FORCED
	 * evidence in CLAUDE.md's sense: it exists only because something in
	 * the source was a signed 8-bit value, and no differential test can
	 * ever see it, because `x & 1` is 0 or 1 under either reading.
	 *
	 * ENUMERATED, 35 cells: seven spellings of this statement crossed with
	 * all five positions of the `rateMask = 0` below it (finding F7819).
	 * Four distinct emissions.  TWENTY cells reach this one -- a char
	 * local, a signed-char local, this two-step accumulate, and the
	 * three-step accumulate with an explicit `Trellis = 0` -- so what is
	 * decoded is a FACT and not an order: the intermediate is narrowed to
	 * a signed `char` before it is shifted.  The four are indistinguishable
	 * to the compiler, so this file cannot say which the author typed.
	 *
	 * TWO CELLS ARE EXCLUDED, and they are the useful half.  The single
	 * expression we had emits 46 differing bytes; an inner `(char)` cast
	 * on it emits 47, because the load happens first and the conversion
	 * after (lever 8's rule, 7803).  And the ROLLED loop -- the shape
	 * `Rate` and the six h-values use -- comes out a different SIZE
	 * entirely, so the author did not write one here.
	 *
	 * Rolled, and measured NOT to be what the object was built from:
	 *     Trellis = 0;
	 *     for (i = 0x1e; i > 0x1c; i--)
	 *             Trellis = (char)((Trellis << 1) | (bits[i] & 1));
	 *
	 * Residual after this: ONE byte, `pop %esi` against our `pop %ebx` in
	 * the epilogue, which `alpha_equal` accepts -- grade 1, a register
	 * permutation, not a source property.
	 */
	Trellis = (char)(bits[0x1e] & 1);
	Trellis = (char)((Trellis << 1) | (bits[0x1d] & 1));

	/*
	 * WHOLE BYTES, not masked bits.  `movzbl 0x3b(%ebx),%eax; mov
	 * %al,0x3(%ebx)` -- there is no `and $0x1` on these three, so a
	 * `bits` element holding 2 arrives in the field as 2.
	 */
	NonLin = (char)bits[0x1f];
	Shaping = (char)bits[0x20];
	CPack = (char)bits[0x21];

	/*
	 * WHERE THIS LINE SITS IS NOT RECOVERABLE, and that is measured rather
	 * than assumed.  The blob emits its `movw $0x0,0x6(%ebx)` in the MIDDLE
	 * of the Trellis computation above, four statements earlier than we
	 * write it, which reads exactly like a statement-order difference.  It
	 * is not: all five source positions -- before `Trellis`, and after each
	 * of `Trellis`, `NonLin`, `Shaping` and `CPack` -- emit the SAME bytes,
	 * for every one of the seven Trellis spellings enumerated above.  The
	 * map is constant, so by lever 1's own rule the difference is not a
	 * store-order difference at all; GCC schedules this store where it
	 * likes.  Do not re-run that domain.  Finding F7819.
	 */
	rateMask = 0;
	for (i = 0x24; i <= 0x31; i++)
		if (bits[i])
			rateMask = (short)(rateMask | (1 << (i - 0x24)));

	h1Real = 0;
	h1Imag = 0;
	h2Real = 0;
	h2Imag = 0;
	h3Real = 0;
	h3Imag = 0;

	if (t == 0)
		return;

	for (i = 0x43; i > 0x33; i--)
		h1Real = (short)((h1Real << 1) | (bits[i] & 1));
	for (i = 0x54; i > 0x44; i--)
		h1Imag = (short)((h1Imag << 1) | (bits[i] & 1));
	for (i = 0x65; i > 0x55; i--)
		h2Real = (short)((h2Real << 1) | (bits[i] & 1));
	for (i = 0x76; i > 0x66; i--)
		h2Imag = (short)((h2Imag << 1) | (bits[i] & 1));
	for (i = 0x87; i > 0x77; i--)
		h3Real = (short)((h3Real << 1) | (bits[i] & 1));
	for (i = 0x98; i > 0x88; i--)
		h3Imag = (short)((h3Imag << 1) | (bits[i] & 1));
}

void
V90MP::printNofRecievedMpMpNot()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90MP: received %d MP, %d MPNot\r\n",
				     nofRecievedMp, nofRecievedMpNot);
}

/*
 * evaluateInfo -- read the thirteen fields at +0x00 back out of `bits`.
 *
 * 482 bytes at 0x1f720, and the exact inverse of `infoToBits` field for field,
 * which is what makes the round trip a test rather than a restatement.
 *
 * TWO SHAPES OF UNPACK, and the object keeps them apart.  The four-bit `Rate`
 * and the six sixteen-bit h-values are accumulated MOST SIGNIFICANT BIT FIRST
 * by counting DOWN -- `bits[0x1b]` ends up weighing eight, `bits[0x43]` weighs
 * 0x8000 -- while the fourteen-bit rate mask is built by counting UP with a
 * variable shift, `1 << (i - 0x24)`, out of a running read-modify-write of the
 * field itself (`movzwl 0x6(%ebx)`, `or`, `mov %cx`).  Both orderings put bit
 * zero at the LOW index, so the two agree; only the code differs.
 *
 * `Type` IS THE GATE.  The six h-values are zeroed unconditionally and then
 * filled only when +0x18 is non-zero (`cmpb $0x0,0x3(%esp)` against the copy
 * saved on entry, `je` to the epilogue), because a type-zero message is five
 * frames long and stops before them.
 *
 * VOID, and measured: the two `ret` paths leave different leftovers in %eax --
 * a shifted mask on one, the last accumulator on the other -- and nothing
 * arranges it on either.
 */
/*
 * calcSequenceLength -- 0x1f910, 116 bytes, between `evaluateInfo` and
 * `infoToBits` in the blob as here.
 *
 * Round `bodyLength + 1` UP to a multiple of the group size and store it in
 * `seqLength` -- except that the exact-multiple arm stores `bodyLength + 1`
 * through a BYTE increment of the saved copy (`incb 0x3(%esp)`), so the two
 * arms agree only below 256.  The division is `div` against `groupSize`:
 * unsigned, and a group size of zero traps exactly as the object does.
 */
void
V90MP::calcSequenceLength()
{
	unsigned char b = bodyLength;
	unsigned int w = (unsigned int)b + 1;
	unsigned int q = w / groupSize;

	if (q * groupSize != w)
		seqLength = (unsigned char)((q + 1) * groupSize);
	else
		seqLength = (unsigned char)(b + 1);
}

/*
 * infoToBits -- lay the thirteen fields out as an MP sequence.
 *
 * 1,948 bytes at 0x1f990.  Seventeen ones, then 17-bit frames of a zero
 * framing bit and sixteen data bits; eleven frames when `Type` is non-zero and
 * five when it is zero.  V90MP.h has the map.
 *
 * THE CRC IS WRITTEN OUT HERE RATHER THAN CALLED.  `resetCRC` (0x1f150) and
 * `calcCRC` (0x1f170) are plain GLOBAL symbols in `.text`, and GCC 3.4 at -O2
 * does not inline one of those, so an original that had called them would have
 * left two calls behind and there are none.  It is finding F1237's argument
 * again -- the same argument that says `reset` repeats the constructor -- and
 * it applies three times over, since `bitsToInfo` carries two more copies.
 *
 * `calcCRC` and `evaluateCRC` are now written, further down this file, and
 * the four copies of the register are the ORIGINAL'S OWN repetition.  Do not
 * turn any of them into a call to the others: that would remove instructions
 * the object has.
 *
 * TWO DEFECTS OF THE ORIGINAL ARE REPRODUCED HERE ON PURPOSE.  Finding F1386.
 *
 *   - The type-zero arm pads from 0x45, which is where it has just put the
 *     CRC, so it destroys it: `movb $0x0,0x61(%ebx)` and a loop from 0x45,
 *     against the type-one arm's correct 0xbb and 0xbc.  +0x118 is at least
 *     0x56 for every non-zero group size, so this happens every time.
 *
 *   - +0x118 is read ONCE into a register before that pad loop, and the loop
 *     can reach far enough to overwrite +0x118 itself (index 0xfc).  `len`
 *     here is that register: writing `i < seqLength` instead would re-read the
 *     field and stop early.
 */
void
V90MP::calcCRC()
{
	unsigned int i, end;
	unsigned char a;

	end = type ? 0xaa : 0x44;

	for (i = 0x12; i < end; ) {
		if (i % 17 == 0)
			i++;
		a = (unsigned char)(crc[0] + bits[i]);
		i++;

		crc[0] = crc[1];
		crc[1] = crc[2];
		crc[2] = crc[3];
		crc[3] = (unsigned char)((crc[4] + a) & 1);
		crc[4] = crc[5];
		crc[5] = crc[6];
		crc[6] = crc[7];
		crc[7] = crc[8];
		crc[8] = crc[9];
		crc[9] = crc[10];
		crc[10] = (unsigned char)((crc[11] + a) & 1);
		crc[11] = crc[12];
		crc[12] = crc[13];
		crc[13] = crc[14];
		crc[14] = crc[15];
		crc[15] = (unsigned char)(a & 1);
	}
}

void
V90MP::infoToBits()
{
	unsigned int i;
	int k;
	int v;
	unsigned char t = (unsigned char)Type;
	unsigned char x, n, end, len;

	for (i = 0; i <= 0x10; i++)
		bits[i] = 1;
	bits[0x11] = 0;
	bits[0x12] = t;
	for (i = 0x13; i <= 0x17; i++)
		bits[i] = 0;

	v = Rate;
	for (i = 0; i <= 3; i++) {
		bits[0x18 + i] = (unsigned char)(v & 1);
		v >>= 1;
	}

	bits[0x1c] = 0;

	/*
	 * A SWITCH, not two shifts: the object tests 1, then `jle` to a test
	 * against 0, then 2, then 3, and writes NOTHING outside 0..3 -- so a
	 * `Trellis` of 4 leaves the two bits holding whatever they held.  The
	 * `jle` is also what makes the field signed.
	 */
	switch (Trellis) {
	case 0:
		bits[0x1d] = 0;
		bits[0x1e] = 0;
		break;
	case 1:
		bits[0x1d] = 1;
		bits[0x1e] = 0;
		break;
	case 2:
		bits[0x1d] = 0;
		bits[0x1e] = 1;
		break;
	case 3:
		bits[0x1d] = 1;
		bits[0x1e] = 1;
		break;
	}

	bits[0x1f] = (unsigned char)NonLin;
	bits[0x20] = (unsigned char)Shaping;
	bits[0x21] = (unsigned char)CPack;
	bits[0x22] = 0;
	bits[0x23] = 0;
	for (i = 0x24; i <= 0x31; i++)
		bits[i] = (unsigned char)(((rateMask >> (i - 0x24)) & 1) != 0);
	bits[0x32] = 0;
	bits[0x33] = 0;

	type = (char)t;
	n = t ? 0xbb : 0x55;
	bodyLength = n;

	/*
	 * calcSequenceLength's body, repeated for the reason above: round
	 * +0x119 + 1 up to a multiple of the group size.  The divide is
	 * `div`, unsigned, and it faults on a group size of zero exactly as
	 * the object does.
	 */
	{
		unsigned int want = (unsigned int)n + 1;
		unsigned int q = want / groupSize;

		if (q * groupSize == want)
			seqLength = (unsigned char)want;
		else
			seqLength = (unsigned char)((q + 1) * groupSize);
	}

	if (t != 0) {
		v = h1Real;
		for (i = 0x34; i <= 0x43; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x44] = 0;
		v = h1Imag;
		for (i = 0x45; i <= 0x54; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x55] = 0;
		v = h2Real;
		for (i = 0x56; i <= 0x65; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x66] = 0;
		v = h2Imag;
		for (i = 0x67; i <= 0x76; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x77] = 0;
		v = h3Real;
		for (i = 0x78; i <= 0x87; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x88] = 0;
		v = h3Imag;
		for (i = 0x89; i <= 0x98; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x99] = 0;
		for (i = 0x9a; i <= 0xa9; i++)
			bits[i] = 0;
		bits[0xaa] = 0;
	} else {
		for (i = 0x34; i <= 0x43; i++)
			bits[i] = 0;
		bits[0x44] = 0;
	}

	/*
	 * The CRC covers the message but not its framing bits, and it stops at
	 * the last frame before the CRC's own -- 0xaa for the long message,
	 * 0x44 for the short one.  Both are `type ? 0xaa : 0x44` in the
	 * object, taken from the type flag and not from +0x119.
	 */
	end = t ? 0xaa : 0x44;

	for (k = 0; k <= 15; k++)
		crc[k] = 1;

	for (i = 0x12; i < end; i++) {
		if (i % 17 == 0)
			i++;
		x = (unsigned char)(crc[0] + bits[i]);
		for (k = 0; k < 15; k++)
			crc[k] = crc[k + 1];
		crc[3] = (unsigned char)((crc[3] + x) & 1);
		crc[10] = (unsigned char)((crc[10] + x) & 1);
		crc[15] = (unsigned char)(x & 1);
	}

	for (k = 0; k <= 15; k++)
		bits[end + 1 + k] = crc[k];

	len = seqLength;
	if (t != 0) {
		bits[0xbb] = 0;
		for (i = 0xbc; i < len; i++)
			bits[i] = 0;
	} else {
		bits[0x45] = 0;
		for (i = 0x45; i < len; i++)
			bits[i] = 0;
	}
}

/*
 * calcCRC -- 0x1f170, 583 bytes.  The WRITE side of the CRC register: pass
 * the information bits of the sequence now in `bits` through the CCITT shift
 * register, leaving the sixteen result bits in `crc`.
 *
 * THE REGISTER IS THE SAME ONE AS `V90CP::calcCRC`'s, tap for tap -- the
 * feedback bit `crc[0] ^ bits[i]` enters at 15 and is XORed into 3 and 10,
 * and the frame skip is the same `if (i % 17 == 0) i++` compiled as
 * `mul $0xf0f0f0f1` / `shr $4` for the divide and `cmp $1` / `adc $0` for
 * the branchless increment.  Unsigned bounds throughout: `jae` on the guard
 * at 0x1f194 and `jb` on the back edge at 0x1f30e.  Neither member seeds the
 * register (there is no store of 1 anywhere in either) and neither returns
 * anything.
 *
 * WHERE IT GENUINELY DIFFERS FROM THE V.90 CP TWIN IS THE EXTENT, and this
 * is the whole of the difference.  `V90CP::calcCRC` computes `end` as
 * `word_3bb0 - 0x11` from the four-byte sequence length; this one takes it
 * from the ONE-BYTE type flag at +0x18 and a pair of constants:
 *
 *     1f180:  movzbl 0x18(%edi),%edx
 *     1f184:  cmp    $0x1,%dl
 *     1f187:  sbb    %eax,%eax           -1 when type == 0, else 0
 *     1f189:  and    $0xffffff9a,%eax    -0x66 when type == 0, else 0
 *     1f18c:  lea    0xaa(%eax),%ebp     0x44 when type == 0, else 0xaa
 *
 * so `calcCRC` never reads +0x118 or +0x119 at all.  It is the same
 * `end = type ? 0xaa : 0x44` that `infoToBits` and `bitsToInfo` compute, and
 * IT IS NOT ALGEBRAICALLY THE CP FORM: 0xbb - 0x11 is 0xaa and 0x55 - 0x11 is
 * 0x44, so the two agree on every consistent object and part the moment
 * +0x119 and +0x18 disagree.  t_v90mp.cpp drives exactly those pairs.
 *
 * The object promotes the whole register into the sixteen bytes of its stack
 * frame for the duration of the loop and writes it back at the end.  That is
 * register promotion the compiler is free to do and we do not encode; our
 * source touches `crc[]` directly, exactly as the CP twin's does.
 *
 * The guard at 0x1f194 is DEAD CODE -- `end` is 0x44 or 0xaa and `i` starts
 * at 0x12 -- and is just the loop guard GCC emits for a `for` whose bound it
 * cannot fold.
 *
 * THE FEEDBACK BIT IS NOT MASKED WHERE IT IS FORMED, and that is measurable
 * rather than a preference.  `add 0x1c(%esi,%edi,1),%al` at 0x1f277 is a
 * plain byte add, and there is no `and` on %al before either of its uses --
 * the mask is at each store instead (`and $0x1,%bl` at 0x1f2b0, `and $0x1,%cl`
 * at 0x1f2e9, `and $0x1,%al` at 0x1f2f5).  Writing `a = (crc[0] + bits[i]) &
 * 1` puts an extra `and` in the loop under the period compiler, so this is
 * the object's form; it is also the form the three copies elsewhere in this
 * file already use.  Mod-2 arithmetic makes the two identical in behaviour,
 * so NO differential test can tell them apart and the codegen tier is the
 * only thing that can.
 *
 * WHAT IS LEFT is 595 bytes against the object's 583, and 666 against 651 for
 * `evaluateCRC`, on GCC 3.4.2 at this tree's flags.  The residue is three
 * things, all in CLAUDE.md's "free" column: a seven-byte `lea` NOP aligning
 * the loop head, `lea 0x1(%ecx),%esi` where the object reuses %esi with
 * `inc`, and one `movzbl` widening the feedback byte whose upper half is then
 * discarded (finding F614).  Nothing forced is outstanding.
 *
 * CONFORMANT WITH 10.1.2.3.2/V.34, which is what 8.6.3/V.90 cites for MP.
 * See the note above `evaluateCRC` for the derivation and for the extent
 * clause, which is the part this member decides.
 */
/*
 * evaluateCRC -- 0x1f470, 651 bytes.  The read side: seed the register,
 * recompute the CRC over the information bits of a RECEIVED sequence, and
 * compare it against the sixteen bits the peer sent.
 *
 * IT RETURNS A VALUE.  0x1f6ec is `xor %eax,%eax` / `test %bl,%bl` /
 * `sete %al`; see V90MP.h.
 *
 * THE TWO EXTENTS COME FROM TWO DIFFERENT FIELDS, and that is the sharpest
 * difference from `V90CP::evaluateCRC`, which takes both from `word_3bb0`:
 *
 *   - the information bits are bounded by `type ? 0xaa : 0x44`, read from
 *     +0x18 by the same five instructions `calcCRC` uses (0x1f48f..0x1f4a0);
 *   - the peer's sixteen CRC bits are found from +0x119, through the single
 *     displacement `0xc(%edi,%ecx,1)` with %edi holding `this + bodyLength`
 *     (0x1f6cb, 0x1f6d8).  `bits` is at +0x1c, so +0x119 + 0xc + k is
 *     `bits[bodyLength - 0x10 + k]` -- the same expression `bitsToInfo`'s two
 *     inlined copies use with its own `n`.
 *
 * They agree for every value `bitsToInfo` writes (0xbb and 0x55, whose -0x11
 * and -0x10 land on 0xaa/0xab and 0x44/0x45), and only for those.
 *
 * THAT EXPRESSION REACHES OUTSIDE `bits[]` for values of +0x119 the class
 * never writes: below 0x10 it runs back into the decoded message at +0x0c,
 * and above 0xe6 it runs forward into `crc` and the fields past it.  It
 * cannot leave the object -- the widest address the byte can name is
 * `this + 0x11a`, and `sizeof(V90MP)` is 0x124 -- so this is not D923's
 * family (an unbounded store running off an allocation) and it is not
 * recorded in docs/deviations.md.  Finding F7411.
 *
 * THE SEED IS THE SAME SIXTEEN ONES `resetCRC` (0x1f150) writes, inlined
 * rather than called for finding F1237's reason, and it runs BEFORE the guard
 * so it happens whatever the extent.  It is a SIGNED bound -- `cmp $0xf` /
 * `jle` at 0x1f48a -- where the comparison loop below is UNSIGNED
 * (`cmp $0xf` / `jbe` at 0x1f6e7), which is why the two counters here have
 * different types.
 *
 * THE COMPARISON IS A BYTE.  `cltd` / `xor %edx,%eax` / `sub %edx,%eax` is
 * the object's inlined `abs` and `add %al,%bl` accumulates sixteen of them
 * into one byte, so sixteen differences summing to a multiple of 256 read as
 * a match.  As in `bitsToInfo`, the truncation is the DECLARATION's job and
 * `sum` is accumulated with `+=`: written `sum = (unsigned char)(sum + ...)`
 * the declared width would stop mattering and the mutation that tests this
 * claim would survive.
 *
 * CONFORMANCE WITH 10.1.2.3.2/V.34.  8.6.3/V.90 says only "The CRC generator
 * used is described in 10.1.2.3.2/V.34", and that clause says: load the
 * register with all ones; shift in the binary sequence; output the register
 * starting with bit 0, bit 0 being the LSB; and pass "all of the information
 * bits in a sequence, except the frame sync bits, the start bits, and the
 * fill bits".  Figure 14/V.34 draws sixteen stages numbered 15..0 with the
 * information bits entering at the bit-0 end and adders between stages 11/10
 * and 4/3 -- which is this register exactly, and is the reflected form of
 * x^16 + x^12 + x^5 + 1 (0x1021 reversed is 0x8408, bits 15, 10 and 3).
 *
 * The extent clause checks out against Table 16/V.90, which is where the
 * object's two constants come from.  Frame sync is bits 0:16 and every start
 * bit is a multiple of 17 (17, 34, ... 170), which is what the `i % 17` skip
 * steps over; the CRC itself is 171:186 for Type 1 and 69:84 for Type 0, so
 * the information bits run 18..169 and 18..67 and the two bounds are 0xaa
 * and 0x44.  Nine sixteen-bit groups for Type 1, three for Type 0.  The
 * fill bits are past the CRC and are never reached.  Neither the object nor
 * the recommendation is wrong here, so there is nothing to deviate.
 * t_v90mp.cpp's `run_mp_crc_spec` is the test that says so without asking
 * the blob.
 */
int
V90MP::evaluateCRC()
{
	unsigned int i, end;
	int c;
	unsigned char a;
	unsigned char sum, n;

	for (c = 0; c <= 0xf; c++)
		crc[c] = 1;

	end = type ? 0xaa : 0x44;

	for (i = 0x12; i < end; ) {
		if (i % 17 == 0)
			i++;
		a = (unsigned char)(crc[0] + bits[i]);
		i++;

		crc[0] = crc[1];
		crc[1] = crc[2];
		crc[2] = crc[3];
		crc[3] = (unsigned char)((crc[4] + a) & 1);
		crc[4] = crc[5];
		crc[5] = crc[6];
		crc[6] = crc[7];
		crc[7] = crc[8];
		crc[8] = crc[9];
		crc[9] = crc[10];
		crc[10] = (unsigned char)((crc[11] + a) & 1);
		crc[11] = crc[12];
		crc[12] = crc[13];
		crc[13] = crc[14];
		crc[14] = crc[15];
		crc[15] = (unsigned char)(a & 1);
	}

	n = bodyLength;
	sum = 0;
	for (i = 0; i <= 0xf; i++) {
		int d = (int)crc[i] - (int)bits[n - 0x10 + i];

		sum += (unsigned char)(d < 0 ? -d : d);
	}

	return sum == 0;
}

/*
 * bitsToInfo -- take one received bit, and say what it completed.
 *
 * 2,597 bytes at 0x20190, and the only member of the class that returns
 * anything: 0 for nothing, 1 for an MP, 2 for an MPnot, 3 for Ed.  %edi is
 * zeroed at entry and moved to %eax at both `ret`s.
 *
 * THE RUN COUNTERS COME FIRST and are independent of the state: every bit
 * lengthens one run and clears the other, and a run of 2 * the group size of
 * zeros arriving while the bit index is still at its initial 18 is Ed -- the
 * far end has stopped transmitting.  That answer does not stop the state
 * machine, which runs on and can overwrite it with 1 or 2.
 *
 * THE CRC IS CHECKED TWICE.  If the sixteen received CRC bits do not match the
 * sixteen computed ones, and the message is the long one (+0x119 above 0x6f),
 * the object INVERTS bits[0x70] and tries again, reporting "modified good CRC"
 * if that rescued it.  It is a repair of one known-bad bit position, and it is
 * destructive: the flipped bit stays flipped whether or not it helped.
 *
 * THE COMPARISON IS A BYTE.  `add %al,0x4e(%esp)` accumulates sixteen absolute
 * differences into one byte and `cmpb $0x0` tests it, so sixteen differences
 * summing to a multiple of 256 read as a match.  An `int` sum would not.
 * The truncation is therefore the DECLARATION's job and `sum` is accumulated
 * with `+=`; written `sum = (unsigned char)(sum + ...)` the cast would do the
 * truncating instead, the declared width would stop mattering, and the
 * mutation that tests this claim would survive while looking exactly like a
 * missing test.  It did, until the suite was pointed at it.
 *
 * THE DIAGNOSTICS ARE NOT AT THE SAME LEVEL IN THE TWO ARMS.  Both announce
 * the message at level 2, but the MP arm's four follow-ups need level 3
 * (`cmpl $0x2`) and the MPnot arm's need only level 2 (`cmpl $0x1`).  That is
 * finding F150's trap exactly, and it is why the test sweeps 0..3.
 *
 * `PrintBase2` (0x20130) is a plain global symbol, so as with the CRC the two
 * copies of its body here are the original's own repetition, not an inlining.
 */

/*
 * PrintBase2 -- 0x20130, 81 bytes, immediately before `bitsToInfo` in the
 * blob as here.  Render `value` in binary into `out`, NUL-terminated.
 *
 * With `nofBits` non-zero the mask starts at bit `nofBits - 1` and every
 * position prints, leading zeros included -- which is the form the two
 * inline copies in `bitsToInfo` specialise (nofBits = 14).  With it zero the
 * mask starts at bit 31 and `started` suppresses the leading zeros, so the
 * output is the minimal representation -- and an all-zero value prints
 * NOTHING but the terminator, which is the object's behaviour and not an
 * edge this file guards.
 */
void
V90MP::PrintBase2(char *out, unsigned long value, unsigned short nofBits)
{
	unsigned long mask = 0x80000000ul;
	int started;

	if (nofBits != 0)
		mask = 1ul << (nofBits - 1);
	started = (nofBits != 0);

	while (mask != 0) {
		if ((value & mask) != 0) {
			*out++ = '1';
			started = 1;
		} else if (started) {
			*out++ = '0';
		}
		mask >>= 1;
	}
	*out = '\0';
}

int
V90MP::bitsToInfo(int bit)
{
	unsigned int i;
	int k;
	int rc = 0;
	unsigned char x, sum, n, next, end;
	char str[76];

	if (bit != 0) {
		onesRun++;
		zerosRun = 0;
	} else {
		zerosRun++;
		onesRun = 0;
	}

	if (zerosRun == 2 * groupSize && bitIndex == 18) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90MP: Ed detected\r\n");
		rc = 3;
	}

	switch (rxState) {
	case 0:
		/* Seventeen ones is the preamble; sixteen are not enough. */
		if (onesRun > 0x10)
			rxState = 1;
		break;

	case 1:
		/* The framing zero, or start again. */
		if (bit == 0) {
			rxState = 2;
		} else {
			bitIndex = 18;
			rxState = 0;
			onesRun = 0;
			zerosRun = 0;
		}
		break;

	case 2:
		/*
		 * The type bit.  It is stored as a byte -- `mov %cl` -- so a
		 * `bit` of 0x100 counts as a one for the run counters above
		 * and as a type of zero here.
		 */
		type = (char)bit;
		bits[bitIndex] = (unsigned char)bit;
		bitIndex++;
		n = type ? 0xbb : 0x55;
		bodyLength = n;
		{
			unsigned int want = (unsigned int)n + 1;
			unsigned int q = want / groupSize;

			if (q * groupSize == want)
				seqLength = (unsigned char)want;
			else
				seqLength = (unsigned char)((q + 1) * groupSize);
		}
		rxState = 3;
		break;

	case 3:
		bits[bitIndex] = (unsigned char)bit;
		next = (unsigned char)(bitIndex + 1);
		n = bodyLength;
		if (next != n) {
			bitIndex = next;
			break;
		}

		end = type ? 0xaa : 0x44;

		for (k = 0; k <= 15; k++)
			crc[k] = 1;
		for (i = 0x12; i < end; i++) {
			if (i % 17 == 0)
				i++;
			x = (unsigned char)(crc[0] + bits[i]);
			for (k = 0; k < 15; k++)
				crc[k] = crc[k + 1];
			crc[3] = (unsigned char)((crc[3] + x) & 1);
			crc[10] = (unsigned char)((crc[10] + x) & 1);
			crc[15] = (unsigned char)(x & 1);
		}

		sum = 0;
		for (k = 0; k <= 15; k++) {
			int d = (int)crc[k] - (int)bits[n - 0x10 + k];

			sum += (unsigned char)(d < 0 ? -d : d);
		}

		if (sum == 0) {
			bitIndex = next;
			rxState = 4;
			break;
		}

		/* One known-bad bit, inverted in place, and try again. */
		if (n > 0x6f)
			bits[0x70] = (unsigned char)(bits[0x70] == 0);

		for (k = 0; k <= 15; k++)
			crc[k] = 1;
		for (i = 0x12; i < end; i++) {
			if (i % 17 == 0)
				i++;
			x = (unsigned char)(crc[0] + bits[i]);
			for (k = 0; k < 15; k++)
				crc[k] = crc[k + 1];
			crc[3] = (unsigned char)((crc[3] + x) & 1);
			crc[10] = (unsigned char)((crc[10] + x) & 1);
			crc[15] = (unsigned char)(x & 1);
		}

		sum = 0;
		for (k = 0; k <= 15; k++) {
			int d = (int)crc[k] - (int)bits[n - 0x10 + k];

			sum += (unsigned char)(d < 0 ? -d : d);
		}

		if (sum == 0) {
			bitIndex = next;
			rxState = 4;
			if (DSPLIB_DEBUG_VERBOSE())
				dsplibs_debug_printf("V90MP: recieved MP with " "modified good CRC\r\n");
		} else {
			bitIndex = 18;
			onesRun = 0;
			zerosRun = 0;
			rxState = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90MP: recieved MP with " "bad CRC\r\n");
		}
		break;

	case 4:
		/* The padding, which is counted and not stored. */
		bitIndex++;
		if (bitIndex != seqLength)
			break;

		evaluateInfo();

		bitIndex = 18;
		rxState = 0;
		onesRun = 0;
		zerosRun = 0;

		if (CPack == 0) {
			nofRecievedMp++;
			rc = 1;
			if (nofRecievedMp <= 2) {
				char *p = str;
				unsigned int mask;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: MP detect"
					    "ed. Type%d,Rate%d,Trellis%d,NonLi" "n%d,Shaping%d,CPack%d\r\n",
					    Type, Rate * 2400, Trellis, NonLin,
					    Shaping, CPack);

				for (mask = 0x2000; mask != 0; mask >>= 1)
					*p++ = (rateMask & mask) ? '1' : '0';
				*p = 0;

				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: Rate Mask" " - %s\r\n", str);
				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: h1 real =" " %d, imag = %d\r" "\n", h1Real,
							     h1Imag);
				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: h2 real =" " %d, imag = %d\r" "\n", h2Real,
							     h2Imag);
				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: h3 real =" " %d, imag = %d\r" "\n", h3Real,
							     h3Imag);
			}
		} else {
			nofRecievedMpNot++;
			rc = 2;
			if (nofRecievedMpNot <= 2) {
				char *q = str;
				unsigned int mask;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: MPnot det"
					    "ected. Type%d,Rate%d,Trellis%d,No" "nLin%d,Shaping%d,CPack%d\r\n",
					    Type, Rate * 2400, Trellis, NonLin,
					    Shaping, CPack);

				for (mask = 0x2000; mask != 0; mask >>= 1)
					*q++ = (rateMask & mask) ? '1' : '0';
				*q = 0;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: Rate Mask" " - %s\r\n", str);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: h1 real =" " %d, imag = %d\r" "\n", h1Real,
							     h1Imag);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: h2 real =" " %d, imag = %d\r" "\n", h2Real,
							     h2Imag);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: h3 real =" " %d, imag = %d\r" "\n", h3Real,
							     h3Imag);
			}
		}
		break;
	}

	return rc;
}
