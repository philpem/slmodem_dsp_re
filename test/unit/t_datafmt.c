/*
 * t_datafmt.c -- differential test of Data.c's two message renderers:
 *
 *   data_unformatted_output   .text 0x0904f0    117
 *   data_formatted_output     .text 0x090570   1310
 *
 * `data_raw`, the third function of that translation unit, is driven by
 * t_cidleaves.c and is not repeated here; what IS repeated is the inlined
 * copy of it that ends every MESG field, which only this test reaches.
 *
 * THE MESSAGES ARE BUILT, NOT FUZZED RAW, and for the reason t_cidleaves.c
 * gives: a length byte is read as a SIGNED char by the TLV walk, so -2 leaves
 * the position where it was and the walk never terminates.  Both sides would
 * hang identically and the test would prove nothing while looking like an
 * infrastructure fault.  Every generated entry therefore has a length of
 * 0..15; the hostile shapes are hand-built, bounded, and chosen so the
 * position still advances.
 *
 * THE OBJECT IS EMBEDDED IN A LARGER BOX because both functions read past
 * `cid->data`.  The copy loops stop at 255 counting the two header bytes, so
 * an offset-126 entry with a 0xff length byte reads to `data[380]` -- 164
 * bytes beyond the 0x160 the receiver is allocated.  512 bytes of randomly
 * filled slack sit behind the struct so those reads land on defined memory
 * that is identical on both sides.  That is D956's rule: the fixture is sized
 * from the bound the OBJECT imposes, not from what looks comfortable.
 *
 * THE OUTPUT BUFFER IS 4096 AND THE OBJECT'S OWN IS 600.  `cid_get_strings`
 * zeroes 0x258 bytes at `cid_modem + 0x008` and passes that; the worst case
 * reachable here is 12 + 12 + 261 + 261 for the four fixed fields plus eight
 * bytes per leftover tag and one 498-byte MESG, about 1550.  Nothing this
 * test feeds gets near either number, and the whole 4096 is compared, so a
 * write outside the intended region is a failure rather than a silence.
 *
 * NINE THINGS, all counted off the REFERENCE output and asserted at the end
 * -- a renderer that emitted nothing, or one that never reached the MDMF
 * half, would pass every byte comparison and fail these:
 *
 *   - the empty case: pack_len 0 leaves the buffer untouched
 *   - both message shapes render
 *   - NAME present, and NAME absent (the field is skipped whole)
 *   - an empty NMBR (the label is emitted even with no tag 2)
 *   - a MESG field, and two of them, which is where the object overwrites
 *   - both length caps fire: 245 on the SDMF number, 253 on an MDMF field
 *   - the missing-date quirk, checked against the bytes it must have read
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cid.h"
#include "dsplib/cid_modem.h"

extern void ref_data_unformatted_output(void *cid, char *out);
extern void ref_data_formatted_output(void *cid, char *out);

#define SLACK	512
#define OUTN	4096
#define FILLER	0x5a

struct box {
	struct cid cid;
	unsigned char slack[SLACK];
};

struct outbuf {
	char b[OUTN];
};

static struct box boxa, boxb, boxc;
static struct outbuf outa, outb;

static unsigned long seed = 20260831UL;

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

/* Anti-vacuity counters, every one read off the reference side. */
static int cov_nothing;
static int cov_sdmf;
static int cov_mdmf;
static int cov_name;
static int cov_noname;
static int cov_mesg;
static int cov_mesg2;
static int cov_nmbr_empty;
static int cov_cap245;
static int cov_cap253;
static int cov_negdate;
static int cov_unfmt;

/* ------------------------------------------------------------------ */

/*
 * The untouched part of the output is FILLER, never NUL, so a search over
 * the whole buffer can only match bytes the renderer wrote.
 */
static int
count_sub(const char *hay, int n, const char *needle)
{
	int len = (int)strlen(needle);
	int i, c = 0;

	for (i = 0; i + len <= n; i++)
		if (memcmp(hay + i, needle, (size_t)len) == 0)
			c++;
	return c;
}

/* Offset of the first occurrence, or -1. */
static int
find_sub(const char *hay, int n, const char *needle)
{
	int len = (int)strlen(needle);
	int i;

	for (i = 0; i + len <= n; i++)
		if (memcmp(hay + i, needle, (size_t)len) == 0)
			return i;
	return -1;
}

/*
 * How many bytes the first `label` field carries before its terminator, or
 * -1 if the label is absent and -2 if there is no terminator in range --
 * which would itself be a defect worth failing on.
 */
static int
value_len(const char *hay, int n, const char *label)
{
	int at = find_sub(hay, n, label);
	int i;

	if (at < 0)
		return -1;
	at += (int)strlen(label);
	for (i = at; i < n; i++)
		if (hay[i] == '\0')
			return i - at;
	return -2;
}

/* ------------------------------------------------------------------ */

/*
 * Plant a message.  `n` may exceed sizeof(cid->data) -- the framer's own
 * `pack_len` is not bounded against it either -- so the copy goes through a
 * byte pointer into the box rather than through the array.
 */
static void
setup(const unsigned char *m, int n, short pack_len)
{
	unsigned char *d = (unsigned char *)&boxa;
	int i;

	fill(&boxa, (unsigned)sizeof(boxa));
	for (i = 0; i < n; i++)
		d[offsetof(struct cid, data) + i] = m[i];
	boxa.cid.pack_len = pack_len;
	memcpy(&boxb, &boxa, sizeof(boxa));
	memcpy(&boxc, &boxa, sizeof(boxa));
}

static void
compare(const char *what, long tag)
{
	diff_eq_obj(what, struct outbuf, &outb, &outa, tag);
	diff_eq_obj(what, struct box, &boxb, &boxa, tag);
	/* Neither renderer writes the receiver; boxc is the pre-call copy. */
	diff_eq_obj(what, struct box, &boxa, &boxc, tag);
}

static void
run_fmt(const char *what, long tag, int mdmf)
{
	int nmesg, vl;

	memset(&outa, FILLER, sizeof(outa));
	memset(&outb, FILLER, sizeof(outb));
	ref_data_formatted_output(&boxa.cid, outa.b);
	data_formatted_output(&boxb.cid, outb.b);
	compare(what, tag);

	if (outa.b[0] == (char)FILLER) {
		cov_nothing++;
		return;
	}
	if (mdmf)
		cov_mdmf++;
	else
		cov_sdmf++;

	nmesg = count_sub(outa.b, OUTN, "MESG = ");
	if (nmesg >= 1)
		cov_mesg++;
	if (nmesg >= 2)
		cov_mesg2++;
	if (find_sub(outa.b, OUTN, "NAME = ") >= 0)
		cov_name++;
	else if (mdmf)
		cov_noname++;

	vl = value_len(outa.b, OUTN, "NMBR = ");
	diff_eq_int("NMBR field terminated (%ld)", vl >= 0, 1, tag);
	if (vl == 0)
		cov_nmbr_empty++;
	if (vl == 245)
		cov_cap245++;
	if (vl == 253)
		cov_cap253++;
	if (value_len(outa.b, OUTN, "NAME = ") == 253)
		cov_cap253++;
}

static void
run_unfmt(const char *what, long tag)
{
	memset(&outa, FILLER, sizeof(outa));
	memset(&outb, FILLER, sizeof(outb));
	ref_data_unformatted_output(&boxa.cid, outa.b);
	data_unformatted_output(&boxb.cid, outb.b);
	compare(what, tag);
	if (outa.b[0] != (char)FILLER && outa.b[0] != '\0')
		cov_unfmt++;
}

/*
 * Assert the reference produced exactly this run of NUL-terminated fields and
 * touched nothing after it.  An oracle independent of both implementations:
 * it is what the protocol says the message means.
 */
static void
expect(const char *what, const char *want, int wantn)
{
	char lbl[96];
	int i;

	snprintf(lbl, sizeof(lbl), "%s: the fields the protocol says (%%ld)",
		 what);
	diff_eq_int(lbl, memcmp(outa.b, want, (size_t)wantn), 0, wantn);
	for (i = wantn; i < OUTN; i++)
		if (outa.b[i] != (char)FILLER)
			break;
	snprintf(lbl, sizeof(lbl), "%s: nothing written past them (%%ld)",
		 what);
	diff_eq_int(lbl, i, OUTN, wantn);
}

/* ------------------------------------------------------------------ */
/* hand-built messages                                                 */

/* SDMF: type, length, MMDD, HHMM, then the number. */
static const unsigned char sdmf_full[] = {
	0x04, 15,
	'0', '8', '3', '1', '1', '2', '3', '4',
	'5', '5', '5', '1', '2', '3', '4'
};

/* The same with a zero length byte: the number field comes out empty. */
static const unsigned char sdmf_empty[] = {
	0x04, 0,
	'0', '8', '3', '1', '1', '2', '3', '4'
};

/*
 * MDMF: type 0x80, length, then (tag, len, value).  Tag 1 is the date and
 * time, 2 the number, 7 the name; `length` is set so the walk reaches the
 * last entry, which is what the object's `pos < data[1]` bound requires.
 */
static const unsigned char mdmf_full[] = {
	0x80, 28,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	2, 7, '5', '5', '5', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E'
};

/* No tag 1: DATE and TIME are read from data[1..8] with no guard at all. */
static const unsigned char mdmf_nodate[] = {
	0x80, 18,
	2, 7, '5', '5', '5', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E'
};

/* No tag 2: the NMBR label is still emitted, with nothing after it. */
static const unsigned char mdmf_nonmbr[] = {
	0x80, 19,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E'
};

/* No tag 7: the NAME field is skipped entirely, label included. */
static const unsigned char mdmf_noname[] = {
	0x80, 21,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	2, 7, '5', '5', '5', '1', '2', '3', '4'
};

/* One leftover tag, four bytes: one MESG field of eight hex digits. */
static const unsigned char mdmf_mesg[] = {
	0x80, 34,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	2, 7, '5', '5', '5', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E',
	11, 4, 'A', 'B', 'C', 'D'
};

/* Two leftover tags: the second MESG starts eight bytes on and overwrites. */
static const unsigned char mdmf_mesg2[] = {
	0x80, 36,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	2, 7, '5', '5', '5', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E',
	11, 2, 'A', 'B',
	12, 2, 'C', 'D'
};

/* A leftover tag whose length is 1 and one whose length is 0: both skipped. */
static const unsigned char mdmf_mesg_short[] = {
	0x80, 32,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	2, 7, '5', '5', '5', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E',
	11, 1, 'A',
	12, 0
};

/*
 * The signedness split in one message.  The MESG guard reads the length
 * UNSIGNED -- 0xff passes `> 1` -- and the inlined data_raw then reads the
 * same byte SIGNED and dumps -1 + 2 = 1 byte.  Two hex digits, not 512.
 */
static const unsigned char mdmf_mesg_ff[] = {
	0x80, 29,
	1, 8, '0', '8', '3', '1', '1', '2', '3', '4',
	2, 7, '5', '5', '5', '1', '2', '3', '4',
	7, 5, 'A', 'L', 'I', 'C', 'E',
	11, 0xff, 'Z'
};

/* ------------------------------------------------------------------ */

static int
run_hand(void)
{
	static unsigned char m[400];
	int i;

	diff_begin("data_formatted_output on built messages");

	/* Nothing at all: pack_len is what says a message arrived. */
	setup(mdmf_full, (int)sizeof(mdmf_full), 0);
	run_fmt("pack_len 0", 0, 1);

	setup(sdmf_full, (int)sizeof(sdmf_full), 17);
	run_fmt("sdmf full", 1, 0);
	expect("sdmf full", "DATE = 0831\0TIME = 1234\0NMBR = 5551234\0", 39);

	setup(sdmf_full, (int)sizeof(sdmf_full), -1);
	run_fmt("sdmf full, pack_len -1", 2, 0);

	setup(sdmf_empty, (int)sizeof(sdmf_empty), 10);
	run_fmt("sdmf empty number", 3, 0);
	expect("sdmf empty", "DATE = 0831\0TIME = 1234\0NMBR = \0", 32);

	/* The 255-byte cap on the SDMF number: 245 bytes survive it. */
	for (i = 0; i < (int)sizeof(m); i++)
		m[i] = (unsigned char)('0' + (i % 10));
	m[0] = 0x04;
	m[1] = 0xff;
	setup(m, 300, 300);
	run_fmt("sdmf capped number", 4, 0);

	/* And one just under it. */
	m[1] = 0x80;
	setup(m, 300, 300);
	run_fmt("sdmf long number", 5, 0);

	setup(mdmf_full, (int)sizeof(mdmf_full), 28);
	run_fmt("mdmf full", 6, 1);
	expect("mdmf full",
	       "DATE = 0831\0TIME = 1234\0NMBR = 5551234\0NAME = ALICE\0", 52);

	setup(mdmf_nodate, (int)sizeof(mdmf_nodate), 18);
	run_fmt("mdmf no date", 7, 1);
	/*
	 * date is -1, so DATE takes data[1..4] and TIME data[5..8]: the
	 * length byte, the first entry's tag and length, and the first two
	 * value bytes.  Asserted against the message, not against either
	 * implementation.
	 */
	diff_eq_int("no-date DATE reads data[1..4]",
		    memcmp(outa.b + 7, mdmf_nodate + 1, 4), 0, 7);
	diff_eq_int("no-date TIME reads data[5..8]",
		    memcmp(outa.b + 19, mdmf_nodate + 5, 4), 0, 7);
	cov_negdate++;

	setup(mdmf_nonmbr, (int)sizeof(mdmf_nonmbr), 19);
	run_fmt("mdmf no number", 8, 1);
	expect("mdmf no number",
	       "DATE = 0831\0TIME = 1234\0NMBR = \0NAME = ALICE\0", 45);

	setup(mdmf_noname, (int)sizeof(mdmf_noname), 21);
	run_fmt("mdmf no name", 9, 1);
	expect("mdmf no name",
	       "DATE = 0831\0TIME = 1234\0NMBR = 5551234\0", 39);

	setup(mdmf_mesg, (int)sizeof(mdmf_mesg), 34);
	run_fmt("mdmf one mesg", 10, 1);
	expect("mdmf one mesg",
	       "DATE = 0831\0TIME = 1234\0NMBR = 5551234\0NAME = ALICE\0"
	       "MESG = 0b0441424344\0", 72);

	setup(mdmf_mesg2, (int)sizeof(mdmf_mesg2), 38);
	run_fmt("mdmf two mesg", 11, 1);
	/*
	 * `i` is left at 7 after a MESG field rather than at the end of the
	 * hex, so the second field starts eight bytes into the first and
	 * overwrites all but its label.  The object does this; so do we.
	 */
	expect("mdmf two mesg",
	       "DATE = 0831\0TIME = 1234\0NMBR = 5551234\0NAME = ALICE\0"
	       "MESG = 0MESG = 0c024344\0", 76);

	setup(mdmf_mesg_short, (int)sizeof(mdmf_mesg_short), 32);
	run_fmt("mdmf mesg too short", 12, 1);
	diff_eq_int("length 1 and 0 leftovers emit no MESG",
		    count_sub(outa.b, OUTN, "MESG = "), 0, 12);

	setup(mdmf_mesg_ff, (int)sizeof(mdmf_mesg_ff), 32);
	run_fmt("mdmf mesg length 0xff", 13, 1);
	expect("mdmf mesg 0xff",
	       "DATE = 0831\0TIME = 1234\0NMBR = 5551234\0NAME = ALICE\0"
	       "MESG = 0b\0", 62);

	/*
	 * The 255-byte cap on an MDMF field: a tag 2 entry whose length byte
	 * is 0xff copies 253 bytes and reads well past the receiver.
	 */
	memset(m, 0, sizeof(m));
	m[0] = 0x80;
	m[1] = 14;
	m[2] = 1;
	m[3] = 8;
	for (i = 0; i < 8; i++)
		m[4 + i] = (unsigned char)('0' + i);
	m[12] = 2;
	m[13] = 0xff;
	for (i = 14; i < (int)sizeof(m); i++)
		m[i] = (unsigned char)('a' + (i % 26));
	setup(m, (int)sizeof(m), 300);
	run_fmt("mdmf capped number", 14, 1);

	/*
	 * THE ONLY SHAPE THAT SEPARATES `except == 0` FROM ANY OTHER VALUE.
	 * The other-than walk starts at 2 and every well-formed entry moves it
	 * forward, so position 0 -- and every other small position -- is
	 * unreachable and the exclusion never fires.  A length byte of -3 on
	 * the first entry steps it back to 1, where the length byte itself is
	 * then read as a tag; the walk returns 1 and MESG dumps from there.
	 * Without this case a mutant excluding 1 instead of 0 survived the
	 * whole suite.  Every step after that one is forward, so it stops.
	 */
	memset(m, 0, sizeof(m));
	for (i = 20; i < 40; i++)
		m[i] = (unsigned char)('0' + (i % 10));
	m[0] = 0x80;
	m[1] = 20;
	m[2] = 7;
	m[3] = 0xfd;			/* -3: the walk steps back to 1 */
	for (i = 4; i < 10; i++)
		m[i] = (unsigned char)('a' + i);
	m[10] = 9;
	m[11] = 2;
	m[12] = 'x';
	m[13] = 'y';
	m[14] = 1;
	m[15] = 2;
	m[16] = 'A';
	m[17] = 'B';
	m[18] = 2;
	m[19] = 0;
	setup(m, 40, 20);
	run_fmt("mdmf walk steps back to 1", 16, 1);
	diff_eq_int("stepping back to 1 emitted two MESG fields (%ld)",
		    count_sub(outa.b, OUTN, "MESG = "), 2, 16);

	/* A negative length byte that still advances the walk (-1 -> +1). */
	memset(m, 0, sizeof(m));
	m[0] = 0x80;
	m[1] = 20;
	m[2] = 5;
	m[3] = 0xff;
	for (i = 4; i < 40; i++)
		m[i] = (unsigned char)(3 + i);
	setup(m, 40, 20);
	run_fmt("mdmf negative length", 15, 1);

	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * Random but well-formed: entries of 0..15 bytes so the walk always
 * advances, tags drawn from 0..12 so 1, 2 and 7 turn up alongside the ones
 * that become MESG fields.
 */
static int
build_mdmf(unsigned char *m, int cap)
{
	int pos = 2;

	m[0] = 0x80;
	while (pos < cap - 20) {
		int len = (int)(rnd() % 16UL);

		m[pos] = (unsigned char)(rnd() % 13UL);
		m[pos + 1] = (unsigned char)len;
		fill(m + pos + 2, (unsigned)len);
		pos += len + 2;
		if (rnd() % 4UL == 0)
			break;
	}
	m[1] = (unsigned char)pos;
	return pos;
}

static int
run_fuzz(void)
{
	static unsigned char m[128];
	int t;

	diff_begin("data_formatted_output / data_unformatted_output, fuzzed");
	for (t = 0; t < 300; t++) {
		int n = build_mdmf(m, 110);

		setup(m, n, (short)(rnd() % 400UL));
		run_fmt("mdmf fuzz", t, 1);
		setup(m, n, (short)(rnd() % 400UL));
		run_unfmt("mdmf fuzz, unformatted", t);
	}
	for (t = 0; t < 300; t++) {
		int n = 2 + (int)(rnd() % 120UL);

		fill(m, (unsigned)sizeof(m));
		do
			m[0] = (unsigned char)(rnd() & 0xff);
		while (m[0] == 0x80);
		setup(m, n, (short)(rnd() % 400UL));
		run_fmt("sdmf fuzz", t, 0);
		setup(m, n, (short)(rnd() % 400UL));
		run_unfmt("sdmf fuzz, unformatted", t);
	}
	return diff_end();
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_hand();
	rc |= run_fuzz();

	diff_begin("both renderers actually rendered");
	diff_eq_int("pack_len 0 wrote nothing (%ld)", cov_nothing > 0, 1,
		    cov_nothing);
	diff_eq_int("SDMF messages rendered (%ld)", cov_sdmf > 0, 1,
		    cov_sdmf);
	diff_eq_int("MDMF messages rendered (%ld)", cov_mdmf > 0, 1,
		    cov_mdmf);
	diff_eq_int("a NAME field was emitted (%ld)", cov_name > 0, 1,
		    cov_name);
	diff_eq_int("a missing tag 7 skipped the NAME field (%ld)",
		    cov_noname > 0, 1, cov_noname);
	diff_eq_int("an empty NMBR field was emitted (%ld)",
		    cov_nmbr_empty > 0, 1, cov_nmbr_empty);
	diff_eq_int("a MESG field was emitted (%ld)", cov_mesg > 0, 1,
		    cov_mesg);
	diff_eq_int("two MESG fields were emitted (%ld)", cov_mesg2 > 0, 1,
		    cov_mesg2);
	diff_eq_int("the SDMF number hit the 255 cap (%ld)", cov_cap245 > 0,
		    1, cov_cap245);
	diff_eq_int("an MDMF field hit the 255 cap (%ld)", cov_cap253 > 0, 1,
		    cov_cap253);
	diff_eq_int("the missing-date read was checked (%ld)",
		    cov_negdate > 0, 1, cov_negdate);
	diff_eq_int("data_unformatted_output produced digits (%ld)",
		    cov_unfmt > 0, 1, cov_unfmt);
	rc |= diff_end();

	return rc;
}
