/*
 * nulldp.h -- Class 1 fax: the null datapump.
 *
 * Five leaves at `.text` 0x09f0b0..0x09f135, immediately before `SDM_scrambler`
 * (0x09f150) and immediately after `cTOOLS_handle_hdlc_output` (0x09ef10..
 * 0x09f0a3, class1tx.c): `null_create`, `null_delete`, `null_process`,
 * `null_status`, `null_control`.  Not V.17/21/27/29-specific and not
 * `sdm.c`'s, so they get a file of their own.
 *
 * WHAT THEY ARE.  `relocscan.py` finds every one of the five referenced ONLY
 * from `.rodata`, five consecutive pointers at the head of a 13-slot table --
 * `vxx_create` (0x9620), `vxx_delete` (0x95e0), `vxx_process` (0x9520),
 * `vxx_status` (0x95a0), `vxx_control` (0x9560) -- laid out every 0x40 bytes
 * alongside the already-written `vxx_message` (0x94e0), all six sharing the
 * same 13-slot order: 0..4 null, 5 v21tx, 6 v21rx, 7 v27tx, 8 v27rx, 9 v29tx,
 * 10 v29rx, 11 v17tx, 12 v17rx (`faxvmi.h`'s slot map, confirmed again here).
 * So these are the "no modulation is active" arm of the same dispatch
 * `vxx_message` already serves, and the SLOT is `struct faxvmi_link` itself
 * being decorated in place, not a distinct object.
 *
 * NONE OF THE FIVE TABLES ABOVE IS WRITTEN HERE.  Every one of them is the
 * exact analogue of `vxx_message`, which lives in `faxvmi.c` beside
 * `FAXVMI_message` -- so that is where they belong too, and `faxvmi.c` is
 * another strand's file this wave.  Writing the five leaves here still
 * clears them out of `vxx_create`/`vxx_delete`/`vxx_process`/`vxx_status`/
 * `vxx_control`'s blocked-on lists for whoever writes that file next
 * (`readyqueue.py`, 2026-09-02).
 *
 * SIGNATURES ARE READ OFF THE CALL SITES, NOT THE FAXADAPT.C SIBLINGS,
 * because a table entry's formal types are fixed by every OTHER slot's call
 * convention, not by what one particular slot happens to touch:
 *
 *   - `null_create`/`null_delete` match `v??tx_create`/`v??tx_delete`'s own
 *     shape in `faxadapt.h` directly (2 args / 1 arg, both `void`).
 *   - `null_status`/`null_control` are read off `FAXVMI_status` (0x95638,
 *     `mov 0x28(%esi),%eax` / `mov 0x18(%ebx),%eax`) and `FAXVMI_control`
 *     (0x9578e, `mov 0x28(%esi),%eax` / `mov 0x14(%ebx),%eax`) respectively:
 *     both call their table through `struct faxvmi_link *dp` (the `link`
 *     field FAXVMI's own struct carries at +0x28) and a second, untyped
 *     pointer, returning `int` in `eax` -- which is ALL neither the caller
 *     nor `null_status`/`null_control` ever narrow, so `void *` is exact
 *     rather than a guess for that second argument's type.
 *   - `null_process` is read off its OWN body -- see class1tx.h's process
 *     note (mirrored from `faxadapt.h`) for the RX shape
 *     `(dp, in, result, count)` it matches: it never touches `result`.
 */

#ifndef DSPLIB_NULLDP_H
#define DSPLIB_NULLDP_H

struct faxvmi_link;

/*
 * dp->int_0014 = 0; dp->pack_count = 0x32; dp->pack_width = 8;
 * dp->unpack_width = 8 -- in that order, the object's own (0x09f0b4..
 * 0x09f0cb).  `cfg` is never read.
 */
void null_create(struct faxvmi_link *dp, const void *cfg);

/* `ret`.  Neither argument is read. */
void null_delete(struct faxvmi_link *dp);

/*
 * Copy `*count` elements from `in` into `dp->ptr_0000` and return -1,
 * always -- there is no modem to report a real count from.  `result` is
 * never written.  `*count` gates the copy with a `<= 0` test before the
 * loop (0x09f0ec) as well as inside it, so a zero or negative count copies
 * nothing.
 */
int null_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		 unsigned short *count);

/* `mov $0xffffffff,%eax; ret`.  Neither argument is read. */
int null_status(struct faxvmi_link *dp, void *status);

/* `mov $0xffffffff,%eax; ret`.  Neither argument is read. */
int null_control(struct faxvmi_link *dp, void *arg);

#endif /* DSPLIB_NULLDP_H */
