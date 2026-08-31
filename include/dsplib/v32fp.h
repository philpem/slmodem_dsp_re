/*
 * v32fp.h -- ITU-T V.32 / V.32bis: the datapump's PARAMETER BLOCK and the
 *            configuration templates the FP layer builds one from.
 *
 * `v32fpctl.h` models the V.32 instance by OFFSET MACROS and says why.  This
 * header does NOT overturn that: it models only the instance's FIRST 48
 * BYTES, and it models them because the object itself treats those 48 bytes
 * as one movable object rather than as a set of offsets --
 *
 *     7e89e   V32FP_recreate   cld; mov $0xc,%ecx; rep movsl   arg1 -> obj
 *     7f5fd   V32FP_recreate   the same twelve dwords from V32_CFG when the
 *                              caller passes a null parameter block
 *     7f714   V32FP_create     the same twelve dwords from V32_CFG onto the
 *                              stack, patched, and handed to V32FP_recreate
 *
 * -- three `rep movsl` of exactly twelve dwords in two functions.  A block
 * that is copied wholesale is a struct; the offsets it is read at afterwards
 * stay v32fpctl.h's business, and the two agree at every offset both name
 * (+0x14, +0x18, +0x1c), which `t_v32fptab.c` asserts at compile time.
 *
 * V.22 HAS THE SAME PAIR AND IT IS THE MODEL FOR THIS ONE.
 * `struct v22fp_params` (v22fp.h) is 28 bytes, `V22_CFG` is its template,
 * `V22FP_create` copies the template to the stack and patches six fields, and
 * one of the patches is a READ-MODIFY-WRITE OF THE BYTE AT +0x11 inside a
 * 32-bit flag word at +0x10.  V.32 does all of that, at the same two
 * offsets, with the same 120000 at +0x08 and the same 103 sitting in the
 * template field that `V32DiconnectThreshTable[3]` immediately overwrites.
 * The correspondence is what says the field boundaries here are the author's
 * and not ours.  Finding F8641.
 *
 * ---------------------------------------------------------------------------
 * SIX OF THE EIGHTEEN FIELDS ARE NAMED BY THE AUTHOR, NOT BY US
 *
 * `V32FP_recreate` prints its whole parameter block at debug level 2, and the
 * format string is `.rodata.str1.4 + 0x11000`:
 *
 *     V32FP Config: protocol=%d,tx_rate=%d,rx_rate=%d,timeout=%d,
 *     energy_drop_time=%d,tx_scale=%d,options=0x%x,trellis=%d
 *
 * The eight arguments are pushed at 7f577..7f5b5 in this order, so the
 * correspondence is read off the instructions and not guessed:
 *
 *     protocol          +0x00  movswl        trellis           +0x1c  32-bit
 *     tx_rate           +0x02  movswl        options           +0x10  32-bit
 *     rx_rate           +0x04  movswl        tx_scale          +0x0c  32-bit
 *     timeout           +0x08  32-bit        energy_drop_time  +0x2a  movswl
 *
 * That is CLAUDE.md's strongest evidence class -- the original author's own
 * words -- and it is what settles `tx_rate` against `rx_rate`, which nothing
 * else here can: `V32FP_control` writes one value into both.  Finding F8644.
 *
 * ---------------------------------------------------------------------------
 * WHY THE UNKNOWN FIELDS ARE `rNN` AND NOT `short_NNNN`
 *
 * CLAUDE.md's convention for "modelled, unnamed" is `type_NNNN`.  This struct
 * uses `rNN`, which is what its sibling `struct v22fp_params` uses, because a
 * reader comparing the two side by side is the main way either gets checked.
 * Every one of them is modelled -- width and offset come from an instruction
 * -- and none of them is named.
 */

#ifndef DSPLIB_V32FP_H
#define DSPLIB_V32FP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---------------------------------------------------------------------------
 * The parameter block: 48 bytes, and `V32_CFG` is its template.
 *
 * Field widths come from the patch and read instructions:
 *
 *   +0x00  movzwl 0x0(%ebp)      7e990, 7e9\*        16-bit
 *   +0x02  movzwl 0x2(%ebp)      7e901              16-bit
 *   +0x04  movzwl 0x4(%ebp)      7e950              16-bit
 *   +0x08  mov 0x8(%ebx),%eax    7f765  (create)    32-bit
 *   +0x11  movzbl/movb           7f740, 7f75d       8-bit, inside +0x10
 *   +0x14  movzwl 0x14(%ebx)     7f778  (create)    16-bit
 *   +0x16  movswl 0x16(%ebp)     7f13d, 7f150       16-bit
 *   +0x1c  mov %ecx,0x2c(%esp)   7f7a0  (create)    32-bit
 *   +0x24  mov 0x24(%ebp),%esi   7e8bc              32-bit
 *   +0x28  mov %di,0x28(%ebp)    7e8c3              16-bit
 *   +0x2a  mov %di,0x3a(%esp)    7f773  (create)    16-bit
 *   +0x2c  movw $0x0,0x2c(%ebp)  7e8b6              16-bit
 */
struct v32fp_params {
	/*
	 * +0x00.  `V32FP_recreate` ladders on it -- 0, 1, anything else --
	 * to pick the half-duplex mode, and the name is the author's own:
	 * the "protocol=%d" format string this function prints it with.
	 * v32nsloop.c tabulates the three arms.
	 */
	short protocol;
	/*
	 * +0x02 and +0x04, both in bit/s, and NAMED BY DIRECTION IN THE
	 * AUTHOR'S OWN FORMAT STRING.  `V32FP_recreate` runs the SAME
	 * four-way ladder on each -- 0x3840, 0x2ee0, 0x2580, 0x1c20 -- and
	 * `V32FP_control` writes one value into both (846af..846b3), so no
	 * differential test can tell a swap of them apart; what tells them
	 * apart is `V32FP Config: ... tx_rate=%d,rx_rate=%d ...`, printed
	 * with +0x02 then +0x04.  +0x04 is the one `GetRateV32` reports,
	 * which is v32fpctl.h's V32_OBJ_BPS.
	 */
	short tx_rate;		/* +0x02 template 14400                      */
	short rx_rate;		/* +0x04 template 14400                      */
	short r06;		/* +0x06 template 0                          */
	int timeout;		/* +0x08 template 120000; <- cfg + 0x08      */
	int tx_scale;		/* +0x0c template 17887                      */
	/*
	 * +0x10 is `options` -- the author's word, and printed as `0x%x` --
	 * a bit set read a byte at a time exactly as V.22's is.
	 * Bits 0, 1 and 2 are copied out into three ints at fp + 0x1c, +0x20
	 * and +0x24 (7e8bf..7e8fe); bit 10 -- that is, bit 2 of the byte at
	 * +0x11 -- is patched from the caller's configuration by
	 * `V32FP_create` and is the only part of it that function touches.
	 *
	 * One `unsigned int` and not bitfields: the object's accesses are
	 * `movzbl`/`andb` against immediates, which is what CLAUDE.md's
	 * measurement says a bitfield would move away from.
	 */
	unsigned int options;	/* +0x10 template 0x68b                      */
	/*
	 * +0x14.  v32fpctl.h's V32_OBJ_EC_NEAR_DELAY: `SetAdaptEcV32` mode 0
	 * copies it into `fpm_ecc::near_delay`.  `V32FP_create` patches it
	 * 16-bit from cfg + 0x04.
	 */
	unsigned short ec_near_delay;
	/*
	 * +0x16 and +0x18 are BOTH selectors into the three two-entry length
	 * tables, and they are not the same field.
	 *
	 *   V32FP_recreate  indexes V32_SYMBOL_LEN, V32_SAMPLE_LEN and
	 *                   V32_TURNAROUND_DLY with +0x16 (7f13d, 7f150,
	 *                   7f165)
	 *   V32FP_control   indexes V32_SAMPLE_LEN with BOTH (845cf, 845d7),
	 *                   divides the +0x18 entry by the +0x16 entry, and
	 *                   scales hdx + 0x9c by the ratio -- so +0x16 is the
	 *                   length in force and +0x18 the one being moved to
	 *                   -- and then re-seeds hdx + 0x84, +0x9e and +0xa0
	 *                   from +0x18 alone (84606..8462c).
	 *
	 * v32fpctl.h names +0x18 V32_OBJ_SYMLEN_SEL and this header agrees
	 * with it.  +0x16 is left unnamed: "the previous one" is what the
	 * division says and it is usage inference, not a measurement.
	 */
	short r16;		/* +0x16 template 0                          */
	short symlen_sel;	/* +0x18 template 0; V32_OBJ_SYMLEN_SEL      */
	short r1a;		/* +0x1a template 0                          */
	/*
	 * +0x1c.  v32fpctl.h's V32_OBJ_TRELLIS -- `GetRateV32` reports
	 * V32_RATE_9600 when it is set and V32_RATE_9600_NT when it is clear.
	 * `V32FP_create` sets it to `cfg->bps > 0x1c1f`, i.e. to "faster than
	 * 7200", and `V32FP_control` copies ctl + 0x10 into it.
	 */
	int trellis;		/* +0x1c template 0                          */
	int r20;		/* +0x20 template 0                          */
	int r24;		/* +0x24 template 0; read at 7e8bc           */
	/*
	 * +0x28 is overwritten from V32DiconnectThreshTable[3] -- 150 -- by
	 * `V32FP_recreate` on every path, so the template's 103 never
	 * survives.  Named for the table it is loaded from, which is the only
	 * evidence there is, and which is exactly V.22's argument for the
	 * field at the same position in its own parameter block.
	 */
	short disconnect_thresh; /* +0x28 template 103                       */
	/*
	 * +0x2a is `energy_drop_time` in the format string, printed with
	 * `movswl` -- so it is a signed short and the extension is not dead.
	 * `V32FP_create` patches it from the low half of cfg + 0x0c.
	 */
	short energy_drop_time;	/* +0x2a template 0                          */
	short r2c;		/* +0x2c template 0; zeroed at 7e8b6         */
	short r2e;		/* +0x2e template 0                          */
};

/*
 * ---------------------------------------------------------------------------
 * The control request: 32 bytes, and `V32_CTL` is its template.
 *
 * `v32_data` and `V32FP_status` each copy the template onto the stack as
 * eight dword moves, set a bit or two in the byte pair at +0x0c, and hand the
 * result to `V32FP_control`; `Control_Flag` is what makes the call happen on
 * the NEXT entry to `v32_data` rather than on this one.
 *
 * Widths are from `V32FP_control`, which is the only reader:
 *
 *   +0x00  movzwl (%edi)         84645  16-bit; the bit/s ladder again
 *   +0x0c  movzbl 0xc(%edi)      84549  8-bit, six bits used
 *   +0x0d  movzbl 0xd(%edi)      84637  8-bit, two bits used
 *   +0x10  mov 0x10(%edi),%edx   846a2  32-bit -> obj->trellis
 *   +0x14  mov 0x14(%edi),%ecx   845b2  32-bit, tested against zero
 *   +0x18  mov 0x18(%edi),%ecx   84592  32-bit -> fp + 0x14
 *   +0x1c  mov 0x1c(%edi),%ebp   84598  32-bit -> fp + 0x18
 *
 * +0x0c and +0x0d are TWO BYTES and not one `unsigned int`: every access to
 * either is byte-wide in the object, on both the reading and the writing
 * side (`orb $0x8,0x2d(%esp)` in `v32_data`, `andb $0xf7,0xd(%edi)` in
 * `V32FP_control`).  The bits are NOT named here -- what is established is
 * each one's destination and nothing about its meaning, and a destination
 * offset dressed as a name is what CLAUDE.md says not to write.  The
 * destinations are tabulated in `v32fptab.c`.
 *
 * ONE BIT IS NAMED, AND BY THE AUTHOR.  `V32FP_control` prints
 * `Patch: set ctl_ptr->vxx_ctl.options.retrain = TRUE` at 84737 and then sets
 * bit 2 of +0x0d on both the debug and the non-debug path (8472e, 84743);
 * the arm that bit selects re-runs `V32FP_recreate` and hands the machine to
 * `V32OrgNextState` or `V32AnsNextState`, which is a retrain.  So
 * `V32_CTL1_RETRAIN` is 0x04 in the original's own words -- and the string
 * also says the argument is called `ctl_ptr` and that the block is reached as
 * `vxx_ctl`, a per-modulation control record inside something larger.
 *
 * Nothing names any other bit and none is guessed.  The string's dotted path
 * says the original spelled these as BITFIELDS inside an `options` member;
 * they are two `unsigned char` here because CLAUDE.md's rule is that a
 * bitfield declaration additionally claims a packing order, and because the
 * byte values are what the object settles.  Finding F8644.
 */

/* `.rodata.str1.4 + 0x110bc`, printed by `V32FP_control` itself. */
#define V32_CTL1_RETRAIN	0x04
/*
 * The other arm's bit, and it is NOT named by anything.  `V32FP_control`
 * tests it at 84680 and clears it at 84700; the arm it selects re-enters the
 * ring handshake through `V32RngInitNextState` or `V32RngRespNextState` and
 * installs a new line rate from the request, which is a renegotiation.  That
 * is USAGE INFERENCE and the weakest name in this header; the bit VALUE is
 * measured and the meaning is not.
 */
#define V32_CTL1_RENEG		0x08
struct v32fp_ctl {
	unsigned short bps;	/* +0x00 template 9600                       */
	short r02;		/* +0x02 template 9600; no reader written    */
	int r04;		/* +0x04 template 120000                     */
	int r08;		/* +0x08 template 1                          */
	unsigned char ctl0;	/* +0x0c template 0x83                       */
	unsigned char ctl1;	/* +0x0d template 0x01                       */
	short r0e;		/* +0x0e template 0                          */
	int trellis;		/* +0x10 template 0 -> obj->trellis          */
	int r14;		/* +0x14 template 0; non-zero re-seeds the
				 *       hdx length fields                   */
	int r18;		/* +0x18 template 1                          */
	int r1c;		/* +0x1c template 1                          */
};

/*
 * ---------------------------------------------------------------------------
 * The tables.
 */

extern const struct v32fp_params V32_CFG;	/* .rodata 0x006da0  48      */
extern const struct v32fp_ctl V32_CTL;		/* .rodata 0x007f20  32      */

/*
 * Eight disconnect thresholds; `V32FP_recreate` reads index 3 -- 150 -- and
 * nothing in the object reads any other entry or the symbol by name.  It is
 * `.rodata` and file-local in the blob, so `const` here and not `static`,
 * which is what makes it reachable from its differential test.
 */
extern const short V32DiconnectThreshTable[8];

/*
 * Two six-entry tables indexed by the V.32bis RATE INDEX, v32seq.h's 0..5.
 * Both are `.data` in the object -- so not `const` -- and file-local.
 *
 * `RATEv32` is the line rate in bit/s and it SETTLES v32seq.h's one open
 * inference: index 0 is 4800.  That header derived indices 1..5 from
 * `V32FP_recreate`'s own ladder and had to label index 0 from the
 * Recommendation, because the object's ladder only says "none of the other
 * four".  This table says 4800 in the author's own bytes.  Finding F8640.
 */
extern short RATEv32[6];
extern short SnrToRetrainTable[6];

/*
 * `.bss`, GLOBAL, one int.  `v32_data` is the only referrer: it builds a
 * `struct v32fp_ctl` on the stack and sets this, and the NEXT entry to
 * `v32_data` sees it set, issues the `V32FP_control` call and clears it.
 * The name is the author's; what it flags is the pending call.
 */
extern int Control_Flag;

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32FP_H */
