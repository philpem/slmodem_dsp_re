/*
 * data.c -- Caller ID: the message renderers.
 *
 * Reconstructed from dsplibs.o Data.c (finding F1410 pins the TU: the three
 * `data_*` functions from 0x090470 are a complete file of their own):
 *
 *   data_raw                  .text 0x090470    117
 *   data_unformatted_output   .text 0x0904f0    117
 *   data_formatted_output     .text 0x090570   1310
 *
 * WHAT THE TWO OUTER ONES ARE FOR, from the only caller either has:
 * `cid_get_strings` (0x090320) zeroes `cid_modem + 0x008` for 0x258 bytes,
 * calls one of them with `cid_modem->fsk` and that buffer, and returns the
 * buffer.  So the object argument is a `struct cid` -- the FSK receiver --
 * and `out` is 600 bytes inside the `cid_modem`.  `cid_value`'s slot at
 * +0x264 chooses: 2 takes the unformatted route, anything else the formatted
 * one.
 *
 * The message itself is `cid->data` (+0x0d8) with `cid->pack_len` (+0x15c)
 * saying how many bytes the framer stored; both were named from
 * `pack_next_bit` and are used here exactly as that name says.
 *
 * THE HEX DUMP (`data_raw`, and `data_unformatted_output` which is one
 * inlined call to it) covers buf[1] + 2 bytes -- the two-byte header plus the
 * declared length -- capped at 0xf5 = 245 BYTES, i.e. 490 output digits, so
 * the cap protects a 512-byte output buffer with a terminator to spare.
 * Digits above 9 are lowercase ('a' is 0x57 + 10).
 */

#include "dsplib/cid.h"
#include "dsplib/cid_modem.h"

void
data_raw(const char *buf, char *out)
{
	int n = buf[1] + 2;
	int i;

	if (n > 0xf5)
		n = 0xf5;

	for (i = 0; i < n; i++) {
		unsigned char hi = (unsigned char)buf[i] >> 4;
		unsigned char lo = (unsigned char)buf[i] & 0x0f;

		out[2 * i] = (char)(hi > 9 ? hi + 0x57 : hi + 0x30);
		out[2 * i + 1] = (char)(lo > 9 ? lo + 0x57 : lo + 0x30);
	}
	out[2 * i] = '\0';
}

/*
 * Hex-dump the whole received message.  The object is `data_raw` with the
 * buffer address supplied rather than passed -- the same 117 bytes with an
 * `add $0xd8` in front of them, which is one inlined call and not a second
 * copy of the loop.
 */
void
data_unformatted_output(struct cid *cid, char *out)
{
	data_raw((const char *)cid->data, out);
}

/*
 * ---------------------------------------------------------------------------
 * The two TLV walkers, AGAIN.
 *
 * `data_formatted_output` contains four inlined copies of these -- one of the
 * tag search and three of the other-than search -- and cid.c's exported
 * `_look_for` / `_look_for_other_than` are a different translation unit, so
 * GCC cannot have inlined those.  Data.c therefore had its own; they are
 * written here as statics because the object's copies carry no symbol.  The
 * bodies are src/service/cid.c's, character for character, including the
 * signed `char` length and the `short` position that make a length byte
 * >= 0x80 step backwards.
 */

static int
data_look_for(const char *buf, char tag)
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

static int
data_look_for_other_than(const char *buf, short start, int except)
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

/*
 * ---------------------------------------------------------------------------
 * The formatted renderer.
 *
 * THE TWO MESSAGE SHAPES ARE THE TWO TELCORDIA ONES, and that reading is
 * usage inference from the code's own structure rather than from any string
 * -- this function references no `.rodata` at all, so there is no format
 * string to take the author's words from.  `data[0] == 0x80` walks
 * tag-length-value entries and picks tag 1 for the date and time, tag 2 for
 * the number and tag 7 for the name; anything else reads a fixed layout of
 * MMDD at [2], HHMM at [6] and the number from [10].  That is MDMF and SDMF,
 * and the tag numbers are theirs.
 *
 * THE OUTPUT IS A RUN OF NUL-TERMINATED FIELDS, which is the shape
 * `CID_process` walks: `out += i + 1` steps past each terminator, `i` is the
 * write index within the current field, and every field is `LABEL = ` (seven
 * characters, no NUL) followed by its value.  SDMF's first two are therefore
 * exactly twelve bytes each and the object folds the pointer step to
 * `add $0xc`.
 *
 * FOUR THINGS THE OBJECT DOES THAT LOOK LIKE MISTAKES AND ARE REPRODUCED
 * (findings F8700-F8705; deviations D970, D971 and D972):
 *
 *   - MDMF reads the date field through `data[date + 2 .. date + 9]` with no
 *     test on `date`, so a message with no tag 1 renders `data[1..8]`.  The
 *     -1 is sign-extended and used as an offset; there is no guard in the
 *     object and there is none here.
 *   - the NMBR label is emitted even when tag 2 is absent, giving an empty
 *     `NMBR = `, while the NAME field is skipped whole when tag 7 is.
 *   - `i` is left at 7 after a MESG field rather than at the end of the hex,
 *     so a second MESG field starts eight bytes on and overwrites the first.
 *   - the length caps differ between the paths: the copied fields stop at
 *     255 counting the two header bytes, `data_raw`'s dump at 245.
 *
 * SIGNEDNESS IS NOT UNIFORM AND IS THE OBJECT'S.  Every length this function
 * reads for itself is a `movzbl` -- so `data` is `unsigned char`, as cid.h
 * declares it -- while the walkers and `data_raw` take a `const char *` and
 * read theirs signed.  Both readings of `data[1]` appear in this one
 * function.
 */

/* data[0] of a multiple-data-message frame; anything else is single. */
#define CID_MSG_MDMF	0x80

/* The copied fields stop here, counting the tag and length bytes. */
#define CID_FIELD_MAX	0xff

void
data_formatted_output(struct cid *cid, char *out)
{
	const unsigned char *msg = cid->data;
	short date, name, nmbr, pos;
	int i, j, n;

	if (cid->pack_len == 0)
		return;

	if (msg[0] != CID_MSG_MDMF) {
		out[0] = 'D';
		out[1] = 'A';
		out[2] = 'T';
		out[3] = 'E';
		out[4] = ' ';
		out[5] = '=';
		out[6] = ' ';
		i = 7;
		for (j = 2; j < 6; j++)
			out[i++] = (char)msg[j];
		out[i] = '\0';
		out += i + 1;

		out[0] = 'T';
		out[1] = 'I';
		out[2] = 'M';
		out[3] = 'E';
		out[4] = ' ';
		out[5] = '=';
		out[6] = ' ';
		i = 7;
		for (j = 6; j < 10; j++)
			out[i++] = (char)msg[j];
		out[i] = '\0';
		out += i + 1;

		out[0] = 'N';
		out[1] = 'M';
		out[2] = 'B';
		out[3] = 'R';
		out[4] = ' ';
		out[5] = '=';
		out[6] = ' ';
		i = 7;
		n = msg[1] + 2;
		if (n > CID_FIELD_MAX)
			n = CID_FIELD_MAX;
		for (j = 10; j < n; j++)
			out[i++] = (char)msg[j];
		out[i] = '\0';
		return;
	}

	date = (short)data_look_for((const char *)msg, 1);
	name = (short)data_look_for((const char *)msg, 7);
	nmbr = (short)data_look_for((const char *)msg, 2);

	out[0] = 'D';
	out[1] = 'A';
	out[2] = 'T';
	out[3] = 'E';
	out[4] = ' ';
	out[5] = '=';
	out[6] = ' ';
	i = 7;
	for (j = 2; j < 6; j++)
		out[i++] = (char)msg[date + j];
	out[i] = '\0';
	out += i + 1;

	out[0] = 'T';
	out[1] = 'I';
	out[2] = 'M';
	out[3] = 'E';
	out[4] = ' ';
	out[5] = '=';
	out[6] = ' ';
	i = 7;
	for (j = 6; j < 10; j++)
		out[i++] = (char)msg[date + j];
	out[i] = '\0';
	out += i + 1;

	out[0] = 'N';
	out[1] = 'M';
	out[2] = 'B';
	out[3] = 'R';
	out[4] = ' ';
	out[5] = '=';
	out[6] = ' ';
	i = 7;
	if (nmbr > 0) {
		n = msg[nmbr + 1] + 2;
		if (n > CID_FIELD_MAX)
			n = CID_FIELD_MAX;
		for (j = 2; j < n; j++)
			out[i++] = (char)msg[nmbr + j];
	}
	out[i] = '\0';

	if (name > 0) {
		out += i + 1;
		out[0] = 'N';
		out[1] = 'A';
		out[2] = 'M';
		out[3] = 'E';
		out[4] = ' ';
		out[5] = '=';
		out[6] = ' ';
		i = 7;
		n = msg[name + 1] + 2;
		if (n > CID_FIELD_MAX)
			n = CID_FIELD_MAX;
		for (j = 2; j < n; j++)
			out[i++] = (char)msg[name + j];
		out[i] = '\0';
	}

	pos = (short)data_look_for_other_than((const char *)msg, 2, 0);
	while (pos > 0) {
		if (msg[pos + 1] > 1) {
			out += i + 1;
			out[0] = 'M';
			out[1] = 'E';
			out[2] = 'S';
			out[3] = 'G';
			out[4] = ' ';
			out[5] = '=';
			out[6] = ' ';
			i = 7;
			data_raw((const char *)msg + pos, out + i);
		}
		pos = (short)data_look_for_other_than((const char *)msg, pos,
						      pos);
	}
}
