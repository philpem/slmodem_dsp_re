/*
 * t_faxvmicp.c -- differential test of FAXVMI_create, FAXVMI_process, and the
 * vxx_create/vxx_process dispatch tables they complete.  See faxvmi.h for
 * what each is; this file's own job is the orchestration -- allocation,
 * (re)initialisation, and the pack/process/unpack/reverse pipeline -- not the
 * per-modulation adapters or the framing functions, which are tested
 * elsewhere (t_faxadapt.c, t_nulldp.c, t_faxpack.c, t_faxframing.c,
 * t_faxunframe.c).
 *
 * WHY THE NON-NULL SLOTS ARE NOT INVOKED, same reasoning as t_faxvmids.c:
 * `vxx_create[5..12]`/`vxx_process[5..12]` wrap real per-modulation
 * constructors/processors that need a fully-formed modem instance this file
 * has no route to build safely.  Wiring is proven by pointer identity
 * (mirroring t_faxvmids.c's `run_tables`); CALLS are made only through the
 * five NULL slots, which are already proven safe (t_nulldp.c) and behave
 * identically at every slot value 0..4.
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
extern void *ref_vxx_create[13];
extern void *ref_vxx_process[13];

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

int
main(void)
{
	int rc = 0;

	rc |= run_tables();
	rc |= run_create_fresh();
	rc |= run_create_reinit();
	rc |= run_process();
	return rc;
}
