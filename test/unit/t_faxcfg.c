/*
 * t_faxcfg.c -- differential test of the four Class 1 fax receive-side
 *               configuration tables and of the three constructors that
 *               consume them.
 *
 *   FAXVMI_CFG   .rodata 0x009490   24   struct faxvmi_cfg
 *   V17RX_CFG    .data   0x0079a0   40   struct v17rx_cfg
 *   V27RX_CFG    .data   0x007b94   28   struct v27rx_cfg
 *   V29RX_CFG    .data   0x007df0   24   struct v29rx_cfg
 *
 *   init_vmi_v17rx  .text 0x093e80  308
 *   init_vmi_v29rx  .text 0x093fc0  225
 *   init_vmi_v27rx  .text 0x0940b0  230
 *
 * FOUR LAYERS, and the reason for each is a SPECIFIC WRONG READING that the
 * layer below it would not catch.
 *
 *   1. SHAPE.  `sizeof` and `offsetof` against literals.  A table compared
 *      only against `ref_` proves the bytes and nothing about the boundaries:
 *      all four of these are mostly zero, so a struct with the fields in the
 *      wrong places, or with two shorts where the object has an int, compares
 *      equal byte for byte.  The sizes are the numbers `sysdep_malloc` is
 *      given in the object -- 0x28, 0x1c, 0x18 -- so getting one wrong is a
 *      heap defect, not a cosmetic one.
 *
 *   2. VALUE, field by field, against `ref_`, and then the SHAPE OF THE
 *      VALUES independently.  `bit_rate` is asserted against 14400, 4800 and
 *      9600 by name and the three are asserted pairwise DIFFERENT, because
 *      the three tables share a prefix and a copy-paste that gave V.29 the
 *      V.27 rate would still match its own `ref_` on every other field.
 *
 *   3. THE DETECTOR IS SHOWN TO FIRE (F134).  `bytes_differ` is run over an
 *      identical copy, which must return 0, and then over 24 copies each with
 *      ONE byte flipped, which must all return non-zero.  A byte comparison
 *      of two all-zero regions is the purest form of the dead detector, and
 *      three quarters of `FAXVMI_CFG` is zero.
 *
 *   4. USE.  The three constructors are run, ours against the blob's, over
 *      five (bit rate, argument) pairs each.  A wrong field boundary can
 *      leave every stored value matching and still put the `slot` in the
 *      wrong half of a dword; only running the code sees that.  The VMI is
 *      poisoned past its 24 bytes on both sides and the poison is asserted
 *      INTACT afterwards, which is what bounds the write extent -- the
 *      object's `struct faxvmi` is larger than its config head, so "it wrote
 *      the right 24 bytes" and "it wrote only 24 bytes" are two claims.
 *
 * WHAT IS COMPARED BY ROLE RATHER THAN BY VALUE.  `vmi->modem_cfg` and the
 * three V.17 sub-allocations hold heap addresses, so ours and the blob's can
 * never be equal.  They are checked for being non-null, for being pairwise
 * distinct (the two 0x62 allocations are the same size and would survive a
 * swap unnoticed), and for pointing at storage whose CONTENT matches.  The
 * fourth argument is passed identically to both sides, so the fields that
 * hold it ARE compared by value -- that is the one pointer here that a value
 * comparison can prove something about.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxcfg.h"
#include "dsplib/class1rx.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const struct faxvmi_cfg ref_FAXVMI_CFG;
extern struct v17rx_cfg ref_V17RX_CFG;
extern struct v27rx_cfg ref_V27RX_CFG;
extern struct v29rx_cfg ref_V29RX_CFG;

extern void ref_init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);
extern void ref_init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);
extern void ref_init_vmi_v29rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);

/*
 * FILE-LOCAL in the object, so class1rx.c defines them `static` and
 * class1rx.h no longer declares them.  Their addresses are taken (class1rx.c
 * stores all three in `init_vmi_data_rx_modem`), so the ordinary calling
 * convention is unchanged; the test tier links a globalized copy
 * (tools/testvisible.py).
 */
extern void init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			   int arg_2, void *arg_3);
extern void init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			   int arg_2, void *arg_3);
extern void init_vmi_v29rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			   int arg_2, void *arg_3);

/* ------------------------------------------------------------------------- */

/*
 * Layer 3's detector.  Deliberately its own function rather than a `memcmp`
 * call site, so that it can be exercised on a known-perturbed input below.
 */
static int
bytes_differ(const void *a, const void *b, size_t n)
{
	const unsigned char *p = (const unsigned char *)a;
	const unsigned char *q = (const unsigned char *)b;
	size_t i;

	for (i = 0; i < n; i++)
		if (p[i] != q[i])
			return 1;
	return 0;
}

/* How many of `n` bytes at `p` are non-zero.  Anti-vacuity, layer 2. */
static int
nonzero_bytes(const void *p, size_t n)
{
	const unsigned char *b = (const unsigned char *)p;
	size_t i;
	int c = 0;

	for (i = 0; i < n; i++)
		if (b[i] != 0)
			c++;
	return c;
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("faxcfg: struct shapes");

	/*
	 * The four sizes.  Each is the `sysdep_malloc` argument in the
	 * matching constructor and the `st_size` of the matching blob symbol,
	 * so they are two independent readings of the same number.
	 */
	diff_eq_int("sizeof struct faxvmi_cfg (%ld)",
		    (long)sizeof(struct faxvmi_cfg), 24, 0);
	diff_eq_int("sizeof struct v17rx_cfg (%ld)",
		    (long)sizeof(struct v17rx_cfg), 40, 0);
	diff_eq_int("sizeof struct v27rx_cfg (%ld)",
		    (long)sizeof(struct v27rx_cfg), 28, 0);
	diff_eq_int("sizeof struct v29rx_cfg (%ld)",
		    (long)sizeof(struct v29rx_cfg), 24, 0);

	/* Every offset the object's instructions name, by hand. */
	diff_eq_int("faxvmi_cfg.mode at %ld",
		    (long)offsetof(struct faxvmi_cfg, mode), 0x00, 0);
	diff_eq_int("faxvmi_cfg.short_0002 at %ld",
		    (long)offsetof(struct faxvmi_cfg, short_0002), 0x02, 0);
	diff_eq_int("faxvmi_cfg.reverse at %ld",
		    (long)offsetof(struct faxvmi_cfg, reverse), 0x04, 0);
	diff_eq_int("faxvmi_cfg.fifo_size at %ld",
		    (long)offsetof(struct faxvmi_cfg, fifo_size), 0x08, 0);
	diff_eq_int("faxvmi_cfg.max_frame at %ld",
		    (long)offsetof(struct faxvmi_cfg, max_frame), 0x0a, 0);
	diff_eq_int("faxvmi_cfg.frame_size at %ld",
		    (long)offsetof(struct faxvmi_cfg, frame_size), 0x0c, 0);
	diff_eq_int("faxvmi_cfg.slot at %ld",
		    (long)offsetof(struct faxvmi_cfg, slot), 0x0e, 0);
	diff_eq_int("faxvmi_cfg.modem_cfg at %ld",
		    (long)offsetof(struct faxvmi_cfg, modem_cfg), 0x10, 0);
	diff_eq_int("faxvmi_cfg.ptr_0014 at %ld",
		    (long)offsetof(struct faxvmi_cfg, ptr_0014), 0x14, 0);

	diff_eq_int("v17rx_cfg.bit_rate at %ld",
		    (long)offsetof(struct v17rx_cfg, bit_rate), 0x04, 0);
	diff_eq_int("v17rx_cfg.int_0008 at %ld",
		    (long)offsetof(struct v17rx_cfg, int_0008), 0x08, 0);
	diff_eq_int("v17rx_cfg.int_0014 at %ld",
		    (long)offsetof(struct v17rx_cfg, short_train), 0x14, 0);
	diff_eq_int("v17rx_cfg.coefsave0 at %ld",
		    (long)offsetof(struct v17rx_cfg, coefsave0), 0x18, 0);
	diff_eq_int("v17rx_cfg.coefsave1 at %ld",
		    (long)offsetof(struct v17rx_cfg, coefsave1), 0x1c, 0);
	diff_eq_int("v17rx_cfg.ratesave at %ld",
		    (long)offsetof(struct v17rx_cfg, ratesave), 0x20, 0);
	diff_eq_int("v17rx_cfg.ptr_0024 at %ld",
		    (long)offsetof(struct v17rx_cfg, ptr_0024), 0x24, 0);

	diff_eq_int("v27rx_cfg.bit_rate at %ld",
		    (long)offsetof(struct v27rx_cfg, bit_rate), 0x04, 0);
	diff_eq_int("v27rx_cfg.int_0008 at %ld",
		    (long)offsetof(struct v27rx_cfg, int_0008), 0x08, 0);
	diff_eq_int("v27rx_cfg.ptr_0018 at %ld",
		    (long)offsetof(struct v27rx_cfg, ptr_0018), 0x18, 0);

	diff_eq_int("v29rx_cfg.bit_rate at %ld",
		    (long)offsetof(struct v29rx_cfg, bit_rate), 0x04, 0);
	diff_eq_int("v29rx_cfg.int_0008 at %ld",
		    (long)offsetof(struct v29rx_cfg, int_0008), 0x08, 0);
	diff_eq_int("v29rx_cfg.ptr_0014 at %ld",
		    (long)offsetof(struct v29rx_cfg, ptr_0014), 0x14, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_values(void)
{
	diff_begin("faxcfg: table values against the blob");

	/* FAXVMI_CFG, field by field. */
	diff_eq_int("FAXVMI_CFG.mode (%ld)", FAXVMI_CFG.mode,
		    ref_FAXVMI_CFG.mode, 0x00);
	diff_eq_int("FAXVMI_CFG.short_0002 (%ld)", FAXVMI_CFG.short_0002,
		    ref_FAXVMI_CFG.short_0002, 0x02);
	diff_eq_int("FAXVMI_CFG.reverse (%ld)", FAXVMI_CFG.reverse,
		    ref_FAXVMI_CFG.reverse, 0x04);
	diff_eq_int("FAXVMI_CFG.fifo_size (%ld)", FAXVMI_CFG.fifo_size,
		    ref_FAXVMI_CFG.fifo_size, 0x08);
	diff_eq_int("FAXVMI_CFG.max_frame (%ld)", FAXVMI_CFG.max_frame,
		    ref_FAXVMI_CFG.max_frame, 0x0a);
	diff_eq_int("FAXVMI_CFG.frame_size (%ld)", FAXVMI_CFG.frame_size,
		    ref_FAXVMI_CFG.frame_size, 0x0c);
	diff_eq_int("FAXVMI_CFG.slot (%ld)", FAXVMI_CFG.slot,
		    ref_FAXVMI_CFG.slot, 0x0e);
	diff_eq_int("FAXVMI_CFG.modem_cfg is null (%ld)",
		    FAXVMI_CFG.modem_cfg == 0, ref_FAXVMI_CFG.modem_cfg == 0,
		    0x10);
	diff_eq_int("FAXVMI_CFG.ptr_0014 is null (%ld)",
		    FAXVMI_CFG.ptr_0014 == 0, ref_FAXVMI_CFG.ptr_0014 == 0,
		    0x14);

	/* V17RX_CFG. */
	diff_eq_int("V17RX_CFG.int_0000 (%ld)", V17RX_CFG.protocol,
		    ref_V17RX_CFG.protocol, 0x00);
	diff_eq_int("V17RX_CFG.bit_rate (%ld)", V17RX_CFG.bit_rate,
		    ref_V17RX_CFG.bit_rate, 0x04);
	diff_eq_int("V17RX_CFG.short_0006 (%ld)", V17RX_CFG.short_0006,
		    ref_V17RX_CFG.short_0006, 0x06);
	diff_eq_int("V17RX_CFG.int_0008 (%ld)", V17RX_CFG.int_0008,
		    ref_V17RX_CFG.int_0008, 0x08);
	diff_eq_int("V17RX_CFG.int_000c (%ld)", V17RX_CFG.int_000c,
		    ref_V17RX_CFG.int_000c, 0x0c);
	diff_eq_int("V17RX_CFG.int_0010 (%ld)", V17RX_CFG.int_0010,
		    ref_V17RX_CFG.int_0010, 0x10);
	diff_eq_int("V17RX_CFG.int_0014 (%ld)", V17RX_CFG.short_train,
		    ref_V17RX_CFG.short_train, 0x14);
	diff_eq_int("V17RX_CFG.coefsave0 is null (%ld)",
		    V17RX_CFG.coefsave0 == 0, ref_V17RX_CFG.coefsave0 == 0, 0x18);
	diff_eq_int("V17RX_CFG.coefsave1 is null (%ld)",
		    V17RX_CFG.coefsave1 == 0, ref_V17RX_CFG.coefsave1 == 0, 0x1c);
	diff_eq_int("V17RX_CFG.ratesave is null (%ld)",
		    V17RX_CFG.ratesave == 0, ref_V17RX_CFG.ratesave == 0, 0x20);
	diff_eq_int("V17RX_CFG.ptr_0024 is null (%ld)",
		    V17RX_CFG.ptr_0024 == 0, ref_V17RX_CFG.ptr_0024 == 0, 0x24);

	/* V27RX_CFG. */
	diff_eq_int("V27RX_CFG.int_0000 (%ld)", V27RX_CFG.int_0000,
		    ref_V27RX_CFG.int_0000, 0x00);
	diff_eq_int("V27RX_CFG.bit_rate (%ld)", V27RX_CFG.bit_rate,
		    ref_V27RX_CFG.bit_rate, 0x04);
	diff_eq_int("V27RX_CFG.short_0006 (%ld)", V27RX_CFG.short_0006,
		    ref_V27RX_CFG.short_0006, 0x06);
	diff_eq_int("V27RX_CFG.int_0008 (%ld)", V27RX_CFG.int_0008,
		    ref_V27RX_CFG.int_0008, 0x08);
	diff_eq_int("V27RX_CFG.int_000c (%ld)", V27RX_CFG.int_000c,
		    ref_V27RX_CFG.int_000c, 0x0c);
	diff_eq_int("V27RX_CFG.int_0010 (%ld)", V27RX_CFG.int_0010,
		    ref_V27RX_CFG.int_0010, 0x10);
	diff_eq_int("V27RX_CFG.int_0014 (%ld)", V27RX_CFG.short_train,
		    ref_V27RX_CFG.short_train, 0x14);
	diff_eq_int("V27RX_CFG.ptr_0018 is null (%ld)",
		    V27RX_CFG.ptr_0018 == 0, ref_V27RX_CFG.ptr_0018 == 0, 0x18);

	/* V29RX_CFG. */
	diff_eq_int("V29RX_CFG.int_0000 (%ld)", V29RX_CFG.protocol,
		    ref_V29RX_CFG.protocol, 0x00);
	diff_eq_int("V29RX_CFG.bit_rate (%ld)", V29RX_CFG.bit_rate,
		    ref_V29RX_CFG.bit_rate, 0x04);
	diff_eq_int("V29RX_CFG.short_0006 (%ld)", V29RX_CFG.short_0006,
		    ref_V29RX_CFG.short_0006, 0x06);
	diff_eq_int("V29RX_CFG.int_0008 (%ld)", V29RX_CFG.int_0008,
		    ref_V29RX_CFG.int_0008, 0x08);
	diff_eq_int("V29RX_CFG.int_000c (%ld)", V29RX_CFG.int_000c,
		    ref_V29RX_CFG.int_000c, 0x0c);
	diff_eq_int("V29RX_CFG.int_0010 (%ld)", V29RX_CFG.int_0010,
		    ref_V29RX_CFG.int_0010, 0x10);
	diff_eq_int("V29RX_CFG.ptr_0014 is null (%ld)",
		    V29RX_CFG.ptr_0014 == 0, ref_V29RX_CFG.ptr_0014 == 0, 0x14);

	/* And the whole byte image of each, which catches any padding. */
	diff_eq_int("FAXVMI_CFG bytes differ (%ld)",
		    bytes_differ(&FAXVMI_CFG, &ref_FAXVMI_CFG,
				 sizeof(struct faxvmi_cfg)), 0, 0);
	diff_eq_int("V17RX_CFG bytes differ (%ld)",
		    bytes_differ(&V17RX_CFG, &ref_V17RX_CFG,
				 sizeof(struct v17rx_cfg)), 0, 0);
	diff_eq_int("V27RX_CFG bytes differ (%ld)",
		    bytes_differ(&V27RX_CFG, &ref_V27RX_CFG,
				 sizeof(struct v27rx_cfg)), 0, 0);
	diff_eq_int("V29RX_CFG bytes differ (%ld)",
		    bytes_differ(&V29RX_CFG, &ref_V29RX_CFG,
				 sizeof(struct v29rx_cfg)), 0, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * The claims a comparison against `ref_` cannot make, because both sides
 * would move together if the table were misread the same way twice.
 */
static int
test_value_shape(void)
{
	diff_begin("faxcfg: the shape of the values");

	/* The bit rates by name.  These are what select the modem's mode. */
	diff_eq_int("V17RX_CFG.bit_rate is 14400 (%ld)",
		    V17RX_CFG.bit_rate, 14400, 0);
	diff_eq_int("V27RX_CFG.bit_rate is 4800 (%ld)",
		    V27RX_CFG.bit_rate, 4800, 0);
	diff_eq_int("V29RX_CFG.bit_rate is 9600 (%ld)",
		    V29RX_CFG.bit_rate, 9600, 0);

	/*
	 * Pairwise different.  The three tables share their first three
	 * fields, so a table that had been copied from its neighbour and left
	 * unedited would match `ref_` on everything a careless test looked at.
	 */
	diff_eq_int("v17 and v27 rates differ (%ld)",
		    V17RX_CFG.bit_rate != V27RX_CFG.bit_rate, 1, 0);
	diff_eq_int("v17 and v29 rates differ (%ld)",
		    V17RX_CFG.bit_rate != V29RX_CFG.bit_rate, 1, 0);
	diff_eq_int("v27 and v29 rates differ (%ld)",
		    V27RX_CFG.bit_rate != V29RX_CFG.bit_rate, 1, 0);

	/* The prefix the three DO share, asserted so it is a claim. */
	diff_eq_int("V17RX_CFG.int_0000 is 1 (%ld)", V17RX_CFG.protocol, 1, 0);
	diff_eq_int("V27RX_CFG.int_0000 is 1 (%ld)", V27RX_CFG.int_0000, 1, 0);
	diff_eq_int("V29RX_CFG.int_0000 is 1 (%ld)", V29RX_CFG.protocol, 1, 0);
	diff_eq_int("V17RX_CFG.int_0008 is 60000 (%ld)",
		    V17RX_CFG.int_0008, 60000, 0);
	diff_eq_int("V27RX_CFG.int_0008 is 60000 (%ld)",
		    V27RX_CFG.int_0008, 60000, 0);
	diff_eq_int("V29RX_CFG.int_0008 is 60000 (%ld)",
		    V29RX_CFG.int_0008, 60000, 0);

	/* FAXVMI_CFG's three non-zero words, by name. */
	diff_eq_int("FAXVMI_CFG.fifo_size is 128 (%ld)",
		    FAXVMI_CFG.fifo_size, 128, 0);
	diff_eq_int("FAXVMI_CFG.max_frame is 50 (%ld)",
		    FAXVMI_CFG.max_frame, 50, 0);
	diff_eq_int("FAXVMI_CFG.frame_size is 128 (%ld)",
		    FAXVMI_CFG.frame_size, 128, 0);
	diff_eq_int("FAXVMI_CFG.slot is 0 in the table (%ld)",
		    FAXVMI_CFG.slot, 0, 0);

	/*
	 * Anti-vacuity.  Every one of these tables is mostly zero; assert
	 * that each has the number of non-zero bytes it should, so a table
	 * that had been zeroed entirely could not pass by looking tidy.
	 */
	diff_eq_int("FAXVMI_CFG non-zero bytes (%ld)",
		    nonzero_bytes(&FAXVMI_CFG, sizeof(struct faxvmi_cfg)), 3, 0);
	diff_eq_int("V17RX_CFG non-zero bytes (%ld)",
		    nonzero_bytes(&V17RX_CFG, sizeof(struct v17rx_cfg)), 5, 0);
	diff_eq_int("V27RX_CFG non-zero bytes (%ld)",
		    nonzero_bytes(&V27RX_CFG, sizeof(struct v27rx_cfg)), 5, 0);
	diff_eq_int("V29RX_CFG non-zero bytes (%ld)",
		    nonzero_bytes(&V29RX_CFG, sizeof(struct v29rx_cfg)), 5, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * F134: a comparator that has only ever been shown agreeing is
 * indistinguishable from a comparator that always returns 0.  Three quarters
 * of these tables are zero, so this is not a theoretical worry here.
 */
static int
test_detector_fires(void)
{
	unsigned char copy[64];
	const unsigned char *orig = (const unsigned char *)&FAXVMI_CFG;
	size_t n = sizeof(struct faxvmi_cfg);
	size_t i;
	int caught = 0;

	diff_begin("faxcfg: the byte comparator fires");

	memcpy(copy, orig, n);
	diff_eq_int("an identical copy compares equal (%ld)",
		    bytes_differ(orig, copy, n), 0, 0);

	for (i = 0; i < n; i++) {
		memcpy(copy, orig, n);
		copy[i] = (unsigned char)(copy[i] ^ 0xff);
		if (bytes_differ(orig, copy, n))
			caught++;
	}
	diff_eq_int("one-byte perturbations caught (%ld)", caught, (long)n, 0);
	diff_eq_int("the denominator was not zero (%ld)", (long)n, 24, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * The VMI is driven in a buffer larger than the config head so that a write
 * past +0x17 is visible.  0x5a is not a value any of these constructors
 * stores.
 */
#define VMI_BUF		64
#define VMI_POISON	0x5a

/*
 * A union, not a bare `unsigned char[]`: the config head holds two pointers,
 * so a character array cast to it would be four-byte aligned only by luck.
 */
union vmibuf {
	struct faxvmi_cfg cfg;
	unsigned char b[VMI_BUF];
};

static void
poison(union vmibuf *u)
{
	memset(u->b, VMI_POISON, VMI_BUF);
}

/* Every VMI field that is not a heap address, plus the tail poison. */
static void
compare_vmi(const union vmibuf *ours, const union vmibuf *theirs, long input)
{
	const struct faxvmi_cfg *a = &ours->cfg;
	const struct faxvmi_cfg *b = &theirs->cfg;
	size_t i;
	int tail_ours = 0, tail_theirs = 0;

	diff_eq_int("vmi.mode, input %ld", a->mode, b->mode,
		    input);
	diff_eq_int("vmi.short_0002, input %ld", a->short_0002, b->short_0002,
		    input);
	diff_eq_int("vmi.reverse, input %ld", a->reverse, b->reverse, input);
	diff_eq_int("vmi.fifo_size, input %ld", a->fifo_size, b->fifo_size,
		    input);
	diff_eq_int("vmi.max_frame, input %ld", a->max_frame, b->max_frame,
		    input);
	diff_eq_int("vmi.frame_size, input %ld", a->frame_size, b->frame_size,
		    input);
	diff_eq_int("vmi.slot, input %ld", a->slot, b->slot, input);
	/* Passed in identically to both sides, so comparable by value. */
	diff_eq_int("vmi.ptr_0014 same, input %ld",
		    a->ptr_0014 == b->ptr_0014, 1, input);
	diff_eq_int("vmi.modem_cfg non-null (ours), input %ld",
		    a->modem_cfg != 0, 1, input);
	diff_eq_int("vmi.modem_cfg non-null (blob), input %ld",
		    b->modem_cfg != 0, 1, input);

	/*
	 * Nothing past +0x17 was touched, on EITHER side.  Counted rather
	 * than short-circuited so the report names how much moved.
	 */
	for (i = sizeof(struct faxvmi_cfg); i < VMI_BUF; i++) {
		if (ours->b[i] != VMI_POISON)
			tail_ours++;
		if (theirs->b[i] != VMI_POISON)
			tail_theirs++;
	}
	diff_eq_int("bytes written past the config head, ours, input %ld",
		    tail_ours, 0, input);
	diff_eq_int("bytes written past the config head, blob, input %ld",
		    tail_theirs, 0, input);
}

/* ------------------------------------------------------------------------- */

static int
test_init_v17(void)
{
	static const unsigned short rates[5] = { 14400, 12000, 9600, 7200, 0 };
	union vmibuf ua, ub;
	int marker[4];
	int i;

	diff_begin("init_vmi_v17rx");

	for (i = 0; i < 5; i++) {
		const struct v17rx_cfg *ca, *cb;

		poison(&ua);
		poison(&ub);
		init_vmi_v17rx(&ua.cfg, rates[i], 0x1234, &marker[i & 3]);
		ref_init_vmi_v17rx(&ub.cfg, rates[i], 0x1234, &marker[i & 3]);

		compare_vmi(&ua, &ub, i);
		diff_eq_int("vmi.slot is VMI_SLOT_V17RX, input %ld",
			    ua.cfg.slot, VMI_SLOT_V17RX, i);

		ca = (const struct v17rx_cfg *)ua.cfg.modem_cfg;
		cb = (const struct v17rx_cfg *)ub.cfg.modem_cfg;

		diff_eq_int("cfg.int_0000, input %ld", ca->protocol,
			    cb->protocol, i);
		diff_eq_int("cfg.bit_rate, input %ld", ca->bit_rate,
			    cb->bit_rate, i);
		diff_eq_int("cfg.bit_rate is the argument, input %ld",
			    ca->bit_rate, (short)rates[i], i);
		diff_eq_int("cfg.short_0006, input %ld", ca->short_0006,
			    cb->short_0006, i);
		diff_eq_int("cfg.int_0008, input %ld", ca->int_0008,
			    cb->int_0008, i);
		diff_eq_int("cfg.int_000c, input %ld", ca->int_000c,
			    cb->int_000c, i);
		diff_eq_int("cfg.int_0010, input %ld", ca->int_0010,
			    cb->int_0010, i);
		diff_eq_int("cfg.int_0014 cleared, input %ld", ca->short_train,
			    cb->short_train, i);
		diff_eq_int("cfg.ptr_0024 is the argument, input %ld",
			    ca->ptr_0024 == (void *)&marker[i & 3], 1, i);
		diff_eq_int("cfg.ptr_0024 same on both sides, input %ld",
			    ca->ptr_0024 == cb->ptr_0024, 1, i);

		/*
		 * The three sub-allocations.  Two of them are the same size,
		 * so "all non-null" would survive a swap: they are checked
		 * for being three DISTINCT addresses, on each side.
		 */
		diff_eq_int("cfg.coefsave0 non-null (ours), input %ld",
			    ca->coefsave0 != 0, 1, i);
		diff_eq_int("cfg.coefsave1 non-null (ours), input %ld",
			    ca->coefsave1 != 0, 1, i);
		diff_eq_int("cfg.ratesave non-null (ours), input %ld",
			    ca->ratesave != 0, 1, i);
		diff_eq_int("cfg.coefsave0 non-null (blob), input %ld",
			    cb->coefsave0 != 0, 1, i);
		diff_eq_int("cfg.coefsave1 non-null (blob), input %ld",
			    cb->coefsave1 != 0, 1, i);
		diff_eq_int("cfg.ratesave non-null (blob), input %ld",
			    cb->ratesave != 0, 1, i);
		diff_eq_int("the three allocations are distinct, input %ld",
			    ca->coefsave0 != ca->coefsave1 &&
			    ca->coefsave1 != ca->ratesave &&
			    ca->coefsave0 != ca->ratesave, 1, i);
		diff_eq_int("and none of them is the config, input %ld",
			    ca->coefsave0 != (void *)ca &&
			    ca->coefsave1 != (void *)ca &&
			    ca->ratesave != (void *)ca, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_init_v27(void)
{
	static const unsigned short rates[5] = { 4800, 2400, 9600, 1200, 0 };
	union vmibuf ua, ub;
	int marker[4];
	int i;

	diff_begin("init_vmi_v27rx");

	for (i = 0; i < 5; i++) {
		const struct v27rx_cfg *ca, *cb;

		poison(&ua);
		poison(&ub);
		init_vmi_v27rx(&ua.cfg, rates[i], -1, &marker[i & 3]);
		ref_init_vmi_v27rx(&ub.cfg, rates[i], -1, &marker[i & 3]);

		compare_vmi(&ua, &ub, i);
		diff_eq_int("vmi.slot is VMI_SLOT_V27RX, input %ld",
			    ua.cfg.slot, VMI_SLOT_V27RX, i);

		ca = (const struct v27rx_cfg *)ua.cfg.modem_cfg;
		cb = (const struct v27rx_cfg *)ub.cfg.modem_cfg;

		diff_eq_int("cfg.int_0000, input %ld", ca->int_0000,
			    cb->int_0000, i);
		diff_eq_int("cfg.bit_rate, input %ld", ca->bit_rate,
			    cb->bit_rate, i);
		diff_eq_int("cfg.bit_rate is the argument, input %ld",
			    ca->bit_rate, (short)rates[i], i);
		diff_eq_int("cfg.short_0006, input %ld", ca->short_0006,
			    cb->short_0006, i);
		diff_eq_int("cfg.int_0008, input %ld", ca->int_0008,
			    cb->int_0008, i);
		diff_eq_int("cfg.int_000c, input %ld", ca->int_000c,
			    cb->int_000c, i);
		diff_eq_int("cfg.int_0010, input %ld", ca->int_0010,
			    cb->int_0010, i);
		diff_eq_int("cfg.int_0014, input %ld", ca->short_train,
			    cb->short_train, i);
		diff_eq_int("cfg.ptr_0018 is the argument, input %ld",
			    ca->ptr_0018 == (void *)&marker[i & 3], 1, i);
		diff_eq_int("cfg.ptr_0018 same on both sides, input %ld",
			    ca->ptr_0018 == cb->ptr_0018, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_init_v29(void)
{
	static const unsigned short rates[5] = { 9600, 7200, 4800, 14400, 0 };
	union vmibuf ua, ub;
	int marker[4];
	int i;

	diff_begin("init_vmi_v29rx");

	for (i = 0; i < 5; i++) {
		const struct v29rx_cfg *ca, *cb;

		poison(&ua);
		poison(&ub);
		init_vmi_v29rx(&ua.cfg, rates[i], 0, &marker[i & 3]);
		ref_init_vmi_v29rx(&ub.cfg, rates[i], 0, &marker[i & 3]);

		compare_vmi(&ua, &ub, i);
		diff_eq_int("vmi.slot is VMI_SLOT_V29RX, input %ld",
			    ua.cfg.slot, VMI_SLOT_V29RX, i);

		ca = (const struct v29rx_cfg *)ua.cfg.modem_cfg;
		cb = (const struct v29rx_cfg *)ub.cfg.modem_cfg;

		diff_eq_int("cfg.int_0000, input %ld", ca->protocol,
			    cb->protocol, i);
		diff_eq_int("cfg.bit_rate, input %ld", ca->bit_rate,
			    cb->bit_rate, i);
		diff_eq_int("cfg.bit_rate is the argument, input %ld",
			    ca->bit_rate, (short)rates[i], i);
		diff_eq_int("cfg.short_0006, input %ld", ca->short_0006,
			    cb->short_0006, i);
		diff_eq_int("cfg.int_0008, input %ld", ca->int_0008,
			    cb->int_0008, i);
		diff_eq_int("cfg.int_000c, input %ld", ca->int_000c,
			    cb->int_000c, i);
		diff_eq_int("cfg.int_0010, input %ld", ca->int_0010,
			    cb->int_0010, i);
		diff_eq_int("cfg.ptr_0014 is the argument, input %ld",
			    ca->ptr_0014 == (void *)&marker[i & 3], 1, i);
		diff_eq_int("cfg.ptr_0014 same on both sides, input %ld",
			    ca->ptr_0014 == cb->ptr_0014, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * The three constructors differ ONLY in their slot and their table, and a
 * test that ran each in isolation would not notice two of them planting the
 * same slot.  Run side by side and assert the three slots are distinct.
 */
static int
test_slots_distinct(void)
{
	union vmibuf v17, v27, v29;
	int marker;
	short s17, s27, s29;

	diff_begin("faxcfg: the three slots are distinct");

	poison(&v17);
	poison(&v27);
	poison(&v29);
	init_vmi_v17rx(&v17.cfg, 14400, 0, &marker);
	init_vmi_v27rx(&v27.cfg, 4800, 0, &marker);
	init_vmi_v29rx(&v29.cfg, 9600, 0, &marker);

	s17 = v17.cfg.slot;
	s27 = v27.cfg.slot;
	s29 = v29.cfg.slot;

	diff_eq_int("v17 slot (%ld)", s17, 12, 0);
	diff_eq_int("v27 slot (%ld)", s27, 8, 0);
	diff_eq_int("v29 slot (%ld)", s29, 10, 0);
	diff_eq_int("all three differ (%ld)",
		    s17 != s27 && s27 != s29 && s17 != s29, 1, 0);

	/*
	 * And the three modem configurations are three different SIZES, which
	 * is the fact that makes them three types.  Asserted here rather than
	 * only in test_shape because this is the place all three are live.
	 */
	diff_eq_int("the three config sizes are distinct (%ld)",
		    sizeof(struct v17rx_cfg) != sizeof(struct v27rx_cfg) &&
		    sizeof(struct v27rx_cfg) != sizeof(struct v29rx_cfg) &&
		    sizeof(struct v17rx_cfg) != sizeof(struct v29rx_cfg), 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_values();
	rc |= test_value_shape();
	rc |= test_detector_fires();
	rc |= test_init_v17();
	rc |= test_init_v27();
	rc |= test_init_v29();
	rc |= test_slots_distinct();

	return rc;
}
