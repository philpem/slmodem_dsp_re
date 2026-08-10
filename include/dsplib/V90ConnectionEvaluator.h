/*
 * V90ConnectionEvaluator.h -- the V.90 connection evaluator, so far as its
 * constructor, destructor and `reset` need it.
 *
 * Reconstructed from dsplibs.o.  THE CLASS DID NOT ARRIVE WITH THIS FILE.  It
 * was defined inside `V90Demodulator.h` from task #60 onwards, because the
 * only things in the tree that touched it were `V90Demodulator::enterPhase3`
 * (four words it clears) and `VPcmFloModem::getV90CpBits` (a copy from +0x78
 * to +0x7c).  Task #88 wrote the three members the class actually owns, so it
 * has its own header now.  **No field moved and no field was renamed** --
 * `word_70`, `word_74`, `word_78`, `word_7c`, `word_84` and `word_88` are the
 * names those two earlier batches gave them, and the offset assertions moved
 * from `V90Demodulator.cpp` to `V90ConnectionEvaluator.cpp` unchanged.
 *
 * NOT POLYMORPHIC.  `nm` gives `D1` at 0x3e390 and `D2` at 0x3e380 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding 228).
 *
 * THE OBJECT IS 0xbc = 188 BYTES, and that is an allocation and not a
 * displacement: `movl $0xbc,(%esp); call sysdep_malloc` at 0x1c47e inside
 * `V90Demodulator`'s constructor, with the result passed straight to
 * `V90ConnectionEvaluator(V90Parameters *)`.  `reset`'s highest write is the
 * two-byte store at +0xb4, so six bytes at the end are untouched by anything
 * written here.
 *
 * WHAT `reset` SAYS ABOUT THE OBJECT.  Nineteen of its forty-eight stores are
 * copies out of the parameter block, and every one of those nineteen slots
 * has the ORIGINAL AUTHOR'S OWN NAME (findings 860-862, `tools/vparse.py`).
 * The field names below are taken from the parameter each field receives --
 * `retrainDetectDuration` holds `params->RETRAIN_DETECT_DURATION` and nothing
 * else assigns to it in anything reconstructed so far -- which is a
 * derivation and not a guess.  The other twenty-nine are zeroes, two -1s and
 * two 1600s, and those are offset-named because a zero says nothing about
 * what a field holds.
 *
 * THE CONSTRUCTOR IS FIFTEEN BYTES AND MOST OF `reset` IS ITS BODY:
 *
 *     3e560:  8b 54 24 04     mov 0x4(%esp),%edx     this
 *     3e564:  8b 44 24 08     mov 0x8(%esp),%eax     the V90Parameters *
 *     3e568:  89 02           mov %eax,(%edx)
 *     3e56a:  e9 ..           jmp <V90ConnectionEvaluator::reset>
 *
 * -- a tail call, so the object is "store the parameter block, then reset".
 * The destructor is one byte, `c3`.
 */

#ifndef DSPLIB_V90CONNECTIONEVALUATOR_H
#define DSPLIB_V90CONNECTIONEVALUATOR_H

/*
 * A POINTER ONLY, so a forward declaration is what belongs here.  Two
 * incompatible definitions of `V90Parameters` exist in this tree -- the 0x504
 * word block in `V90PreFilter.h` and the 0x558 named map in
 * `V90Parameters.h` -- and no translation unit may include both.  This header
 * is included by `V90Demodulator.h`, which pulls in the first; the .cpp picks
 * the second, because the names are the point.  Finding 1112.
 */
class V90Parameters;

class V90ConnectionEvaluator {
public:
	/*
	 * The three members task #88 defines.  The signatures are the
	 * mangling's: `_ZN22V90ConnectionEvaluatorC1EP13V90Parameters`,
	 * `_ZN22V90ConnectionEvaluatorD1Ev` and
	 * `_ZN22V90ConnectionEvaluator5resetEv`.
	 */
	V90ConnectionEvaluator(V90Parameters *params);
	~V90ConnectionEvaluator();
	void reset();

	/*
	 * Declared for the record and deliberately left undefined -- their
	 * signatures come from the mangling, so this list is a specification
	 * rather than a guess, and a return type is not mangled and is
	 * therefore unknown for all of them.  They belong to whichever batch
	 * writes this class's processing half; `evaluateConnection` alone is
	 * 3,857 bytes.  Nothing reconstructed calls any of them.
	 */
	void updateAvePdsnr(float, unsigned int);
	void updateCurrentConstellationData(short, float, float, float);
	void indicateRemoteRateReneg();
	void evaluateConnection();
	void evaluatePhase3();
	void evaluateMeanErrorStdPhase3(float);
	void evaluateMeanErrorStdPhase4(float, float);
	void evaluatePhase4(float);
	void indicateLocalRetrain();
	void indicateRemoteRetrain();
	void printStatus() const;

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with __builtin_offsetof.
	 */

	/*
	 * +0x00  The parameter block, stored by the constructor and read by
	 * `reset`.  Not owned.
	 */
	V90Parameters *params;

	/*
	 * +0x04 .. +0x24  Nine consecutive counters `reset` zeroes.  Nothing
	 * written so far reads any of them, so they are offset-named.
	 */
	unsigned int word_04;
	unsigned int word_08;
	unsigned int word_0c;
	unsigned int word_10;
	unsigned int word_14;
	unsigned int word_18;
	unsigned int word_1c;
	unsigned int word_20;
	unsigned int word_24;

	/*
	 * +0x28 .. +0x6c  The configuration, copied out of the parameter
	 * block one word at a time.  The names are the parameters' -- see the
	 * file comment -- and the two that are not copied are the pair of
	 * 1600s at +0x64 and +0x68, which are an immediate `movl $0x640` each
	 * and carry no relocation, so they are integers and not addresses.
	 */
	int enableRrnDown;			/* +0x28 ENABLE_RRN_DOWN     */
	int enableRrnUp;			/* +0x2c ENABLE_RRN_UP       */
	int nofRemoteRateRenegBeforeRetrain;	/* +0x30                     */
	int debugAlternateDebug;		/* +0x34                     */
	int debugFallBack;			/* +0x38                     */
	int debugRetrain;			/* +0x3c                     */
	int debugRateUp;			/* +0x40                     */
	int debugRateDown;			/* +0x44                     */
	int retrainCounterFadeCount;		/* +0x48                     */
	int remoteRrnCounterFadeCount;		/* +0x4c                     */
	int rateUpDetectDuration;		/* +0x50                     */
	int minDurationInDataBeforeRrnUp;	/* +0x54                     */
	int rateDownDetectDuration;		/* +0x58                     */
	int minDurationInDataBeforeRrnDown;	/* +0x5c                     */
	int retrainDetectDuration;		/* +0x60                     */
	unsigned int word_64;			/* +0x64 reset plants 1600   */
	unsigned int word_68;			/* +0x68 reset plants 1600   */
	int debugPeriod;			/* +0x6c                     */

	unsigned int word_70;		/* +0x70 cleared by enterPhase3      */
	unsigned int word_74;		/* +0x74 cleared by enterPhase3      */
	/*
	 * +0x78 and +0x7c were `pad_78` until the VPcmFloModem batch read a
	 * second function that touches this object: `getV90CpBits` copies
	 * +0x78 to +0x7c each time a CP sequence finishes.  Two batches, two
	 * functions, one object -- neither would have found both.
	 */
	unsigned int word_78;		/* +0x78 copied to word_7c           */
	unsigned int word_7c;		/* +0x7c                             */
	unsigned int word_80;		/* +0x80 zeroed by reset             */
	unsigned int word_84;		/* +0x84 cleared by enterPhase3      */
	unsigned int word_88;		/* +0x88 cleared by enterPhase3;
					 *       reset does NOT touch it     */

	/*
	 * +0x8c  Set to -1 by `reset`, not to 0 -- `mov $0xffffffff,%eax`, a
	 * full 32-bit store -- which is why it is `int` and not `unsigned`.
	 */
	int word_8c;

	unsigned int word_90;		/* +0x90 zeroed by reset             */
	unsigned int word_94;		/* +0x94 zeroed by reset             */
	unsigned char pad_98[4];	/* +0x98 reset does not reach it     */

	/*
	 * +0x9c, +0x9e  A 16-bit pair, -1 and 0.  The store widths are the
	 * evidence: `mov %cx,0x9c(%ebx)` with %ecx holding 0xffffffff writes
	 * two bytes and leaves 0xffff, which is a different value from the
	 * -1 at +0x8c even though the source constant is the same.
	 */
	short short_9c;
	short short_9e;

	unsigned int word_a0;		/* +0xa0 zeroed by reset             */
	unsigned int word_a4;		/* +0xa4 zeroed by reset             */
	unsigned int word_a8;		/* +0xa8 zeroed by reset             */

	/*
	 * +0xac  = params->PHASE4_ERROR_FOR_V34_FALLBACK.  Copied as a 32-bit
	 * word and never loaded into the x87 stack, so it is moved as a float
	 * rather than converted -- the same shape as V90Phase3Demodulator's
	 * +0x418 and V90Equalizer's +0x90.
	 */
	float phase4ErrorForV34Fallback;

	/*
	 * +0xb0 .. +0xb4  Three 16-bit flags, 1, 0 and 0.  +0xb4 is the
	 * highest byte anything written here touches; the object runs to
	 * 0xbc.
	 */
	short short_b0;
	short short_b2;
	short short_b4;

	unsigned char pad_b6[6];	/* +0xb6 .. +0xbb                    */
};

#endif /* DSPLIB_V90CONNECTIONEVALUATOR_H */
