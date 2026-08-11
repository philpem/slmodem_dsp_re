/*
 * V92EchoCanceller.h -- the V.92 upstream echo canceller's state.
 *
 * Reconstructed from dsplibs.o.  Twelve members, 3,622 bytes; seven of them
 * are written here -- the constructor, which the object emits as the
 * byte-identical pair `C1` and `C2`, `setEchoDelay`, which is the one
 * `v34handshak` reaches, `reset`, the destructor, which it emits as the
 * byte-identical pair `D1` and `D2` (finding 1270), and the three that carry
 * the signal: `setState`, `updateEchoHistory` and `process`.
 *
 * THE FIVE THAT ARE NOT WRITTEN are `setEchoParams`, `setEchoBeta`,
 * `setDecayFactor`, `zeroEchoCoeff` and `resetEchoHistory`.  The last four
 * are all inlined into members that ARE written, so their bodies are here
 * even though their symbols are not -- see the .cpp.
 *
 * THE CONSTRUCTOR IS WHERE THE SIZING HAPPENS, which is D72's whole premise
 * made concrete: it allocates `echoHistory` once, `historyAlloc` floats long,
 * and nothing reallocates it afterwards.  See the .cpp.
 *
 * NOT POLYMORPHIC: `~V92EchoCanceller` is listed with `D1` and `D2` and no
 * `D0`, so there is no vptr.
 *
 * THE OBJECT IS 60 BYTES.  The largest `this`-relative displacement across
 * all fourteen defined members is +0x38, four bytes wide, so the object ends
 * at 0x3c.
 *
 * +0x2c IS A TAP COUNT, NOT A POINTER, and the object's own arithmetic is
 * what settles it.  `setEchoDelay` does
 *
 *     add %ecx,0x2c(%eax)          ecx = newDelay - oldDelay
 *
 * with the delta UNSCALED.  Had +0x2c been a `float *` into the coefficient
 * array, C++ pointer arithmetic would have multiplied the delta by four and
 * GCC would have emitted the shift; it does not.  `resetEchoHistory` then
 * builds the same field from scratch --
 *
 *     ecx = (this->+0x14 >> 1) + this->echoDelay + this->params->+0x74
 *
 * -- and immediately uses it as the bound of a loop that zeroes `+0x24[i]`
 * with a 4-byte stride.  So it counts the active taps, it moves one for one
 * with the delay, and `setEchoDelay` is keeping it in step rather than
 * recomputing it.
 *
 * +0x38 IS `echoDelay`, from the method name and from the diagnostic the
 * method ends with, which prints the value just stored.
 *
 * THE `pad_00` REGION IS NOW SEVEN NAMED FIELDS AND THREE HOLES, and every
 * name below is either a method name's or a diagnostic string's.  `reset()`
 * and `~V92EchoCanceller()` between them touch +0x00, +0x04, +0x08, +0x14,
 * +0x20, +0x24, +0x28, +0x30 and +0x34, and what each one is is forced by
 * how it is used rather than by what it is called:
 *
 *   +0x00  dereferenced for a 4-byte read at its +0x74, which is
 *          `V92_ECHO_DELAY_OFFSET` in V92Parameters.h.
 *   +0x04  handed to `FloatARMA::reset` by `reset`, and to
 *          `FloatARMA::~FloatARMA` and then `sysdep_free` by the destructor
 *          -- so it is an OWNED pointer to a FloatARMA, not an embedded one.
 *   +0x14  the bound of the loop that zeroes +0x20, and the `>> 1` term of
 *          `echoLength`.  D72 reads it as `V92_ECHO_FILTER_LENGTH & ~3`.
 *   +0x20  a 4-byte-stride array zeroed for +0x14 entries by the body the
 *          object also emits standalone as `zeroEchoCoeff`.
 *   +0x24  a 4-byte-stride array zeroed for `echoLength` entries by the body
 *          the object also emits standalone as `resetEchoHistory`.
 *   +0x28  set to zero beside +0x24's loop.  IT IS A CURSOR AND IT IS NOT
 *          THE WRITE CURSOR: `process` steps it one per sample, wraps it at
 *          `historyAlloc - word_18`, and uses it to index `echoHistory` for
 *          the filter -- while `updateEchoHistory`, the writer, indexes by
 *          `echoLength` at +0x2c and never reads or writes +0x28 at all.  So
 *          the name is now use-derived, in the reader's direction.
 *   +0x30, +0x34  `echoBeta` and `echoBetaDecay`, NAMED BY THE OBJECT: the
 *          format strings `setEchoBeta` and `setDecayFactor` print are
 *          "V92EchoCanceller: echoBeta = %c%d.%06d\r\n" and
 *          "V92EchoCanceller: echoBetaDecay = %c%d.%06d\r\n".
 *
 * THE HOLE AT +0x0c..+0x13 IS NOW TWO FIELDS, and it is the state machine's:
 * `setState` writes +0x0c, `process` reads it and accumulates into +0x10, and
 * `setState` clears +0x10.  Neither is touched by the constructor, by `reset`
 * or by the destructor, which is why the four members written first left the
 * region unexplained rather than unexplored.  D72's +0x1c and the
 * `filterLength - 1` beside it are named below now that the constructor that
 * writes them has been read.
 *
 * THE OBJECT IS NOW FULLY MAPPED: fifteen fields, no `pad_*` left.
 *
 * A CORRECTION TO D72's DERIVATION, not to its verdict.  D72 and finding 1188
 * both spell the filter length `V92_ECHO_FILTER_LENGTH & ~3`.  The object
 * does a SIGNED divide-and-multiply -- `test %eax,%eax; js; add $0x3; and
 * $0xfffffffc` -- which is `x / 4 * 4` on an `int`, and the two readings
 * differ for every negative value: -6 gives -8 under the mask and -4 under
 * the object's rounding-toward-zero.  At the shipped 180 they agree, so
 * nothing about D72's CANNOT FIRE verdict moves; the arithmetic is corrected
 * where it is stated.  Finding 1312.
 */

#ifndef DSPLIB_V92ECHOCANCELLER_H
#define DSPLIB_V92ECHOCANCELLER_H

class FloatARMA;
class V92Parameters;

/*
 * THE FOUR STATES ARE `setState`'s OWN DISPATCH, AND THE OBJECT NAMES EVERY
 * ONE OF THEM.  The method compares its argument against 0, 1, 2 and 3 and
 * opens each arm with a message that says what that arm is:
 *
 *     0  "V92EchoCanceller: echo state set to filter only"
 *     1  "V92EchoCanceller: echo state set to count delay before training"
 *     2  "V92EchoCanceller: echo state set to fast echo training"
 *     3  "V92EchoCanceller: echo state set to slow echo training"
 *
 * and anything else "V92EchoCanceller: setState ERROR: illegal state".  The
 * two training arms are told apart by the parameters they load as well as by
 * their messages: 2 takes `V92_ECHO_FAST_BETA_FACTOR`, `..._FAST_DECAY_FACTOR`
 * and `..._FAST_UPDATE_DURATION`, 3 the three `..._SLOW_...` fields.
 *
 * THE UNDERLYING TYPE IS SIGNED, and that is measured rather than assumed:
 * the dispatch is `cmp $0x1,%eax; je; jle; cmp $0x2 ...`, and `jle` is the
 * SIGNED branch.  An enum whose enumerators are 0..3 gets `unsigned int` from
 * GCC by default and would have compared with `jbe`, so the parameter is an
 * `int` and the base is fixed here to say so.  Fixing it also makes every
 * `int` value representable, so the differential test may sweep outside the
 * four without reaching for undefined behaviour -- the argument
 * `V90Phase3Demodulator.h` gives for `Phase3DemodulatorState`.
 */
enum V92EchoCancellerState : int {
	V92_ECHO_FILTER_ONLY = 0,	/* filter, do not adapt             */
	V92_ECHO_COUNT_DELAY = 1,	/* count the delay before training  */
	V92_ECHO_FAST_TRAINING = 2,	/* adapt with the FAST parameters   */
	V92_ECHO_SLOW_TRAINING = 3	/* adapt with the SLOW parameters   */
};

class V92EchoCanceller {
public:
	/*
	 * THREE ARGUMENTS, and only the first is a thing.  The second and
	 * third appear nowhere but in the history length: the second is the
	 * `div` divisor and the multiplier of the `2 *` term, the third the
	 * final addend.  Finding 1188 traced both to `VPcmFloModem`'s
	 * constructors and read them as 40 and 199 for the shipped
	 * configuration; the names here describe what THIS constructor does
	 * with them, which is all it can say.  A zero `blockLen` traps at the
	 * divide -- the object does not guard it and neither does this.
	 */
	V92EchoCanceller(V92Parameters *params, unsigned int blockLen,
			 unsigned int extra);

	/* Defined in src/pump/v90/V92EchoCanceller.cpp. */
	void setEchoDelay(unsigned int);
	void reset();

	/*
	 * The state machine and the two signal paths.
	 *
	 * `process` is the only caller of `setState` in this class, and it
	 * drives the sequence the four messages describe: 1 counts out the
	 * delay and then asks for 2, 2 adapts fast and then asks for 3, 3
	 * adapts slow and then asks for 0, and 0 filters for ever without
	 * touching a coefficient or the sample counter.
	 *
	 * `updateEchoHistory` is the WRITER into `echoHistory` and `process`
	 * is the READER; they use different cursors -- +0x2c and +0x28 -- and
	 * neither touches the other's.  See the .cpp.
	 */
	void setState(V92EchoCancellerState newState);
	void updateEchoHistory(float *in, unsigned int count);
	void process(float *in, float *out, unsigned int count);

	/*
	 * DECLARING THIS COSTS THE CLASS ITS TRIVIALITY, and that is not free:
	 * a class with a user-declared destructor cannot be a union member, so
	 * `test/unit/t_v90leaves.cpp`'s `union ec_slot` had to become a byte
	 * array with a cast.  It is declared anyway because the object has one
	 * -- `D1` and `D2`, 195 bytes each and byte-identical -- and because
	 * one C++ definition is what produces exactly that pair.
	 */
	~V92EchoCanceller();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V92Parameters *params;		/* +0x00 not owned                  */
	FloatARMA *arma;		/* +0x04 OWNED; freed by ~this       */
	/*
	 * +0x08 IS THE STATE, and `setState` is what names it: the method
	 * stores 0, 1, 2 or 3 here -- one per arm, each beside that arm's
	 * message -- returns without doing anything else when the field
	 * already holds the argument, and `process` switches on it.  `reset`
	 * clears it, which is `V92_ECHO_FILTER_ONLY`.
	 */
	V92EchoCancellerState state;	/* +0x08                             */
	/*
	 * +0x0c IS HOW LONG THE CURRENT STATE LASTS, IN SAMPLES, and the
	 * parameter block names it: `setState` stores
	 * `V92_ECHO_FAST_UPDATE_DURATION` into it for state 2 and
	 * `V92_ECHO_SLOW_UPDATE_DURATION` for state 3.  State 1 stores
	 * `echoDelay + 400` instead, which is the same quantity built from
	 * the delay rather than read from the block.
	 *
	 * +0x10 IS WHAT IS COMPARED AGAINST IT, and NOTHING NAMES IT.
	 * `process` adds its block length to it and, when the sum reaches
	 * +0x0c, asks `setState` for the next state; `setState` clears it on
	 * every state CHANGE and only on a change.  So its role is forced and
	 * its name is not recoverable -- finding 226, and `word_10` is the
	 * honest spelling.
	 *
	 * BOTH ARE UNSIGNED BECAUSE THE COMPARISON IS.  `cmp 0xc(%edi),%esi;
	 * jb` is the unsigned branch, which needs one unsigned operand; which
	 * of the two it is the object cannot say, so both are written that
	 * way and the `int` parameters are converted on the way in.
	 */
	unsigned int updateDuration;	/* +0x0c samples in this state       */
	unsigned int word_10;		/* +0x10 samples so far in it        */
	unsigned int filterLength;	/* +0x14 taps in `echoCoeff`         */
	/*
	 * +0x18 IS `filterLength - 1` AND IS NOTHING ELSE.  The constructor
	 * writes it as `lea -0x1(%eax),%ecx` one instruction before it writes
	 * `filterLength` from the same `%eax`, and the only other reader in
	 * the class is the constructor itself, four instructions later, where
	 * it is the first term of the history length.  So the field is real
	 * and its VALUE is measured; what it was called is not, and inventing
	 * a name for a quantity no method name and no diagnostic mentions is
	 * what finding 226 warns against.  `word_18` it stays.
	 */
	unsigned int word_18;		/* +0x18 == filterLength - 1         */
	/*
	 * +0x1c IS THE HISTORY'S ALLOCATED LENGTH, and it is use-derived
	 * rather than named: the constructor computes it, stores it here,
	 * shifts a copy left by two and hands that to `sysdep_malloc` as the
	 * byte count for `echoHistory`.  A number that is the argument of the
	 * allocation is the allocation's length.  Nothing else in the class
	 * reads it -- `reset` bounds its clear by `echoLength` and never by
	 * this -- which is D72's whole mechanism.
	 */
	unsigned int historyAlloc;	/* +0x1c floats in `echoHistory`     */
	float *echoCoeff;		/* +0x20 OWNED; freed by ~this       */
	float *echoHistory;		/* +0x24 OWNED; freed by ~this       */
	unsigned int historyIndex;	/* +0x28 `process`'s READ cursor     */
	/*
	 * +0x2c IS ALSO A CURSOR, and `updateEchoHistory` is what shows it.
	 * The writer stores its ARMA's output at `echoHistory[echoLength]`
	 * and increments the field once per sample, so what the tap count
	 * counts is how much of the history is filled; `reset` and
	 * `setEchoDelay` set the starting distance between this and +0x28,
	 * which is the echo delay.  The name is the one the first four
	 * members were written under and is left alone.
	 */
	unsigned int echoLength;	/* +0x2c the WRITE cursor            */
	float echoBeta;			/* +0x30                             */
	float echoBetaDecay;		/* +0x34                             */
	unsigned int echoDelay;		/* +0x38                            */
};

#endif /* DSPLIB_V92ECHOCANCELLER_H */
