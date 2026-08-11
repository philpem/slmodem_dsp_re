/*
 * V92ConvolutionEncoder.h -- the V.92 trellis encoder and its state
 * transition table.
 *
 * Reconstructed from dsplibs.o.  Six members and two static tables, 2,486
 * bytes of code; this tree defines the constructor and the destructor -- both
 * of which are one `ret` -- and declares the rest.
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
 * object ends at 0x2008.  1,024 ints each (finding 1249).
 */

#ifndef DSPLIB_V92CONVOLUTIONENCODER_H
#define DSPLIB_V92CONVOLUTIONENCODER_H

#define V92CONV_TABLE_ENTRIES 1024

class V92ConvolutionEncoder {
public:
	/* Written.  Both bodies are empty; both symbols exist. */
	V92ConvolutionEncoder();
	~V92ConvolutionEncoder();

	/*
	 * Declared and deliberately not defined.  The signatures are the
	 * mangling's.  `reset(int)` stores its argument at +0x00, rebuilds the
	 * tables and then zeroes +0x04, in that order; `process` and
	 * `makeStateTtransitionTable` both switch on +0x00 against 1 and 2, so
	 * it is the constellation or trellis selector and the tables are a
	 * function of it.
	 */
	void reset(int mode);
	void process(int *inout);
	void inverseMap(int *inout);
	void makeStateTtransitionTable();

	/*
	 * DECLARED, NOT DEFINED, AND THE ELEMENT TYPE IS NOT RECOVERED.  Both
	 * are `D` in the blob -- ordinary writable data, so neither is const
	 * -- and the sizes are `nm`'s.  Whichever batch writes the four
	 * members above should replace these declarations rather than add a
	 * second pair.
	 */
	static unsigned char cosetMapping4D[0x100];
	static unsigned char subsetLabelTable[0x40];

	/*
	 * Public for the usual reason: the original's access specifiers are
	 * not recoverable and one access section keeps the class
	 * standard-layout.
	 */

	/* +0x00  `reset`'s argument; the selector `process` switches on. */
	int mode;

	/* +0x04  Zeroed by `reset` after the tables are rebuilt. */
	int state;

	/*
	 * +0x08 and +0x1008  The state transition table, built by
	 * `makeStateTtransitionTable` and read by `process` and `inverseMap`.
	 * Two arrays and not one 2,048-entry array: the two are written at
	 * different indices in the same statement and read back the same way.
	 */
	int transition[V92CONV_TABLE_ENTRIES];
	int output[V92CONV_TABLE_ENTRIES];
};

#endif /* DSPLIB_V92CONVOLUTIONENCODER_H */
