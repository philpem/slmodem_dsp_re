/*
 * V90CPpck.cpp -- the analogue modem's own CP builder.
 *
 * Reconstructed from dsplibs.o:
 *
 *   0x3be30   283  float2Bits(float, short *, int)
 *   0x3bf50  2641  V90CPPacker(V90MappingParams *, tagV90AdditionalCPinfo *,
 *                              short *, int)
 *   .data+0xb00   64  fltTable2
 *   .data+0xb40   28  fltTable1
 *
 * THE FILE NAME IS THE AUTHOR'S and the attribution is two orderings agreeing
 * by exclusion rather than one shared bracket; `include/dsplib/V90CPpck.h`
 * sets that out, together with the two-overload trap on `float2Bits`.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS FUNCTION IS, AND WHY IT WAS HARD TO FIND.
 *
 * V.90 clause 9.4.2.3 obliges the ANALOGUE modem to send a CP sequence
 * describing the constellations it wants the digital modem to use.  Finding
 * F7000 mapped every caller of `V90CP`, `V90MP` and `V92CP` and could not find
 * an encoder for it: `V90CP::infoToBits` builds the DIGITAL side's message
 * out of the class's own fields, and nothing in the class assembles one from
 * a `V90MappingParams`.
 *
 * It is this FREE FUNCTION, outside the class, called by
 * `VPcmFloModem::v90RunDemodulator` (unwritten).  A caller map over the class
 * could not see it because it is not a member of anything.  Finding F7001.
 *
 * Its V.92 twin is `setV92CPpckFromParamsInfo` in `V90MappingParamsInt.cpp`,
 * which takes the same two sources and fills a `V92CP` object; this one
 * writes the BITS directly and never touches a `V90CP`.  That is the whole
 * structural difference between the two sides.
 *
 * ---------------------------------------------------------------------------
 * THE LAYOUT IS SEVENTEEN-BIT FRAMES, AND THAT IS MEASURED RATHER THAN
 * ASSUMED.
 *
 * Every store of a literal zero this function makes, in index order:
 *
 *     17  34  51  68  85  102  119  136        then 136 + 136*k + 17*j
 *     for k < groups and j <= 7, then the same again for the codec block,
 *     then `pos`, then `pos + 17 .. pos + 19`
 *
 * -- and every one of those is a multiple of seventeen.  Block 0 is
 * `bits[0..16]`, the seventeen ones that open the sequence; every later block
 * is one zero followed by sixteen information bits.  Nothing is written
 * twice, no index between 0 and `pos + 19` is left unwritten, and the CRC
 * loop's own extent (`17m + 1 .. 17m + 16`, m from 1) closes on the same
 * grid.  So the frame size is confirmed by internal consistency and not only
 * by reading each store.
 *
 * The information blocks, 1 through 7, are fixed:
 *
 *   block 1   bits[18]      0
 *             bits[19]      `info->word_04`, TRUNCATED TO 16 BITS, not a
 *                           boolean -- `mov 0x4(%ecx),%eax; mov %ax,0x26(%edx)`
 *             bits[20..24]  the data bit rate, five bits, LSB first
 *             bits[25..29]  0
 *             bits[30]      `info->word_10`, truncated the same way
 *             bits[31..32]  `params->shaperSR`, two bits, LSB first
 *             bits[33]      `info->word_00`, truncated
 *             bits[34]      0
 *   block 2   bits[35]      `info->word_0c`, truncated
 *             bits[36..48]  `info->short_14`, THIRTEEN bits, LSB first
 *             bits[49..50]  `params->shaperId`, two bits
 *             bits[51]      0
 *   blocks 3  bits[52..67]  `info->float_08` as Q3.13
 *   and 4     bits[68]      0
 *             bits[69..76]  `params->shaperA1` as Q1.6 + sign
 *             bits[77..84]  `params->shaperA2` as Q1.6 + sign
 *             bits[85]      0
 *   block 5   bits[86..93]  `params->shaperB1` as Q1.6 + sign
 *             bits[94..101] `params->shaperB2` as Q1.6 + sign
 *             bits[102]     0
 *   block 6   bits[103..118] the group number of constellations 0..3, four
 *                            bits each, LSB first
 *             bits[119]      0
 *   block 7   bits[120..127] the group number of constellations 4 and 5
 *             bits[128]      `params->word_61c == 1` -- an EQUALITY, `sete`,
 *                            and the gate on the whole codec-mask half
 *             bits[129..135] 0
 *
 * then, for each of the `groups` distinct constellations, eight blocks of
 * sixteen bits each carrying `getConstellationMask`'s eight words; then, IF
 * `bits[128]`, the same again from `getCodecConstellationMask`; then the CRC
 * block and three trailing zeros.
 *
 * ITS RELATION TO TABLE 14/V.90 IS INTERPRETATION AND IS FLAGGED AS SUCH.
 * What is measured is the grid above and the field ORIGINS; the object states
 * no field names.  Read against Table 14 the seventeen-bit frame, the leading
 * all-ones block, the terminating CRC-16 and the per-constellation bitmaps
 * line up, but "bits[19] is CP's such-and-such field" is not something this
 * object can be made to say, so no member here carries a spec name.
 *
 * ---------------------------------------------------------------------------
 * THE CRC IS THE SAME CCITT REGISTER `V90CP::calcCRC` RUNS, AND ITS STORAGE
 * IS NOT.
 *
 * Taps out of positions 4 and 11 into 3 and 10, feedback into 15 --
 * x^16 + x^12 + x^5 + 1, identical to `V90CP::calcCRC` and `V90CP::
 * evaluateCRC`.  What differs is the WIDTH: `V90CP`'s register is
 * `unsigned char crc[16]`, and this one's is `int crc[16]` -- `movl $0x1`
 * with a four-byte stride at 0x3c40b, and a 32-bit load feeding a 16-bit
 * store at 0x3c81a.  Do not carry the class's spelling across.
 *
 * The extent is blocks 1 through `groups`-many-blocks-minus-one; the last
 * block is where the CRC itself goes.  The object walks it as two running
 * bounds 17 apart rather than as a block index, which is strength reduction
 * and not a different loop.
 *
 * ---------------------------------------------------------------------------
 * THE MASK BUFFER IS SIZED FOR WHAT THE CALLEE REACHES, NOT FOR THE OBJECT'S
 * STACK SLOT.  See docs/deviations.md D791; it is behaviourally inert and the
 * derivation is there rather than repeated here.
 */

#include <math.h>

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/V90CPpck.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/tagV90AdditionalCPinfo.h"

/*
 * ===========================================================================
 * fltTable2 (.data+0xb00, 64 bytes) and fltTable1 (.data+0xb40, 28 bytes)
 *
 * TRANSCRIBED AND NOT GENERATED, for exactly the reason `V92CP.cpp` gives for
 * its own pair: read as a geometric sequence `fltTable2` is 2^2 down to
 * 2^-13, except that 2^-8 IS ABSENT and 2^-9 appears twice.
 *
 *     0b20  0000803c 0000003c 0000003b 0000003b
 *           2^-6     2^-7     2^-9     2^-9
 *
 * 0x3b800000 (2^-8) would sit between the third and fourth and does not.  The
 * expansion is greedy, so a magnitude in [2^-8, 2^-7) sets a different entry
 * under a generated table than under this one, and the difference reaches the
 * message.
 *
 * These are NOT `V92CP.cpp`'s `fltTable_2` and `fltTable_1`: different names,
 * different addresses (+0x69e0 and +0x6a20), separate symbols measured as
 * such in finding F826.  Byte-identical contents, which is the whole hazard.
 * ===========================================================================
 */
float fltTable2[16] = {
	4.0f,		2.0f,		1.0f,		0.5f,
	0.25f,		0.125f,		0.0625f,	0.03125f,
	0.015625f,	0.0078125f,	0.001953125f,	0.001953125f,
	0.0009765625f,	0.00048828125f,	0.000244140625f, 0.0001220703125f
};

float fltTable1[7] = {
	1.0f,		0.5f,		0.25f,		0.125f,
	0.0625f,	0.03125f,	0.015625f
};

/*
 * ===========================================================================
 * float2Bits (.text+0x3be30, 283 bytes)
 * ===========================================================================
 *
 * THE MODE IS TESTED AS `== 0` THEN `== 1`, and the object leaves by the
 * front door on anything else: `test %eax,%eax; je` then `dec %eax; je`, and
 * the fall-through is the epilogue.  So a third mode writes nothing at all,
 * which is a store the fixture has to be able to see -- an all-zero seed
 * cannot.
 *
 * THE RANGE TESTS ARE ORDERED COMPARES AND THE WARNING IS THE ONLY THING THEY
 * DO.  `fcoms` against 8.0f and 0.0f in mode 0, against 1.0f and -1.0f in
 * mode 1; each arm reaches its own `cmpl $0x1,dsplibs_debug_level`, and both
 * fall into the same expansion afterwards.  Nothing is clamped.  The strings
 * are the author's: "Q3.13 format violation!!" and "Q1.6 format violation!!",
 * and they are what NAMES the two modes.
 *
 * The comparison order in the loop is `table[i] > x`, not `x >= table[i]`:
 * `flds table[i]` puts the weight in %st(0) and `fcom %st(1)` compares it
 * against the remainder, so the table entry is the LEFT operand.  Finding
 * F1990's flag makes that an ordered `fcom`; the two spellings differ only on
 * a NaN, which is out of the grid on purpose (the same exclusion
 * `test/mutations/v92info.json` states for the twin).
 *
 * The subtraction is `de e9`, which objdump prints as `fsubrp` and which IS
 * `FSUBP`: %st(1) = %st(1) - %st(0), so the REMAINDER loses the weight.
 * Finding F245.
 *
 * The sign in mode 1 is `setb` on the compare against 0.0f, taken BEFORE the
 * `fabs`, and it is stored at bits[7] -- past the seven magnitude entries,
 * not before them.  A magnitude of -0.0f therefore sets the sign entry while
 * every magnitude entry stays zero.
 */
void
float2Bits(float f, short *bits, int mode)
{
	float x;
	int i;

	if (mode == 0) {
		if (f > 8.0f || f < 0.0f) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"Q3.13 format violation!!\r\n");
		}

		x = fabsf(f);

		for (i = 0; i <= 15; i++) {
			if (fltTable2[i] > x) {
				bits[15 - i] = 0;
			} else {
				bits[15 - i] = 1;
				x -= fltTable2[i];
			}
		}
	} else if (mode == 1) {
		if (f > 1.0f || f < -1.0f) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"Q1.6 format violation!!\r\n");
		}

		bits[7] = (short)(f < 0.0f);
		x = fabsf(f);

		for (i = 0; i <= 6; i++) {
			if (fltTable1[i] > x) {
				bits[6 - i] = 0;
			} else {
				bits[6 - i] = 1;
				x -= fltTable1[i];
			}
		}
	}
}

/*
 * ===========================================================================
 * V90CPPacker (.text+0x3bf50, 2641 bytes)
 * ===========================================================================
 *
 * TWENTY-TWO RELOCATIONS AND THEY ARE THE WHOLE CLOSURE: five calls to
 * `float2Bits(float, short *, int)` -- the `short *` overload, `Ps` and not
 * `Ph`, which is how the overload is settled -- one each to
 * `getConstellationsIndex`, `getConstellationMask`,
 * `getCodecConstellationMask` and `getDataBitRate`, two to `edprintf`, one to
 * `dsplibs_debug_printf`, one against `dsplibs_debug_level`, and seven
 * against `.rodata` strings.  No `V90CP`, no `V92CP`, nothing else.
 *
 * THE FOUR ARGUMENT NAMES.  The two source types are the mangling's own.
 * `cleardown` is named from the message its non-zero arm prints -- the
 * author's own word -- and is the strongest kind of evidence this tree
 * accepts for a name.  `bits` is what the callee `float2Bits` calls it.
 *
 * THE RETURN IS `pos + 20`, with `pos` the index of the CRC block's framing
 * bit: sixteen CRC bits, three trailing zeros and the framing bit itself.
 * The only caller is `VPcmFloModem::v90RunDemodulator`, which is unwritten,
 * so nothing here says what it is used for.
 *
 * TWO PRINT SITES AND THEY ARE ASYMMETRIC, deliberately.  CLEARDOWN goes
 * through `dsplibs_debug_printf` behind `cmpl $0x1,dsplibs_debug_level`; the
 * closing "V90CP packed: CP%s%s%s bits:" and the seven rows of seventeen go
 * through `edprintf`, which carries its own level test inside, so those eight
 * calls happen at every level and print at none below 2.  That is the same
 * shape `displaySpectralParams` has and is checked the same way.
 *
 * THE THREE `%s` NAME THE MESSAGE and are three separate ternaries on three
 * separate fields, which is what types those fields as flags:
 *
 *     info->word_04 == 0  ->  "t"      CPt
 *     info->word_10 != 0  ->  "s"      CPs
 *     info->word_00 != 0  ->  "'"      CP'
 *
 * -- note that the first is inverted with respect to the other two, and that
 * the SAME field also selects `getDataBitRate`'s constant.  A message with
 * every flag clear prints as "CPt".
 *
 * THE ROW DUMP IS SEVEN ROWS FROM `bits[17]` AND IS FIXED.  `mov $0x6,%esi`
 * and `dec`/`jns`, with the cursor starting at bits + 0x22 and advancing by
 * 0x22, so it covers blocks 1..7 -- bits[17..135] -- and never the
 * constellation blocks however many groups there are.  It reads each entry
 * with `movswl`, which is a `short` promoted through varargs.
 */
int
V90CPPacker(V90MappingParams *params, tagV90AdditionalCPinfo *info,
	    short *bits, int cleardown)
{
	/*
	 * `getConstellationMask` clears eight words and then sets
	 * `mask[b >> 4]` UNMASKED, so a constellation byte at or above 0x80
	 * reaches entries 8..15.  The object's stack slot is eight words and
	 * the overspill lands in its own `crc[]`, which it re-initialises
	 * after the last mask call.  Ours is sixteen: same behaviour, no
	 * out-of-bounds write on our side.  docs/deviations.md D791.
	 */
	short mask[16];
	int group[6];
	int crc[16];
	int nofGroups, rate, pos, nofBlocks;
	int i, j, k, n, b, m, t;

	nofGroups = (int)getConstellationsIndex(params, group);

	/* Block 0: seventeen ones, and the first framing zero after them. */
	for (i = 0; i <= 16; i++)
		bits[i] = 1;
	bits[17] = 0;

	/*
	 * THE FIVE RATE BITS AND THE ARM THAT ZEROES THEM.  `cleardown`
	 * non-zero logs and leaves the rate at 0; otherwise the rate is
	 * `getDataBitRate`, whose second argument is the SAME `word_04` that
	 * has just been stored into bits[19] -- the object reuses %eax.
	 */
	bits[18] = 0;
	bits[19] = (short)info->word_04;

	if (cleardown != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V90CPPacker: CLEARDOWN indicated !\r\n");
		rate = 0;
	} else {
		rate = getDataBitRate(params, (int)info->word_04);
	}

	for (i = 0; i <= 4; i++) {
		bits[20 + i] = (short)(rate & 1);
		rate >>= 1;
	}

	for (i = 0x19; i <= 0x1d; i++)
		bits[i] = 0;

	/*
	 * `shaperSR` and `shaperId` are each sent as TWO BITS and the object
	 * shifts them with `shr`, not `sar` -- so the value entering the
	 * extraction is unsigned.  `shaperId` already is; `shaperSR` is
	 * `int` in the header for the reasons that header sets out at length,
	 * and the cast is stated here at the one site that needs it rather
	 * than the field being retyped for a two-bit read.
	 */
	bits[30] = (short)info->word_10;
	bits[31] = (short)((unsigned int)params->shaperSR & 1u);
	bits[32] = (short)(((unsigned int)params->shaperSR >> 1) & 1u);
	bits[33] = (short)info->word_00;
	bits[34] = 0;
	bits[35] = (short)info->word_0c;

	/*
	 * THIRTEEN BITS OUT OF ONE SIGNED SHORT, and the object loads it ONCE
	 * (`movswl 0x14(%esi),%edx` sits outside the loop) and shifts the
	 * copy.  Thirteen bits of a sixteen-bit value are the same under
	 * either extension, so the `movswl` is not observable here -- it is
	 * written faithfully anyway because the field IS signed.
	 */
	t = info->short_14;
	for (i = 0; i <= 12; i++) {
		bits[36 + i] = (short)(t & 1);
		t >>= 1;
	}

	bits[49] = (short)(params->shaperId & 1u);
	bits[50] = (short)((params->shaperId >> 1) & 1u);
	bits[51] = 0;

	/*
	 * The four Q1.6 fields are two pairs with a framing bit between them,
	 * which is the same arrangement `V92CP::evaluateInfo` reads back as
	 * two loops of two.
	 */
	float2Bits(info->float_08, &bits[52], 0);
	bits[68] = 0;
	float2Bits(params->shaperA1, &bits[69], 1);
	float2Bits(params->shaperA2, &bits[77], 1);
	bits[85] = 0;
	float2Bits(params->shaperB1, &bits[86], 1);
	float2Bits(params->shaperB2, &bits[94], 1);
	bits[102] = 0;

	/*
	 * The six group numbers, four bits each.  Two loops and not one,
	 * because the framing bit at 119 falls between the fourth and the
	 * fifth -- the same reason `V92CP::infoToBits`'s six four-bit counts
	 * are split.  The shift is `sar`: `group[]` is `int`.
	 */
	for (i = 0; i <= 3; i++) {
		t = group[i];
		for (j = 0; j <= 3; j++) {
			bits[103 + 4 * i + j] = (short)(t & 1);
			t >>= 1;
		}
	}
	bits[119] = 0;

	for (i = 0; i <= 1; i++) {
		t = group[4 + i];
		for (j = 0; j <= 3; j++) {
			bits[120 + 4 * i + j] = (short)(t & 1);
			t >>= 1;
		}
	}

	/*
	 * AN EQUALITY AND NOT A TEST FOR NON-ZERO: `cmpl $0x1,0x61c(%ebx)`
	 * then `sete`.  `word_61c` at 2 sends a zero here and skips the codec
	 * block; at 1 it sends a one and runs it.  The two readings differ on
	 * every value but 0 and 1, and they differ in the LENGTH of the
	 * message, so this bit is load-bearing twice over.
	 */
	bits[128] = (short)(params->word_61c == 1u);

	for (i = 0x81; i <= 0x87; i++)
		bits[i] = 0;

	/*
	 * THE CONSTELLATION MASKS.  One block of seventeen per mask word,
	 * eight words per group: 136 entries per group, starting at 136.
	 *
	 * The mask word is loaded with `movzwl` and shifted with `sar`, which
	 * is a SIGNED int holding a zero-extended short -- the cast below,
	 * not `int m = mask[j]` (that is `movswl`) and not `unsigned m` (that
	 * is `shr`).  The two readings agree over all sixteen extractions, so
	 * this is a codegen-tier reading and not a behavioural one.
	 */
	for (k = 0; k < nofGroups; k++) {
		getConstellationMask(params, k, mask);

		for (j = 0; j <= 7; j++) {
			int base = 136 + 136 * k + 17 * j;

			bits[base] = 0;
			m = (int)(unsigned short)mask[j];
			for (n = 0; n <= 15; n++) {
				bits[base + 1 + n] = (short)(m & 1);
				m >>= 1;
			}
		}
	}

	pos = 136 + 136 * nofGroups;

	if (bits[128] != 0) {
		for (k = 0; k < nofGroups; k++) {
			getCodecConstellationMask(params, k, mask);

			for (j = 0; j <= 7; j++) {
				int base = pos + 136 * k + 17 * j;

				bits[base] = 0;
				m = (int)(unsigned short)mask[j];
				for (n = 0; n <= 15; n++) {
					bits[base + 1 + n] = (short)(m & 1);
					m >>= 1;
				}
			}
		}

		pos += 136 * nofGroups;
	}

	bits[pos] = 0;

	/*
	 * THE CRC.  Sixteen ints, all seeded to one, then the CCITT register
	 * over the information bits of blocks 1 .. nofBlocks-1.  `nofBlocks`
	 * is `pos / 17` and the object divides by the constant with
	 * `imul $0x78787879` / `sar $3` and a signed correction, so `pos` is
	 * signed and the division is exact -- every framing bit is a multiple
	 * of seventeen, so `pos` always is.
	 *
	 * `t` is NOT masked before it is used, exactly as the object leaves
	 * it: `bits[i]` here can hold a value other than 0 or 1, because
	 * bits[19], [30], [33] and [35] are truncated fields rather than
	 * booleans.  Masking early and masking late agree on parity, so the
	 * two spellings are indistinguishable -- but the object's is the one
	 * written.
	 */
	for (i = 0; i <= 0xf; i++)
		crc[i] = 1;

	nofBlocks = pos / 17;

	for (b = 1; b < nofBlocks; b++) {
		for (i = 17 * b + 1; i <= 17 * b + 16; i++) {
			t = bits[i] + crc[0];

			crc[0] = crc[1];
			crc[1] = crc[2];
			crc[2] = crc[3];
			crc[3] = (crc[4] + t) & 1;
			crc[4] = crc[5];
			crc[5] = crc[6];
			crc[6] = crc[7];
			crc[7] = crc[8];
			crc[8] = crc[9];
			crc[9] = crc[10];
			crc[10] = (crc[11] + t) & 1;
			crc[11] = crc[12];
			crc[12] = crc[13];
			crc[13] = crc[14];
			crc[14] = crc[15];
			crc[15] = t & 1;
		}
	}

	for (i = 0; i <= 0xf; i++)
		bits[pos + 1 + i] = (short)crc[i];

	for (i = 0; i <= 2; i++)
		bits[pos + 17 + i] = 0;

	edprintf("V90CP packed: CP%s%s%s bits:\n",
		 info->word_04 != 0 ? "" : "t",
		 info->word_10 != 0 ? "s" : "",
		 info->word_00 != 0 ? "'" : "");

	for (i = 0; i <= 6; i++) {
		const short *r = &bits[17 + 17 * i];

		edprintf("%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d" "%1d\r\n",
			 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
			 r[8], r[9], r[10], r[11], r[12], r[13], r[14],
			 r[15], r[16]);
	}

	return pos + 0x14;
}
