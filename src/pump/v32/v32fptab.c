/*
 * v32fptab.c -- ITU-T V.32 / V.32bis: the FP layer's configuration templates.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32_CFG                  .rodata 0x006da0   48   GLOBAL
 *   V32DiconnectThreshTable  .rodata 0x006dd0   16   file-local
 *   SnrToRetrainTable        .data   0x007750   12   file-local
 *   RATEv32                  .data   0x00775c   12   file-local
 *   V32_CTL                  .rodata 0x007f20   32   GLOBAL
 *   SMCv32_CFG               .bss    0x000188    4   GLOBAL
 *   Control_Flag             .bss    0x0001a0    4   GLOBAL
 *
 * SECTION PLACEMENT IS THE OBJECT'S AND IS NOT A PREFERENCE.  `V32_CFG`,
 * `V32DiconnectThreshTable` and `V32_CTL` are `.rodata`, so `const`; the two
 * rate-indexed tables are `.data`, so not, even though nothing writes either.
 * `v32hdx_tables.c` makes the same point about the four tables it owns and
 * for the same reason.
 *
 * The two file-local ones are written WITHOUT `static` here.  That is the
 * tree's convention for a blob-local data symbol -- `symmap.py` globalises
 * them on the reference side so the differential test can name `ref_RATEv32`,
 * and a `static` on ours would leave the test comparing one side only.
 * `V22DiconnectThreshTable` is the precedent.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE TWO STRUCT TEMPLATES ARE, IN ONE PLACE
 *
 * `v32fp.h` derives both layouts from the instructions that read them.  What
 * is worth having beside the values is the DESTINATION of each bit in
 * `V32_CTL`'s byte pair, because those bits are the whole reason the control
 * request exists and none of them is named.
 *
 * `V32FP_control` (0x84530) with `obj` in %esi, `ctl` in %edi and
 * `fp = obj->fp` in %ebx:
 *
 *   ctl0 & 0x01  ->  fp + 0x1c = 1 / 0        84549
 *   ctl0 & 0x02  ->  fp + 0x20 = 1 / 0        84558
 *   ctl0 & 0x04  ->  fp + 0x24 = 1 / 0        84564
 *   ctl0 & 0x08  ->  fp + 0x00 = 0 / 1        84571   INVERTED
 *   ctl0 & 0x10  ->  fp + 0x0c = 0 / 1        8457c   INVERTED
 *   ctl0 & 0x20  ->  fp + 0x10 = 0 / 1        84588   INVERTED
 *   ctl0 & 0x80  ->  obj + 0x11 bit 1         8459e   V32_OBJ_FLAGS
 *   ctl1 & 0x04  ->  the rate-change arm,  cleared with andb $0xfb  84669
 *   ctl1 & 0x08  ->  the mode-change arm,  cleared with andb $0xf7  84700
 *   ctl  + 0x14  ->  non-zero re-seeds the hdx length fields  845b2
 *   ctl  + 0x18  ->  fp + 0x14                84592
 *   ctl  + 0x1c  ->  fp + 0x18                84598
 *
 * fp + 0x00 .. fp + 0x24 is ten `int`s, all set to 1 by `V32FP_recreate`
 * (7e8e4 .. 7e91e) and individually set by this table's bits.  What they
 * switch is not established by anything written, so they are neither named
 * here nor modelled as a struct.
 *
 * The template's own `ctl0` is 0x83 -- bits 0, 1 and 7 -- and `ctl1` is 0x01.
 * `v32_data` rebuilds bits 0, 1 and 2 of `ctl0` from a byte of its own, forces
 * bit 7, and ORs either 0x04 or 0x08 into `ctl1` depending on which arm it
 * took.
 *
 * ---------------------------------------------------------------------------
 * `RATEv32` SETTLES v32seq.h's ONE OPEN INFERENCE
 *
 * v32seq.h tabulates the rate index -> line rate correspondence out of
 * `V32FP_recreate`'s four-way ladder and says of index 0: "INDEX 0 IS THE ONE
 * INFERENCE HERE, and it is from the Recommendation rather than the object:
 * the object only says 'not 14400, 12000, 9600 or 7200', and V.32's remaining
 * rate is 4800.  Labelled rather than asserted."
 *
 * This table asserts it.  `RATEv32[0]` is 4800 in the object's own bytes, and
 * the other five entries reproduce v32seq.h's ladder exactly, including the
 * 9600 appearing twice for the trellis and non-trellis variants at indices 2
 * and 1.  Finding F8640.
 *
 * `SnrToRetrainTable` is indexed the same way and read once, at 84ab9 in
 * `V32FP_status`, with a `movzwl` whose result is compared 16-bit -- so the
 * extension is dead and says nothing about the element type (finding F614);
 * `short` stands on the symbol's twelve bytes over six rates.  Its entries
 * ascend with the line rate except for the same 1/2 pair, which is what says
 * it is indexed by RATE and not by anything else.
 */

#include "dsplib/v32fp.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32smc.h"

/*
 * The two models of the V.32 instance have to agree, and this is where that
 * is checked rather than asserted in prose.  `v32fpctl.h` reaches the object
 * by offset macro and this file's `struct v32fp_params` reaches its first 48
 * bytes by field; three offsets are named on both sides, and if they ever
 * disagree the compile fails here instead of silently reading two fields.
 *
 * The guard is on `__SIZEOF_POINTER__`, a GCC 4.6+ predefine, so under the
 * period compiler every line below vanishes -- which is the variance
 * `docs/method/compilers.md` records and not an accident of this file.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V32FP_ASSERT_OFF(tag, type, field, off) \
	typedef char v32fp_off_##tag[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V32FP_ASSERT_OFF(p_bps, struct v32fp_params, bps, 0x02);
V32FP_ASSERT_OFF(p_r08, struct v32fp_params, r08, 0x08);
V32FP_ASSERT_OFF(p_flags, struct v32fp_params, flags, 0x10);
V32FP_ASSERT_OFF(p_ecdly, struct v32fp_params, ec_near_delay,
		 V32_OBJ_EC_NEAR_DELAY);
V32FP_ASSERT_OFF(p_symlen, struct v32fp_params, symlen_sel,
		 V32_OBJ_SYMLEN_SEL);
V32FP_ASSERT_OFF(p_trellis, struct v32fp_params, trellis, V32_OBJ_TRELLIS);
V32FP_ASSERT_OFF(p_disc, struct v32fp_params, disconnect_thresh, 0x28);
V32FP_ASSERT_OFF(p_r2e, struct v32fp_params, r2e, 0x2e);

V32FP_ASSERT_OFF(c_ctl0, struct v32fp_ctl, ctl0, 0x0c);
V32FP_ASSERT_OFF(c_ctl1, struct v32fp_ctl, ctl1, 0x0d);
V32FP_ASSERT_OFF(c_trellis, struct v32fp_ctl, trellis, 0x10);
V32FP_ASSERT_OFF(c_r1c, struct v32fp_ctl, r1c, 0x1c);

/*
 * And the sizes, which are what the three `rep movsl` sites and `v32_data`'s
 * eight dword moves actually copy.
 */
typedef char v32fp_params_is_48[(sizeof(struct v32fp_params) == 48) ? 1 : -1];
typedef char v32fp_ctl_is_32[(sizeof(struct v32fp_ctl) == 32) ? 1 : -1];
typedef char v32_smc_cfg_is_4[(sizeof(struct v32_smc_cfg) == 4) ? 1 : -1];

#endif

/* --------------------------------------------------------------------- */
/* .rodata, in the object's address order.                               */

const struct v32fp_params V32_CFG = {
	0,			/* protocol                                  */
	14400,			/* bps                                       */
	14400,			/* bps2                                      */
	0,			/* r06                                       */
	120000,			/* r08                                       */
	17887,			/* r0c                                       */
	0x68b,			/* flags                                     */
	0,			/* ec_near_delay                             */
	0,			/* r16                                       */
	0,			/* symlen_sel                                */
	0,			/* r1a                                       */
	0,			/* trellis                                   */
	0,			/* r20                                       */
	0,			/* r24                                       */
	103,			/* disconnect_thresh -- never survives       */
	0,			/* r2a                                       */
	0,			/* r2c                                       */
	0			/* r2e                                       */
};

const short V32DiconnectThreshTable[8] = {
	75, 95, 119, 150, 168, 174, 212, 238
};

/* --------------------------------------------------------------------- */
/* .data.                                                                */

short SnrToRetrainTable[6] = {
	9,			/* 0   4800                                  */
	13,			/* 1   9600, no trellis                      */
	13,			/* 2   9600, trellis                         */
	11,			/* 3   7200                                  */
	20,			/* 4  12000                                  */
	24			/* 5  14400                                  */
};

short RATEv32[6] = {
	4800,			/* 0                                         */
	9600,			/* 1   no trellis                            */
	9600,			/* 2   trellis                               */
	7200,			/* 3                                         */
	12000,			/* 4                                         */
	14400			/* 5                                         */
};

/* --------------------------------------------------------------------- */
/* .rodata again -- V32_CTL is 0x1180 above V32_CFG in the object.        */

const struct v32fp_ctl V32_CTL = {
	9600,			/* bps                                       */
	9600,			/* r02                                       */
	120000,			/* r04                                       */
	1,			/* r08                                       */
	0x83,			/* ctl0                                      */
	0x01,			/* ctl1                                      */
	0,			/* r0e                                       */
	0,			/* trellis                                   */
	0,			/* r14                                       */
	1,			/* r18                                       */
	1			/* r1c                                       */
};

/* --------------------------------------------------------------------- */
/* .bss.  Both are zero, and both are zero because the object says so.    */

/*
 * `V32FP_recreate` loads all four bytes at once (7e9db, `mov 0x0,%ecx`),
 * replaces the low half with `fp + 0x28 != 0` and stores the result to
 * fp + 0x48 -- which v32fpctl.h names as the symbol coder, `struct v32_smc`.
 * So this is that struct's first two shorts and nothing else, and the whole
 * of it is a template that only ever contributes `pad02`.
 */
struct v32_smc_cfg SMCv32_CFG;

int Control_Flag;
