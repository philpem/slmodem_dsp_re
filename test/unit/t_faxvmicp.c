/*
 * t_faxvmicp.c -- differential test of FAXVMI_create, FAXVMI_process,
 * FAXVMI_control, and the vxx_create/vxx_process/vxx_control dispatch
 * tables they complete.  See faxvmi.h for what each is; this file's own job
 * is the orchestration -- allocation, (re)initialisation, the
 * pack/process/unpack/reverse pipeline, and the six control effects -- not
 * the per-modulation adapters or the framing functions, which are tested
 * elsewhere (t_faxadapt.c, t_nulldp.c, t_faxpack.c, t_faxframing.c,
 * t_faxunframe.c).
 *
 * WHY THE NON-NULL SLOTS ARE NOT INVOKED, same reasoning as t_faxvmids.c:
 * `vxx_create[5..12]`/`vxx_process[5..12]`/`vxx_control[5..12]` wrap real
 * per-modulation constructors/processors/controllers that need a
 * fully-formed modem instance this file has no route to build safely.
 * Wiring is proven by pointer identity (mirroring t_faxvmids.c's
 * `run_tables`); CALLS are made only through the five NULL slots, which are
 * already proven safe (t_nulldp.c) and behave identically at every slot
 * value 0..4.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1tx.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"
#include "dsplib/sysdep.h"

extern struct faxvmi *ref_FAXVMI_create(struct faxvmi *vmi,
					const struct faxvmi_cfg *cfg);
extern int ref_FAXVMI_process(struct faxvmi *vmi, unsigned short *data,
			      short *pcm, short *count,
			      unsigned short *result);
extern int ref_FAXVMI_control(struct faxvmi *vmi,
			      const struct faxvmi_ctl *ctl);
extern void *ref_vxx_create[13];
extern void *ref_vxx_process[13];
extern void *ref_vxx_control[13];
extern const unsigned char ref_FAXVMI_CTL[24];

extern void ref_null_create(void);
extern void ref_v21tx_create(void), ref_v21rx_create(void);
extern void ref_v27tx_create(void), ref_v27rx_create(void);
extern void ref_v29tx_create(void), ref_v29rx_create(void);
extern void ref_v17tx_create(void), ref_v17rx_create(void);

extern void ref_null_process(void);
extern void ref_v21tx_process(void), ref_v21rx_process(void);
extern void ref_v27tx_process(void), ref_v27rx_process(void);
extern void ref_v29tx_process(void), ref_v29rx_process(void);
extern void ref_v17tx_process(void), ref_v17rx_process(void);

extern void ref_null_control(void);
extern void ref_v21tx_control(void), ref_v21rx_control(void);
extern void ref_v27tx_control(void), ref_v27rx_control(void);
extern void ref_v29tx_control(void), ref_v29rx_control(void);
extern void ref_v17tx_control(void), ref_v17rx_control(void);

static unsigned long seed = 20260903UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

/*
 * `vxx_create`/`vxx_process` wiring, same technique t_faxvmids.c uses for
 * `vxx_delete`/`vxx_status`: pointer identity, both sides.  `vxx_process`'s
 * eight typed entries all need the cast for the same reason `vxx_status`'s
 * four do (faxvmi.h).
 */
static int
run_tables(void)
{
	diff_begin("vxx_create / vxx_process wiring");

	diff_eq_int("vxx_create[0..4] (%ld)",
		    (long)(vxx_create[0] == null_create
			   && vxx_create[1] == null_create
			   && vxx_create[2] == null_create
			   && vxx_create[3] == null_create
			   && vxx_create[4] == null_create),
		    1, 0);
	diff_eq_int("vxx_create[5..12] (%ld)",
		    (long)(vxx_create[5] == (faxvmi_create_fn)v21tx_create
			   && vxx_create[6] == (faxvmi_create_fn)v21rx_create
			   && vxx_create[7] == (faxvmi_create_fn)v27tx_create
			   && vxx_create[8] == (faxvmi_create_fn)v27rx_create
			   && vxx_create[9] == (faxvmi_create_fn)v29tx_create
			   && vxx_create[10] == (faxvmi_create_fn)v29rx_create
			   && vxx_create[11] == (faxvmi_create_fn)v17tx_create
			   && vxx_create[12] == (faxvmi_create_fn)v17rx_create),
		    1, 0);

	diff_eq_int("vxx_process[0..4] (%ld)",
		    (long)(vxx_process[0] == null_process
			   && vxx_process[1] == null_process
			   && vxx_process[2] == null_process
			   && vxx_process[3] == null_process
			   && vxx_process[4] == null_process),
		    1, 0);
	diff_eq_int("vxx_process[5..12] (%ld)",
		    (long)(vxx_process[5] == (faxvmi_process_fn)v21tx_process
			   && vxx_process[6] == (faxvmi_process_fn)v21rx_process
			   && vxx_process[7] == (faxvmi_process_fn)v27tx_process
			   && vxx_process[8] == (faxvmi_process_fn)v27rx_process
			   && vxx_process[9] == (faxvmi_process_fn)v29tx_process
			   && vxx_process[10] == (faxvmi_process_fn)v29rx_process
			   && vxx_process[11] == (faxvmi_process_fn)v17tx_process
			   && vxx_process[12] == (faxvmi_process_fn)v17rx_process),
		    1, 0);

	diff_eq_int("ref_vxx_create[0..4] (%ld)",
		    (long)(ref_vxx_create[0] == (void *)ref_null_create
			   && ref_vxx_create[1] == (void *)ref_null_create
			   && ref_vxx_create[2] == (void *)ref_null_create
			   && ref_vxx_create[3] == (void *)ref_null_create
			   && ref_vxx_create[4] == (void *)ref_null_create),
		    1, 0);
	diff_eq_int("ref_vxx_create[5..12] (%ld)",
		    (long)(ref_vxx_create[5] == (void *)ref_v21tx_create
			   && ref_vxx_create[6] == (void *)ref_v21rx_create
			   && ref_vxx_create[7] == (void *)ref_v27tx_create
			   && ref_vxx_create[8] == (void *)ref_v27rx_create
			   && ref_vxx_create[9] == (void *)ref_v29tx_create
			   && ref_vxx_create[10] == (void *)ref_v29rx_create
			   && ref_vxx_create[11] == (void *)ref_v17tx_create
			   && ref_vxx_create[12] == (void *)ref_v17rx_create),
		    1, 0);

	diff_eq_int("ref_vxx_process[0..4] (%ld)",
		    (long)(ref_vxx_process[0] == (void *)ref_null_process
			   && ref_vxx_process[1] == (void *)ref_null_process
			   && ref_vxx_process[2] == (void *)ref_null_process
			   && ref_vxx_process[3] == (void *)ref_null_process
			   && ref_vxx_process[4] == (void *)ref_null_process),
		    1, 0);
	diff_eq_int("ref_vxx_process[5..12] (%ld)",
		    (long)(ref_vxx_process[5] == (void *)ref_v21tx_process
			   && ref_vxx_process[6] == (void *)ref_v21rx_process
			   && ref_vxx_process[7] == (void *)ref_v27tx_process
			   && ref_vxx_process[8] == (void *)ref_v27rx_process
			   && ref_vxx_process[9] == (void *)ref_v29tx_process
			   && ref_vxx_process[10] == (void *)ref_v29rx_process
			   && ref_vxx_process[11] == (void *)ref_v17tx_process
			   && ref_vxx_process[12] == (void *)ref_v17rx_process),
		    1, 0);

	diff_eq_int("vxx_create: null != v21tx (%ld)",
		    (long)(vxx_create[4] != vxx_create[5]), 1, 0);
	diff_eq_int("vxx_process: null != v21tx (%ld)",
		    (long)(vxx_process[4] != vxx_process[5]), 1, 0);

	diff_eq_int("vxx_control[0..4] (%ld)",
		    (long)(vxx_control[0] == null_control
			   && vxx_control[1] == null_control
			   && vxx_control[2] == null_control
			   && vxx_control[3] == null_control
			   && vxx_control[4] == null_control),
		    1, 0);
	diff_eq_int("vxx_control[5..12] (%ld)",
		    (long)(vxx_control[5] == (faxvmi_control_fn)v21tx_control
			   && vxx_control[6] == (faxvmi_control_fn)v21rx_control
			   && vxx_control[7] == (faxvmi_control_fn)v27tx_control
			   && vxx_control[8] == (faxvmi_control_fn)v27rx_control
			   && vxx_control[9] == (faxvmi_control_fn)v29tx_control
			   && vxx_control[10] == (faxvmi_control_fn)v29rx_control
			   && vxx_control[11] == (faxvmi_control_fn)v17tx_control
			   && vxx_control[12] == (faxvmi_control_fn)v17rx_control),
		    1, 0);

	diff_eq_int("ref_vxx_control[0..4] (%ld)",
		    (long)(ref_vxx_control[0] == (void *)ref_null_control
			   && ref_vxx_control[1] == (void *)ref_null_control
			   && ref_vxx_control[2] == (void *)ref_null_control
			   && ref_vxx_control[3] == (void *)ref_null_control
			   && ref_vxx_control[4] == (void *)ref_null_control),
		    1, 0);
	diff_eq_int("ref_vxx_control[5..12] (%ld)",
		    (long)(ref_vxx_control[5] == (void *)ref_v21tx_control
			   && ref_vxx_control[6] == (void *)ref_v21rx_control
			   && ref_vxx_control[7] == (void *)ref_v27tx_control
			   && ref_vxx_control[8] == (void *)ref_v27rx_control
			   && ref_vxx_control[9] == (void *)ref_v29tx_control
			   && ref_vxx_control[10] == (void *)ref_v29rx_control
			   && ref_vxx_control[11] == (void *)ref_v17tx_control
			   && ref_vxx_control[12] == (void *)ref_v17rx_control),
		    1, 0);

	diff_eq_int("vxx_control: null != v21tx (%ld)",
		    (long)(vxx_control[4] != vxx_control[5]), 1, 0);

	return diff_end();
}

/* got=ours (b), want=blob (a) -- the convention every other t_faxvmi*.c uses */
static void
cmp_vmi_scalars(const char *what, struct faxvmi *a, struct faxvmi *b,
		long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(mode);
	FLD(pad_0002);
	FLD(reverse);
	FLD(fifo_size);
	FLD(max_frame);
	FLD(frame_size);
	FLD(slot);
	FLD(int_0010);
	FLD(int_0014);
	FLD(underrun);
	FLD(overflow);
	FLD(status);
#undef FLD
}

static void
cmp_framer(const char *what, struct faxvmi_framer *a, struct faxvmi_framer *b,
	   long tag)
{
	struct faxvmi_framer ca = *a, cb = *b;

	ca.fifo = cb.fifo = NULL;
	ca.frame = cb.frame = NULL;
	diff_eq_obj(what, struct faxvmi_framer, &cb, &ca, tag);
}

static void
cmp_link(const char *what, struct faxvmi_link *a, struct faxvmi_link *b,
	 long tag)
{
	struct faxvmi_link ca = *a, cb = *b;

	ca.ptr_0000 = cb.ptr_0000 = NULL;
	ca.buf = cb.buf = NULL;
	diff_eq_obj(what, struct faxvmi_link, &cb, &ca, tag);
}

static void
cmp_u16(const char *label, unsigned short *ours, unsigned short *ref, int n,
	long tag)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(label, ours[i], ref[i], tag * 100000 + i);
}

#define NCFG 5

static const struct faxvmi_cfg create_cfgs[NCFG] = {
	{ 0, 0, 0, 32, 10, 20, 0, NULL, NULL },
	{ 1, 0, 1, 8, 40, 5, 1, NULL, NULL },
	{ 2, 0, 0, 100, 3, 30, 2, NULL, NULL },
	{ 0, 0, 1, 0, 0, 0, 3, NULL, NULL },
	{ 2, 0, 1, 5, 5, 60, 4, NULL, NULL },
};

static struct faxvmi *created_a[NCFG];
static struct faxvmi *created_b[NCFG];

/*
 * Fresh creates (`vmi == NULL`), one per row of `create_cfgs` plus one with
 * `cfg == NULL` (the `FAXVMI_CFG` default).  Every allocated block's CONTENT
 * is compared, not just the top-level struct -- the ring, the frame buffer
 * and both link buffers, all of which `FAXVMI_create` zeroes/fills before
 * returning.
 */
static int
run_create_fresh(void)
{
	unsigned c;

	diff_begin("FAXVMI_create: fresh (vmi == NULL)");

	for (c = 0; c < NCFG; c++) {
		struct faxvmi *va, *vb;
		long tag = (long)c;

		va = ref_FAXVMI_create(NULL, &create_cfgs[c]);
		vb = FAXVMI_create(NULL, &create_cfgs[c]);

		cmp_vmi_scalars("fresh", va, vb, tag);
		cmp_framer("fresh framer", va->framer, vb->framer, tag);
		cmp_link("fresh link", va->link, vb->link, tag);

		cmp_u16("fresh fifo[%ld]", vb->framer->fifo, va->framer->fifo,
			va->framer->fifo_size, tag);
		cmp_u16("fresh frame[%ld]", vb->framer->frame,
			va->framer->frame, va->framer->frame_size, tag);
		cmp_u16("fresh buf[%ld]", vb->link->buf, va->link->buf, 50,
			tag);
		cmp_u16("fresh ptr_0000[%ld]", vb->link->ptr_0000,
			va->link->ptr_0000, 200, tag);

		created_a[c] = va;
		created_b[c] = vb;
	}

	{
		struct faxvmi *va = ref_FAXVMI_create(NULL, NULL);
		struct faxvmi *vb = FAXVMI_create(NULL, NULL);

		cmp_vmi_scalars("cfg==NULL", va, vb, 100);
		cmp_framer("cfg==NULL framer", va->framer, vb->framer, 100);
		cmp_link("cfg==NULL link", va->link, vb->link, 100);
	}

	return diff_end();
}

/*
 * Reinit (`vmi != NULL`), reusing the instances `run_create_fresh` built.
 * The runtime state is scribbled to a non-zero pattern -- identically on
 * both sides -- before the reinit call, so this also proves the clear/refill
 * actually happens rather than finding the fields already at these values.
 * No new allocation is expected: `harness_alloc.allocs` must not move.
 */
static int
run_create_reinit(void)
{
	static const struct faxvmi_cfg cfg2 = { 1, 0, 1, 20, 4, 10, 3, NULL,
						 NULL };
	unsigned c;

	diff_begin("FAXVMI_create: reinit (vmi != NULL)");

	for (c = 0; c < NCFG; c++) {
		struct faxvmi *va = created_a[c], *vb = created_b[c];
		struct faxvmi *ra, *rb;
		long allocs_before;
		unsigned i;
		long tag = (long)c;

		va->framer->rd = vb->framer->rd = 7;
		va->framer->wr = vb->framer->wr = 3;
		va->framer->count = vb->framer->count = 2;
		va->framer->pack_bit = vb->framer->pack_bit = 5;
		for (i = 0; i < va->framer->fifo_size; i++)
			va->framer->fifo[i] = vb->framer->fifo[i] =
			    (unsigned short)(0x1000 + i);
		for (i = 0; i < va->framer->frame_size; i++)
			va->framer->frame[i] = vb->framer->frame[i] =
			    (unsigned short)(0x2000 + i);
		for (i = 0; i < 50; i++)
			va->link->buf[i] = vb->link->buf[i] =
			    (unsigned short)(0x3000 + i);
		for (i = 0; i < 200; i++)
			va->link->ptr_0000[i] = vb->link->ptr_0000[i] =
			    (unsigned short)(0x4000 + i);

		allocs_before = harness_alloc.allocs;
		ra = ref_FAXVMI_create(va, &cfg2);
		rb = FAXVMI_create(vb, &cfg2);

		diff_eq_int("reinit: no new allocation (%ld)",
			    harness_alloc.allocs, allocs_before, tag);
		diff_eq_int("reinit: returns the same pointer, ours (%ld)",
			    (long)(rb == vb), 1, tag);
		diff_eq_int("reinit: returns the same pointer, blob (%ld)",
			    (long)(ra == va), 1, tag);

		cmp_vmi_scalars("reinit", ra, rb, tag);
		cmp_framer("reinit framer", ra->framer, rb->framer, tag);
		cmp_link("reinit link", ra->link, rb->link, tag);

		cmp_u16("reinit fifo[%ld]", rb->framer->fifo, ra->framer->fifo,
			ra->framer->fifo_size, tag);
		cmp_u16("reinit frame[%ld]", rb->framer->frame,
			ra->framer->frame, ra->framer->frame_size, tag);
		cmp_u16("reinit buf[%ld]", rb->link->buf, ra->link->buf, 50,
			tag);
		cmp_u16("reinit ptr_0000[%ld]", rb->link->ptr_0000,
			ra->link->ptr_0000, 200, tag);
	}

	return diff_end();
}

#define DBUF 256
#define PBUF 64

/*
 * `FAXVMI_process` over the null slots, cycling every framing mode
 * (`FAXVMI_MODE_SIMP`/`ASYC`/`HDLC`) and both `reverse` settings.  `data` is
 * sized generously past `fifo_size`/`link->pack_count`: the HDLC unpacker's
 * own guard does not bound one call's OUTPUT by `max_frame` (faxvmi.h,
 * D956/F8607's shape), so a tight buffer here would be testing this file's
 * own overflow, not the object's behaviour.
 */
static int
run_process(void)
{
	static const struct faxvmi_cfg cfg = { 0, 0, 0, 64, 20, 64, 0, NULL,
						NULL };
	struct faxvmi *va, *vb;
	unsigned iter;

	diff_begin("FAXVMI_process");

	va = ref_FAXVMI_create(NULL, &cfg);
	vb = FAXVMI_create(NULL, &cfg);

	for (iter = 0; iter < 60; iter++) {
		unsigned short data_a[DBUF], data_b[DBUF];
		short pcm_a[PBUF], pcm_b[PBUF];
		short count_a, count_b;
		unsigned short result_a, result_b;
		int ra, rb;
		long tag = (long)iter;
		unsigned i;

		va->mode = vb->mode = (unsigned short)(iter % 3);
		va->reverse = vb->reverse = (int)(iter & 1);

		/*
		 * `data` is CLAMPED, not full-range: when `reverse` is set and
		 * `mode` is HDLC, `vmi_reverse[2]` is `faxvmi_frame_reverse`,
		 * which reads the FIRST element of every `count`-th run as a
		 * frame LENGTH and walks that many further elements
		 * (faxvmi.h).  A full 16-bit random value there is a real
		 * out-of-bounds walk on ANY buffer size and would be testing
		 * this file's own overflow, not the object's behaviour --
		 * exactly the same reasoning DBUF's own comment gives for the
		 * unpack side.  0..7 keeps every such length small while
		 * still exercising the bit engine, the ring and both `reverse`
		 * settings meaningfully.
		 */
		for (i = 0; i < DBUF; i++)
			data_a[i] = data_b[i] = (unsigned short)(rnd() & 0x7);
		for (i = 0; i < PBUF; i++)
			pcm_a[i] = pcm_b[i] = (short)(rnd() & 0xffff);

		count_a = count_b = (short)(iter % 12);
		result_a = result_b = (unsigned short)(iter % 8);

		ra = ref_FAXVMI_process(va, data_a, pcm_a, &count_a,
					&result_a);
		rb = FAXVMI_process(vb, data_b, pcm_b, &count_b, &result_b);

		diff_eq_int("process: return (%ld)", rb, ra, tag);
		diff_eq_int("process: vmi->status (%ld)", vb->status,
			    va->status, tag);
		diff_eq_int("process: *count (%ld)", count_b, count_a, tag);
		diff_eq_int("process: *result (%ld)", result_b, result_a,
			    tag);

		cmp_u16("process data[%ld]", data_b, data_a, DBUF, tag);
		for (i = 0; i < PBUF; i++)
			diff_eq_int("process pcm[%ld]", pcm_b[i], pcm_a[i],
				    tag * 100000 + i);

		cmp_framer("process framer", va->framer, vb->framer, tag);
		cmp_link("process link", va->link, vb->link, tag);
	}

	return diff_end();
}

/*
 * FAXVMI_control, over a fresh slot-0 (null) instance -- the same "only the
 * NULL slots are safe to actually invoke through" reasoning run_tables' own
 * comment gives for vxx_create/vxx_process, since a real modulation's
 * `int_0014` recursion target needs a fully-formed modem handle this file
 * has no route to build.  Six cases: ctl==NULL, the all-zero FAXVMI_CTL
 * (quiescent), ptr_0000-only (ring empty), int_000c+short_0010 in range
 * (full reset + mode change), int_000c+short_0010 OUT of range (treated as
 * quiescent for the reset, per faxvmi.h step 3), and int_0014 (recursion
 * through the null slot, ret == -1).  Every case also exercises the
 * unconditional zero_run_send/zero_run_bits copy from int_0004/short_0008.
 */
static int
run_control(void)
{
	static const struct faxvmi_cfg cfg = { 0, 0, 0, 32, 10, 20, 0, NULL,
						NULL };
	struct faxvmi *va, *vb;
	struct faxvmi_ctl ctl;
	int ra, rb;

	diff_begin("FAXVMI_control");

	va = ref_FAXVMI_create(NULL, &cfg);
	vb = FAXVMI_create(NULL, &cfg);

	/* ctl == NULL: untouched, both return -1. */
	ra = ref_FAXVMI_control(va, NULL);
	rb = FAXVMI_control(vb, NULL);
	diff_eq_int("ctl==NULL: return (%ld)", rb, ra, 0);
	cmp_vmi_scalars("ctl==NULL", va, vb, 0);
	cmp_framer("ctl==NULL framer", va->framer, vb->framer, 0);

	/* The quiescent all-zero record: no-op except the unconditional copy. */
	ra = ref_FAXVMI_control(va, (const struct faxvmi_ctl *)ref_FAXVMI_CTL);
	rb = FAXVMI_control(vb, &FAXVMI_CTL);
	diff_eq_int("FAXVMI_CTL: return (%ld)", rb, ra, 1);
	cmp_vmi_scalars("FAXVMI_CTL", va, vb, 1);
	cmp_framer("FAXVMI_CTL framer", va->framer, vb->framer, 1);

	/* ptr_0000 nonzero: empty the ring, scribbled first on both sides. */
	va->framer->rd = vb->framer->rd = 7;
	va->framer->wr = vb->framer->wr = 3;
	va->framer->count = vb->framer->count = 2;
	va->framer->residue = vb->framer->residue = 9;
	{
		unsigned i;

		for (i = 0; i < va->framer->fifo_size; i++)
			va->framer->fifo[i] = vb->framer->fifo[i] =
			    (unsigned short)(0x5000 + i);
	}
	memset(&ctl, 0, sizeof(ctl));
	ctl.ptr_0000 = &ctl;		/* any nonzero pointer */
	ctl.int_0004 = 111;
	ctl.short_0008 = 22;
	ra = ref_FAXVMI_control(va, &ctl);
	rb = FAXVMI_control(vb, &ctl);
	diff_eq_int("ptr_0000: return (%ld)", rb, ra, 2);
	cmp_vmi_scalars("ptr_0000", va, vb, 2);
	cmp_framer("ptr_0000 framer", va->framer, vb->framer, 2);
	cmp_u16("ptr_0000 fifo[%ld]", vb->framer->fifo, va->framer->fifo,
		va->framer->fifo_size, 2);

	/* int_000c + short_0010 == 1 (in range): full reset + mode change. */
	va->framer->pack_bit = vb->framer->pack_bit = 6;
	va->framer->pack_mask = vb->framer->pack_mask = 0x55;
	va->framer->async_hunt = vb->framer->async_hunt = 0;
	va->framer->ones = vb->framer->ones = 4;
	va->framer->in_frame = vb->framer->in_frame = 1;
	va->mode = vb->mode = 0;
	memset(&ctl, 0, sizeof(ctl));
	ctl.int_000c = 1;
	ctl.short_0010 = 1;
	ctl.int_0004 = 222;
	ctl.short_0008 = 33;
	ra = ref_FAXVMI_control(va, &ctl);
	rb = FAXVMI_control(vb, &ctl);
	diff_eq_int("int_000c in-range: return (%ld)", rb, ra, 3);
	cmp_vmi_scalars("int_000c in-range", va, vb, 3);
	cmp_framer("int_000c in-range framer", va->framer, vb->framer, 3);

	/* int_000c + short_0010 == 3 (out of range): treated as quiescent. */
	va->framer->pack_bit = vb->framer->pack_bit = 6;
	va->mode = vb->mode = 1;
	memset(&ctl, 0, sizeof(ctl));
	ctl.int_000c = 1;
	ctl.short_0010 = 3;
	ctl.int_0004 = 44;
	ctl.short_0008 = 55;
	ra = ref_FAXVMI_control(va, &ctl);
	rb = FAXVMI_control(vb, &ctl);
	diff_eq_int("int_000c out-of-range: return (%ld)", rb, ra, 4);
	cmp_vmi_scalars("int_000c out-of-range", va, vb, 4);
	cmp_framer("int_000c out-of-range framer", va->framer, vb->framer, 4);

	/* int_0014 nonzero: recurse through vxx_control[slot] (slot 0, null). */
	memset(&ctl, 0, sizeof(ctl));
	ctl.int_0014 = 0x1234;
	ctl.int_0004 = 66;
	ctl.short_0008 = 77;
	ra = ref_FAXVMI_control(va, &ctl);
	rb = FAXVMI_control(vb, &ctl);
	diff_eq_int("int_0014: return (%ld)", rb, ra, 5);
	diff_eq_int("int_0014: return is null_control's -1 (%ld)", rb, -1, 5);
	cmp_vmi_scalars("int_0014", va, vb, 5);
	cmp_framer("int_0014 framer", va->framer, vb->framer, 5);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_tables();
	rc |= run_create_fresh();
	rc |= run_create_reinit();
	rc |= run_process();
	rc |= run_control();
	return rc;
}
