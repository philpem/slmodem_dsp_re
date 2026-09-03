/*
 * t_class1txvmi.c -- differential test of the transmit-side VMI
 * constructors `init_vmi_v17tx`, `init_vmi_v27tx`, `init_vmi_v29tx`
 * (class1tx.c, `.text` 0x094870/0x094a70/0x094970).
 *
 * SAME METHOD AS `t_faxcfg.c`'s RX trio: poison a VMI buffer larger than
 * the config head so a write past +0x17 is visible, compare every
 * non-heap field by value, compare the built config struct field by field
 * against `ref_`, and check the RETURN VALUE -- these three, unlike their
 * RX siblings, reload `cfg->bitrate` into `%eax` right before `ret`
 * (finding F10054), so the object returns it and a `void` reconstruction
 * would not even compile against this test.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/class1tx.h"
#include "dsplib/faxcfg.h"
#include "dsplib/v17fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29data.h"

extern int ref_init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);
extern int ref_init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);
extern int ref_init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);

#define VMI_BUF		64
#define VMI_POISON	0x5a

union vmibuf {
	struct faxvmi_cfg cfg;
	unsigned char b[VMI_BUF];
};

static void
poison(union vmibuf *u)
{
	memset(u->b, VMI_POISON, VMI_BUF);
}

static void
compare_vmi(const union vmibuf *ours, const union vmibuf *theirs, long input)
{
	const struct faxvmi_cfg *a = &ours->cfg;
	const struct faxvmi_cfg *b = &theirs->cfg;
	size_t i;
	int tail_ours = 0, tail_theirs = 0;

	diff_eq_int("vmi.short_0000, input %ld", a->short_0000, b->short_0000,
		    input);
	diff_eq_int("vmi.short_0002, input %ld", a->short_0002, b->short_0002,
		    input);
	diff_eq_int("vmi.int_0004, input %ld", a->int_0004, b->int_0004, input);
	diff_eq_int("vmi.short_0008, input %ld", a->short_0008, b->short_0008,
		    input);
	diff_eq_int("vmi.short_000a, input %ld", a->short_000a, b->short_000a,
		    input);
	diff_eq_int("vmi.short_000c, input %ld", a->short_000c, b->short_000c,
		    input);
	diff_eq_int("vmi.slot, input %ld", a->slot, b->slot, input);
	diff_eq_int("vmi.ptr_0014 same, input %ld",
		    a->ptr_0014 == b->ptr_0014, 1, input);
	diff_eq_int("vmi.modem_cfg non-null (ours), input %ld",
		    a->modem_cfg != 0, 1, input);
	diff_eq_int("vmi.modem_cfg non-null (blob), input %ld",
		    b->modem_cfg != 0, 1, input);

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

/* ------------------------------------------------------------------- */

static int
test_init_v17(void)
{
	static const unsigned short rates[5] = { 14400, 12000, 9600, 7200, 0 };
	union vmibuf ua, ub;
	int marker[4];
	int i;

	diff_begin("init_vmi_v17tx");

	for (i = 0; i < 5; i++) {
		const struct v17tx_cfg *ca, *cb;
		int ra, rb;

		poison(&ua);
		poison(&ub);
		ra = init_vmi_v17tx(&ua.cfg, rates[i], 0x1234, &marker[i & 3]);
		rb = ref_init_vmi_v17tx(&ub.cfg, rates[i], 0x1234,
					&marker[i & 3]);

		compare_vmi(&ua, &ub, i);
		diff_eq_int("vmi.slot is VMI_SLOT_V17TX, input %ld",
			    ua.cfg.slot, VMI_SLOT_V17TX, i);
		diff_eq_int("return value, input %ld", ra, rb, i);
		diff_eq_int("return value is the argument, input %ld",
			    ra, (short)rates[i], i);

		ca = (const struct v17tx_cfg *)ua.cfg.modem_cfg;
		cb = (const struct v17tx_cfg *)ub.cfg.modem_cfg;

		diff_eq_int("cfg.protocol, input %ld", ca->protocol,
			    cb->protocol, i);
		diff_eq_int("cfg.bitrate, input %ld", ca->bitrate,
			    cb->bitrate, i);
		diff_eq_int("cfg.bitrate is the argument, input %ld",
			    ca->bitrate, (short)rates[i], i);
		diff_eq_int("cfg.short_0004, input %ld", ca->short_0004,
			    cb->short_0004, i);
		diff_eq_int("cfg.short_0006, input %ld", ca->short_0006,
			    cb->short_0006, i);
		diff_eq_int("cfg.int_0008, input %ld", ca->int_0008,
			    cb->int_0008, i);
		diff_eq_int("cfg.int_000c, input %ld", ca->int_000c,
			    cb->int_000c, i);
		diff_eq_int("cfg.int_0010, input %ld", ca->int_0010,
			    cb->int_0010, i);
		diff_eq_int("cfg.int_0014, input %ld", ca->int_0014,
			    cb->int_0014, i);
		diff_eq_int("cfg.int_0018 is 0, input %ld", ca->int_0018, 0,
			    i);
		diff_eq_int("cfg.int_0018, input %ld", ca->int_0018,
			    cb->int_0018, i);
		diff_eq_int("cfg.int_001c is the argument, input %ld",
			    ca->int_001c == (int)(long)&marker[i & 3], 1, i);
		diff_eq_int("cfg.int_001c same on both sides, input %ld",
			    ca->int_001c == cb->int_001c, 1, i);
	}

	return diff_end();
}

static int
test_init_v27(void)
{
	static const unsigned short rates[5] = { 4800, 2400, 9600, 1200, 0 };
	union vmibuf ua, ub;
	int marker[4];
	int i;

	diff_begin("init_vmi_v27tx");

	for (i = 0; i < 5; i++) {
		const struct v27tx_cfg *ca, *cb;
		int ra, rb;

		poison(&ua);
		poison(&ub);
		ra = init_vmi_v27tx(&ua.cfg, rates[i], -1, &marker[i & 3]);
		rb = ref_init_vmi_v27tx(&ub.cfg, rates[i], -1, &marker[i & 3]);

		compare_vmi(&ua, &ub, i);
		diff_eq_int("vmi.slot is VMI_SLOT_V27TX, input %ld",
			    ua.cfg.slot, VMI_SLOT_V27TX, i);
		diff_eq_int("return value, input %ld", ra, rb, i);
		diff_eq_int("return value is the argument, input %ld",
			    ra, (short)rates[i], i);

		ca = (const struct v27tx_cfg *)ua.cfg.modem_cfg;
		cb = (const struct v27tx_cfg *)ub.cfg.modem_cfg;

		diff_eq_int("cfg.protocol, input %ld", ca->protocol,
			    cb->protocol, i);
		diff_eq_int("cfg.bitrate, input %ld", ca->bitrate,
			    cb->bitrate, i);
		diff_eq_int("cfg.bitrate is the argument, input %ld",
			    ca->bitrate, (short)rates[i], i);
		diff_eq_int("cfg.int_0004, input %ld", ca->int_0004,
			    cb->int_0004, i);
		diff_eq_int("cfg.int_0008, input %ld", ca->int_0008,
			    cb->int_0008, i);
		diff_eq_int("cfg.int_000c, input %ld", ca->int_000c,
			    cb->int_000c, i);
		diff_eq_int("cfg.flags_0010, input %ld", ca->flags_0010,
			    cb->flags_0010, i);
		diff_eq_int("cfg.short_0012, input %ld", ca->short_0012,
			    cb->short_0012, i);
		diff_eq_int("cfg.int_0014, input %ld", ca->int_0014,
			    cb->int_0014, i);
		diff_eq_int("cfg.int_0018, input %ld", ca->int_0018,
			    cb->int_0018, i);
		diff_eq_int("cfg.int_001c is the argument, input %ld",
			    ca->int_001c == (int)(long)&marker[i & 3], 1, i);
		diff_eq_int("cfg.int_001c same on both sides, input %ld",
			    ca->int_001c == cb->int_001c, 1, i);
	}

	return diff_end();
}

static int
test_init_v29(void)
{
	static const unsigned short rates[5] = { 9600, 7200, 4800, 14400, 0 };
	union vmibuf ua, ub;
	int marker[4];
	int i;

	diff_begin("init_vmi_v29tx");

	for (i = 0; i < 5; i++) {
		const struct v29tx_cfg *ca, *cb;
		int ra, rb;

		poison(&ua);
		poison(&ub);
		ra = init_vmi_v29tx(&ua.cfg, rates[i], 0, &marker[i & 3]);
		rb = ref_init_vmi_v29tx(&ub.cfg, rates[i], 0, &marker[i & 3]);

		compare_vmi(&ua, &ub, i);
		diff_eq_int("vmi.slot is VMI_SLOT_V29TX, input %ld",
			    ua.cfg.slot, VMI_SLOT_V29TX, i);
		diff_eq_int("return value, input %ld", ra, rb, i);
		diff_eq_int("return value is the argument, input %ld",
			    ra, (short)rates[i], i);

		ca = (const struct v29tx_cfg *)ua.cfg.modem_cfg;
		cb = (const struct v29tx_cfg *)ub.cfg.modem_cfg;

		diff_eq_int("cfg.protocol, input %ld", ca->protocol,
			    cb->protocol, i);
		diff_eq_int("cfg.bitrate, input %ld", ca->bitrate,
			    cb->bitrate, i);
		diff_eq_int("cfg.bitrate is the argument, input %ld",
			    ca->bitrate, (short)rates[i], i);
		diff_eq_int("cfg.short_0004, input %ld", ca->short_0004,
			    cb->short_0004, i);
		diff_eq_int("cfg.short_0006, input %ld", ca->short_0006,
			    cb->short_0006, i);
		diff_eq_int("cfg.int_0008, input %ld", ca->int_0008,
			    cb->int_0008, i);
		diff_eq_int("cfg.int_000c, input %ld", ca->int_000c,
			    cb->int_000c, i);
		diff_eq_int("cfg.flags_10, input %ld", ca->flags_10,
			    cb->flags_10, i);
		diff_eq_int("cfg.short_0012, input %ld", ca->short_0012,
			    cb->short_0012, i);
		diff_eq_int("cfg.int_0014, input %ld", ca->int_0014,
			    cb->int_0014, i);
		diff_eq_int("cfg.int_0018 is the argument, input %ld",
			    ca->int_0018 == (int)(long)&marker[i & 3], 1, i);
		diff_eq_int("cfg.int_0018 same on both sides, input %ld",
			    ca->int_0018 == cb->int_0018, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------- */

/*
 * The three constructors differ only in slot and table; run side by side
 * and assert the three slots are distinct, matching `t_faxcfg.c`'s own
 * `test_slots_distinct` on the RX side.
 */
static int
test_slots_distinct(void)
{
	union vmibuf v17, v27, v29;
	int marker;
	short s17, s27, s29;

	diff_begin("class1txvmi: the three slots are distinct");

	poison(&v17);
	poison(&v27);
	poison(&v29);
	init_vmi_v17tx(&v17.cfg, 14400, 0, &marker);
	init_vmi_v27tx(&v27.cfg, 4800, 0, &marker);
	init_vmi_v29tx(&v29.cfg, 9600, 0, &marker);

	s17 = v17.cfg.slot;
	s27 = v27.cfg.slot;
	s29 = v29.cfg.slot;

	diff_eq_int("v17 slot (%ld)", s17, 11, 0);
	diff_eq_int("v27 slot (%ld)", s27, 7, 0);
	diff_eq_int("v29 slot (%ld)", s29, 9, 0);
	diff_eq_int("all three differ (%ld)",
		    s17 != s27 && s27 != s29 && s17 != s29, 1, 0);

	/*
	 * UNLIKE THE RX TRIO, V.17's and V.27ter's configs are the SAME size
	 * (0x20 -- v17fax.h and v27fax.h both say so): only V.29's (0x1c) is
	 * shorter, one dword, per v27fax.h's own comment on the shape.  So
	 * this asserts the one inequality that holds rather than all three.
	 */
	diff_eq_int("v17tx_cfg and v27tx_cfg are the same size (%ld)",
		    sizeof(struct v17tx_cfg) == sizeof(struct v27tx_cfg), 1, 0);
	diff_eq_int("v29tx_cfg is one dword shorter (%ld)",
		    sizeof(struct v17tx_cfg) - sizeof(struct v29tx_cfg), 4, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_init_v17();
	rc |= test_init_v27();
	rc |= test_init_v29();
	rc |= test_slots_distinct();

	return rc;
}
