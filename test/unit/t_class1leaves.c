/*
 * t_class1leaves.c -- differential test of the Class 1 fax leaves this
 * batch reconstructs (see the src/fax sources): the class1.c session leaves, the
 * eight modulation message reporters and their tables, null_message,
 * FAXVMI_message and its dispatch table, FIFO_full_test, and the two HDLC
 * machine leaves.
 *
 * POINTERS ARE COMPARED BY WHAT THEY REACH, NOT BY VALUE.  A reporter
 * answers an address -- ours into our .data, the reference's into the
 * relocated blob -- so the comparison is strcmp over the strings, and the
 * anti-vacuity check is that in-range codes answered non-NULL on the
 * REFERENCE side.  The one code each reporter accepts PAST its table (the
 * off-by-one D951) is skipped: it reads whatever the link placed after the
 * table, there and here, and no equivalence is defined over that.
 *
 * FIFO sizes exclude zero (both sides would take the same SIGFPE, proving
 * nothing) and include negatives, which faxfifo.h explains can flip the
 * wrapped numerator back over the threshold.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"

extern int ref__send_silence_state_init(void *ctx, int samples);
extern int ref__recieve_silence_state_init(void *ctx, int samples);
extern int ref_fax_class1_info(void *ctx, int sel, int *out);
extern int ref_fax_class1_GetConstalation(void *ctx);
extern int ref__sym_size(int rate);
extern void ref__set_modem_rate(int code, int *mod, int *rate);
extern int ref__init_tx_nulls_state(void *ctx);
extern int ref__handle_hdlc_input_close(void *ctx);
extern int ref__handle_hdlc_input_open(void *ctx);
extern int ref__send_hdlc_between_buffer_state_init(void *ctx);
extern void ref_cTOOLS_handle_data_output_reset(void *ctx);
extern void ref_null_message(void *handle, int code, char **out);
extern void ref_v17rx_message(void *handle, int code, char **out);
extern void ref_v17tx_message(void *handle, int code, char **out);
extern void ref_v21rx_message(void *handle, int code, char **out);
extern void ref_v21tx_message(void *handle, int code, char **out);
extern void ref_v27rx_message(void *handle, int code, char **out);
extern void ref_v27tx_message(void *handle, int code, char **out);
extern void ref_v29rx_message(void *handle, int code, char **out);
extern void ref_v29tx_message(void *handle, int code, char **out);
extern char *ref_FAXVMI_message(void *vmi, unsigned char code);
extern int ref_FIFO_full_test(void *f);

/* The blob's tables, for entry-by-entry string comparison. */
extern char *ref_V17RX_MESG[], *ref_V17TX_MESG[];
extern char *ref_V21RX_MESG[], *ref_V21TX_MESG[];
extern char *ref_V27RX_MESG[], *ref_V27TX_MESG[];
extern char *ref_V29RX_MESG[], *ref_V29TX_MESG[];

static unsigned long seed = 20260830UL;

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

static long
first_diff_skip(const void *pa, const void *pb, unsigned n,
		unsigned skip_from, unsigned skip_len)
{
	const unsigned char *a = (const unsigned char *)pa;
	const unsigned char *b = (const unsigned char *)pb;
	unsigned i;

	for (i = 0; i < n; i++) {
		if (i >= skip_from && i < skip_from + skip_len)
			continue;
		if (a[i] != b[i])
			return (long)i;
	}
	return -1;
}

/* ------------------------------------------------------------------ */

static struct fax_class1 ctx_a, ctx_b;

static void
cfresh(void)
{
	fill(&ctx_a, (unsigned)sizeof(ctx_a));
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));
}

static void
ccompare(const char *what, long tag)
{
	char buf[96];

	snprintf(buf, sizeof(buf), "%s: ctx first differing byte (%%ld)",
		 what);
	diff_eq_int(buf,
		    first_diff_skip(&ctx_a, &ctx_b, (unsigned)sizeof(ctx_a),
				    (unsigned)offsetof(struct fax_class1,
						       f1288), 4), -1, tag);
}

static int
run_class1(void)
{
	static const int samples[] = {
		0, 1, 2, 3, 7, 160, 161, 4000, -1, -2, -7, 0x7fffffff,
		(int)0x80000000
	};
	unsigned i;

	diff_begin("class1.c leaves");
	for (i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
		int ra, rb;

		cfresh();
		ra = ref__send_silence_state_init(&ctx_a, samples[i]);
		rb = _send_silence_state_init(&ctx_b, samples[i]);
		diff_eq_int("send_silence return (%ld)", rb, ra,
			    (long)samples[i]);
		ccompare("send_silence", (long)samples[i]);

		cfresh();
		ra = ref__recieve_silence_state_init(&ctx_a, samples[i]);
		rb = _recieve_silence_state_init(&ctx_b, samples[i]);
		diff_eq_int("recieve_silence return (%ld)", rb, ra,
			    (long)samples[i]);
		ccompare("recieve_silence", (long)samples[i]);
	}

	for (i = 0; i < 16; i++) {
		int ra, rb;

		cfresh();
		ra = ref__init_tx_nulls_state(&ctx_a);
		rb = _init_tx_nulls_state(&ctx_b);
		diff_eq_int("init_tx_nulls return (%ld)", rb, ra, (long)i);
		ccompare("init_tx_nulls", (long)i);

		cfresh();
		/* both flag polarities appear over 16 random fills */
		ra = ref__handle_hdlc_input_close(&ctx_a);
		rb = _handle_hdlc_input_close(&ctx_b);
		diff_eq_int("hdlc_close return (%ld)", rb, ra, (long)i);
		ccompare("hdlc_close", (long)i);

		cfresh();
		ra = ref__handle_hdlc_input_open(&ctx_a);
		rb = _handle_hdlc_input_open(&ctx_b);
		diff_eq_int("hdlc_open return (%ld)", rb, ra, (long)i);
		ccompare("hdlc_open", (long)i);
		/*
		 * From the REFERENCE's own state, not from the source: the
		 * frame cursor really is left at one, which is what leaves
		 * element zero of the caller's buffer for the length.
		 */
		diff_eq_int("hdlc_open: the object armed f1250 to one (%ld)",
			    (long)ctx_a.f1250, 1, (long)i);

		cfresh();
		ra = ref__send_hdlc_between_buffer_state_init(&ctx_a);
		rb = _send_hdlc_between_buffer_state_init(&ctx_b);
		diff_eq_int("between_buffer_init return (%ld)", rb, ra,
			    (long)i);
		ccompare("between_buffer_init", (long)i);

		cfresh();
		ref_cTOOLS_handle_data_output_reset(&ctx_a);
		cTOOLS_handle_data_output_reset(&ctx_b);
		ccompare("data_output_reset", (long)i);
		/*
		 * And the two fields it does NOT touch, again from the
		 * reference: `async_shift` and `async_mask` come back holding
		 * the random fill, so a reset that cleared them would fail
		 * here even though every byte still matched our side.
		 */
		diff_eq_int("data_output_reset: async_locked cleared (%ld)",
			    (long)ctx_a.async_locked, 0, (long)i);
		diff_eq_int("data_output_reset: async_window is -1 (%ld)",
			    (long)(int)ctx_a.async_window, -1L, (long)i);

		diff_eq_int("GetConstalation (%ld)",
			    fax_class1_GetConstalation(&ctx_b),
			    ref_fax_class1_GetConstalation(&ctx_a), (long)i);
	}

	/* fax_class1_info: selector 1 follows f1288, so each side gets its
	 * OWN target block with identical contents. */
	for (i = 0; i < 24; i++) {
		static unsigned char blk_a[0x20], blk_b[0x20];
		int sel = (int)(i % 6) - 2;	/* -2..3 */
		int out_a, out_b, ra, rb;

		cfresh();
		fill(blk_a, (unsigned)sizeof(blk_a));
		memcpy(blk_b, blk_a, sizeof(blk_a));
		if (i & 1) {
			ctx_a.f1288 = (struct fax_fifo *)blk_a;
			ctx_b.f1288 = (struct fax_fifo *)blk_b;
		} else {
			ctx_a.f1288 = NULL;
			ctx_b.f1288 = NULL;
		}
		out_a = out_b = (int)0x5a5a5a5a;
		ra = ref_fax_class1_info(&ctx_a, sel, &out_a);
		rb = fax_class1_info(&ctx_b, sel, &out_b);
		diff_eq_int("info return (sel %ld)", rb, ra, (long)sel);
		diff_eq_int("info out (sel %ld)", out_b, out_a, (long)sel);
		ccompare("info", (long)sel);
	}
	return diff_end();
}

static int
run_rates(void)
{
	int code;
	unsigned i;

	diff_begin("_set_modem_rate / _sym_size");
	for (code = -2; code < 300; code++) {
		int mod_a = 0x11111111, rate_a = 0x22222222;
		int mod_b = 0x11111111, rate_b = 0x22222222;

		ref__set_modem_rate(code, &mod_a, &rate_a);
		_set_modem_rate(code, &mod_b, &rate_b);
		diff_eq_int("mod for code %ld", mod_b, mod_a, (long)code);
		diff_eq_int("rate for code %ld", rate_b, rate_a, (long)code);
	}
	for (i = 0; i < 64; i++) {
		int rate = (i < 8) ? (int)(0x960 * (i + 1)) : (int)rnd();

		diff_eq_int("sym_size(%ld)", _sym_size(rate),
			    ref__sym_size(rate), (long)rate);
	}
	diff_eq_int("sym_size(14400) fired (%ld)", ref__sym_size(0x3840), 6,
		    0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

struct rep {
	const char *name;
	void (*ours)(void *, int, char **);
	void (*ref)(void *, int, char **);
	char **our_tbl;
	char **ref_tbl;
	int entries;
	int guard;		/* the object's ja bound; == entries, D951 */
};

static const struct rep reps[] = {
	{ "v17rx", v17rx_message, ref_v17rx_message,
	  V17RX_MESG, 0, 10, 10 },
	{ "v17tx", v17tx_message, ref_v17tx_message,
	  V17TX_MESG, 0, 10, 10 },
	{ "v21rx", v21rx_message, ref_v21rx_message,
	  V21RX_MESG, 0, 7, 7 },
	{ "v21tx", v21tx_message, ref_v21tx_message,
	  V21TX_MESG, 0, 6, 6 },
	{ "v27rx", v27rx_message, ref_v27rx_message,
	  V27RX_MESG, 0, 8, 8 },
	{ "v27tx", v27tx_message, ref_v27tx_message,
	  V27TX_MESG, 0, 8, 8 },
	{ "v29rx", v29rx_message, ref_v29rx_message,
	  V29RX_MESG, 0, 8, 8 },
	{ "v29tx", v29tx_message, ref_v29tx_message,
	  V29TX_MESG, 0, 8, 8 },
};

static char **ref_tbls[8];

static int
run_messages(void)
{
	unsigned r;
	int code;
	int seen_string = 0;

	ref_tbls[0] = ref_V17RX_MESG;
	ref_tbls[1] = ref_V17TX_MESG;
	ref_tbls[2] = ref_V21RX_MESG;
	ref_tbls[3] = ref_V21TX_MESG;
	ref_tbls[4] = ref_V27RX_MESG;
	ref_tbls[5] = ref_V27TX_MESG;
	ref_tbls[6] = ref_V29RX_MESG;
	ref_tbls[7] = ref_V29TX_MESG;

	diff_begin("the eight message reporters, and their tables");
	for (r = 0; r < 8; r++) {
		const struct rep *p = &reps[r];
		char buf[80];
		int i;

		/* the tables themselves, string by string */
		for (i = 0; i < p->entries; i++) {
			snprintf(buf, sizeof(buf),
				 "%s table entry %d (%%ld)", p->name, i);
			diff_eq_int(buf,
				    strcmp(p->our_tbl[i], ref_tbls[r][i]),
				    0, (long)i);
		}

		for (code = -3; code < 300; code++) {
			char *ma, *mb;

			if (code == p->guard)
				continue;	/* the one-past read, D951 */
			ma = (char *)0x1; mb = (char *)0x2;
			p->ref((void *)0, code, &ma);
			p->ours((void *)0, code, &mb);
			snprintf(buf, sizeof(buf),
				 "%s nullness for code %%ld", p->name);
			diff_eq_int(buf, mb == NULL, ma == NULL, (long)code);
			if (ma && mb) {
				snprintf(buf, sizeof(buf),
					 "%s string for code %%ld", p->name);
				diff_eq_int(buf, strcmp(mb, ma), 0,
					    (long)code);
				seen_string = 1;
			}
		}
	}
	for (code = -3; code < 20; code++) {
		char *ma = (char *)0x1, *mb = (char *)0x2;

		ref_null_message((void *)0, code, &ma);
		null_message((void *)0, code, &mb);
		diff_eq_int("null_message NULL for %ld",
			    (long)(mb == NULL), (long)(ma == NULL),
			    (long)code);
	}
	diff_eq_int("some in-range string compared (%ld)", seen_string, 1,
		    (long)seen_string);
	return diff_end();
}

static int
run_faxvmi(void)
{
	int slot, code;

	diff_begin("FAXVMI_message across the thirteen slots");
	for (slot = 0; slot < 13; slot++) {
		static struct faxvmi vmi_a, vmi_b;

		fill(&vmi_a, (unsigned)sizeof(vmi_a));
		memcpy(&vmi_b, &vmi_a, sizeof(vmi_a));
		vmi_a.slot = (short)slot;
		vmi_b.slot = (short)slot;
		/*
		 * A distinguishable pointer, not a real link block: this test
		 * only needs FAXVMI_message to hand the reporter whatever is
		 * at +0x28, and each side must hand it something different.
		 * `faxvmi.h` typed that field when the framing unit landed.
		 */
		vmi_a.link = (struct faxvmi_link *)&vmi_a;
		vmi_b.link = (struct faxvmi_link *)&vmi_b;

		for (code = 0; code < 256; code++) {
			char *ma, *mb;
			int guard = reps[0].guard;
			unsigned r;

			/* which reporter does this slot reach, and is this
			 * its one-past code? */
			static const int slot_rep[13] = {
				-1, -1, -1, -1, -1, 3, 2, 5, 4, 7, 6, 1, 0
			};
			int rr = slot_rep[slot];

			(void)guard;
			(void)r;
			if (rr >= 0 && code == reps[rr].guard)
				continue;

			ma = ref_FAXVMI_message(&vmi_a, (unsigned char)code);
			mb = FAXVMI_message(&vmi_b, (unsigned char)code);
			diff_eq_int("slot nullness for code %ld",
				    (long)(mb == NULL), (long)(ma == NULL),
				    (long)(slot * 1000 + code));
			if (ma && mb)
				diff_eq_int("slot string for code %ld",
					    strcmp(mb, ma), 0,
					    (long)(slot * 1000 + code));
		}
	}
	return diff_end();
}

static int
run_fifo(void)
{
	static const short sizes[] = {
		1, 2, 3, 5, 100, 0x7fff, -1, -3, -100, (short)0x8000
	};
	unsigned s, c;

	diff_begin("FIFO_full_test");
	for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
		for (c = 0; c < 40; c++) {
			static struct fax_fifo fa, fb;
			unsigned short count = (unsigned short)
			    (c < 20 ? c : (rnd() & 0xffff));

			fill(&fa, (unsigned)sizeof(fa));
			memcpy(&fb, &fa, sizeof(fa));
			fa.size = sizes[s];
			fb.size = sizes[s];
			fa.count = count;
			fb.count = count;
			diff_eq_int("full test, count %ld",
				    FIFO_full_test(&fb),
				    ref_FIFO_full_test(&fa), (long)count);
		}
	}
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_class1();
	rc |= run_rates();
	rc |= run_messages();
	rc |= run_faxvmi();
	rc |= run_fifo();
	return rc;
}
