/**
 * @file V92ConvolutionEncoder.h
 * @brief The V.92 trellis encoder and its state transition table.
 *
 * Six members and two static tables, all defined.
 *
 * A one-byte constructor is still a user declaration: GCC emits an
 * out-of-line constructor/destructor symbol only for a user-declared one
 * (an implicit trivial destructor produces no symbol at all), and the blob
 * has one-byte C1/C2/D1/D2 for this class -- so the original declared both
 * and wrote empty bodies. The differential test for the pair asserts they
 * write nothing, not one byte of a seeded 0x2008-byte object, on either
 * side.
 *
 * The object is 0x2008 bytes, measured from the `sysdep_malloc(0x2008)`
 * immediately preceding this constructor's call, which settles the map
 * exactly where displacements alone could not: `process` and
 * `makeStateTtransitionTable` index two arrays, at +0x08 and +0x1008, both
 * with a scale of four, so the first is 0x1000 bytes (the second begins
 * there) and the second is 0x1000 bytes (the object ends at 0x2008) --
 * 1,024 ints each (finding F1249).
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
	/** @brief Construct with every member left indeterminate; reset()
	 *         must be called before use. Empty body. */
	V92ConvolutionEncoder();
	/** @brief Destroy. Empty body. */
	~V92ConvolutionEncoder();

	/**
	 * @brief Select the encoder mode and rebuild the state transition
	 *        table for it.
	 * @param mode  Stored at +0x00, then the table is rebuilt, then
	 *              `state` (+0x04) is zeroed -- in that order, per the
	 *              object.
	 */
	void reset(int mode);

	/**
	 * @brief Encode four input bits for the current state, advancing to
	 *        the next state.
	 * @param in  Four-int input vector (residues/subset bits).
	 * @return The encoded output value. The full 32 bits of the return
	 *         register are deliberate and compared -- the object ends
	 *         `mov %esi,%eax` on every arm, including one that never
	 *         loaded `%esi`.
	 */
	int process(int *in);

	/**
	 * @brief Map an input vector to a coset/subset index for process()'s
	 *        table lookup.
	 * @param in  Four-int input vector, read but not written (an
	 *            in-parameter despite the mangling not distinguishing it).
	 * @return The index `process` switches on next.
	 */
	int inverseMap(int *in);

	/** @brief (Re)build the current mode's `output`/`nextState` tables. */
	void makeStateTtransitionTable();

	/** @brief Coset mapping table: `8 * a + b` for subset value `a`
	 *  (0..7) and residue `b` (0..7), 64 entries. `int`, not `char`
	 *  (`inverseMap` indexes it with a four-byte scale); ordinary
	 *  writable data in the blob, not const. */
	static int cosetMapping4D[64];
	/** @brief Subset label table, indexed 0..15 by the four 2-bit
	 *  residues; values are 0..7 subset selectors feeding
	 *  cosetMapping4D's `a`. Same width/mutability rationale as
	 *  cosetMapping4D. */
	static int subsetLabelTable[16];

	/*
	 * Public for the usual reason: the original's access specifiers are
	 * not recoverable and one access section keeps the class
	 * standard-layout.
	 */

	/* +0x00  reset()'s argument; the selector both `process` and
	 * `makeStateTtransitionTable` switch on, against 0, 1 and 2. */
	int mode;

	/* +0x04  Zeroed by reset() after the tables are rebuilt, and carried
	 * across calls by process(), which is what makes this a coder. */
	int state;

	/* +0x08  Indexed by the *next* state, yields the value process()
	 * returns. Named the other way round from an earlier reading of this
	 * header, corrected by tracing process()'s own two adjacent loads
	 * (finding F1375). Two separate 1,024-entry arrays, not one
	 * 2,048-entry array -- they're written at different indices in the
	 * same statement. */
	int output[V92CONV_TABLE_ENTRIES];	/* +0x08   */
	/* +0x1008  Indexed by the *current* state (`state*16 + in`), yields
	 * the next state (finding F1375). */
	int nextState[V92CONV_TABLE_ENTRIES];	/* +0x1008 */
};

#endif /* DSPLIB_V92CONVOLUTIONENCODER_H */
