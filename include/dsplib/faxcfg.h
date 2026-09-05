/**
 * @file faxcfg.h
 * @brief Class 1 fax: the four configuration tables the receive-side VMI
 *        constructors initialise themselves from.
 *
 *   FAXVMI_CFG   .rodata 0x009490   24   struct faxvmi_cfg   (const)
 *   V17RX_CFG    .data   0x0079a0   40   struct v17rx_cfg
 *   V27RX_CFG    .data   0x007b94   28   struct v27rx_cfg
 *   V29RX_CFG    .data   0x007df0   24   struct v29rx_cfg
 *
 * The three modem tables are three types, not one (F9051): their sizes are
 * 40, 28 and 24, and each is both the exact argument `sysdep_malloc` is
 * given in the matching `init_vmi_*` (0x28, 0x1c, 0x18) and the exact
 * number of dwords the matching `v??rx_create` copies onto its stack
 * before calling `V??RX_create`. Two independent readings of the same
 * number, so the sizes are measured rather than assumed, and a shared
 * type would be wrong for two of the three.
 *
 * None of the four contains a pointer in the object. Checked with a
 * relocation sweep over each symbol's own byte range, which is the
 * question `relocscan.py --range` does not answer (that one is "who
 * points at it"). Zero relocations inside all four, so every dword here
 * is a literal. The neighbouring `AGCv17_CFG`/`AGCv27_CFG`/`AGCv29_CFG`
 * are the opposite case, two relocations each at +0x0c and +0x10, and are
 * not written here.
 *
 * Several fields have evidence for a name; eight are new this wave
 * (F10169), on top of `slot`/`bit_rate`/`modem_cfg` from earlier ones:
 *
 *   - `slot` (+0x0e of `struct faxvmi_cfg`) and `bit_rate` (+0x04 of each
 *     modem table) -- see their own field comments below, and findings
 *     F9052/F9053.
 *   - `struct faxvmi_cfg`'s own `mode`/`reverse`/`fifo_size`/`max_frame`/
 *     `frame_size` (+0x00/+0x04/+0x08/+0x0a/+0x0c) -- `FAXVMI_create`
 *     (`faxvmi.c`) copies this table's head verbatim onto `struct faxvmi`'s
 *     OWN already-named fields of those exact names (`vmi->mode =
 *     src->short_0000;` and so on down to `vmi->frame_size =
 *     src->short_000c;`), and `faxvmi.h`'s own cross-reference comment
 *     ALREADY states the correspondence field for field, with F9010 for
 *     `reverse` and F9011/F9019 for `max_frame` -- it had simply never been
 *     carried across to this struct's own declaration, the same
 *     evidence-stranded-in-one-file shape F10139/F10140 named for a V.90
 *     class pair. `faxvmi.h` is another strand of this wave and is not
 *     edited here; only this struct's own five names move.
 *   - Three of `struct v17rx_cfg`'s own pointer slots, +0x18/+0x1c/+0x20,
 *     renamed to `coefsave0`/`coefsave1`/`ratesave`: `src/fax/v17.c`'s own
 *     `V17RX_create` header comment already mapped them to
 *     `fpm_fse_cfg::icoff`/`qcoff` and, by construction (F9470, the receive
 *     instance's head IS this struct), those three offsets are exactly
 *     `V17RX_OBJ_COEFSAVE0`/`V17RX_OBJ_COEFSAVE1`/`V17RX_OBJ_RATESAVE` in
 *     `v17fax.h` -- the same storage, named there from
 *     `StoreCoefV17`/`Restore_rateV17`'s own matched read/write pair, just
 *     never carried across to this struct's own declaration either.
 *     `V17RX_OBJ_COEFSAVE0`/`1` are themselves NUMBERED rather than named in
 *     `v17fax.h` ("which of the two arrays is which rail is not
 *     established"), so `coefsave0`/`coefsave1` carry the same caveat here.
 *
 * Everything else keeps a `type_NNNN` name: the values are known and their
 * meaning is not. +0x00 is 1 in all three modem tables, +0x08 is 60000 in
 * all three (never read back by anything reconstructed -- checked directly,
 * not assumed), and `FAXVMI_CFG`'s own `fifo_size`/`max_frame`/`frame_size`
 * hold 128, 50 and 128 -- values every constructor immediately overwrites
 * with 320, 165 and 0 before `FAXVMI_create` ever reads them back, so the
 * table's OWN three numbers are exactly as arbitrary as before; only the
 * FIELDS' roles are now established, not these particular defaults.
 *
 * `struct faxvmi_cfg` is a separate type from `struct faxvmi` (`faxvmi.h`)
 * because the object's `struct faxvmi` is at least 0x2c bytes -- `faxvmi.h`
 * has the wrapped handle at +0x28 -- and `FAXVMI_CFG` is 24. What the
 * constructors do is copy those 24 bytes over the head of a `struct
 * faxvmi` and then overwrite six of its fields, so the config is the
 * struct's leading sub-object. Modelling that as an embedded member would
 * mean editing `faxvmi.h`, which belongs to another strand of this wave;
 * the two should be unified once both halves are written, and until then
 * the `init_vmi_*rx` parameter is spelled `struct faxvmi_cfg *`.
 */

#ifndef DSPLIB_FAXCFG_H
#define DSPLIB_FAXCFG_H

/*
 * The head of a `struct faxvmi`, and the whole of `FAXVMI_CFG`.
 *
 * The widths are the constructors' own stores: +0x00, +0x08, +0x0a and
 * +0x0c and +0x0e are written with `movw`, +0x04 with `movl`, and +0x10
 * and +0x14 with a 32-bit `mov` of a pointer value. +0x02 is never
 * written by anything reconstructed here and is 16 bits by subtraction.
 */
struct faxvmi_cfg {
	short		mode;		/* +0x00  0 in the table, set to 0.
					 * `FAXVMI_create` (`faxvmi.c`) copies
					 * this straight into `struct faxvmi`'s
					 * OWN already-named `mode` field
					 * (`FAXVMI_MODE_SIMP` etc, `faxvmi.h`),
					 * unchanged.  F10169.               */
	short		short_0002;	/* +0x02  never written              */
	int		reverse;	/* +0x04  0 in the table, set to 1.
					 * -> `struct faxvmi::reverse`, F9010:
					 * `FAXVMI_process` routes the block
					 * through `vmi_reverse[mode]` when
					 * set.  F10169.                      */
	short		fifo_size;	/* +0x08  128 -> 320.  -> `struct
					 * faxvmi::fifo_size`, the ring
					 * capacity `FAXVMI_create` asks for.
					 * F10169.                            */
	short		max_frame;	/* +0x0a   50 -> 165.  -> `struct
					 * faxvmi::max_frame`, F9011/F9019
					 * (four independent confirmations).
					 * F10169.                            */
	short		frame_size;	/* +0x0c  128 -> 0.  -> `struct
					 * faxvmi::frame_size`, the HDLC
					 * assembly buffer's size.  F10169.   */
	short		slot;		/* +0x0e  index into the vxx tables.
					 * Corroborated three ways: derived
					 * from `vxx_message`'s relocations
					 * in faxvmi.h, and matched exactly
					 * by the three values the
					 * constructors themselves plant here
					 * (F9053)                           */
	void	       *modem_cfg;	/* +0x10  the allocated modem table  */
	void	       *ptr_0014;	/* +0x14  the constructor's 4th arg  */
};

/*
 * V.17 receive.  40 bytes.  `init_vmi_v17rx` copies the table over a fresh
 * `sysdep_malloc(0x28)` and then sets `bit_rate`, clears +0x14, and installs
 * three further allocations of 0x62, 0x62 and 2 bytes at +0x18, +0x1c and
 * +0x20.  Those three are pointers BECAUSE a malloc result is stored in them,
 * which is a fact about the store and not about the table -- in the table
 * itself all three are zero.
 */
struct v17rx_cfg {
	int		int_0000;	/* +0x00  1                          */
	short		bit_rate;	/* +0x04  14400.  Compared against
					 * 14400/12000/9600 by `v17rx_create`
					 * -- the rates V.17 defines and
					 * nothing else -- then overwritten
					 * by `init_vmi_v17rx`'s own second
					 * argument (F9052).  16-bit access
					 * throughout, sign a choice: every
					 * comparison is an equality        */
	short		short_0006;	/* +0x06  0                          */
	int		int_0008;	/* +0x08  60000                      */
	int		int_000c;	/* +0x0c  0                          */
	int		int_0010;	/* +0x10  0                          */
	int		int_0014;	/* +0x14  0, cleared again on init   */
	void	       *coefsave0;	/* +0x18  init: sysdep_malloc(0x62).
					 * `V17RX_OBJ_COEFSAVE0` (v17fax.h) --
					 * same storage, the instance's head
					 * IS this struct (F9470).  Which rail
					 * ("0" vs "1") is which is not
					 * established, so the two are
					 * numbered and not named further. */
	void	       *coefsave1;	/* +0x1c  init: sysdep_malloc(0x62).
					 * `V17RX_OBJ_COEFSAVE1` -- see
					 * `coefsave0`.               */
	void	       *ratesave;	/* +0x20  init: sysdep_malloc(2).
					 * `V17RX_OBJ_RATESAVE` -- a real name,
					 * from a matched pair:
					 * `StoreCoefV17` writes it from
					 * `V17RXS_RATE` and `Restore_rateV17`
					 * writes `V17RXS_RATE` back from it;
					 * "rate" is `Restore_rateV17`'s own
					 * word.                      */
	void	       *ptr_0024;	/* +0x24  init: the 4th argument     */
};

/* V.27ter receive.  28 bytes; the same prefix, four fewer fields. */
struct v27rx_cfg {
	int		int_0000;	/* +0x00  1                          */
	short		bit_rate;	/* +0x04  4800.  Same evidence as
					 * v17rx_cfg's own (F9052)           */
	short		short_0006;	/* +0x06  0                          */
	int		int_0008;	/* +0x08  60000                      */
	int		int_000c;	/* +0x0c  0                          */
	int		int_0010;	/* +0x10  0                          */
	int		int_0014;	/* +0x14  0                          */
	void	       *ptr_0018;	/* +0x18  init: the 4th argument     */
};

/* V.29 receive.  24 bytes. */
struct v29rx_cfg {
	int		int_0000;	/* +0x00  1                          */
	short		bit_rate;	/* +0x04  9600.  Same evidence as
					 * v17rx_cfg's own (F9052)           */
	short		short_0006;	/* +0x06  0                          */
	int		int_0008;	/* +0x08  60000                      */
	int		int_000c;	/* +0x0c  0                          */
	int		int_0010;	/* +0x10  0                          */
	void	       *ptr_0014;	/* +0x14  init: the 4th argument     */
};

/*
 * `FAXVMI_CFG` is `R` in the object -- global and in `.rodata` -- and the
 * other three are `D`, global and writable, even though nothing in the 1.2 MB
 * writes them.  The storage classes are the object's and are asserted by
 * `t_faxcfg.c` through the sections they land in.
 */
extern const struct faxvmi_cfg FAXVMI_CFG;
extern struct v17rx_cfg V17RX_CFG;
extern struct v27rx_cfg V27RX_CFG;
extern struct v29rx_cfg V29RX_CFG;

/*
 * The V.21 channel-2 tone detector's coefficients, `D` at .data 0x7a60.  Two
 * biquad sections of five shorts, which is what `tones = 2` over 20 bytes
 * fixes.  Shared by all three fax receiver constructors, which is why it lives
 * beside the four tables above rather than in a per-modulation file.
 */
extern short V21_CHAN2_MTD_COEFF[10];

#endif /* DSPLIB_FAXCFG_H */
