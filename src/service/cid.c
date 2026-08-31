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
 *
 * THE OTHER HALF OF THIS FILE is the service object's own small setters
 * and TLV walkers, written in the same wave from the V32mod.c span:
 * Reconstructed from dsplibs.o cid.c (finding F1410 for the TU map):
 *
 *   cid_freq_sampl        .text 0x08fd60   107
 *   cid_threshold         .text 0x08fdd0    40
 *   cid_value             .text 0x08fe00    15
 *   _look_for             .text 0x0903c0    72
 *   _look_for_other_than  .text 0x090410    88
 *
 * The four leaves were exported API with no internal referrer (finding
 * F8320's bucket), written on their own merit; `cid_freq_sampl` came with the
 * CID service pass and calls nothing.  The rest of the TU -- cid_reset,
 * cid_create, cid_delete, cid_progress, cid_get_strings -- is written as the
 * receiver underneath it lands.  See docs/findings.md F8492 for why the
 * link constraint, not difficulty, is what orders this file.
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

/*
 * What `cid_freq_sampl` puts back into the FSK receiver's f02c, and what
 * `create_cid` seeds it with.  Usage inference only -- the object has no
 * string and no other caller that types it, and the value is the same 9 down
 * all three of cid_freq_sampl's arms.
 */
#define CID_F02C_RESET	9

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


/*
 * Retune both receivers.  The mode gating is cid_threshold's -- `!= 0` reaches
 * the DTMF receiver, `!= 1` the FSK one -- and `rate` is stored as a short on
 * both sides, which is the object's own truncation of the int argument.
 *
 * THE THREE STORES OF 9 ARE THE OBJECT'S.  It writes `movw $0x9,0x2c(...)`
 * three times over, into the FSK receiver's f02c, from three separate tests:
 * mode 0 with rate 9600, mode 0 with rate 8000, and mode > 1 for any rate.
 * The first two are if-converted in the object -- one `sete` for `mode == 0`,
 * reused, ANDed against a second `sete` per rate -- so they are two
 * statements sharing a condition and not one test of a rate pair.  The value
 * is 9 in all three, which is also what create_cid seeds f02c with, so the
 * net effect over the modes and rates this service uses is to put it back.
 * Kept as three because that is what the object encodes; nothing here reads
 * a rate-dependent value into it.
 *
 * Mode 1 touches the FSK receiver not at all, which is why the pointer may be
 * null there: every f02c store is under `mode == 0` or `mode > 1`.
 */
void
cid_freq_sampl(struct cid_modem *ctx, int rate)
{
	if (ctx->mode != 0)
		ctx->dtmf->rate = (short)rate;
	if (ctx->mode != 1)
		ctx->fsk->rate = (short)rate;

	if (ctx->mode == 0 && rate == CID_RATE_9600)
		ctx->fsk->f02c = CID_F02C_RESET;
	if (ctx->mode == 0 && rate == CID_RATE_8000)
		ctx->fsk->f02c = CID_F02C_RESET;
	if (ctx->mode > 1)
		ctx->fsk->f02c = CID_F02C_RESET;
}

/*
 * Both writes are gated on the mode, with the same two tests cid_create and
 * cid_reset use: `!= 0` reaches the DTMF side, `!= 1` the FSK side, so mode
 * 5 sets both.  The object stores the int argument as a short on both sides.
 */
void
cid_threshold(struct cid_modem *ctx, int thr)
{
	if (ctx->mode != 0)
		ctx->dtmf->sens = (short)thr;
	if (ctx->mode != 1)
		ctx->fsk->f028 = (short)thr;
}

void
cid_value(struct cid_modem *ctx, int v)
{
	ctx->f264 = v;
}

/*
 * Walk the TLV entries: buf[2] is the first tag, buf[pos + 1] each entry's
 * length, and the next entry starts at pos + length + 2.  The scan bound is
 * buf[1], the message length, read once as a SIGNED char; positions live in
 * shorts.  Both are the object's widths -- a length byte >= 0x80 steps the
 * position BACKWARDS here, exactly as it does there.
 */
int
_look_for(const char *buf, char tag)
{
	short len = buf[1];
	short pos = 2;

	while (pos < len) {
		if (buf[pos] == tag)
			return pos;
		pos = (short)(pos + buf[pos + 1] + 2);
	}
	return -1;
}

/*
 * The same walk from `start`, returning the first entry whose tag is none
 * of 1, 2 or 7 and whose position is not `except`.  The object tests 1 and
 * 7 together (two setne into one test) and 2 apart; behaviourally one
 * exclusion set.
 */
int
_look_for_other_than(const char *buf, short start, int except)
{
	short len = buf[1];
	short pos = start;

	while (pos < len) {
		char tag = buf[pos];

		if (tag != 1 && tag != 7 && tag != 2 && pos != except)
			return pos;
		pos = (short)(pos + buf[pos + 1] + 2);
	}
	return -1;
}
