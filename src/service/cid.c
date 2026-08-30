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
 * this file -- `cid_progress` and friends are unwritten and resolve to the
 * blob's own copies at link time.
 *
 * THE SPAN NAME IS NOT THE MODULE NAME (the CLAUDE.md rule): the blob's
 * layout labels 0x340..0x5f5 `dp_init.c`, the same label as the two
 * `prop_dp_*` functions at 0x0 -- but `dcr.c`'s four functions sit between
 * (0x60..0x33f), so those two stretches cannot be one translation unit and
 * this file is separate from `src/core/dp_init.c` on that evidence.
 */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/* Host callback, undefined in the object, supplied by slmodemd's modem.c. */
extern int modem_send_to_tty(void *m, const void *buf, int n);

/*
 * The receiver underneath, all five unwritten.  Only what these three
 * callers establish is declared: `cid_progress` reads four arguments and
 * its result is used as a `short` (`cwtl` after the call), and
 * `cid_create`'s first and third arguments are passed as constant zero
 * here, so their types are this file's reading.
 *
 * WEAK, the `DSPLIB_VPCM_UNWRITTEN` idiom (vpcm.h explains it at length):
 * in the differential binaries `test/harness/unwritten.c` bridges each name
 * to the blob's ref_ alias, and in the interop binaries -- which link no
 * blob and never construct a CID -- the weak references resolve to zero.
 * When the receiver is reconstructed, its definitions take over and the
 * bridge collides loudly.  No null tests at the call sites: the object
 * calls all five unconditionally, and so does this file.
 */
#define DSPLIB_CID_UNWRITTEN __attribute__((weak))
extern void *cid_create(void *m, unsigned cid_val, int w)
	DSPLIB_CID_UNWRITTEN;
extern void cid_delete(void *cid) DSPLIB_CID_UNWRITTEN;
extern void cid_freq_sampl(void *cid, int rate) DSPLIB_CID_UNWRITTEN;
extern short cid_progress(void *cid, short *in, int what, short *count)
	DSPLIB_CID_UNWRITTEN;
extern char *cid_get_strings(void *cid) DSPLIB_CID_UNWRITTEN;

/* The line rates `CID_create` accepts; cid.h names the same two values. */
#define CID_LINE_8000	8000
#define CID_LINE_9600	9600

/*
 * `cid_progress` is fed at most this many samples per call; `CID_process`
 * walks a longer buffer in chunks of it.
 */
#define CID_CHUNK	192

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
