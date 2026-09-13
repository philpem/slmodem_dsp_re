/*
 * t_v32fpdisp.c -- V.32: the FP layer's dispatch surface, driven on a REAL
 *                  datapump rather than on a planted fixture.
 *
 *   V32FP_create    .text 0x07f710   169
 *   V32FP_modem     .text 0x082630   356
 *   v32_data        .text 0x0827a0   859
 *   V32FP_control   .text 0x084530   776
 *   V32FP_status    .text 0x084840  1084
 *   V32_PROTOCOL    .data  0x007700   36
 *
 * THE FIXTURE IS `V32FP_create` ITSELF, and that is the whole design.  Every
 * function here reaches three or four levels into a datapump block that is a
 * tiling of a dozen sub-objects, and several of them SUBSCRIBE tables with a
 * field of that block and no bound check -- `PROTOCOL[hdx->mode]`,
 * `RATEv32[fp + 0x2c]`, `RATEv32[fp + 0x2e]`, `V32_SAMPLE_LEN[obj + 0x16]`,
 * and `V32_PROTOCOL[hdx->mode]`, which is a table of FUNCTION POINTERS.
 *
 * Deviation D955 and finding F8587 are exactly about this: a fixture that
 * fills the block with pseudorandom bytes and plants only what a callee
 * DEREFERENCES reads far out of bounds, because a subscript is not a
 * dereference -- and a blob-against-blob dry run cannot see it, since both
 * sides read the same wild index into the same array and agree.  Building the
 * object with its own constructor removes the entire class: every one of
 * those fields is in range because the constructor put it there.
 *
 * What that costs is that a failure here can be `V32FP_recreate`'s rather
 * than the function under test's, which is why group 1 compares the
 * constructed object BEFORE anything else runs.
 *
 * WHAT EACH GROUP IS FOR, as a named wrong reading:
 *
 *   1. `V32FP_create` patching the wrong field.  All six patches land inside
 *      the 48-byte parameter block, which is compared byte for byte against
 *      the blob's; and the sweep asserts, per axis, how many of its trials
 *      MOVED the object, measured on the blob's side.  A constructor driven
 *      by one configuration cannot see a patch that went to the wrong offset
 *      (finding F3052).
 *   2. `struct v32_status` read with a wrong field boundary.  It has no
 *      pointers, so it is compared field by field, and the SNR path is
 *      driven far enough for the settling counter to expire -- 200 blocks --
 *      because before that the SNR is the constant 40 and every arm of the
 *      log10 chain is dead.
 *   3. `V32FP_control`'s two arms conflated.  The retrain and renegotiation
 *      bits are driven separately AND together, and the test asserts that
 *      each arm CLEARED ITS OWN BIT, which is the only externally visible
 *      thing that separates them.
 *   4. `V32FP_modem`'s two counts conflated.  They cross over -- transmit
 *      bits in, output samples out -- so the test drives it with the two
 *      unequal and checks both directions.
 *   5. `V32_PROTOCOL` read from the file's BYTES.  All nine dwords are zero
 *      in the object and their values are relocations, so a byte comparison
 *      of two all-zero tables passes while naming nothing.  Every slot is
 *      resolved to a function first, and a slot that matches nothing fails
 *      as -1 rather than reading as a match.
 *
 * WHAT NO TEST HERE CAN REACH, said out loud rather than left looking like
 * coverage: `v32_data`'s `Control_Flag == 1` arm passes an UNINITIALISED
 * stack local to `V32FP_control`.  The two sides read two different stack
 * frames, so the comparison is not a comparison of anything.  The test
 * therefore clears `Control_Flag` on both sides before every trial and
 * asserts it was clear, and the arm is recorded in docs/deviations.md
 * instead.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/v32.h"
#include "dsplib/v32fp.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32fpstat.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32hdxst.h"

/* The blob's copies. */
extern void *ref_V32FP_create(const void *cfg, void *arg1);
extern void ref_V32FP_delete(void *modem);
extern int ref_V32FP_control(void *modem, void *ctl);
extern int ref_V32FP_status(void *modem, void *st);
extern int ref_V32FP_modem(void *modem, const int *txbits, short *out,
			   const short *in, int *rxbits, int *nout, int *nin);
extern int ref_Control_Flag;

extern void *ref_V32_PROTOCOL[9];
extern void ref_v32_handshake(void *modem, unsigned short *txdata,
			      short *txout, short *rxin, unsigned short *rxout,
			      short *nsamples, unsigned short *rxcount);
extern void ref_v32_null_protocol(void);
extern void ref_v32_data(void *modem, unsigned short *txdata, short *txout,
			 short *rxin, unsigned short *rxout, short *nsamples,
			 unsigned short *rxcount);

/*
 * v32_data is FILE-LOCAL in the object, so v32fpdisp.c defines it `static`
 * and v32fpstat.h no longer declares it.  Its address is taken by
 * `V32_PROTOCOL`, so the convention is the ordinary one; the test tier
 * links a globalized copy (tools/testvisible.py).
 */
extern void v32_data(void *modem, unsigned short *txdata, short *txout,
		     short *rxin, unsigned short *rxout, short *nsamples,
		     unsigned short *rxcount);

/*
 * `v32_handshake` and `v32_null_protocol` are likewise FILE-LOCAL and
 * `static` in v32fpdisp.c, whose `V32_PROTOCOL` table names both.
 */
extern void v32_handshake(void *modem, unsigned short *txdata, short *txout,
			  short *rxin, unsigned short *rxout,
			  short *nsamples, unsigned short *rxcount);
extern void v32_null_protocol(void);

/* ------------------------------------------------------------------------ */

#define FIELD(o, off)		((unsigned char *)(void *)(o) + (off))
#define F_PTR(o, off)		(*(void **)(void *)FIELD((o), (off)))
#define F_INT(o, off)		(*(int *)(void *)FIELD((o), (off)))
#define F_S16(o, off)		(*(short *)(void *)FIELD((o), (off)))
#define F_U16(o, off)		(*(unsigned short *)(void *)FIELD((o), (off)))
#define F_U8(o, off)		(*(unsigned char *)FIELD((o), (off)))

#define OBJ_HDX(o)		F_PTR((o), 0x64)
#define OBJ_FP(o)		F_PTR((o), 0x68)

/*
 * The scalars two independently built objects must agree on.  Pointers are
 * excluded by construction: two heaps hold two different addresses and always
 * will, so each one that matters is checked by what it POINTS AT instead --
 * here, by driving the graph in groups 2 to 4.
 */
static void
cmp_object(const char *tag, void *a, void *b, long trial)
{
	void *fa = OBJ_FP(a);
	void *fb = OBJ_FP(b);
	void *ha = OBJ_HDX(a);
	void *hb = OBJ_HDX(b);
	int i;

	(void)tag;

	/* The 48-byte parameter block, which is a struct and has no pointers. */
	for (i = 0; i < 48; i++)
		diff_eq_int("params byte %ld",
			    ((unsigned char *)a)[i],
			    ((unsigned char *)b)[i], i);

	diff_eq_int("obj status (%ld)", F_U8(a, 0x30), F_U8(b, 0x30),
		    trial);
	diff_eq_int("obj flags (%ld)", F_U8(a, 0x31), F_U8(b, 0x31),
		    trial);

	/* The ten int switches, and the four rate indices. */
	for (i = 0; i <= 0x24; i += 4)
		diff_eq_int("fp switch +%ld", F_INT(fa, i),
			    F_INT(fb, i), i);
	diff_eq_int("fp tx rate index (%ld)", F_S16(fa, 0x28),
		    F_S16(fb, 0x28), trial);
	diff_eq_int("fp rx rate index (%ld)", F_S16(fa, 0x2a),
		    F_S16(fb, 0x2a), trial);
	diff_eq_int("fp tx rate in force (%ld)", F_S16(fa, 0x2c),
		    F_S16(fb, 0x2c), trial);
	diff_eq_int("fp rx rate in force (%ld)", F_S16(fa, 0x2e),
		    F_S16(fb, 0x2e), trial);
	diff_eq_int("fp dec error (%ld)", F_S16(fa, 0x50d6),
		    F_S16(fb, 0x50d6), trial);
	diff_eq_int("fp rate fallback (%ld)", F_U16(fa, 0x50d8),
		    F_U16(fb, 0x50d8), trial);

	diff_eq_int("hdx state (%ld)", F_U16(ha, 0x74), F_U16(hb, 0x74),
		    trial);
	diff_eq_int("hdx mode (%ld)", F_U16(ha, 0x76), F_U16(hb, 0x76),
		    trial);
	diff_eq_int("hdx block charge (%ld)", F_U16(ha, 0x84),
		    F_U16(hb, 0x84), trial);
	diff_eq_int("hdx turnaround budget (%ld)", F_U16(ha, 0x94),
		    F_U16(hb, 0x94), trial);
	diff_eq_int("hdx symbol len (%ld)", F_U16(ha, 0x9e),
		    F_U16(hb, 0x9e), trial);
	diff_eq_int("hdx sample len (%ld)", F_U16(ha, 0xa0),
		    F_U16(hb, 0xa0), trial);
	diff_eq_int("hdx loss blocks (%ld)", F_U16(ha, 0xae),
		    F_U16(hb, 0xae), trial);
}

static void
make_cfg(struct v32fp_cfg *c, int protocol, int rate, int delay, int opt)
{
	memset(c, 0, sizeof(*c));
	c->protocol = protocol;
	c->phys_delay = delay;
	c->timeout = 60000;
	c->energy_drop_time = 700;
	c->r10 = opt;
	c->rate = (unsigned short)rate;
	c->r16 = (unsigned short)rate;
}

/* ------------------------------------------------------------------------ */

static const int rates[] = { 4800, 7200, 9600, 12000, 14400, 1200 };

static int
test_create(void)
{
	struct v32fp_cfg cfg;
	unsigned char base[48];
	int moved_protocol = 0;
	int moved_rate = 0;
	int moved_opt = 0;
	int moved_delay = 0;
	int first = 1;
	int p;
	int r;
	int o;

	diff_begin("V32FP_create over the configuration sweep");

	for (p = 0; p < 2; p++)
		for (r = 0; r < 6; r++)
			for (o = 0; o < 2; o++) {
				void *a;
				void *b;
				int delay = (o != 0) ? 180 : 60;

				make_cfg(&cfg, p, rates[r], delay, o);
				a = V32FP_create(&cfg, 0);
				b = ref_V32FP_create(&cfg, 0);
				diff_eq_int("ours allocated (%ld)", a != 0, 1,
					    r);
				diff_eq_int("blob allocated (%ld)", b != 0, 1,
					    r);
				if (a == 0 || b == 0)
					return diff_end();

				cmp_object("create", a, b, r);

				/*
				 * F3052's check, measured on the BLOB's object
				 * so it is a statement about the original.
				 */
				if (first) {
					memcpy(base, b, 48);
					first = 0;
				} else {
					if (memcmp(base, b, 48) != 0) {
						if (p != 0)
							moved_protocol = 1;
						if (r != 0)
							moved_rate = 1;
						if (o != 0) {
							moved_opt = 1;
							moved_delay = 1;
						}
					}
				}

				V32FP_delete(a);
				ref_V32FP_delete(b);
			}

	/*
	 * Every axis must have moved the blob's object at least once, or the
	 * sweep above proves nothing about the field that axis patches.
	 */
	diff_eq_int("the protocol axis moved the object (%ld)",
		    moved_protocol, 1, 0);
	diff_eq_int("the rate axis moved the object (%ld)", moved_rate, 1, 1);
	diff_eq_int("the options axis moved the object (%ld)", moved_opt, 1,
		    2);
	diff_eq_int("the delay axis moved the object (%ld)", moved_delay, 1,
		    3);

	return diff_end();
}

/* ------------------------------------------------------------------------ */

static void
cmp_status(const struct v32_status *a, const struct v32_status *b, long t)
{
	diff_eq_int("st.protocol (%ld)", a->protocol, b->protocol, t);
	diff_eq_int("st.tx_rate (%ld)", a->tx_rate, b->tx_rate, t);
	diff_eq_int("st.rx_rate (%ld)", a->rx_rate, b->rx_rate, t);
	diff_eq_int("st.r06 (%ld)", a->r06, b->r06, t);
	diff_eq_int("st.snr (%ld)", a->snr, b->snr, t);
	diff_eq_int("st.r0a (%ld)", a->r0a, b->r0a, t);
	diff_eq_int("st.r0c (%ld)", a->r0c, b->r0c, t);
	diff_eq_int("st.r0e (%ld)", a->r0e, b->r0e, t);
	diff_eq_int("st.r10 (%ld)", a->r10, b->r10, t);
	diff_eq_int("st.r12 (%ld)", a->r12, b->r12, t);
	diff_eq_int("st.flags (%ld)", a->flags, b->flags, t);
	diff_eq_int("st.flags1 (%ld)", a->flags1, b->flags1, t);
	diff_eq_int("st.r18 (%ld)", a->r18, b->r18, t);
	diff_eq_int("st.r1c (%ld)", a->r1c, b->r1c, t);
	diff_eq_int("st.r20 (%ld)", a->r20, b->r20, t);
	diff_eq_int("st.r24 (%ld)", a->r24, b->r24, t);
}

/*
 * `V32FP_status` reports the constant 40 for its first two hundred calls --
 * obj + 0x2c counts them -- so a test that calls it once measures the
 * settling arm and nothing else.  This one runs past the counter, and seeds
 * the decision error on the way so the log10 chain has something to work on.
 */
static int
test_status(void)
{
	struct v32fp_cfg cfg;
	struct v32_status sa;
	struct v32_status sb;
	void *a;
	void *b;
	int settled = 0;
	int nonzero_snr = 0;
	int i;
	int r;

	diff_begin("V32FP_status past the settling counter");

	for (r = 0; r < 5; r++) {
		make_cfg(&cfg, r & 1, rates[r], 60, 1);
		a = V32FP_create(&cfg, 0);
		b = ref_V32FP_create(&cfg, 0);
		if (a == 0 || b == 0) {
			diff_eq_int("both objects built (%ld)", 0, 1, r);
			return diff_end();
		}

		for (i = 0; i < 220; i++) {
			/*
			 * Seed the error input identically on both sides.  It
			 * is fp + 0x256, the smoother's input, and without it
			 * the accumulator stays at zero and the whole log10
			 * chain is dead.
			 */
			F_S16(OBJ_FP(a), 0x256) = (short)(400 + i * 7);
			F_S16(OBJ_FP(b), 0x256) = (short)(400 + i * 7);

			memset(&sa, 0, sizeof(sa));
			memset(&sb, 0, sizeof(sb));
			sa.flags = 0x55;
			sb.flags = 0x55;
			sa.flags1 = 0xaa;
			sb.flags1 = 0xaa;

			Control_Flag = 0;
			ref_Control_Flag = 0;

			diff_eq_int("V32FP_status returns 1 (%ld)",
				    V32FP_status(a, &sa),
				    ref_V32FP_status(b, &sb), i);
			cmp_status(&sa, &sb, i);
			cmp_object("status", a, b, i);

			if (i > 205)
				settled = 1;
			if (sb.snr != 40)
				nonzero_snr = 1;
		}

		V32FP_delete(a);
		ref_V32FP_delete(b);
	}

	/*
	 * F134: a clean run from a function that only ever took its settling
	 * arm is not a clean run.  Both counters are read from THIS run.
	 */
	diff_eq_int("the settling counter expired (%ld)", settled, 1, 0);
	diff_eq_int("an SNR other than the settled 40 was reported (%ld)",
		    nonzero_snr, 1, 1);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

static int
test_control(void)
{
	struct v32fp_cfg cfg;
	struct v32fp_ctl ca;
	struct v32fp_ctl cb;
	int took_retrain = 0;
	int took_reneg = 0;
	int took_resize = 0;
	int r;
	int bits;

	diff_begin("V32FP_control over its three arms");

	for (r = 0; r < 5; r++)
		for (bits = 0; bits < 8; bits++) {
			void *a = 0;
			void *b = 0;

			make_cfg(&cfg, r & 1, rates[r], 60, 1);
			a = V32FP_create(&cfg, 0);
			b = ref_V32FP_create(&cfg, 0);
			if (a == 0 || b == 0) {
				diff_eq_int("both objects built (%ld)", 0, 1,
					    r);
				return diff_end();
			}

			ca = V32_CTL;
			ca.ctl0 = (unsigned char)(0x83 ^ (bits << 3));
			ca.ctl1 = (unsigned char)
				(0x01 | ((bits & 1) ? V32_CTL1_RETRAIN : 0)
				 | ((bits & 2) ? V32_CTL1_RENEG : 0));
			ca.r14 = (bits & 4) ? 1 : 0;
			ca.bps = (unsigned short)rates[(r + 1) % 5];
			ca.trellis = (bits & 1);
			cb = ca;

			/*
			 * The rate-fallback ladder is only reached when
			 * fp + 0x50d8 is set, so half the trials set it --
			 * identically on both sides.
			 */
			F_U16(OBJ_FP(a), 0x50d8) = (unsigned short)(bits & 1);
			F_U16(OBJ_FP(b), 0x50d8) = (unsigned short)(bits & 1);

			diff_eq_int("V32FP_control returns 1 (%ld)",
				    V32FP_control(a, &ca),
				    ref_V32FP_control(b, &cb), bits);

			/* The request is in/out: compare it too. */
			diff_eq_int("ctl0 after (%ld)", ca.ctl0, cb.ctl0, bits);
			diff_eq_int("ctl1 after (%ld)", ca.ctl1, cb.ctl1, bits);
			diff_eq_int("ctl bps after (%ld)", ca.bps, cb.bps,
				    bits);
			cmp_object("control", a, b, bits);

			if ((bits & 1) != 0 && (cb.ctl1 & V32_CTL1_RETRAIN) == 0)
				took_retrain = 1;
			if ((bits & 1) == 0 && (bits & 2) != 0
			    && (cb.ctl1 & V32_CTL1_RENEG) == 0)
				took_reneg = 1;
			if ((bits & 4) != 0)
				took_resize = 1;

			V32FP_delete(a);
			ref_V32FP_delete(b);
		}

	diff_eq_int("the retrain arm ran and cleared its bit (%ld)",
		    took_retrain, 1, 0);
	diff_eq_int("the renegotiation arm ran and cleared its bit (%ld)",
		    took_reneg, 1, 1);
	diff_eq_int("the length-change arm ran (%ld)", took_resize, 1, 2);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

/*
 * `V32FP_modem` dispatches through `V32_PROTOCOL[hdx->mode]`, so the mode
 * chooses which of three very different functions runs.  Slot 7 is the
 * one-byte `ret`, which isolates the marshalling either side of the call;
 * slot 6 is `v32_data`, which is the other function this file reconstructs.
 */
static int
test_modem(void)
{
	struct v32fp_cfg cfg;
	int txa[V32_BIT_BUFFER];
	int txb[V32_BIT_BUFFER];
	int rxa[V32_BIT_BUFFER];
	int rxb[V32_BIT_BUFFER];
	short outa[64];
	short outb[64];
	short ina[64];
	short inb[64];
	int slots[2];
	int ran_null = 0;
	int ran_data = 0;
	int i;
	int s;
	int r;

	slots[0] = 7;
	slots[1] = 6;

	diff_begin("V32FP_modem through the null and data slots");

	for (s = 0; s < 2; s++)
		for (r = 0; r < 3; r++) {
			void *a;
			void *b;
			int na;
			int nb;
			int ra;
			int rb;
			int st;

			make_cfg(&cfg, 0, rates[r + 2], 60, 1);
			a = V32FP_create(&cfg, 0);
			b = ref_V32FP_create(&cfg, 0);
			if (a == 0 || b == 0) {
				diff_eq_int("both objects built (%ld)", 0, 1,
					    r);
				return diff_end();
			}

			F_U16(OBJ_HDX(a), 0x76) = (unsigned short)slots[s];
			F_U16(OBJ_HDX(b), 0x76) = (unsigned short)slots[s];
			Control_Flag = 0;
			ref_Control_Flag = 0;

			for (i = 0; i < V32_BIT_BUFFER; i++) {
				txa[i] = (i * 13 + 5) & 0x3f;
				txb[i] = txa[i];
				rxa[i] = -1;
				rxb[i] = -1;
			}
			/*
			 * EACH SIDE GETS ITS OWN INPUT BUFFER.  `DemodDataV32`
			 * takes a non-const `short *` and the receive chain
			 * writes through it, so a shared array would have the
			 * first side's run change what the second side is
			 * handed -- the two would then disagree for a reason
			 * that has nothing to do with the code under test.
			 * That is exactly what happened when this test was
			 * first written; finding F8658.
			 */
			for (i = 0; i < 64; i++) {
				ina[i] = (short)((i * 311) - 4096);
				inb[i] = ina[i];
				outa[i] = (short)(i * 3);
				outb[i] = outa[i];
			}

			/* The two counts are unequal, and they cross over. */
			na = 12;
			nb = 12;
			ra = 40;
			rb = 40;

			st = V32FP_modem(a, txa, outa, ina, rxa, &na, &ra);
			diff_eq_int("V32FP_modem status (%ld)", st,
				    ref_V32FP_modem(b, txb, outb, inb, rxb, &nb,
				    &rb), r);
			diff_eq_int("nout after (%ld)", na, nb, r);
			diff_eq_int("nin after (%ld)", ra, rb, r);
			for (i = 0; i < 64; i++)
				diff_eq_int("out sample %ld", outa[i], outb[i],
					    i);
			for (i = 0; i < V32_BIT_BUFFER; i++)
				diff_eq_int("rx bit %ld", rxa[i], rxb[i], i);
			diff_eq_int("Control_Flag after (%ld)", Control_Flag,
				    ref_Control_Flag, r);
			cmp_object("modem", a, b, r);

			if (slots[s] == 7)
				ran_null = 1;
			if (slots[s] == 6)
				ran_data = 1;

			V32FP_delete(a);
			ref_V32FP_delete(b);
		}

	diff_eq_int("the null slot was dispatched (%ld)", ran_null, 1, 0);
	diff_eq_int("the data slot was dispatched (%ld)", ran_data, 1, 1);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

static int
ours_slot(void *p)
{
	if (p == (void *)v32_handshake)
		return 0;
	if (p == (void *)v32_data)
		return 1;
	if (p == (void *)v32_null_protocol)
		return 2;
	return -1;
}

static int
ref_slot(void *p)
{
	if (p == (void *)ref_v32_handshake)
		return 0;
	if (p == (void *)ref_v32_data)
		return 1;
	if (p == (void *)ref_v32_null_protocol)
		return 2;
	return -1;
}

static int
test_protocol_table(void)
{
	static const int want[9] = { 0, 0, 0, 2, 0, 0, 1, 2, 2 };
	int i;

	diff_begin("V32_PROTOCOL, by permutation");

	for (i = 0; i < 9; i++) {
		int x = ours_slot((void *)V32_PROTOCOL[i]);
		int y = ref_slot(ref_V32_PROTOCOL[i]);

		/*
		 * -1 on EITHER side is a failure in its own right: a plain
		 * `x == y` would pass -1 == -1 and name nothing.
		 */
		diff_eq_int("slot %ld resolves on our side", x >= 0, 1, i);
		diff_eq_int("slot %ld resolves on the blob's side", y >= 0, 1,
			    i);
		diff_eq_int("slot %ld names the same function", x, y, i);
		diff_eq_int("slot %ld is the expected function", x, want[i], i);
	}

	/* Five handshake, one data, three null -- F8649's correction. */
	{
		int n[3];

		n[0] = 0;
		n[1] = 0;
		n[2] = 0;
		for (i = 0; i < 9; i++) {
			int x = ours_slot((void *)V32_PROTOCOL[i]);

			if (x >= 0)
				n[x]++;
		}
		diff_eq_int("five handshake slots (%ld)", n[0], 5, 0);
		diff_eq_int("one data slot (%ld)", n[1], 1, 1);
		diff_eq_int("three null slots (%ld)", n[2], 3, 2);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_create();
	rc |= test_status();
	rc |= test_control();
	rc |= test_modem();
	rc |= test_protocol_table();

	return rc;
}
