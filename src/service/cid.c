/*
 * cid.c -- the Caller ID service's face to slmodemd, reconstructed from the
 * blob's second span labelled `dp_init.c`.
 *
 *   CID_create    .text 0x000340   195 bytes
 *   CID_delete    .text 0x000410    84 bytes
 *   CID_process   .text 0x000470   386 bytes
 *
 * slmodemd declares all three itself (`ref/slmodemd/modem.c:87-89`):
 *
 *     void *CID_create(struct modem *m, unsigned rate, unsigned cid_val);
 *     void  CID_delete(void *cid);
 *     int   CID_process(void *cid, void *in, int count);
 *
 * so the signatures here are quoted, not inferred.  The object underneath is
 * eight bytes: the modem handle, for `modem_send_to_tty`, and the real CID
 * receiver `cid_create` hands back.  Everything that demodulates lives below
 * this file, in `src/service/Rxcid.c`, `Cidfsd.c`, `Cidmtd.c` and
 * `Dtmf_Rx.c`, and all of it is written -- an unwritten callee would not
 * "resolve to the blob's copy at link time" but fail to link at all (F8492).
 *
 * THE SPAN NAME IS NOT THE MODULE NAME (the CLAUDE.md rule): the blob's
 * layout labels 0x340..0x5f5 `dp_init.c`, the same label as the two
 * `prop_dp_*` functions at 0x0 -- but `dcr.c`'s four functions sit between
 * (0x60..0x33f), so those two stretches cannot be one translation unit and
 * this file is separate from `src/core/dp_init.c` on that evidence.
 *
 * The blob has a SECOND `cid.c` input much later in the partial link.  Its
 * receiver-object setters and TLV walkers live in
 * `src/service/cidcore/cid.c`, whose basename deliberately preserves that
 * original FILE record; keeping the inputs separate is required for
 * partial-link fidelity.
 *
 * See include/dsplib/cid_modem.h for the object and the mode encoding.
 */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/cid_modem.h"
#include "dsplib/cid.h"
#include "dsplib/dtmf_rx.h"

/* Host callback, undefined in the object, supplied by slmodemd's modem.c. */
extern int modem_send_to_tty(void *m, const void *buf, int n);

/*
 * THE WEAK DECLARATION OF `cid_progress` THAT USED TO BE HERE IS GONE.
 * It was the `DSPLIB_*_UNWRITTEN` idiom, bridged to the blob's `ref_` alias
 * by `test/harness/unwritten.c` while the receiver underneath was unwritten;
 * this file defines the function now, so the bridge is deleted and the
 * declaration comes from `cid_modem.h` like every other one here.  That is the
 * "WHEN A BRIDGED SYMBOL IS RECONSTRUCTED" paragraph in `unwritten.c`, taken
 * for the fifth and last CID symbol.
 */

/* The line rates `CID_create` accepts; cid.h names the same two values. */
#define CID_LINE_8000	8000
#define CID_LINE_9600	9600

/*
 * `cid_progress` is fed at most this many samples per call; `CID_process`
 * walks a longer buffer in chunks of it.
 */
#define CID_CHUNK	192

/*
 * What `cid_freq_sampl` puts back into the FSK receiver's
 * `mark_conf_step`, and what `create_cid` seeds it with.  Usage inference
 * only -- the object has no string and no other caller that types it, and
 * the value is the same 9 down all three of cid_freq_sampl's arms.
 */
#define CID_MARK_CONF_STEP	9

/*
 * The one value of `struct cid_modem::cid_val` (was `f264`, renamed this
 * wave; see the struct's own field comment for why) -- the `cid_val`
 * slmodemd hands `CID_create`, which `cid_create` parks at +0x264 and
 * `cid_value` overwrites -- that the object tests for: it sends
 * `cid_get_strings` to the raw hex dump instead of the labelled rendering.
 * Usage inference from that single comparison; no format string names it
 * and no other function reads the field.
 */
#define CID_VALUE_RAW	2

/*
 * The five values of `mode` -- including CID_MODE_AUTOMATIC, whose name is the
 * object's own "\n AUTOMATIC MODULATION MODE \n" -- now live in cid_modem.h
 * beside the object they belong to, because `cid_progress` reads and writes
 * three more of them than this file used to.
 */

struct CID {
	void *modem;		/* +0x0 slmodemd's struct modem       */
	void *cid;		/* +0x4 what `cid_create` handed back */
};

void *
CID_create(void *m, unsigned rate, unsigned cid_val)
{
	struct CID *cid;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("cid: create...\n");

	if (rate != CID_LINE_8000 && rate != CID_LINE_9600)
		return 0;

	cid = sysdep_malloc(sizeof(struct CID));
	if (!cid)
		return 0;
	sysdep_memset(cid, 0, sizeof(struct CID));

	cid->modem = m;
	cid->cid = cid_create(0, cid_val, 0);
	if (!cid->cid) {
		sysdep_free(cid);
		return 0;
	}

	if (rate == CID_LINE_9600)
		cid_freq_sampl(cid->cid, CID_LINE_9600);

	return cid;
}

void
CID_delete(void *cidp)
{
	struct CID *cid = cidp;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("cid: delete...\n");

	cid_delete(cid->cid);
	sysdep_free(cid);
}

/*
 * Run `count` line samples through the receiver.  0 while the message is
 * still arriving, 1 once a whole one has been delivered to the TTY, -1 when
 * the receiver gives up -- and the first chunk that settles it ends the
 * call; the rest of the buffer is not offered.
 *
 * On success the receiver hands back its strings as up to four consecutive
 * NUL-terminated fields in one buffer, ended early by an empty one.  Each is
 * sent to the TTY with its own CRLF.
 */
int
CID_process(void *cidp, void *in, int count)
{
	struct CID *cid = cidp;
	short *buf = in;
	int ret = 0;

	while (count > 0) {
		int chunk = count;
		short n;
		short res;

		if (chunk > CID_CHUNK)
			chunk = CID_CHUNK;
		n = chunk;
		ret = 0;
		res = cid_progress(cid->cid, buf, 0, &n);
		if (res == 1) {
			char *p;
			int i;

			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("cid: CID Success\n");
			ret = 1;
			p = cid_get_strings(cid->cid);
			for (i = 0; i <= 3; i++) {
				if (!p || !*p)
					break;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "cid: STR: '%s'\n", p);
				modem_send_to_tty(cid->modem, p,
						  sysdep_strlen(p));
				modem_send_to_tty(cid->modem, "\r\n", 2);
				p += sysdep_strlen(p) + 1;
			}
		} else if (res != 0) {
			ret = -1;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("cid: CID Failure\n");
		}
		if (ret)
			break;
		count -= chunk;
		buf += chunk;
	}

	return ret;
}
