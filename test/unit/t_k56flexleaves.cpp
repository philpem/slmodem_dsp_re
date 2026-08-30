/*
 * t_k56flexleaves.cpp -- differential test of the six K56flex leaves claimed
 * by the VPcmV34Main leaf pass:
 *
 *     K56FlexFloModem::K56FlexFloModem(Pv, i, P19_tagModemParameters)
 *                                     0x10170 C2 / 0x10180 C1, `c3` each
 *     K56FlexFloModem::~K56FlexFloModem   0x10190 D2 / 0x101a0 D1, `c3` each
 *     K56FlexFloModem::getK56MPsReceiver  0x10290, `31 c0 c3`
 *     K56FLEX_SessionTermination          0x102e0, `31 c0 c3`
 *
 * The interesting claim about a stub is what it does NOT do (the argument at
 * the head of K56FlexFloModem.cpp): both sides get seeded storage and every
 * call must leave every byte of it exactly as seeded, take nothing through
 * its pointer arguments, and -- for the two `31 c0 c3` bodies -- answer
 * zero.  t_v90leaves drives `externalReset` the same way.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/K56FlexFloModem.h"

extern "C" {
void our_k56_ctor1(void *self, void *a, int b, void *c)
	asm("_ZN15K56FlexFloModemC1EPviP19_tagModemParameters");
void our_k56_ctor2(void *self, void *a, int b, void *c)
	asm("_ZN15K56FlexFloModemC2EPviP19_tagModemParameters");
void our_k56_dtor1(void *self) asm("_ZN15K56FlexFloModemD1Ev");
void our_k56_dtor2(void *self) asm("_ZN15K56FlexFloModemD2Ev");
void ref_k56_ctor1(void *self, void *a, int b, void *c)
	asm("ref__ZN15K56FlexFloModemC1EPviP19_tagModemParameters");
void ref_k56_ctor2(void *self, void *a, int b, void *c)
	asm("ref__ZN15K56FlexFloModemC2EPviP19_tagModemParameters");
void ref_k56_dtor1(void *self) asm("ref__ZN15K56FlexFloModemD1Ev");
void ref_k56_dtor2(void *self) asm("ref__ZN15K56FlexFloModemD2Ev");
int ref_k56_getmps(void *self)
	asm("ref__ZN15K56FlexFloModem17getK56MPsReceiverEv");
int ref_k56_sessterm(void) asm("ref_K56FLEX_SessionTermination");
}

#define SLOT	96

static unsigned char slot[2][SLOT];
static unsigned char arg1[2][SLOT];
static unsigned char arg3[2][SLOT];
static unsigned char before[SLOT];

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
seed(long trial)
{
	unsigned i;

	lfsr = 0x6b1u ^ (unsigned)trial * 0x9e37u;
	for (i = 0; i < SLOT; i++) {
		slot[0][i] = nextb();
		arg1[0][i] = nextb();
		arg3[0][i] = nextb();
	}
	memcpy(slot[1], slot[0], SLOT);
	memcpy(arg1[1], arg1[0], SLOT);
	memcpy(arg3[1], arg3[0], SLOT);
	memcpy(before, slot[0], SLOT);
}

static void
check_untouched(const char *what, long tag)
{
	diff_eq_int("the storage agrees (%ld)",
		    memcmp(slot[0], slot[1], SLOT) == 0, 1, tag);
	diff_eq_int("and neither side wrote it (%ld)",
		    memcmp(slot[0], before, SLOT) == 0, 1, tag);
	diff_eq_int("the pointer arguments were not written (%ld)",
		    memcmp(arg1[0], arg1[1], SLOT) == 0
		    && memcmp(arg3[0], arg3[1], SLOT) == 0, 1, tag);
	(void)what;
}

#define NTRIAL	8

int
main(void)
{
	int rc, trial;

	diff_begin("the six K56flex leaves");

	for (trial = 0; trial < NTRIAL; trial++) {
		long tag = trial * 10;
		int r0, r1;

		seed(tag);
		our_k56_ctor1(slot[0], arg1[0], 0x1234 + trial, arg3[0]);
		ref_k56_ctor1(slot[1], arg1[1], 0x1234 + trial, arg3[1]);
		check_untouched("C1", tag);

		seed(tag + 1);
		our_k56_ctor2(slot[0], arg1[0], -trial, arg3[0]);
		ref_k56_ctor2(slot[1], arg1[1], -trial, arg3[1]);
		check_untouched("C2", tag + 1);

		seed(tag + 2);
		our_k56_dtor1(slot[0]);
		ref_k56_dtor1(slot[1]);
		check_untouched("D1", tag + 2);

		seed(tag + 3);
		our_k56_dtor2(slot[0]);
		ref_k56_dtor2(slot[1]);
		check_untouched("D2", tag + 3);

		seed(tag + 4);
		r0 = ((K56FlexFloModem *)(void *)slot[0])->getK56MPsReceiver();
		r1 = ref_k56_getmps(slot[1]);
		diff_eq_int("getK56MPsReceiver returns (%ld)", r0, r1,
			    tag + 4);
		diff_eq_int("... and it is zero (%ld)", r0, 0, tag + 4);
		check_untouched("getK56MPsReceiver", tag + 4);

		seed(tag + 5);
		r0 = K56FLEX_SessionTermination();
		r1 = ref_k56_sessterm();
		diff_eq_int("K56FLEX_SessionTermination returns (%ld)", r0,
			    r1, tag + 5);
		diff_eq_int("... and it is zero (%ld)", r0, 0, tag + 5);
		check_untouched("K56FLEX_SessionTermination", tag + 5);
	}

	rc = diff_end();
	return rc;
}
