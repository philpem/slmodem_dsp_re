/*
 * V92ConvolutionEncoder.cpp -- the V.92 trellis encoder: an empty constructor
 * and destructor, the table builder, `reset`, `inverseMap` and `process`, and
 * the two static tables.
 *
 * Reconstructed from dsplibs.o, 0x54700-0x54f30 plus .data 0x6ac0 and 0x6bc0.
 * `include/dsplib/V92ConvolutionEncoder.h` carries the object map and the
 * 0x2008 that `V92Transmitter::V92Transmitter` measures.
 *
 * THE CONSTRUCTOR AND DESTRUCTOR CLAIM IS THAT THE ORIGINAL DECLARED THEM,
 * not that they do anything.  GCC emits an out-of-line constructor or
 * destructor symbol only for a user-declared one, and the blob has C1, C2, D1
 * and D2, one byte each.  So the source declared both and left the bodies
 * empty, and the state the class needs is set up by `reset(int)` instead.
 *
 * WHAT THE THREE ARMS OF THE BUILDER ARE.  It switches on `mode` against 0, 1
 * and 2 and does nothing for anything else -- the third arm's entry is
 * `test %eax,%eax; jne <epilogue>`, so it is `case 0` and not a `default`.
 * Each arm is a nest of `for (x = 0; x <= 1; x++)` over one bit apiece, and
 * the widths differ: four state bits and two input bits for mode 0, five and
 * three for mode 1, six and four for mode 2.  Sixteen entries per state row
 * in all three, so the largest index written is 15*16+3 = 243, 31*16+7 = 503
 * and 63*16+15 = 1023 -- which is where the 1,024 of finding F1249 comes from
 * a second time, and independently.  Finding F1375.
 *
 * NOTHING HERE CLEARS EITHER ARRAY, and modes 0 and 1 leave most of each row
 * untouched.  That is the object's behaviour: there is no `memset`, no
 * clearing loop and no store outside the nest anywhere in 0x616 bytes.  The
 * slots a given mode never writes keep whatever they held, which after
 * `V92Transmitter` is whatever `sysdep_malloc` returned.
 *
 * EVERY `% 2` AND `% 4` BELOW IS SIGNED, and is written with `%` on a plain
 * `int` because that is what the object compiled: the two-round
 * `shr $31; add; and $~1; sub` idiom, not a mask.  On the values these loops
 * actually take the two readings agree, so no test can tell them apart --
 * only the instruction sequence can, and it says `%`.
 */

#include <stddef.h>

#include "dsplib/V92ConvolutionEncoder.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92CE_OFF(field, off, tag) \
	typedef char v92ce_off_##tag[ \
	    ((int)__builtin_offsetof(V92ConvolutionEncoder, field) == (off)) \
	    ? 1 : -1]

V92CE_OFF(mode,      0x0000, mode);
V92CE_OFF(state,     0x0004, state);
V92CE_OFF(output,    0x0008, output);
V92CE_OFF(nextState, 0x1008, nextstate);
typedef char v92ce_size[(sizeof(V92ConvolutionEncoder) == 0x2008) ? 1 : -1];
#endif

/*
 * The two static tables, read out of .data at 0x6ac0 and 0x6bc0 and defined
 * here in that order because initialised data comes out in declaration order.
 * `nm` calls both `D`, so neither is const; `inverseMap` indexes both with a
 * scale of four, so both are `int`.
 */
int V92ConvolutionEncoder::cosetMapping4D[64] = {
	 0,  0,  1,  1,  8,  8,  9,  9,
	 3,  2,  2,  3, 11, 10, 10, 11,
	 5,  5,  4,  4, 13, 13, 12, 12,
	 6,  7,  7,  6, 14, 15, 15, 14,
	 8,  8,  9,  9,  0,  0,  1,  1,
	11, 10, 10, 11,  3,  2,  2,  3,
	13, 13, 12, 12,  5,  5,  4,  4,
	14, 15, 15, 14,  6,  7,  7,  6
};

int V92ConvolutionEncoder::subsetLabelTable[16] = {
	0, 7, 4, 3, 5, 2, 1, 6,
	4, 3, 0, 7, 1, 6, 5, 2
};

V92ConvolutionEncoder::V92ConvolutionEncoder()
{
}

V92ConvolutionEncoder::~V92ConvolutionEncoder()
{
}

/*
 * The table builder.  Three arms, one per mode, each a nest of one-bit loops
 * from the low state bit outwards; the object strength-reduces every index
 * into a stack accumulator, which is how the term ordering below was read
 * back (`0x44(%esp) += 4` per the fourth loop, `0x50(%esp) += 0x10` per the
 * fifth, and so on).
 *
 * In all three arms the value stored into `output` is the same expression
 * that sits innermost in the `nextState` value -- one register, %ecx, carries
 * it to both stores -- so it is named once here.
 */
void
V92ConvolutionEncoder::makeStateTtransitionTable()
{
	switch (mode) {
	case 0: {
		/* Sixteen states, two input bits. */
		int s0, s1, s2, s3, i0, i1;

		for (s0 = 0; s0 <= 1; s0++)
		 for (s1 = 0; s1 <= 1; s1++)
		  for (s2 = 0; s2 <= 1; s2++)
		   for (s3 = 0; s3 <= 1; s3++)
		    for (i0 = 0; i0 <= 1; i0++)
		     for (i1 = 0; i1 <= 1; i1++) {
	int st = s0 + 2 * s1 + 4 * s2 + 8 * s3;
	int in = i0 + 2 * i1;
	int y = (s2 + i0) % 2;
	int ns = s3 + 2 * ((s0 + s3 + i1) % 2
			   + 2 * ((s1 + i1) % 2 + 2 * y));

	nextState[st * V92CONV_ROW + in] = ns;
	output[ns * V92CONV_ROW + in] = y;
		     }
		break;
	}

	case 1: {
		/* Thirty-two states, three input bits. */
		int t0, t1, t2, t3, t4, j0, j1, j2;

		for (t0 = 0; t0 <= 1; t0++)
		 for (t1 = 0; t1 <= 1; t1++)
		  for (t2 = 0; t2 <= 1; t2++)
		   for (t3 = 0; t3 <= 1; t3++)
		    for (t4 = 0; t4 <= 1; t4++)
		     for (j0 = 0; j0 <= 1; j0++)
		      for (j1 = 0; j1 <= 1; j1++)
		       for (j2 = 0; j2 <= 1; j2++) {
	int st = t0 + 2 * t1 + 4 * t2 + 8 * t3 + 16 * t4;
	int in = j0 + 2 * j1 + 4 * j2;
	int y = (t3 + j1) % 2;
	int ns = t4 + 2 * ((t0 + j1) % 2
			   + 2 * ((t1 + j0) % 2
				  + 2 * ((t2 + j2) % 2 + 2 * y)));

	nextState[st * V92CONV_ROW + in] = ns;
	output[ns * V92CONV_ROW + in] = y;
		       }
		break;
	}

	case 2: {
		/*
		 * Sixty-four states, four input bits, and the only arm with a
		 * multiply: 0x549e3's `imul` is `u2 * ((u1 + k0) % 2)`, and
		 * the 0x5c(%esp) accumulator that gains `u2` once per `k1`
		 * iteration is the strength-reduced `u2 * k1`.
		 */
		int u0, u1, u2, u3, u4, u5, k0, k1, k2, k3;

		for (u0 = 0; u0 <= 1; u0++)
		 for (u1 = 0; u1 <= 1; u1++)
		  for (u2 = 0; u2 <= 1; u2++)
		   for (u3 = 0; u3 <= 1; u3++)
		    for (u4 = 0; u4 <= 1; u4++)
		     for (u5 = 0; u5 <= 1; u5++)
		      for (k0 = 0; k0 <= 1; k0++)
		       for (k1 = 0; k1 <= 1; k1++)
			for (k2 = 0; k2 <= 1; k2++)
			 for (k3 = 0; k3 <= 1; k3++) {
	int st = u0 + 2 * u1 + 4 * u2 + 8 * u3 + 16 * u4 + 32 * u5;
	int in = k0 + 2 * k1 + 4 * k2 + 8 * k3;
	int y = (u4 + u2 + k1) % 2;
	int ns = (u0 + u1 + u2 * ((u1 + k0) % 2) + k3) % 2
		 + 2 * ((u0 + u1 + u3 + k2 + u2 * k1) % 2
			+ 2 * ((u1 + k0 + u2) % 2
			       + 2 * (u2 + 2 * (u5 + 2 * y))));

	nextState[st * V92CONV_ROW + in] = ns;
	output[ns * V92CONV_ROW + in] = y;
			 }
		break;
	}
	}
}

/*
 * `mode` is stored first, the tables are rebuilt from it, and `state` is
 * zeroed last.  The call sits between the two stores in the object, so the
 * order is not a stylistic choice here: the builder reads +0x00 and would
 * build the wrong tables if it ran first, and a `state` cleared before the
 * call would still be zero after it, which is why only the first half of the
 * order is observable at all.
 */
void
V92ConvolutionEncoder::reset(int m)
{
	mode = m;
	makeStateTtransitionTable();
	state = 0;
}

/*
 * The 4D coset lookup.  Four residues, each the positive-modulo idiom
 * `((x + 2) % 4 + 4) % 4` -- two rounds of the signed power-of-two remainder,
 * which is why the object has eight `js` branches here and not four.  Adding
 * two before the reduction is a 90-degree rotation of the quadrant.
 *
 * The indices are closed: the residues are 0..3 so `subsetLabelTable` is
 * indexed 0..15, and its sixteen values are all 0..7 so `8 * a + b` is 0..63.
 * Nothing a caller can pass reaches outside either table.
 */
int
V92ConvolutionEncoder::inverseMap(int *in)
{
	int r0 = ((in[0] + 2) % 4 + 4) % 4;
	int r1 = ((in[1] + 2) % 4 + 4) % 4;
	int r2 = ((in[2] + 2) % 4 + 4) % 4;
	int r3 = ((in[3] + 2) % 4 + 4) % 4;
	int a = subsetLabelTable[r0 + 4 * r1];
	int b = subsetLabelTable[r2 + 4 * r3];

	return cosetMapping4D[8 * a + b];
}

/*
 * One symbol through the trellis.  `inverseMap` gives a coset label 0..15,
 * the switch narrows it to as many bits as the mode's input alphabet has, and
 * the two tables then carry the state forward and produce the output.
 *
 * THE DEFAULT ARM IS THE DEVIATION, D262.  The store to `state` and the
 * `return` are after the switch and are reached by a `mode` the switch does
 * not name, at which point `i` was never written -- and the object agrees, in
 * the most literal way available:
 *
 *     54f75:  89 7b 04    mov %edi,0x4(%ebx)
 *     54f78:  89 f0       mov %esi,%eax
 *
 * two callee-saved registers that nothing on that path assigned, stored into
 * the object and returned.  There is no load from either table on that arm,
 * because with `i` undefined the loads are undefined too and GCC dropped
 * them.  Reproducing it means leaving `i` uninitialised, so the warning is
 * silenced here and recorded rather than repaired: initialising it, or adding
 * a `default:`, would be a different function.  The pragma is GCC 4 syntax
 * and this tree's gcc-3.4.3 ignores it with a warning of its own, which
 * `make similarity` tolerates -- the same trade as V92Precoder::process.
 *
 * The test never drives an unnamed mode through here, on purpose: the two
 * sides would be comparing two different pieces of stack, which is a
 * difference the fixture created and not one the object has.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
int
V92ConvolutionEncoder::process(int *in)
{
	int r = inverseMap(in);
	int ns;
	int i;

	switch (mode) {
	case 0:
		i = r % 4;
		break;
	case 1:
		i = (r & 3) | ((r & 8) >> 1);
		break;
	case 2:
		i = r % 16;
		break;
	}

	ns = nextState[(state << 4) + i];
	state = ns;
	return output[(ns << 4) + i];
}
#pragma GCC diagnostic pop
