/*
 * V90CPpck.h -- the ANALOGUE modem's own CP builder, and the Q-format bit
 * packer under it.
 *
 * THE FILE NAME IS THE ORIGINAL AUTHOR'S.  `tools/tumap.py` recovers the
 * link's `STT_FILE` order and TU 57 is `V90CPpck.cpp`, sitting between
 * `V90SdDetector.cpp` (56) and `V90TRN2dDesigner.cpp` (58).  Two independent
 * orderings put these symbols in it and neither is a shared bracket in the
 * usual weak sense -- they agree BY EXCLUSION:
 *
 *   .text   `float2Bits(float, short *, int)` at 0x3be30 and `V90CPPacker`
 *           at 0x3bf50 are the ONLY symbols between `V90SdDetector::process`
 *           (ends 0x3be30) and `V90TRN2Designer::V90TRN2Designer` (0x3c9b0).
 *   .data   `fltTable2` (+0xb00) and `fltTable1` (+0xb40) are the only data
 *           between TU 48's (`V90SpectralShaper`'s `actionLookupTable` and
 *           `pow10Table`) and TU 60's (`V90ConstellationPower::
 *           averagePowerLimits`), and no other TU in 49..59 defines any
 *           `.data` at all.
 *
 * `setV92CPpckFromParamsInfo` -- the V.92 twin, in `V90MappingParamsInt.cpp`
 * -- spells the same three letters, which is where the confidence in the
 * capitalisation comes from.  `tumap.py` spells TU 58 `V90TRN2dDesigner.cpp`
 * where this tree uses `V90TRN2Designer.cpp`, so a name from that tool is not
 * infallible; this one is corroborated.
 *
 * WHY THIS IS A NEW HEADER rather than a block added to `V90MappingParams.h`:
 * that file's own head gives the reason, and it still holds -- several batches
 * are merging against the V.90 headers and a new file does not perturb them.
 *
 * ===========================================================================
 * THERE ARE TWO `float2Bits` IN THE OBJECT AND ONLY ONE IS DECLARED HERE.
 *
 *     _Z10float2BitsfPsi   .text+0x3be30   283 B   (float, short *, int)
 *     _Z10float2BitsfPhi   .text+0x4ec00   125 B   (float, unsigned char *, int)
 *
 * They are ordinary C++ OVERLOADS and not one template: neither mangling
 * carries an `I...E` template-argument section, both are `T` (a template
 * instantiation would be `W`, weak), and both are in `.text` rather than a
 * `.gnu.linkonce.t.*` section -- which is where this object's real templates
 * (`Scrambler<int, unsigned char>`, `Agc<float>` and the rest) live.  The
 * 283-against-125 size gap is a fourth, independent sign.
 *
 * So they are two unrelated functions that happen to share a name.  The
 * `unsigned char *` one at 0x4ec00 is NOT declared here, is NOT written, and
 * must not be derived from this one.  `V90CPPacker`'s five calls are all
 * `R_386_PC32` against `_Z10float2BitsfPsi`, which is how we know which
 * overload this file owns -- from the relocation, not from the name.
 * ===========================================================================
 */

#ifndef DSPLIB_V90CPPCK_H
#define DSPLIB_V90CPPCK_H

class V90MappingParams;
struct tagV90AdditionalCPinfo;

/*
 * The two weight tables, .data+0xb00 (64 bytes) and .data+0xb40 (28 bytes).
 *
 * THEY ARE NOT `V92CP.cpp`'s `fltTable_2` AND `fltTable_1`.  Those are a
 * SEPARATE pair at .data+0x69e0 and +0x6a20 with different names, and finding
 * 826 measured them as distinct symbols rather than aliases.  The four
 * contents are byte-identical, missing 2^-8 and doubled 2^-9 included, which
 * is why the two pairs are so easy to confuse -- and why the reading has to
 * come from the relocation.  Every reference in `float2Bits` is against the
 * unsuffixed names; every reference in `V92CP::infoToBits` and
 * `V92CP::evaluateInfo` is against the suffixed ones.
 *
 * Not `const`, for the reason `V92CP.cpp` gives for its pair: a
 * namespace-scope `const` array has internal linkage in C++ and these are
 * GLOBAL `D` symbols.
 */
extern float fltTable2[16];
extern float fltTable1[7];

/*
 * Expand `f` into one bit per `short`, greedily and most significant first.
 *
 *   mode 0   Q3.13.  Sixteen magnitude entries off `fltTable2`, weighted 4
 *            down to 2^-13, with the HEAVIEST at `bits[15]`; no sign entry,
 *            the magnitude is taken through `fabs`.  Warns outside [0, 8].
 *   mode 1   Q1.6.  Seven magnitude entries off `fltTable1`, 1 down to
 *            2^-6, heaviest at `bits[6]`, then the sign at `bits[7]`.
 *            Warns outside [-1, 1].
 *   anything else   returns having touched nothing.
 *
 * The warnings are `dsplibs_debug_printf` behind `DSPLIB_DEBUG_ON()` and are
 * the only observable of the out-of-range arms: the expansion runs anyway and
 * saturates, because the greedy loop simply sets every entry it can.
 *
 * This is the WRITE side of the expansion `V92CP::evaluateInfo` reads back --
 * `f += fltTable_2[15 - i]` there against `bits[15 - i] = 1` here -- over the
 * other pair of tables.
 */
void float2Bits(float f, short *bits, int mode);

/*
 * Build the analogue modem's CP sequence into `bits`, one bit per `short`,
 * and return the total length INCLUDING the trailing framing bits: the CRC
 * lands at `bits[pos + 1 .. pos + 16]` and the return is `pos + 20`.
 *
 * `cleardown` non-zero replaces the five data-rate bits with zero and logs
 * "V90CPPacker: CLEARDOWN indicated !"; the message is the object's own and
 * is what names the parameter.
 *
 * See the .cpp for the layout, which is seventeen-bit frames throughout.
 */
int V90CPPacker(V90MappingParams *params, tagV90AdditionalCPinfo *info,
		short *bits, int cleardown);

#endif /* DSPLIB_V90CPPCK_H */
