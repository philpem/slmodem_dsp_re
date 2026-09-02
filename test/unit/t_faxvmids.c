/*
 * t_faxvmids.c -- differential test of FAXVMI_delete, FAXVMI_status, the
 * vxx_delete/vxx_status dispatch tables they complete, and the object's own
 * FAXVMI_CTL quiescent control record.  See faxvmi.h for what each is.
 *
 * WHY THE NON-NULL SLOTS ARE NOT INVOKED.  `vxx_delete[5..12]` and
 * `vxx_status[5..12]` wrap real per-modulation destructors
 * (`V21TX_delete`, `V17TX_status`, ...) that dereference their own modem
 * instance with no NULL check -- `V21TX_delete` reads `param->0x24`
 * unconditionally, confirmed from `dis.py` before this file was written --
 * and the constructors that would produce a safe instance are still
 * BLOCKED this wave.  So this fixture proves those eight entries are wired
 * to the RIGHT function by POINTER IDENTITY, the same technique
 * t_faxpack.c uses for `vmi_pack`/`vmi_unpack`/`vmi_reverse`, and only
 * CALLS through the five NULL slots, which are already proven safe
 * (t_nulldp.c) and behave identically at every slot value 0..4.
 *
 * FAXVMI_delete IS TESTED VIA THE ALLOCATION LOG, not by inspecting freed
 * memory (undefined the moment it is freed).  `harness_alloc` counts real
 * sysdep_malloc/sysdep_free traffic, so allocating the seven objects
 * FAXVMI_delete is known (from dis.py) to free -- link->ptr_0000, link->buf,
 * framer->frame, framer->fifo, link, framer, vmi itself -- and checking
 * `frees == 7, live == 0, bad_free == 0` afterward proves it read exactly
 * those seven pointers off exactly those offsets, on both sides.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1tx.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"
#include "dsplib/sysdep.h"

extern void ref_FAXVMI_delete(struct faxvmi *vmi);
extern int ref_FAXVMI_status(struct faxvmi *vmi, struct faxvmi_status *status);
extern void *ref_vxx_delete[13];
extern void *ref_vxx_status[13];
extern const unsigned char ref_FAXVMI_CTL[24];

extern void ref_null_delete(void);
extern void ref_v21tx_delete(void), ref_v21rx_delete(void);
extern void ref_v27tx_delete(void), ref_v27rx_delete(void);
extern void ref_v29tx_delete(void), ref_v29rx_delete(void);
extern void ref_v17tx_delete(void), ref_v17rx_delete(void);

extern void ref_null_status(void);
extern void ref_v21tx_status(void), ref_v21rx_status(void);
extern void ref_v27tx_status(void), ref_v27rx_status(void);
extern void ref_v29tx_status(void), ref_v29rx_status(void);
extern void ref_v17tx_status(void), ref_v17rx_status(void);

static unsigned long seed = 20260902UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill(void *p, unsigned n)
{
	unsigned char *b = (unsigned char *)p;
	unsigned i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

/*
 * `compare.py`/`byteident.py` are the codegen-tier tools; the identity
 * checks below are a STRUCTURAL claim about the object's own data, made
 * with plain `==` on function addresses, exactly as t_faxpack.c's
 * `ref_vmi_pack[i] == (void *)ref_faxvmi_simp_pack` checks do.  Four casts
 * are unavoidable: `v17tx_status`/`v17rx_status`/`v21tx_status`/
 * `v21rx_status` take a TYPED second argument in `faxadapt.h`
 * (`struct v17_status *` / `struct v21_status *`), where the table's own
 * element type is the untyped form `FAXVMI_status` calls through
 * (`faxvmi.h`'s own derivation).
 */
static int
run_tables(void)
{
	diff_begin("vxx_delete / vxx_status wiring");

	diff_eq_int("vxx_delete[0..4] (%ld)",
		    (long)(vxx_delete[0] == null_delete
			   && vxx_delete[1] == null_delete
			   && vxx_delete[2] == null_delete
			   && vxx_delete[3] == null_delete
			   && vxx_delete[4] == null_delete),
		    1, 0);
	diff_eq_int("vxx_delete[5..12] (%ld)",
		    (long)(vxx_delete[5] == v21tx_delete
			   && vxx_delete[6] == v21rx_delete
			   && vxx_delete[7] == v27tx_delete
			   && vxx_delete[8] == v27rx_delete
			   && vxx_delete[9] == v29tx_delete
			   && vxx_delete[10] == v29rx_delete
			   && vxx_delete[11] == v17tx_delete
			   && vxx_delete[12] == v17rx_delete),
		    1, 0);

	diff_eq_int("vxx_status[0..4] (%ld)",
		    (long)(vxx_status[0] == null_status
			   && vxx_status[1] == null_status
			   && vxx_status[2] == null_status
			   && vxx_status[3] == null_status
			   && vxx_status[4] == null_status),
		    1, 0);
	diff_eq_int("vxx_status[5..12] (%ld)",
		    (long)(vxx_status[5] == (faxvmi_status_fn)v21tx_status
			   && vxx_status[6] == (faxvmi_status_fn)v21rx_status
			   && vxx_status[7] == v27tx_status
			   && vxx_status[8] == v27rx_status
			   && vxx_status[9] == v29tx_status
			   && vxx_status[10] == v29rx_status
			   && vxx_status[11] == (faxvmi_status_fn)v17tx_status
			   && vxx_status[12] == (faxvmi_status_fn)v17rx_status),
		    1, 0);

	diff_eq_int("ref_vxx_delete[0..4] (%ld)",
		    (long)(ref_vxx_delete[0] == (void *)ref_null_delete
			   && ref_vxx_delete[1] == (void *)ref_null_delete
			   && ref_vxx_delete[2] == (void *)ref_null_delete
			   && ref_vxx_delete[3] == (void *)ref_null_delete
			   && ref_vxx_delete[4] == (void *)ref_null_delete),
		    1, 0);
	diff_eq_int("ref_vxx_delete[5..12] (%ld)",
		    (long)(ref_vxx_delete[5] == (void *)ref_v21tx_delete
			   && ref_vxx_delete[6] == (void *)ref_v21rx_delete
			   && ref_vxx_delete[7] == (void *)ref_v27tx_delete
			   && ref_vxx_delete[8] == (void *)ref_v27rx_delete
			   && ref_vxx_delete[9] == (void *)ref_v29tx_delete
			   && ref_vxx_delete[10] == (void *)ref_v29rx_delete
			   && ref_vxx_delete[11] == (void *)ref_v17tx_delete
			   && ref_vxx_delete[12] == (void *)ref_v17rx_delete),
		    1, 0);

	diff_eq_int("ref_vxx_status[0..4] (%ld)",
		    (long)(ref_vxx_status[0] == (void *)ref_null_status
			   && ref_vxx_status[1] == (void *)ref_null_status
			   && ref_vxx_status[2] == (void *)ref_null_status
			   && ref_vxx_status[3] == (void *)ref_null_status
			   && ref_vxx_status[4] == (void *)ref_null_status),
		    1, 0);
	diff_eq_int("ref_vxx_status[5..12] (%ld)",
		    (long)(ref_vxx_status[5] == (void *)ref_v21tx_status
			   && ref_vxx_status[6] == (void *)ref_v21rx_status
			   && ref_vxx_status[7] == (void *)ref_v27tx_status
			   && ref_vxx_status[8] == (void *)ref_v27rx_status
			   && ref_vxx_status[9] == (void *)ref_v29tx_status
			   && ref_vxx_status[10] == (void *)ref_v29rx_status
			   && ref_vxx_status[11] == (void *)ref_v17tx_status
			   && ref_vxx_status[12] == (void *)ref_v17rx_status),
		    1, 0);

	/*
	 * An off-by-one that merely repeated the previous slot would still
	 * pass every check above; confirm the boundary actually changes.
	 */
	diff_eq_int("vxx_delete: null != v21tx (%ld)",
		    (long)(vxx_delete[4] != vxx_delete[5]), 1, 0);
	diff_eq_int("vxx_status: null != v21tx (%ld)",
		    (long)(vxx_status[4] != vxx_status[5]), 1, 0);

	return diff_end();
}

static int
run_ctl(void)
{
	static const unsigned char zero[24];

	diff_begin("FAXVMI_CTL");
	diff_eq_int("FAXVMI_CTL size (%ld)", (long)sizeof(FAXVMI_CTL), 24, 0);
	diff_eq_int("FAXVMI_CTL matches the blob (%ld)",
		    memcmp(&FAXVMI_CTL, ref_FAXVMI_CTL, sizeof(FAXVMI_CTL)),
		    0, 0);
	diff_eq_int("FAXVMI_CTL is all zero (%ld)",
		    memcmp(&FAXVMI_CTL, zero, sizeof(zero)), 0, 0);
	return diff_end();
}

/*
 * Every null slot, both sides given their OWN heap objects so each side's
 * frees land on pointers only that side's allocator handed out.
 */
static int
run_delete(void)
{
	unsigned p;

	diff_begin("FAXVMI_delete");
	for (p = 0; p < 5; p++) {
		struct faxvmi *vmi;
		struct faxvmi_framer *fr;
		struct faxvmi_link *lk;
		long tag = (long)p;

		harness_alloc_reset();
		vmi = sysdep_malloc(sizeof(*vmi));
		fr = sysdep_malloc(sizeof(*fr));
		lk = sysdep_malloc(sizeof(*lk));
		fr->fifo = sysdep_malloc(16);
		fr->frame = sysdep_malloc(16);
		lk->ptr_0000 = sysdep_malloc(16);
		lk->buf = sysdep_malloc(16);
		vmi->slot = (short)p;
		vmi->framer = fr;
		vmi->link = lk;

		ref_FAXVMI_delete(vmi);

		diff_eq_int("delete: frees, blob (%ld)", harness_alloc.frees,
			    7, tag);
		diff_eq_int("delete: live, blob (%ld)", harness_alloc.live,
			    0, tag);
		diff_eq_int("delete: bad_free, blob (%ld)",
			    harness_alloc.bad_free, 0, tag);

		harness_alloc_reset();
		vmi = sysdep_malloc(sizeof(*vmi));
		fr = sysdep_malloc(sizeof(*fr));
		lk = sysdep_malloc(sizeof(*lk));
		fr->fifo = sysdep_malloc(16);
		fr->frame = sysdep_malloc(16);
		lk->ptr_0000 = sysdep_malloc(16);
		lk->buf = sysdep_malloc(16);
		vmi->slot = (short)p;
		vmi->framer = fr;
		vmi->link = lk;

		FAXVMI_delete(vmi);

		diff_eq_int("delete: frees, ours (%ld)", harness_alloc.frees,
			    7, tag);
		diff_eq_int("delete: live, ours (%ld)", harness_alloc.live,
			    0, tag);
		diff_eq_int("delete: bad_free, ours (%ld)",
			    harness_alloc.bad_free, 0, tag);
	}
	harness_alloc_reset();
	return diff_end();
}

#define STATLOOPS 12

static int
run_status(void)
{
	unsigned p;
	long dispatched = 0, skipped = 0;

	diff_begin("FAXVMI_status");

	{
		struct faxvmi vmi;
		int ra, rb;

		fill(&vmi, sizeof(vmi));
		ra = ref_FAXVMI_status(&vmi, NULL);
		rb = FAXVMI_status(&vmi, NULL);
		diff_eq_int("status(vmi,NULL) return (%ld)", rb, ra, 0);
	}

	for (p = 0; p < STATLOOPS; p++) {
		struct faxvmi vmi_a, vmi_b;
		struct faxvmi_framer fr_a, fr_b;
		struct faxvmi_link lk_a, lk_b;
		struct faxvmi_status st_a, st_b;
		unsigned char modembuf[8];
		int ra, rb;
		long tag = (long)p;

		fill(&vmi_a, sizeof(vmi_a));
		fill(&fr_a, sizeof(fr_a));
		fill(&lk_a, sizeof(lk_a));
		fill(&st_a, sizeof(st_a));
		fill(modembuf, sizeof(modembuf));

		vmi_a.slot = (short)(p % 5);	/* every null slot, repeated */
		vmi_a.framer = &fr_a;
		vmi_a.link = &lk_a;
		st_a.modem_status = (p & 1) ? modembuf : NULL;

		memcpy(&vmi_b, &vmi_a, sizeof(vmi_a));
		memcpy(&fr_b, &fr_a, sizeof(fr_a));
		memcpy(&lk_b, &lk_a, sizeof(lk_a));
		memcpy(&st_b, &st_a, sizeof(st_a));
		vmi_b.framer = &fr_b;
		vmi_b.link = &lk_b;

		ra = ref_FAXVMI_status(&vmi_a, &st_a);
		rb = FAXVMI_status(&vmi_b, &st_b);

		diff_eq_int("status: return (%ld)", rb, ra, tag);
		diff_eq_int("status: room (%ld)", st_b.room, st_a.room, tag);
		diff_eq_int("status: residue (%ld)", st_b.residue,
			    st_a.residue, tag);
		diff_eq_int("status: underrun (%ld)", st_b.underrun,
			    st_a.underrun, tag);
		diff_eq_int("status: overflow (%ld)", st_b.overflow,
			    st_a.overflow, tag);
		diff_eq_int("status: zero_run_seen (%ld)", st_b.zero_run_seen,
			    st_a.zero_run_seen, tag);
		diff_eq_int("status: flag_0014 (%ld)", st_b.flag_0014,
			    st_a.flag_0014, tag);
		diff_eq_int("status: modem_status unchanged (%ld)",
			    (long)(st_b.modem_status == st_a.modem_status),
			    1, tag);
		diff_eq_obj("status: framer untouched", struct faxvmi_framer,
			    &fr_b, &fr_a, tag);
		diff_eq_obj("status: link untouched", struct faxvmi_link,
			    &lk_b, &lk_a, tag);

		if (st_a.modem_status != NULL) {
			/* dispatched to null_status, which always answers -1 */
			diff_eq_int("status: dispatched return is -1 (%ld)",
				    ra, -1, tag);
			dispatched++;
		} else {
			skipped++;
		}
	}

	diff_eq_int("status: some calls dispatched through vxx_status (%ld)",
		    dispatched > 0, 1, dispatched);
	diff_eq_int("status: some calls skipped vxx_status (%ld)",
		    skipped > 0, 1, skipped);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_tables();
	rc |= run_ctl();
	rc |= run_delete();
	rc |= run_status();
	return rc;
}
