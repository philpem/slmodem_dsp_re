/*
 * v34pcmmain.cpp -- the C++ half of VPcmV34Main.cpp.
 *
 * `src/pump/v34/v34pcmif.c` holds the `extern "C"` exports of the same
 * translation unit and explains why they are a `.c`: nothing about them is
 * C++, and splitting one TU by language would be a worse map than splitting
 * it by role.  THIS file is the exception that forces itself.  VPcmV34Main.cpp
 * starts at .text+0x9250 with
 *
 *     _Z14getMPrecvdBitsP12tagV34Object
 *
 * and a C translation unit cannot emit that name.  So the C++ half gets a
 * file, and it gets a DIFFERENT STEM from v34pcmif -- the Makefile turns both
 * `%.c` and `%.cpp` into `$(BUILD)/%.o`, so `v34pcmif.cpp` beside
 * `v34pcmif.c` would be two sources racing for one object file.
 *
 * `tagV34Object` is the object's own name for `struct v34_object`, and this
 * symbol is the only place it survives.  It is a distinct, incomplete type
 * here rather than a typedef, because a typedef would mangle as
 * `P11v34_object` and the point of the exercise is the exact string.
 *
 * The file is built with the same -fno-exceptions -fno-rtti -nostdinc++ as
 * FloatIIR.cpp; see the Makefile.  Nothing here is a class, has a virtual, or
 * allocates, which is what lets the test binaries link with $(CC).
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34rx.h"

/*
 * The session object's MP block.  Six flag bytes and seven shorts, read as a
 * group and never written; the field names come from the diagnostics the
 * function prints out of them -- "V90mp - drn", "type", "trellisState",
 * "nonlinearEncoder", "constellationShaping" and "rateMask" -- so five of the
 * six bytes are named by the object and the sixth, at +5, is not.
 */
#define SESS_MP			0x1744
#define SESS_PCM		0x610c
#define SESS_GATE		0x6120

/* Inside the block the session hands over. */
#define MP_TYPE			0x00
#define MP_DRN			0x01
#define MP_TRELLIS		0x02
#define MP_NONLINEAR		0x03
#define MP_SHAPING		0x04
#define MP_FLAG5		0x05
#define MP_RATEMASK		0x06

/* The PCM receiver's two words, and the configuration's one. */
#define PCM_SENS		0x04f8
#define PCM_RATE		0x04fc
#define CFG_RATE		0x003c

/* Where the answer goes. */
#define OB_INFO			0xaa0c		/* seven shorts follow  */
#define OB_CAPS			0xaa3c
#define OB_CAPS_PTR		0xaa6c
#define OB_DMA			0x2a68

/*
 * 33599, which is 14 rate steps of 2400 less one, and the largest rate the
 * four-bit field below can carry.  Above it the field is left alone.
 */
#define RATE_MAX		0x833f

void
getMPrecvdBits(struct tagV34Object *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	short *mp = (short *)(m + OB_INFO);
	const unsigned char *sess = (const unsigned char *)obj->p3548;
	int rate;

	if (obj->v90_receiver > 0) {
		const unsigned char *d = sess + SESS_MP;
		int v;

		/*
		 * The first short of the INFO record, assembled bit by bit:
		 * one flag in bit 0, a four-bit field at 6, a two-bit one at
		 * 11, and three more flags at 13, 14 and 15.  The masks are
		 * the object's -- `and $0xf` and `and $0x3` -- so a byte
		 * larger than its field is truncated rather than rejected.
		 */
		v = (d[MP_TYPE] != 0) ? 1 : 0;
		v |= (d[MP_DRN] & 0xf) << 6;
		v |= (d[MP_TRELLIS] & 3) << 11;
		if (d[MP_NONLINEAR] != 0)
			v |= 0x2000;
		if (d[MP_SHAPING] != 0)
			v |= 0x4000;
		if (d[MP_FLAG5] != 0)
			v |= 0x8000;

		/*
		 * Seven shorts straight across, except the first, which comes
		 * back with its top bit forced on.  `rate_mask`'s sign bit is
		 * "asymmetric rates are on the table" (v34fsk.h), so this says
		 * a V.90 MP always permits them.
		 */
		mp[1] = (short)(*(const unsigned short *)(d + MP_RATEMASK)
				| 0x8000);
		mp[2] = *(const short *)(d + 0x08);
		mp[3] = *(const short *)(d + 0x0a);
		mp[4] = *(const short *)(d + 0x0c);
		mp[5] = *(const short *)(d + 0x0e);
		mp[6] = *(const short *)(d + 0x10);
		mp[7] = *(const short *)(d + 0x12);

		/*
		 * And the four-bit field at 6 is COPIED DOWN over bits 2..5,
		 * which is where v34fsk.h says the two per-direction rate
		 * nibbles live: the V.90 upstream rate becomes the V.34
		 * one as well.  ORed, not assigned, so a bit already set
		 * there stays set.
		 */
		v |= (v >> 4) & 0x3c;
		obj->info_rates = (short)v;

		if (v & 1)
			txrxdmainit((short *)(m + OB_DMA), mp);

		/*
		 * Seven diagnostics, each with its OWN level test, because
		 * the call between them can change the level.  The names in
		 * them are the whole reason this function's fields have any.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - drn = %d\r\n",
					     (signed char)d[MP_DRN]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - type = %d\r\n",
					     (signed char)d[MP_TYPE]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - trellisState = %d\r\n",
					     (signed char)d[MP_TRELLIS]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90mp - nonlinearEncoder = %d\r\n",
			    (signed char)d[MP_NONLINEAR]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90mp - constellationShaping = %d\r\n",
			    (signed char)d[MP_SHAPING]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - rateMask = 0x%X\r\n",
					     *(const short *)(d + MP_RATEMASK));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "mp->data[0] = 0x%X , mp->data[1] = 0x%X\r\n",
			    (unsigned short)obj->info_rates,
			    (unsigned short)mp[1]);
	}

	/*
	 * AND THE SAME CALL AGAIN, on the same bit, unconditionally on the
	 * branch above.  With a V.90 receiver running and bit 0 set,
	 * `txrxdmainit` runs TWICE over the same six shorts; it is
	 * idempotent, so the repeat is wasted work rather than a defect.  The
	 * test at 0x9333 is a `testb` on the low byte, which is bit 0 of the
	 * short on this little-endian target.
	 */
	if (*(const unsigned char *)mp & 1)
		txrxdmainit((short *)(m + OB_DMA), mp);

	/*
	 * The capability word, built from scratch: 0x9dc3 first, then four
	 * bits of it replaced below.  The pointer at +0xaa6c is aimed at it,
	 * which is what says this word is a message the handshake will clock
	 * out through `getbit` -- that is the only reader of +0xaa6c.
	 */
	*(short *)(m + OB_CAPS) = (short)0x9dc3;
	*(void **)(m + OB_CAPS_PTR) = m + OB_CAPS;

	rate = *(const int *)((const unsigned char *)obj->pac3c + CFG_RATE);

	if (*(const int *)(sess + SESS_GATE) != 0 && obj->v90_receiver > 1
	    && *(const int *)(*(const unsigned char *const *)(sess + SESS_PCM)
			      + PCM_SENS) != 0) {
		int cap = *(const int *)(
		    *(const unsigned char *const *)(sess + SESS_PCM)
		    + PCM_RATE) * 0x960;

		if (rate > cap)
			rate = cap;
		edprintf("on get max upstream rate, on sensitive ISP, "
			 "returning %d\r\n", rate);
	} else {
		edprintf("on get max upstream rate, on regular ISP, "
			 "returning %d\r\n", rate);
	}

	if (rate <= RATE_MAX) {
		/*
		 * bits per second -> rate index, as `rate * 7 >> 14`: 2400 Hz
		 * of rate is 1.0254 of a step, so this is a divide by 2340
		 * rather than by 2400 and 33600 still lands on 14.  The four
		 * bits go in REVERSED, at positions 6..9, which is the same
		 * treatment v34fsk.h records for `info_caps`'s two nibbles.
		 */
		int rev = (short)bitreverse((unsigned short)((rate * 7) >> 14),
					    4);
		int mask = -0x41;			/* ~0x40 */
		int k;

		for (k = 0; k <= 3; k++) {
			unsigned short w = *(const unsigned short *)
			    (m + OB_CAPS);

			if ((rev >> k) & 1)
				w = (unsigned short)(w | (unsigned)~mask);
			else
				w = (unsigned short)(w & (unsigned)mask);
			*(short *)(m + OB_CAPS) = (short)w;
			mask = (short)(mask * 2 + 1);
		}
	}

	/*
	 * The rest of the record, cleared.  Nine shorts from +0xaa3e, and the
	 * one non-zero constant among them is +0xaa3e itself.  The object
	 * writes them in an order the compiler chose; nothing reads any of
	 * them in between, so this is the same state.
	 */
	*(short *)(m + 0xaa3e) = 0x7ffd;
	*(short *)(m + 0xaa40) = 0;
	*(short *)(m + 0xaa42) = 0;
	*(short *)(m + 0xaa44) = 0;
	*(short *)(m + 0xaa46) = 0;
	*(short *)(m + 0xaa48) = 0;
	*(short *)(m + 0xaa4a) = 0;
	*(short *)(m + 0xaa4c) = 0;
}
