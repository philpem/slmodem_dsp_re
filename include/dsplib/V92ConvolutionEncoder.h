/*
 * V92ConvolutionEncoder.h -- the V.92 trellis encoder and its state
 * transition table.
 *
 * Reconstructed from dsplibs.o.  Six members and two static tables; all six
 * and both tables are now defined.
 *
 * A ONE-BYTE CONSTRUCTOR IS STILL A DECLARATION.  GCC emits an out-of-line
 * constructor or destructor symbol only for a USER-DECLARED one; an implicit
 * trivial destructor produces no symbol at all.  The blob has C1, C2, D1 and
 * D2 for this class, each one byte, so the original declared both and wrote
 * empty bodies.  That is the whole content of the claim, and the test that
 * carries it asserts the pair writes NOTHING -- not one byte of a seeded
 * object of the full 0x2008, on either side.
 *
 * THE OBJECT IS 0x2008 BYTES, measured:
 *
 *     53be1:  c7 04 24 08 20 00 00   movl $0x2008,(%esp)
 *     53be8:  e8 ..                  call sysdep_malloc
 *     53bf2:  e8 ..                  call V92ConvolutionEncoder::V92ConvolutionEncoder()
 *
 * and that settles the map exactly, which the displacements alone could not.
 * `process` and `makeStateTtransitionTable` index two arrays, at +0x08 and at
 * +0x1008, both with a scale of four; the first is therefore 0x1000 bytes
 * because the second begins there, and the second is 0x1000 bytes because the
 * object ends at 0x2008.  1,024 ints each (finding F1249).
 */

#ifndef DSPLIB_V92CONVOLUTIONENCODER_H
#define DSPLIB_V92CONVOLUTIONENCODER_H

/*
 * Sixteen entries per state -- the index is always `state * 16 + in`, in both
 * members that touch either array, so the row stride is not a guess.
 */
#define V92CONV_TABLE_ENTRIES	1024
#define V92CONV_ROW		16

class V92ConvolutionEncoder {
public:
	/* Both bodies are empty; both symbols exist. */
	V92ConvolutionEncoder();
	~V92ConvolutionEncoder();

	/*
	 * `reset` stores its argument at +0x00, rebuilds the tables and THEN
	 * zeroes +0x04, in that order -- the call sits between the two stores
	 * in the object, which is the only reason the order is claimable.
	 */
	void reset(int mode);

	/*
	 * `process` returns an int: the object ends `mov %esi,%eax` on every
	 * arm including the one that never loaded %esi, so the whole of %eax
	 * is deliberate and is compared.
	 */
	int process(int *in);

	/*
	 * `inverseMap` returns an int too -- `process` uses the value in %eax
	 * as the switch's operand.  It reads four ints and writes none, so the
	 * `int *` is an in-parameter despite the mangling not saying so.
	 */
	int inverseMap(int *in);

	void makeStateTtransitionTable();

	/*
	 * BOTH TABLES ARE int, not char: `inverseMap` indexes each with a
	 * four-byte scale (`mov 0x0(,%ecx,4),%eax` against both relocations),
	 * and `nm`'s 0x100 and 0x40 then give 64 and 16 entries.  Both are `D`
	 * in the blob -- ordinary writable data -- so neither is const, and
	 * both are defined in the .cpp in the order .data lays them out.
	 *
	 * The reachable index of each is closed by construction: the four
	 * residues are 0..3, so `subsetLabelTable`'s index is 0..15; its
	 * values are 0..7, so `cosetMapping4D`'s `8 * a + b` is 0..63.
	 */
	static int cosetMapping4D[64];
	static int subsetLabelTable[16];

	/*
	 * Public for the usual reason: the original's access specifiers are
	 * not recoverable and one access section keeps the class
	 * standard-layout.
	 */

	/* +0x00  `reset`'s argument; the selector both `process` and
	 * `makeStateTtransitionTable` switch on, against 0, 1 and 2. */
	int mode;

	/* +0x04  Zeroed by `reset` after the tables are rebuilt, and carried
	 * across calls by `process`, which is what makes this a coder. */
	int state;

	/*
	 * +0x08 AND +0x1008, AND THE ROLES ARE THE REVERSE OF WHAT THESE TWO
	 * MEMBERS WERE ONCE CALLED HERE.  `process` proves which is which in
	 * two adjacent instructions:
	 *
	 *     8b bc b3 08 10 00 00   mov 0x1008(%ebx,%esi,4),%edi   esi = state*16 + in
	 *     89 7b 04               mov %edi,0x4(%ebx)             ... and it IS the new state
	 *     8b 74 8b 08            mov 0x8(%ebx,%ecx,4),%esi      ecx = newstate*16 + in
	 *
	 * so +0x1008 is indexed by the CURRENT state and yields the NEXT one,
	 * and +0x08 is indexed by the NEXT state and yields the value
	 * `process` returns.  `makeStateTtransitionTable` writes them the same
	 * way round.  Two arrays and not one 2,048-entry array: the two are
	 * written at different indices in the same statement.
	 */
	int output[V92CONV_TABLE_ENTRIES];	/* +0x08   */
	int nextState[V92CONV_TABLE_ENTRIES];	/* +0x1008 */
};

#endif /* DSPLIB_V92CONVOLUTIONENCODER_H */
