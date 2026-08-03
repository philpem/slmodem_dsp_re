/*
 * t_v34hshak.c -- differential test of the V.34 handshake's symbol emitters.
 *
 * Both functions END IN A TAIL CALL to `txmit`, so testing them means running
 * the whole transmit chain -- modulator, sample queue, echo pre-filter, both
 * cancellers -- and comparing the entire object afterwards.  The fixture is
 * therefore the one t_v34rx.c uses for txmit, for the same reasons and with
 * the same three arrays kept OUTSIDE the object (finding 116b: pointing them
 * inside collides with what V34InitializeImplementationSpecific laid out, and
 * does so identically on both sides, so the comparison cannot see it).
 *
 * Testing them any other way would test the first six lines and none of the
 * consequences, which for a function whose whole output is a side effect is
 * no test at all.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"

extern void txmitdibit(void *obj, short bits);
extern void txmitquadbit(void *obj, short bits);
extern void ref_txmitdibit(void *obj, short bits);
extern void ref_txmitquadbit(void *obj, short bits);
extern const int ref_vect4[4];
extern const int ref_vect16[16];
extern void ref_txinit(void *obj);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern void ref_V34SetupModulator(void *m, short baud, short carrier,
				  short a, short b, short c);

int
main(void)
{
	int rc = 0;

	diff_begin("v34 handshake constellations");
	{
		diff_eq_int("vect4", memcmp(vect4, ref_vect4, sizeof(vect4)),
			    0, 0);
		diff_eq_int("vect16",
			    memcmp(vect16, ref_vect16, sizeof(vect16)), 0, 0);
	}
	rc |= diff_end();

	/*
	 * txmitdibit and txmitquadbit.
	 *
	 * `quad` picks which of the two runs; `gpc` drives bit 0 of f25c2,
	 * which is the scrambler generator and the one branch inside them;
	 * `gate` drives bit 9, which is txmit's echo feed, so both settings
	 * of it exercise a different amount of the tail call.
	 *
	 * The bit patterns run past the field width on purpose -- txmitdibit
	 * takes two bits out of a short and txmitquadbit four, and neither
	 * masks what it was handed, so a caller passing 0xffff is a caller
	 * the object accepts.  The scrambler shifts ARITHMETICALLY, so a
	 * negative value feeds ones for ever rather than running out, and
	 * that is reachable from a plain -1.
	 */
	diff_begin("v34 txmitdibit/txmitquadbit");
	{
		static struct v34_object oa, ob;
		static short shp_a[512], shp_b[512];
		static short bra[64], brb[64];
		static const short bits[10] = { 0, 1, 2, 3, 5, 10, 15,
						0x5a5a, -1, 0x7fff };
		int quad, gpc, gate, it;

		for (quad = 0; quad <= 1; quad++)
		for (gpc = 0; gpc <= 1; gpc++)
		for (gate = 0; gate <= 1; gate++) {
			memset(&oa, 0, sizeof(oa)); memset(&ob, 0, sizeof(ob));
			memset(shp_a, 0, sizeof(shp_a));
			memset(shp_b, 0, sizeof(shp_b));
			memset(bra, 0, sizeof(bra));
			memset(brb, 0, sizeof(brb));

			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);

			((struct v34_modulator *)((char *)&oa + 0x1450))->shaped
				= shp_a;
			((struct v34_modulator *)((char *)&ob + 0x1450))->shaped
				= shp_b;
			V34SetupModulator((struct v34_modulator *)
					  ((char *)&oa + 0x1450), 2400, 1600,
					  0, 0, 1);
			ref_V34SetupModulator((char *)&ob + 0x1450, 2400,
					      1600, 0, 0, 1);

			oa.prefilter.coeff = ob.prefilter.coeff =
				V34TimingPrefilterCoeff;
			oa.prefilter.shift = ob.prefilter.shift = 14;
			oa.f25d4 = ob.f25d4 = 0x4000;
			oa.f25c2 = ob.f25c2 =
				(short)((gate ? 0x200 : 0) | (gpc ? 1 : 0));
			oa.bulk_ring = bra;  ob.bulk_ring = brb;
			oa.bulk_len  = ob.bulk_len = 64;

			/* A scrambler state that is not all zeroes. */
			oa.f25cc = ob.f25cc = 0x2f6b3d51;
			oa.f25c6 = ob.f25c6 = 2;

			for (it = 0; it < 40; it++) {
				short b = bits[it % 10];
				unsigned k;

				if (quad) {
					txmitquadbit(&oa, b);
					ref_txmitquadbit(&ob, b);
				} else {
					txmitdibit(&oa, b);
					ref_txmitdibit(&ob, b);
				}

				for (k = 0; k < sizeof(oa); k++) {
					/* Every pointer field: two objects. */
					if ((k >= 0x268 && k < 0x270)
					    || (k >= 0x2074 && k < 0x2078)
					    || (k >= 0x20cc && k < 0x20d0)
					    || (k >= 0x2220 && k < 0x2228)
					    || (k >= 0x35b0 && k < 0x35b4)
					    || (k >= 0x80b8 && k < 0x80d8)
					    || (k >= 0x9138 && k < 0x9158)
					    || (k >= 0x1450 + 0x10
						&& k < 0x1450 + 0x18)
					    || (k >= 0x1450 + 0xc24
						&& k < 0x1450 + 0xc28)
					    || (k >= 0x1450 + 0xc7c
						&& k < 0x1450 + 0xc80)
					    || (k >= 0x1450 + 0xcb0
						&& k < 0x1450 + 0xcb4))
						continue;
					/*
					 * Stride larger than the object, so
					 * (case, iteration, offset) stays
					 * unambiguous -- finding 116a.
					 */
					diff_eq_int("txmit* at %ld",
						    ((unsigned char *)&oa)[k],
						    ((unsigned char *)&ob)[k],
						    ((long)quad * 4
						     + gpc * 2 + gate)
						    * 100000000L
						    + (long)it * 100000 + k);
				}
				for (k = 0; k < 64; k++)
					diff_eq_int("bulk ring", bra[k],
						    brb[k], k);
				for (k = 0; k < 512; k++)
					diff_eq_int("shaped", shp_a[k],
						    shp_b[k], k);
			}
		}
	}
	rc |= diff_end();

	return rc;
}
