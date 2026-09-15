/**
 * @file V92EchoCanceller.h
 * @brief V.92 upstream echo canceller: adaptive FIR filter plus a
 *        delay-then-train-fast-then-train-slow state machine (`setState`),
 *        driven one block at a time by `process`.
 *
 * The canceller keeps two owned buffers -- `echoCoeff`, the filter taps, and
 * `echoHistory`, a ring of past samples -- plus a `FloatARMA` used to
 * pre-filter what goes into the history. `updateEchoHistory` is the sole
 * writer into `echoHistory` (cursor `echoLength`, i.e. "how many taps are
 * active"); `process` is the sole reader (cursor `historyIndex`), and the
 * two cursors never touch each other's field. `historyAlloc`, sized once in
 * the constructor from `filterLength`, `echoDelay` and the caller's block
 * length, is never reallocated afterwards (deviation D72's premise).
 *
 * The constructor and destructor are each emitted as a byte-identical pair
 * (`C1`/`C2`, `D1`/`D2`) with no `D0`, so the class is not polymorphic
 * (finding F1270). `sizeof(V92EchoCanceller)` is 0x3c; every field below is
 * mapped, with no `pad_*` remaining.
 *
 * Two fields have no name the object gives them, forced rather than chosen:
 * `word_10`, the sample count `process` compares against `updateDuration`
 * before asking `setState` for the next state, and `word_18`, always
 * `filterLength - 1` and used only by the constructor's history-length
 * arithmetic (finding F226 -- a value with no method name or diagnostic
 * naming it stays a `word_NNNN`, not an invented name).
 *
 * **A correction to deviation D72's arithmetic, not its verdict.** D72 and
 * finding F1188 both spell the filter length `V92_ECHO_FILTER_LENGTH & ~3`;
 * the object actually does a signed divide-and-multiply (`x / 4 * 4` on an
 * `int`), which differs from the mask reading only for negative inputs and
 * agrees with it at the shipped value of 180. D72's CANNOT-FIRE verdict is
 * unaffected; finding F1312.
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
 * The underlying type is signed, measured rather than assumed: the dispatch
 * (`cmp $0x1,%eax; je; jle; cmp $0x2 ...`) uses the signed `jle` branch,
 * where an enum with 0..3 enumerators would default to `unsigned int` and
 * compare with `jbe`. So the parameter is an `int`, and the base is pinned
 * here to say so -- which also makes every `int` value representable, so a
 * differential test may sweep outside the four states without reaching for
 * undefined behaviour (the same argument `V90Phase3Demodulator.h` gives for
 * `Phase3DemodulatorState`).
 *
 * Spelled as a pin rather than `: int` because a fixed base is C++11 and the
 * author's compiler was C++98 (docs/method/compilers.md, V2). `_BASE_PIN` is
 * ours: the mangling carries the type's name and nothing about its
 * enumerators, so the pin claims only the base.
 */
enum V92EchoCancellerState {
	V92_ECHO_FILTER_ONLY = 0,	/* filter, do not adapt             */
	V92_ECHO_COUNT_DELAY = 1,	/* count the delay before training  */
	V92_ECHO_FAST_TRAINING = 2,	/* adapt with the FAST parameters   */
	V92_ECHO_SLOW_TRAINING = 3,	/* adapt with the SLOW parameters   */
	V92EchoCancellerState_BASE_PIN = -0x7fffffff - 1  /* ours: the base */
};

/**
 * @brief Print the filter's tap bank to the debug log, one `%c%d.%06d` line
 *        per coefficient. Unreferenced anywhere in the object.
 * @param coeffs  Coefficient array.
 * @param len     Number of coefficients.
 */
void print_echo_coeffs(float *coeffs, unsigned int len);

class V92EchoCanceller {
public:
	/**
	 * @brief Construct the canceller: size and allocate `echoCoeff` and
	 *        `echoHistory`, build the pre-filter ARMA, and call reset().
	 * @param params    Parameter block; supplies the initial echo delay
	 *                  and the filter length.
	 * @param blockLen  Divisor and `2*` multiplier in the history-length
	 *                  computation (finding F1188 traces this to
	 *                  `VPcmFloModem`'s constructor, 40 in the shipped
	 *                  configuration). A zero value traps at the object's
	 *                  own unguarded divide.
	 * @param extra     Final addend in the history-length computation
	 *                  (F1188: 199 shipped).
	 */
	V92EchoCanceller(V92Parameters *params, unsigned int blockLen,
			 unsigned int extra);

	/**
	 * @brief Update the echo delay and keep `echoLength` (the tap count)
	 *        in step with it, by the same delta rather than a recompute.
	 * @param delay  The new delay, in samples.
	 */
	void setEchoDelay(unsigned int delay);
	/**
	 * @brief Clear the canceller to its post-construction state: zero
	 *        both buffers, rewind the cursors, zero the adaption betas
	 *        and reset the ARMA.
	 */
	void reset();

	/**
	 * @brief Zero the `filterLength` coefficients in `echoCoeff`. Also
	 *        inlined as reset()'s first loop (finding F1271).
	 */
	void zeroEchoCoeff();
	/**
	 * @brief Rebuild `echoLength` from `filterLength`, `echoDelay` and
	 *        the parameter block, and zero that many entries of
	 *        `echoHistory`. Also inlined as reset()'s second loop
	 *        (finding F1271); the bound is unclamped against
	 *        `historyAlloc` (deviation D72, confirmed present, cannot
	 *        fire at any real `V92_ECHO_INITIAL_DELAY`).
	 */
	void resetEchoHistory();

	/**
	 * @brief Set the fast/slow-training adaption gain and log its sign
	 *        and magnitude.
	 * @param beta  New value of `echoBeta`.
	 */
	void setEchoBeta(float beta);
	/**
	 * @brief Set the adaption gain's decay factor and log its sign and
	 *        magnitude.
	 * @param decay  New value of `echoBetaDecay`.
	 */
	void setDecayFactor(float decay);
	/**
	 * @brief Convenience wrapper: setEchoBeta(), setDecayFactor() and
	 *        setEchoDelay() in one call, in that order.
	 */
	void setEchoParams(float beta, float decay, unsigned int delay);

	/**
	 * @brief Move the canceller to a new state and reset the
	 *        `updateDuration`/`word_10` sample counters for it. A no-op
	 *        if @p newState already holds.
	 * @param newState  One of the #V92EchoCancellerState values.
	 */
	void setState(V92EchoCancellerState newState);
	/**
	 * @brief Pre-filter @p count samples through the ARMA and append them
	 *        to `echoHistory` at the write cursor (`echoLength`).
	 * @param in     Input samples.
	 * @param count  Number of samples.
	 */
	void updateEchoHistory(float *in, unsigned int count);
	/**
	 * @brief Run one block through the FIR filter/adaption state machine.
	 *        Drives setState() through its sequence: COUNT_DELAY counts
	 *        out the initial delay then asks for FAST_TRAINING, which
	 *        adapts and asks for SLOW_TRAINING, which adapts and asks for
	 *        FILTER_ONLY, which filters indefinitely without adapting.
	 * @param in     Input samples.
	 * @param out    Filtered output samples.
	 * @param count  Number of samples in this block; advances the
	 *               per-state sample counter for setState()'s dispatch.
	 */
	void process(float *in, float *out, unsigned int count);

	/**
	 * @brief Free `echoCoeff`, `echoHistory` and the owned `FloatARMA`
	 *        (each null-guarded and nulled after). A user-declared
	 *        destructor, matching the object's `D1`/`D2` pair -- which
	 *        costs the class its triviality (test/unit/t_v90leaves.cpp's
	 *        `union ec_slot` had to become a byte array with a cast).
	 */
	~V92EchoCanceller();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V92Parameters *params;		/* +0x00 not owned                  */
	FloatARMA *arma;		/* +0x04 OWNED; freed by ~this       */
	/*
	 * +0x08  The state, named by `setState`: the method stores 0, 1, 2 or
	 * 3 here (one per arm, each beside that arm's message), is a no-op
	 * when the field already holds the argument, and `process` switches
	 * on it. `reset` clears it to `V92_ECHO_FILTER_ONLY`.
	 */
	V92EchoCancellerState state;	/* +0x08                             */
	/*
	 * +0x0c  How long the current state lasts, in samples. `setState`
	 * stores `V92_ECHO_FAST_UPDATE_DURATION` for state 2 and
	 * `V92_ECHO_SLOW_UPDATE_DURATION` for state 3; state 1 stores
	 * `echoDelay + 400`, the same quantity built from the delay rather
	 * than read from the parameter block.
	 *
	 * +0x10 is what is compared against it, and nothing names it:
	 * `process` adds its block length here and, when the sum reaches
	 * +0x0c, asks `setState` for the next state; `setState` clears it on
	 * every state change and only on a change. The role is forced and the
	 * name is not recoverable (finding F226); `word_10` is the honest
	 * spelling.
	 *
	 * Both are unsigned because the comparison is: `cmp 0xc(%edi),%esi;
	 * jb` is the unsigned branch, which needs one unsigned operand. Which
	 * of the two the object intends is not decidable from that alone, so
	 * both are written unsigned and the `int` constructor/setState
	 * arguments are converted on the way in.
	 */
	unsigned int updateDuration;	/* +0x0c samples in this state       */
	unsigned int word_10;		/* +0x10 samples so far in it        */
	unsigned int filterLength;	/* +0x14 taps in `echoCoeff`         */
	/*
	 * +0x18  `filterLength - 1`, and nothing else: the constructor writes
	 * it (`lea -0x1(%eax),%ecx`) one instruction before writing
	 * `filterLength` from the same register, and its only other read is
	 * four instructions later, as the first term of the history-length
	 * computation. The value is measured; no method name or diagnostic
	 * names the quantity itself, so it stays `word_18` (finding F226).
	 */
	unsigned int word_18;		/* +0x18 == filterLength - 1         */
	/*
	 * +0x1c  The history's allocated length, use-derived rather than
	 * named: the constructor computes it, stores it here, and hands
	 * (length << 2) to `sysdep_malloc` as `echoHistory`'s byte count.
	 * Nothing else in the class reads it -- `reset` bounds its own clear
	 * by `echoLength`, never by this -- which is deviation D72's whole
	 * mechanism.
	 */
	unsigned int historyAlloc;	/* +0x1c floats in `echoHistory`     */
	float *echoCoeff;		/* +0x20 OWNED; freed by ~this       */
	float *echoHistory;		/* +0x24 OWNED; freed by ~this       */
	unsigned int historyIndex;	/* +0x28 `process`'s READ cursor     */
	/*
	 * +0x2c  Also a cursor, shown by `updateEchoHistory`: the writer
	 * stores the ARMA's output at `echoHistory[echoLength]` and
	 * increments the field once per sample, so it counts how much of the
	 * history is filled (equivalently, the active tap count); `reset` and
	 * `setEchoDelay` set the starting distance between this and +0x28,
	 * which is the echo delay.
	 */
	unsigned int echoLength;	/* +0x2c the WRITE cursor            */
	float echoBeta;			/* +0x30                             */
	float echoBetaDecay;		/* +0x34                             */
	unsigned int echoDelay;		/* +0x38                            */
};

#endif /* DSPLIB_V92ECHOCANCELLER_H */
