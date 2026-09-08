/*
 * t_v90cpcrc.cpp -- independent V.90 CP CRC conformance test.
 *
 * This deliberately does not compare the reconstruction with dsplibs.o.
 * ITU-T V.90 Table 14 supplies CP's framing and legal variable lengths; ITU-T
 * V.34 10.1.2.3.2/Figure 14 supplies the CRC: preload all ones, shift the
 * information bits through x^16 + x^12 + x^5 + 1, then transmit bit 0 first.
 * Each implementation is held to that oracle in its own diff group, so a
 * shared departure cannot hide behind a differential pass.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V90CP.h"

extern "C" {
void our_cp_calccrc(void *) asm("_ZN5V90CP7calcCRCEv");
int our_cp_evalcrc(void *) asm("_ZN5V90CP11evaluateCRCEv");
void ref_cp_calccrc(void *) asm("ref__ZN5V90CP7calcCRCEv");
int ref_cp_evalcrc(void *) asm("ref__ZN5V90CP11evaluateCRCEv");
}

typedef void (*calc_fn)(void *);
typedef int (*eval_fn)(void *);

#define NP(a)	((unsigned)(sizeof(a) / sizeof((a)[0])))
#define GUARD	64u

static unsigned char storage[GUARD + sizeof(V90CP) + GUARD]
	__attribute__((aligned(8)));
static unsigned int lfsr;

#if __SIZEOF_POINTER__ == 4
typedef char cp_size_is_3bc0[(sizeof(V90CP) == 0x3bc0) ? 1 : -1];
typedef char cp_bits_are_at_cb8[
	(__builtin_offsetof(V90CP, bits) == 0xcb8) ? 1 : -1];
typedef char cp_crc_is_at_3b98[
	(__builtin_offsetof(V90CP, crc) == 0x3b98) ? 1 : -1];
typedef char cp_length_is_at_3bb0[
	(__builtin_offsetof(V90CP, word_3bb0) == 0x3bb0) ? 1 : -1];
#endif

/*
 * Figure 14/V.34 numbers its register in normal polynomial order, whereas
 * V90CP's crc[k] is the wire bit at index k.  The two integer spellings are
 * therefore bit reversals: the normal residue 0x2a45 means the crc[] word
 * 0xa254.  This is a Figure-14-to-wire mapping, not an assumption imported
 * from V90MP.
 */
static unsigned int
spec_crc16(const unsigned char *bits, unsigned int n)
{
	unsigned int reg = 0xffffu;
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int feedback = (reg ^ (unsigned int)bits[i]) & 1u;

		reg >>= 1;
		if (feedback)
			reg ^= 0x8408u;
	}
	return reg & 0xffffu;
}

/* The normal Figure 14 register, retained to pin that bit-order conversion. */
static unsigned int
normal_crc16(const unsigned char *bits, unsigned int n)
{
	unsigned int reg = 0xffffu;
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int feedback = ((reg >> 15) ^ (unsigned int)bits[i]) & 1u;

		reg = (reg << 1) & 0xffffu;
		if (feedback)
			reg ^= 0x1021u;
	}
	return reg;
}

/* The fixed Table 14 framing starts, transcribed from the Recommendation. */
static const unsigned short table14_fixed_start_bits[] = {
	17, 34, 51, 68, 85, 102, 119, 136,
	153, 170, 187, 204, 221, 238, 255
};

/* Table 14: gamma is 136*m; delta selects the optional codec mask block. */
static unsigned int
table14_delta(unsigned int m, int codec)
{
	unsigned int gamma = 136u * m;

	return codec ? 2u * gamma + 136u : gamma;
}

static unsigned int
table14_final_start(unsigned int m, int codec)
{
	return 272u + table14_delta(m, codec);
}

static unsigned int
table14_word_3bb0(unsigned int m, int codec)
{
	/* CRC is the 16 bits after the final start, so this is 289 + delta. */
	return table14_final_start(m, codec) + 17u;
}

static int
table14_start_bit(unsigned int bit, unsigned int final_start)
{
	unsigned int i;

	for (i = 0; i < NP(table14_fixed_start_bits); i++)
		if (table14_fixed_start_bits[i] == bit)
			return 1;

	/* The variable mask blocks are eight whole 17-bit frames apiece. */
	for (i = 272u; i <= final_start; i += 17u)
		if (i == bit)
			return 1;
	return 0;
}

static unsigned char
next_bit(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 5) & 1u);
}

/* Bits 103:127 contain six four-bit constellation indices, excluding bit 119. */
static const unsigned short table14_constellation_bits[] = {
	103, 107, 111, 115, 120, 124
};

static void
put_lsb_value(V90CP *cp, unsigned int bit, unsigned int value)
{
	unsigned int i;

	for (i = 0; i < 4u; i++)
		cp->bits[bit + i] = (unsigned char)((value >> i) & 1u);
}

/* Lay out a Table 14 CP with a legal framing/length shape and declared m. */
static void
table14_layout(V90CP *cp, unsigned int m, int codec, unsigned int seed)
{
	unsigned int i, final_start = table14_final_start(m, codec);

	memset(storage, 0x5a, sizeof(storage));
	memset((unsigned char *)cp, 0xa5, sizeof(*cp));
	lfsr = seed | 1u;
	for (i = 0; i < V90CP_BITS; i++)
		cp->bits[i] = next_bit();
	for (i = 0; i <= 16u; i++)
		cp->bits[i] = 1;
	for (i = 0; i < NP(table14_fixed_start_bits); i++)
		cp->bits[table14_fixed_start_bits[i]] = 0;
	for (i = 272u; i <= final_start; i += 17u)
		cp->bits[i] = 0;

	/* Every encoded index is legal and the last makes their maximum exactly m. */
	for (i = 0; i < NP(table14_constellation_bits); i++)
		put_lsb_value(cp, table14_constellation_bits[i],
			      (i + 1u == NP(table14_constellation_bits)) ? m :
			      (m == 0 ? 0 : i % (m + 1u)));
	cp->bits[128] = (unsigned char)(codec != 0);
	cp->word_3bb0 = table14_word_3bb0(m, codec);
}

static int
guards_intact(void)
{
	unsigned int i;

	for (i = 0; i < GUARD; i++)
		if (storage[i] != 0x5a ||
		    storage[GUARD + sizeof(V90CP) + i] != 0x5a)
			return 0;
	return 1;
}

/* Gather the Table 14 protected bits; this is deliberately not i % 17. */
static unsigned int
table14_information_bits(const V90CP *cp, unsigned int m, int codec,
			 unsigned char *out)
{
	unsigned int i, n = 0;
	unsigned int final_start = table14_final_start(m, codec);

	for (i = 18u; i < final_start; i++) {
		if (!table14_start_bit(i, final_start))
			out[n++] = cp->bits[i] & 1u;
	}
	return n;
}

static void
seed_register(V90CP *cp)
{
	unsigned int i;

	for (i = 0; i < V90CP_CRC; i++)
		cp->crc[i] = 1;
}

static void
place_crc(V90CP *cp, unsigned int reg, int reverse)
{
	unsigned int i;
	unsigned int at = cp->word_3bb0 - V90CP_CRC;

	for (i = 0; i < V90CP_CRC; i++)
		cp->bits[at + i] = (unsigned char)
			((reg >> (reverse ? 15u - i : i)) & 1u);
}

static unsigned int
register_word(const V90CP *cp)
{
	unsigned int i, word = 0;

	for (i = 0; i < V90CP_CRC; i++)
		word |= (unsigned int)(cp->crc[i] & 1u) << i;
	return word;
}

static unsigned int
reverse16(unsigned int word)
{
	unsigned int reversed = 0;
	unsigned int i;

	for (i = 0; i < 16u; i++)
		reversed |= ((word >> i) & 1u) << (15u - i);
	return reversed;
}

static void
oracle_known_answer(long tag)
{
	static const char text[] = "123456789";
	unsigned char bits[72];
	unsigned int i, j, n = 0;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 8; j++)
			bits[n++] = (unsigned char)(((unsigned int)
				(unsigned char)text[i] >> j) & 1u);
	diff_eq_int("Figure 14/V.34 over \"123456789\" is 0x6f91 (%ld)",
		    (long)spec_crc16(bits, n), 0x6f91L, tag);
}

static void
oracle_register_order(long tag)
{
	static const struct {
		unsigned int n;
		unsigned int normal;
	} cases[] = {
		{ 240u, 0x2a45u }, { 368u, 0x08bfu },
		{ 880u, 0x6948u }, { 1648u, 0x84b7u }
	};
	unsigned char zeros[1648];
	unsigned int i;

	memset(zeros, 0, sizeof(zeros));
	for (i = 0; i < NP(cases); i++) {
		diff_eq_int("Figure 14 normal zero residue (%ld)",
			    (long)normal_crc16(zeros, cases[i].n),
			    (long)cases[i].normal, tag * 10 + (long)i);
		diff_eq_int("Figure 14 normal-to-wire bit reversal (%ld)",
			    (long)spec_crc16(zeros, cases[i].n),
			    (long)reverse16(cases[i].normal),
			    tag * 10 + (long)i);
	}
}

static int
run_subject(const char *group, const char *subject, calc_fn calc, eval_fn eval)
{
	unsigned char information[V90CP_BITS];
	int trial, order_split = 0;
	int rc;

	diff_begin(group);
	oracle_known_answer(0);
	oracle_register_order(1);

	for (trial = 0; trial < 24; trial++) {
		V90CP *cp = (V90CP *)(storage + GUARD);
		unsigned int m = (unsigned int)(trial % 6);
		int codec = (trial / 6) & 1;
		unsigned int n, reg, i, excluded, ignored, included;
		long tag = 100 + trial;
		char msg[128];

		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		n = table14_information_bits(cp, m, codec, information);
		reg = spec_crc16(information, n);

		diff_eq_int("Table 14 protected-bit count (%ld)", (long)n,
			    codec ? 368L + 256L * (long)m : 240L + 128L * (long)m,
			    tag);
		diff_eq_int("Table 14 CRC end is 289 + delta (%ld)",
			    (long)cp->word_3bb0,
			    (long)table14_word_3bb0(m, codec), tag);

		seed_register(cp);
		calc(cp);
		for (i = 0; i < V90CP_CRC; i++) {
			snprintf(msg, sizeof(msg), "%s calcCRC register bit (%ld)",
				 subject, tag * 100 + (long)i);
			diff_eq_int(msg, cp->crc[i], (reg >> i) & 1u,
				    tag * 100 + (long)i);
		}
		diff_eq_int("calcCRC preserved both object guards (%ld)",
			    guards_intact(), 1, tag);

		/*
		 * Name each side of the extent explicitly.  Cycling the positions
		 * covers frame sync, fixed and variable starts, the received CRC,
		 * fill, the type/header, a base mask, and the last mask bit.
		 */
		switch (trial % 5) {
		case 0: excluded = 5u; break;
		case 1: excluded = 34u; break;
		case 2: excluded = table14_final_start(m, codec); break;
		case 3: excluded = table14_final_start(m, codec) + 1u; break;
		default: excluded = cp->word_3bb0; break;
		}
		switch (trial % 3) {
		case 0: included = 18u; break;
		case 1: included = 137u; break;
		default: included = table14_final_start(m, codec) - 1u; break;
		}
		switch (trial % 3) {
		case 0: ignored = 5u; break;
		case 1: ignored = table14_final_start(m, codec); break;
		default: ignored = cp->word_3bb0; break;
		}

		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		cp->bits[excluded] ^= 1u;
		seed_register(cp);
		calc(cp);
		diff_eq_int("an excluded Table 14 bit does not enter calcCRC (%ld)",
			    (long)register_word(cp), (long)reg, tag);

		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		cp->bits[included] ^= 1u;
		n = table14_information_bits(cp, m, codec, information);
		seed_register(cp);
		calc(cp);
		diff_eq_int("an included Table 14 bit enters calcCRC (%ld)",
			    (long)register_word(cp),
			    (long)spec_crc16(information, n), tag);
		diff_eq_int("the included-bit perturbation changed the remainder (%ld)",
			    register_word(cp) != reg, 1, tag);

		/* Figure 14 says bit 0 is transmitted into the first CRC position. */
		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		place_crc(cp, reg, 0);
		snprintf(msg, sizeof(msg), "%s accepts the Table 14 CRC (%ld)",
			 subject, tag);
		diff_eq_int(msg, eval(cp), 1, tag);
		diff_eq_int("evaluateCRC preserved both object guards (%ld)",
			    guards_intact(), 1, tag);

		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		place_crc(cp, reg, 0);
		cp->bits[ignored] ^= 1u;
		snprintf(msg, sizeof(msg), "%s accepts an excluded-bit change (%ld)",
			 subject, tag);
		diff_eq_int(msg, eval(cp), 1, tag);

		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		place_crc(cp, reg, 0);
		cp->bits[included] ^= 1u;
		snprintf(msg, sizeof(msg), "%s rejects an information-bit change (%ld)",
			 subject, tag);
		diff_eq_int(msg, eval(cp), 0, tag);

		/* A changed transmitted CRC bit must make the received sequence bad. */
		table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
		place_crc(cp, reg, 0);
		cp->bits[table14_final_start(m, codec) + 1u] ^= 1;
		snprintf(msg, sizeof(msg), "%s rejects a changed CRC bit (%ld)",
			 subject, tag);
		diff_eq_int(msg, eval(cp), 0, tag);

		/* A reversed order is a distinct invalid encoding unless it is a palindrome. */
		if ((reg & 0xffffu) != reverse16(reg)) {
			order_split = 1;
			table14_layout(cp, m, codec, 0x5519u + (unsigned int)trial);
			place_crc(cp, reg, 1);
			snprintf(msg, sizeof(msg), "%s rejects reversed CRC order (%ld)",
				 subject, tag);
			diff_eq_int(msg, eval(cp), 0, tag);
		}
	}

	diff_eq_int("a Table 14 trial separated the CRC bit orders", order_split,
		    1, 1);
	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_subject("V90CP CRC/reconstruction", "reconstruction",
			  our_cp_calccrc, our_cp_evalcrc);
	rc |= run_subject("V90CP CRC/blob", "blob",
			  ref_cp_calccrc, ref_cp_evalcrc);
	return rc;
}
