/*
 * v34pcmif.c -- the V.90/K56Flex side's hooks into the V.34 machinery.
 *
 * A cluster of very small functions at .text+0x9230 and +0xa250 onwards,
 * separate from V34RX.c and from the handshake: the PCM modem's view of what
 * V.34 is doing.  Reconstructed one at a time as the V.34 code that calls
 * them arrives, rather than as a module, because that is the order the call
 * graph gives (tools/callgraph.py).
 *
 * THE DEFINITION ORDER IS NOW THE OBJECT'S EMISSION ORDER, NOT THE ORDER THEY
 * WERE RECONSTRUCTED IN, and it is load-bearing.  All 34 emitted symbols sit
 * at the blob's own relative index, which is the gate on believing any future
 * null result from here.  The check is `nm -n --defined-only` on
 * `build/tc_out/src_pump_v34_v34pcmif.c.o` and on the blob, compared over the
 * symbols both define -- 34 of 34 today.  There are no per-function `.text`
 * comments in this file to read the order off.  GCC 3.4.2's register
 * allocation depends on the identity of what it compiled before a function, so
 * the order moves bytes: `VPcmV34SetTxScale` and `VPcmV34ReportMiddleOfEcho`
 * `Adapt` both reached byte identity on this reorder alone.
 *
 * WHAT IT CANNOT FIX, and it bounds what a null result here means: the blob's
 * translation unit spans .text 0x64f0..0xa780 and holds 57 functions, of which
 * we define 34.  The other 23 are ours too but live in v34pcmmain.cpp,
 * v34info.c, v34diag.cpp and v34info1a.cpp, or are not written at all -- so
 * our file split is not the original's and the code compiled AHEAD of these
 * definitions is not what was compiled ahead of theirs.  Only the relative
 * order is recoverable here.  See docs/method/refinement.md lever 3.
 *
 * They also fix the object's real extent.  `VPcmV34LogTimingOffset` writes
 * at +0xac0c, past where v34fsk.h had the struct ending -- so the V.34
 * object is at least 0xac0e bytes and the tail of it belongs to this
 * interface rather than to the datapump.
 *
 * WHICH TRANSLATION UNIT THIS IS.  All of it belongs to `VPcmV34Main.cpp`:
 * these entry points are interleaved in .text with mangled names from that
 * file (`_Z25SetUpstreamModulationInfoP12tagV34Object` at 0x6200,
 * `_Z14getMPrecvdBitsP12tagV34Object` at 0x9250), so the TU is C++ and every
 * function here is one of its `extern "C"` exports.  The file stays `.c`,
 * which is the choice the tree already made when `VPcmV34LogTimingOffset`
 * was written: nothing about these functions is C++, and splitting the two
 * halves of one TU by language would be a worse map than splitting it by
 * role.
 *
 * THE C++ HALF HAS STARTED.  `src/pump/v34/v34pcmmain.cpp` holds
 * `getMPrecvdBits`, the second of the two mangled names above, which cannot
 * live here because a C translation unit cannot emit that symbol.  Its stem
 * differs from this file's on purpose: the Makefile compiles `%.c` and
 * `%.cpp` to the same `$(BUILD)/%.o`, so a `v34pcmif.cpp` beside
 * `v34pcmif.c` would be two sources racing for one object.  The rest of
 * `VPcmV34Main.cpp` is still to come.
 *
 * The `V34XF_` prefix is the object's, and marks the direction: these are
 * the functions the *V.34* code calls to tell the PCM side something, or to
 * ask it where to put something.  The traffic the other way is `VPcmV34*`.
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/v34digital.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
/*
 * For `VPcmV34GetCleanedSamples` and `VPcmV34GetCurrentSessionDP` below.
 * `DSPLIB_VPCM_UNWRITTEN` is deliberately NOT defined here: this file is the
 * one that DEFINES two of the five, and a definition compiled under the weak
 * macro would stop being one as soon as anything else defined the name.
 */
#include "dsplib/vpcm.h"

/*
 * The modulation numbers, which are the same five `VPcmV34InitiateRetrain`
 * takes and `v34pcmmain.cpp` spells out at its own head -- both files are
 * halves of `VPcmV34Main.cpp` and each carries the constants it uses, since
 * splitting one translation unit by language leaves no shared private header
 * to put them in.  56 is not one of `v8dp.h`'s ids and there is no V.8 code
 * for it; finding F1090 has why.
 */
#define DP_V34			34
#define DP_K56FLEX		56
#define DP_V90			90
#define DP_V92			92

/*
 * The answerer's value of `role`, the same 0x65/0x66 pair `v34modeminit`,
 * `preinitdigital` and `v34handshakinit` all test it against and the same
 * constant v34pcmmain.cpp spells `PCM_ROLE`.  Named here too because the four
 * "current" getters below test it four times and a bare 0x66 in four places
 * is four chances to transcribe 0x65.
 */
#define PCMIF_ROLE_ANSWER	0x66

/*
 * 0x1f40.  The PCM sample rate answered as a symbol rate: with a PCM receiver
 * running there is no V.34 symbol clock to report, and both baud-rate getters
 * return 8000 rather than zero.
 */
#define PCMIF_PCM_BAUD		8000

/*
 * The configuration byte `VPcmV34RequestDPNotification` clears a bit of, and
 * the bit.  `requestOutputSampleClear` in v34pcmmain.cpp SETS the same bit
 * beside the same three words, and that file spells the pair the same way --
 * both halves of `VPcmV34Main.cpp` carry the constants they use, since
 * splitting one translation unit by language leaves no shared private header.
 *
 * Named rather than written `&= 0xfe` because a mask states a bit position
 * and hides a meaning: this one is "a sample clear is outstanding", which is
 * what the setter and this clearer agree on.  A macro is a compile-time
 * substitution and cannot move code generation, so the name is free.
 */
#define CFG_FLAGS51		0x51
#define CFG_FLAG51_CLEAR	0x01

/*
 * The three-link chain `VPcmV34InitiateRateRenegotiation` and
 * `VPcmV34InitiateHangUp` both walk to hand a request to the V.90 side:
 * `p3548` (the session, a `VPcmFloModem *`) to its `V90Demodulator` at
 * +0x175c, to that demodulator's `connectionEvaluator` at +0x20c
 * (`include/dsplib/V90Demodulator.h`), to `externalDemandCode` at +0x8c of
 * THAT object (`include/dsplib/V90ConnectionEvaluator.h`, which derives the
 * name from `evaluateConnection`'s own dispatch on it).
 *
 * NAMED HERE, NOT TYPED, AND THAT IS DELIBERATE RATHER THAN LEFT OVER.  Both
 * classes at the far end are fully reconstructed now, but `p3548` stays
 * `void *` in `v34fsk.h` because this is a `.c` file and cannot include a
 * C++ class header, so reaching a real field needs a call across the
 * boundary -- the same move `v34hshak.c` makes into `V34SetINFO1aBits`.
 * That move is right where the object ITSELF calls: `V34SetINFO1aBits` is a
 * real `call` with a relocation.  It is wrong here, because the object is
 * not calling anything.  `tools/dis.py` on the blob at both use sites --
 * 0x655e (`VPcmV34InitiateRateRenegotiation`) and 0x6c16
 * (`VPcmV34InitiateHangUp`) -- shows a plain three- or four-instruction
 * load/load/store with no `call`, and `build/tc_out/src_pump_v34_v34pcmif.c.o`
 * already reproduces those exact instructions.  A wrapper function would put
 * a `call` where the object has none, costing byte identity at a site that
 * currently has it -- the same reasoning `v34pcmmain.cpp` gives for spelling
 * the neighbouring `+0x208` (`DEMOD_DESIGNER`) chain the same way up to the
 * point a real member call is reached.  So the chase stays raw pointer
 * arithmetic through these three NAMED offsets rather than becoming either a
 * guessed struct or a function call.
 */
#define SESS_DEMOD		0x175c	/* V90Modem::demodulator, a V90Demodulator*  */
#define DEMOD_CONNEVAL		0x020c	/* V90Demodulator::connectionEvaluator       */
#define CONNEVAL_EXTERNAL_DEMAND 0x8c	/* V90ConnectionEvaluator::externalDemandCode */

/*
 * ---------------------------------------------------------------------------
 * THE PUBLIC ACCESSOR SURFACE.  Everything below is what the layer above the
 * datapump calls to ask what the modem is doing or to tell it something, and
 * it is written as one batch because that is the only way it can be: the
 * harness renames every blob symbol this tree defines, so a half-written
 * group leaves the other half calling a `ref_` name that no longer exists.
 *
 * They are small, and small is where a reconstruction goes wrong quietly --
 * reading the neighbouring field, or the right field at the wrong width,
 * agrees with the object over almost every input.  So every offset below was
 * read out of `tools/dis.py` and every one of them is swept rather than
 * sampled in `test/unit/t_v34pcmif.c`.
 */

/*
 * Tear the datapump down.  Three bytes: `xor %eax,%eax; ret`.
 *
 * It reads nothing, so its ARITY IS NOT OBSERVABLE from the callee and
 * nothing in the object calls it.  One argument is what every other
 * `VPcmV34*` entry point takes and what the caller must have to name an
 * instance; the parameter is therefore a convention here and not a
 * measurement, which is also why it is unused rather than merely ignored.
 * The zero return is measured.
 */
int
VPcmV34Delete(void *objp)
{
	(void)objp;
	return 0;
}

/*
 * The datapump's block length.
 *
 * `obj + 8` IS `ptc`, WHICH ALREADY HAS A NAME FROM ANOTHER STRING, and the
 * two disagree: `initdigital` prints this offset as "PTC" in "for tx data
 * rate - %d, PTC - %d, setting nofTxBits to %d" and this function prints it
 * as "Max Block Length".  Both are the author's words for the same four
 * bytes.  The field keeps `ptc` -- the older name, and the one with a reader
 * behind it rather than only a writer -- and the conflict is recorded at D380
 * rather than resolved by preferring whichever string was read last.
 *
 * The store is reached through `obj + 4` as `0x4(%eax)`, which is the same
 * addressing artefact v34fsk.h describes on the rate group and not a second
 * object.
 */
void
VPcmV34SetMaxBlockLength(void *objp, int len)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->ptc = len;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Max Block Length modified "
				     "to %d\r\n", len);
}

/*
 * Ask for a different rate.
 *
 * `req` is the request code, and it is NOT the same enumeration on the two
 * arms: the PCM arm forwards it verbatim to the demodulator's sub-object,
 * while the V.34 arm reads only four of its values.
 *
 *      0, 2, 5   step DOWN one index, and do not go below `rate_min`
 *      3         step UP one index, and do not go above `rate_max`
 *      anything  ask for no particular rate: `rate_want` becomes -1,
 *      else      which is the value `v34handshak` rejects with `js`
 *
 * A step that would leave the bounds writes NOTHING -- `rate_want` keeps
 * whatever it held -- rather than clamping to the bound.  So a renegotiation
 * asked for at the bottom of the range still tears the handshake down and
 * still counts, it just carries the previous request.
 *
 * 0, 2 and 5 share a body because the compiler gave them one; the object
 * tests all three separately and there is no arithmetic relating them.
 */
void
VPcmV34InitiateRateRenegotiation(void *objp, int req)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);
	int want;

	if ((unsigned)(obj->status - 1) <= 1) {
		unsigned char *sess = (unsigned char *)obj->p3548;
		unsigned char *demod = *(unsigned char **)(sess + SESS_DEMOD);

		*(int *)(*(unsigned char **)(demod + DEMOD_CONNEVAL)
			 + CONNEVAL_EXTERNAL_DEMAND) = req;
		return;
	}

	/*
	 * THE STEP WRAPS AND THE COMPARISON DOES NOT.  The object steps with
	 * `dec` and `inc`, which wrap at the ends of the signed range; C's
	 * `- 1` and `+ 1` on a signed int are UNDEFINED there, and an
	 * optimiser is entitled to fold `rate_now + 1 <= rate_max` into
	 * `rate_now < rate_max` on that basis.
	 *
	 * WRITTEN THIS WAY EVEN THOUGH THE TEST CANNOT TELL.  With the plain
	 * `- 1` this file agrees with the blob byte for byte at both extremes
	 * under the compiler and flags this tree builds with -- the mutation
	 * survives.  That agreement is a property of the code generation and
	 * not of the language, so it is not something the differential tier
	 * can protect: the one case it would break is the one case it cannot
	 * see.  The clamp stays signed, because the object's are `jl`/`jg`.
	 *
	 * A rate index is 0..14 in practice, so nothing here is reachable.
	 * It is spelled correctly because it costs one cast.
	 */
	switch (req) {
	case 0:
	case 2:
	case 5:
		want = (int)((unsigned)obj->rate_now - 1u);
		if (want >= obj->rate_min)
			obj->rate_want = want;
		break;
	case 3:
		want = (int)((unsigned)obj->rate_now + 1u);
		if (want <= obj->rate_max)
			obj->rate_want = want;
		break;
	default:
		obj->rate_want = -1;
		break;
	}

	v34handshakinit(obj, 2);

	obj->progress = 6;

	rx->bad_run = 0;
	rx->bad_long_run = 0;
	rx->good_run = 0;

	*(int *)(m + 0x2218) = 5;

	/* The same event `VPcmV34IndicateLocalRRN` exists to count. */
	obj->rrn_local = (short)(obj->rrn_local + 1);
}

/*
 * ---------------------------------------------------------------------------
 * The three entry points that ask for a change of state.  Two of them tear
 * the V.34 handshake down and start it again; the third rebuilds the
 * transmitter for a V.90 rate renegotiation without going near the handshake.
 *
 * ALL THREE FORK ON WHICH MODEM IS ACTUALLY RUNNING, and the test is the same
 * one everywhere: `(unsigned)(status - 1) <= 1`.  Status 1 and 2 mean a PCM
 * receiver has the line, and then the request is handed to the C++ side down
 * a chain of three pointers instead of being acted on here.  The same test
 * picks the `V90Demodulator` branch in `VPcmV34GetCurrentTxBitRate`, which is
 * where the meaning of the two values comes from.
 *
 * WHAT THE CHAIN IS.  `p3548` is the session object; +0x175c of it is the
 * demodulator -- `VPcmV34GetCurrentRxBitRate` passes exactly that field to
 * `V90Demodulator::getBitRate` -- and +0x20c of the demodulator is
 * `connectionEvaluator`, a `V90ConnectionEvaluator *` whose +0x8c is
 * `externalDemandCode`.  THIS IS STALE AS OF THE PARAGRAPH BELOW: at the time
 * it was written none of the three was reconstructed, so the offsets were
 * spelled out rather than dressed in structs that would be guesses.  All
 * three now ARE reconstructed (`include/dsplib/V90Demodulator.h`,
 * `include/dsplib/V90ConnectionEvaluator.h`), and the macro block above this
 * section is what keeps the chase a pointer walk through named offsets
 * rather than a guess -- see it for why that is still right.
 */

/*
 * Hang up.
 *
 * The V.34 arm clears the rate REQUEST and both bounds and leaves `rate_now`
 * alone, which is the shape of "stop asking for anything" rather than "forget
 * what we settled on".  Then mode 2 of `v34handshakinit` -- the same mode the
 * renegotiation uses, because from the handshake's point of view a hang-up is
 * a renegotiation that never completes -- and the three receiver scalars at
 * +0x4bc, which are `struct v34_receiver`'s bad_run, bad_long_run and good_run and not the
 * object's own trio at +0x25c.
 *
 * The PCM arm is the only place in this file that writes the session flag at
 * `p3548 + 0x173e`; the renegotiation below does not, and that byte is the
 * only thing that distinguishes the two functions' PCM paths.
 *
 * THE THREE CLEARS HAPPEN ON BOTH ARMS, before the fork, and the diagnostic
 * before them is not gated on which modem is running either.
 */
void
VPcmV34InitiateHangUp(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: VPcmV34InitiateHangUp called !\r\n");

	obj->rate_want = 0;
	obj->rate_min = 0;
	obj->rate_max = 0;

	if ((unsigned)(obj->status - 1) <= 1) {
		unsigned char *sess = (unsigned char *)obj->p3548;
		unsigned char *demod;

		sess[0x173e] = 1;
		demod = *(unsigned char **)(sess + SESS_DEMOD);
		*(int *)(*(unsigned char **)(demod + DEMOD_CONNEVAL)
			 + CONNEVAL_EXTERNAL_DEMAND) = 2;
		return;
	}

	v34handshakinit(obj, 2);

	obj->progress = 6;
	*(int *)(m + 0x2218) = 5;

	rx->bad_run = 0;
	rx->bad_long_run = 0;
	rx->good_run = 0;
}

/*
 * Which of the four modulations the session settled on, as a datapump id.
 *
 * The selector is `status`, the same word at +0 that `VPcmV34InitiateHangUp`
 * and `VPcmV34InitiateRateRenegotiation` test with `(unsigned)(status-1) <= 1`
 * to mean "a PCM receiver is running" -- so 1 and 2 are the two PCM cases
 * here and they are exactly the two that do not answer V.34.
 *
 * 1 IS THE CASE THAT IS DECIDED BY SOMETHING ELSE, and it is decided by the
 * V.92 pair `V34GiveINFO0dBits` names: the answer is V.92 only when BOTH
 * `local_v92` and `remote_v92` are non-zero, and V.90 whenever either is
 * zero.  That is the negotiation read the way it is stored -- one side's
 * willingness is not enough.  2 answers V.92 outright, without consulting
 * either, which is what makes it a different status and not a shorthand.
 *
 * 3 IS K56FLEX AND IT IS 56, not one of `v8dp.h`'s ids.  Finding F1090
 * measured that the class behind it is `ret` throughout in this build, so
 * this is the only place a K56flex session can be *named*; nothing downstream
 * of the name does anything.  Everything else -- including every value the
 * status word takes on a V.34 call -- falls through to 34.
 */
int
VPcmV34GetCurrentSessionDP(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	switch (obj->status) {
	case 1:
		if (obj->local_v92 != 0 && obj->remote_v92 != 0)
			return DP_V92;
		return DP_V90;
	case 2:
		return DP_V92;
	case 3:
		return DP_K56FLEX;
	default:
		return DP_V34;
	}
}

/*
 * Whether this connection came up on a quick connect.
 *
 * A BIT SET OVER `status`, NOT A COMPARISON CHAIN.  The object builds
 * `1 << status` once and tests it against three masks in turn, which is what
 * a `switch` with grouped cases compiles to and is written back as the masks
 * because the groups have no arithmetic relating them:
 *
 *     6f09  cmp $0xa,%ecx; ja        status > 10 -> 0, and UNSIGNED, so a
 *                                    negative status leaves here too
 *     6f15  test $0xe7,%dl           0,1,2,5,6,7 -> the stored answer
 *     6f1a  test $0x408,%edx         3,10        -> 0
 *     6f22  and  $0x310,%edx         4,8,9       -> 1
 *
 * The three masks cover 0..10 exactly once each, so the final zero is
 * unreachable from the shift and is the `default` the object still emits.
 *
 * `is_short` is the field `V34SetINFO1aBits` sends as the phase-2 length, so
 * "quick connect" here means the short phase 2 that field selects, and the
 * six states that return it are the ones where the negotiation has got far
 * enough to know.
 */
int
VPcmV34GetQuickConnectIndication(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	int mask;

	if ((unsigned int)obj->status > 10u)
		return 0;

	mask = 1 << obj->status;

	if ((mask & 0xe7) != 0)
		return obj->is_short;
	if ((mask & 0x408) != 0)
		return 0;
	if ((mask & 0x310) != 0)
		return 1;

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * The four "current" getters that answer in baud and in hertz.
 *
 * THEY LOOK LIKE FOUR COPIES OF ONE FUNCTION AND THEY ARE NOT.  All four open
 * on `role == 0x66` and then on a range test over `status`, and all four
 * differ inside it -- the crossing is the same one v34pcmmain.cpp documents
 * for the two BIT rate getters, and the two carriers are a third and fourth
 * shape again:
 *
 *   RxBaudRate   0x66 -> status-1   else status-2   in 0..1 -> 8000
 *   TxBaudRate   0x66 -> status-2   else status-1   in 0..1 -> 8000
 *   RxCarrier    0x66 -> status-1   else status-2   in 0..1 -> 0
 *   TxCarrier    0x66 -> status-2 in 0..1 -> 0;  else status == 2 -> 0
 *
 * so the receive pair and the transmit pair are each other's mirror on the
 * ROLE test, and `VPcmV34GetCurrentTxCarrier` alone asks a single-value
 * question on its non-PCM arm rather than a range one.  Writing any of them
 * from the shape of its neighbour gets it wrong for exactly one value of
 * `status`, which is why the test sweeps both fields over their whole range
 * rather than at a representative point.
 *
 * The subtraction is unsigned and the comparison `jbe`, so `status` below the
 * offset wraps high and takes the far arm; spelled with the cast the object's
 * `sub; cmp; jbe` requires.
 *
 * 8000 is the PCM sample rate answered as a symbol rate, and 0 is "there is
 * no carrier" -- a PCM receiver has none.
 *
 * TWO MORE THINGS ARE THE AUTHOR'S AND BOTH ARE MEASURED, which is why these
 * do not read like the `int n;` version anybody would write first.  All four
 * went byte-exact together on the pair of them, +5 symbols over this file:
 *
 *   - THE ROLE TEST IS DUPLICATED PER ARM.  Reducing it to one `n` and a
 *     single range test costs each of these two bytes: the object tests and
 *     returns inside each arm and joins only at the final return.  Eight
 *     return shapes were compiled -- two returns via `n`, single-exit
 *     if/else, single-exit pre-seeded, ternary, inverted test, a `short`
 *     result -- and exactly ONE reaches zero.  A unique preimage.
 *   - NO `ratecfg` POINTER IS LIVE ACROSS THE TEST.  Held in a local, GCC
 *     3.4.2 materialises the address with a `lea` before the branch; written
 *     at the point of use it folds into the load's own displacement, which is
 *     what the object does.  Nine access spellings compiled; five reach it,
 *     so what is decoded is that FACT and not this particular cast.
 *
 * The two are independent and the cross product says so: giving the other
 * three the shape while keeping their `cfg` local moved nothing, and
 * `VPcmV34GetCurrentTxCarrier` had the shape already and still needed its
 * local inlined.  Do not "tidy" either back.
 */
int
VPcmV34GetCurrentRxBaudRate(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (obj->role == PCMIF_ROLE_ANSWER) {
		if ((unsigned int)(obj->status - 1) <= 1u)
			return PCMIF_PCM_BAUD;
	} else if ((unsigned int)(obj->status - 2) <= 1u) {
		return PCMIF_PCM_BAUD;
	}

	return ((const struct v34_ratecfg *)
	    ((unsigned char *)obj + V34_RATECFG))->rx_baud;
}

int
VPcmV34GetCurrentTxBaudRate(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (obj->role == PCMIF_ROLE_ANSWER) {
		if ((unsigned int)(obj->status - 2) <= 1u)
			return PCMIF_PCM_BAUD;
	} else if ((unsigned int)(obj->status - 1) <= 1u) {
		return PCMIF_PCM_BAUD;
	}

	return ((const struct v34_ratecfg *)
	    ((unsigned char *)obj + V34_RATECFG))->baud;
}

int
VPcmV34GetCurrentRxCarrier(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (obj->role == PCMIF_ROLE_ANSWER) {
		if ((unsigned int)(obj->status - 1) <= 1u)
			return 0;
	} else if ((unsigned int)(obj->status - 2) <= 1u) {
		return 0;
	}

	return ((const struct v34_ratecfg *)
	    ((unsigned char *)obj + V34_RATECFG))->rx_carrier;
}

int
VPcmV34GetCurrentTxCarrier(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (obj->role == PCMIF_ROLE_ANSWER) {
		if ((unsigned int)(obj->status - 2) <= 1u)
			return 0;
	} else if (obj->status == 2) {
		return 0;
	}

	return ((const struct v34_ratecfg *)
	    ((unsigned char *)obj + V34_RATECFG))->carrier;
}

/*
 * The signal-to-noise ratio, in whole dB.
 *
 * NO LOGARITHM AND NO TABLE: the object divides down by two constants and
 * counts how many times it can, which is a base-conversion of the ratio into
 * decibels with the two step sizes chosen so the counts add.
 *
 *     0x1013 / 0x4000 = 0.25074   ~= -6.0 dB, counted 6 at a time
 *     0x32d6 / 0x4000 = 0.79431   ~= -1.0 dB, counted 1 at a time
 *
 * so the coarse loop takes the ratio down to below 4 and the fine loop
 * finishes it, and the answer is the sum of the two counts.  The coarse loop
 * hands the fine one the LAST value that was still positive, not the one that
 * ended it -- `mov %ecx,%esi` at 0x717b saves the pre-multiply value each
 * time round -- so the two loops do not double-count the step between them.
 *
 * WHAT IS DIVIDED BY WHAT.  `sig_energy` over `equerr`: the equaliser error energy
 * republished every 1024 symbols (v34recv.h names it from receiver's own
 * "V34EQU, equerr = %d, preerr = %d") divides into the int at +0x248.  A
 * ratio reported in dB with the error underneath is a signal-to-noise ratio,
 * which is the only thing this says about +0x248 and is not enough to name
 * it: `equerr` could equally be the numerator of something else.
 *
 * `equerr` IS SIGNED AND THE GUARD IS `jle`, so a zero or negative error
 * returns 0 dB rather than dividing.  Declared `short` for that reason and
 * not `unsigned short`, which would make the guard `jbe` and let a large
 * positive error through as a divisor.
 *
 * BOTH MULTIPLIES ARE SPELLED THROUGH `unsigned`, AND THAT IS THE
 * INSTRUCTION AND NOT A STYLE.  0x7175 and 0x7191 are `imul $imm,%r,%r`
 * followed by `sar $0xe` -- a 32-bit multiply that WRAPS, then an arithmetic
 * shift -- and the loop exit is `test`/`jg` on the shifted result.  A ratio
 * past 2^31/0x1013, which is about 522,000 and which
 * `VPcmV34GetDiagnostics` can reach, therefore wraps NEGATIVE and ends the
 * loop; that is the object's behaviour and the only way out for a large
 * ratio.  Written as a plain `v * 0x1013` it is signed overflow, which is
 * undefined, and GCC 13 duly assumes a positive `v` keeps a positive
 * product and compiles the coarse loop into one that never terminates.
 * `(int)((unsigned)v * 0x1013u) >> 14` is the same two instructions with the
 * wrap made legal -- the same reason `VPcmV34InitiateRateRenegotiation`'s
 * step is spelled unsigned, and the same class of defect as findings F2300 to
 * 2302.
 *
 * THIS BODY IS INLINED VERBATIM INTO `VPcmV34GetDiagnostics` at 0x7691, so
 * whatever shape reproduces it here has to reproduce it there too.  It is
 * left as one function rather than factored into a static helper, because a
 * helper has no blob symbol and its bytes would count against neither side.
 */
int
VPcmV34GetSNR(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)
	    ((unsigned char *)obj + 0x264);
	int db = 0;
	int last = 0;

	if (rx->equerr > 0) {
		int v = rx->sig_energy / rx->equerr;

		if (v > 0) {
			for (;;) {
				last = v;
				v = (int)((unsigned int)v * 0x1013u) >> 14;
				if (v <= 0)
					break;
				db += 6;
			}
		}
	}

	if (last > 0) {
		for (;;) {
			last = (int)((unsigned int)last * 0x32d6u) >> 14;
			if (last <= 0)
				break;
			db += 1;
		}
	}

	return db;
}

/*
 * `getTimingOffset` -- 0x71b0, 11 bytes -- and `getTimingPhase` -- 0x71c0,
 * 11 bytes.  Two one-load accessors with NO CALLER anywhere in the object
 * (the no-entry-point bucket: exported API surface), sitting between
 * `VPcmV34GetSNR` and `VPcmV34GetCleanedSamples` where every neighbour takes
 * the V.34 object.  The field comments in `v34fsk.h` carry the naming
 * derivation; nothing but these two functions reads either word.
 */
int
getTimingOffset(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return obj->timing_offset;
}

int
getTimingPhase(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return obj->timing_phase;
}

/*
 * ---------------------------------------------------------------------------
 * TWO OF `vpcm_run`'s FIVE CALLEES, and they are the two that are leaves.
 *
 * `include/dsplib/vpcm.h` declares all five WEAK so that a binary which does
 * not define them links with the reference resolved to zero, and `vpcm_run`
 * tests each pointer before it calls through it.  These two are defined here
 * -- so in every binary that links this file they are no longer null and
 * `vpcm_run` takes the real call rather than `vpcm_notwritten`.  The other
 * three stay unwritten: `VPcmV34Progress` is 7,278 bytes whose closure is the
 * whole receive chain, and both rate getters need `V90Demodulator::getBitRate`
 * (87 bytes), which is a const accessor and belongs to whoever owns that
 * class's processing methods rather than to this batch.
 *
 * They belong in THIS file for the reason the header comment gives: they are
 * `extern "C"` exports of `VPcmV34Main.cpp` -- no mangling on the relocation
 * `vpcm_run` carries for either -- and nothing about them is C++.
 *
 * The declarations come from `vpcm.h` with `DSPLIB_VPCM_UNWRITTEN` left
 * empty, which is what makes these definitions STRONG.  Defining them under
 * the weak macro would link identically today and would silently stop being
 * a definition the moment a real one appeared elsewhere.
 */

/*
 * The echo-cancelled samples the host's data logger asks for, and the count
 * it has accumulated since the last ask.
 *
 * `hist_2f58` is the ring `modem_serrint` fills and `hist2_idx` is its write
 * index, so the count handed back is the index and reading it RESETS it --
 * this is a drain, not a peek, and the zeroing at 0x71e1 is the whole of the
 * function's effect on the object.  The load is `movswl`, so the index is
 * read SIGNED into the caller's int: `hist2_idx` is a `short` and the object
 * sign-extends it rather than masking.
 *
 * The return is `obj + 0x2f58` computed as an `add`, with no test of the
 * count first -- a caller told "0 samples" still gets the buffer address.
 */
void *
VPcmV34GetCleanedSamples(void *objp, int *n)
{
	struct v34_object *obj = (struct v34_object *)objp;

	*n = obj->hist2_idx;
	obj->hist2_idx = 0;
	return obj->hist_2f58;
}

/*
 * The datapump is told something happened.
 *
 * FOUR CODES AND THE OBJECT TESTS THEM AS A `switch`: `cmp $1; je`, then
 * `jle` into a test against zero, then `cmp $2` and `cmp $3`.  Anything else
 * does nothing at all, and that includes every negative value.
 *
 * TWO OF THE THREE FIELDS ARE NAMED BY THESE STRINGS.  +0x262 is set to 1
 * under "Valid in samples" and to 0 under "Invalid in samples", which is what
 * makes it `samples_valid`; and case 3 prints "mohTimer = %d, setting count2
 * to %d" with +0xabdc first and +0xaa74 second, so the MOH sample counter
 * v34pcmmain.cpp calls `O_MOHCOUNT` is the object's `mohTimer` and +0xaa74
 * is its `count2`, a deadline 48,000 samples further on.
 *
 * `train_symcount` KEEPS ITS OFFSET NAME.  "count2" is what the string calls it, and
 * that is a position in a set of counters rather than a description; nothing
 * here reads it back.
 *
 * The diagnostics are `edprintf` and are NOT gated at the call site -- the
 * whole function is unguarded and `edprintf` applies its own level -- unlike
 * `VPcmV34InitMOH` next door, which tests first.  That difference is the
 * object's.
 */
void
VPcmV34NotifyDP(void *objp, int what)
{
	struct v34_object *obj = (struct v34_object *)objp;

	switch (what) {
	case 0:
		edprintf("VPcmV34 Notification: Invalid in samples...\r\n");
		obj->samples_valid = 0;
		break;

	case 1:
		edprintf("VPcmV34 Notification: Valid in samples...\r\n");
		obj->samples_valid = 1;
		break;

	case 2:
		edprintf("VPcmV34 Notification: CAS Detected...\r\n");
		obj->status = 5;
		obj->sample_count = 0;
		break;

	case 3: {
		int moh = obj->moh_timer;
		int count2 = moh + 0xbb80;

		edprintf("VPcmV34 Notification: Validate 3-Way Call...\r\n");
		obj->status = 6;
		obj->train_symcount = count2;
		edprintf("VPcmV34 debug validate: mohTimer = %d, setting "
			 "count2 to %d\r\n", moh, count2);
		break;
	}

	default:
		break;
	}
}

/*
 * Collect the pending output-sample-clear request, if there is one.
 *
 * THE OTHER END OF `requestOutputSampleClear`, which v34pcmmain.cpp writes:
 * that arm stores 1, a sample count and 0 into +0xac40, +0xac44 and +0xac48
 * and sets bit 0 of the configuration's +0x51; this hands all three out and
 * puts the mailbox back to -1, 0, 0 with that same bit cleared.  Between them
 * they are what names the three words, which v34fsk.h carried as
 * `unmapped_ac40` while only the writer existed.
 *
 * NEGATIVE MEANS EMPTY, and the test is `js` on the flag alone: an empty
 * mailbox returns 0 and writes NOTHING through the three pointers, so a
 * caller that does not check the return reads its own uninitialised
 * variables rather than stale ones.
 *
 * The reset order is the object's -- `clr_done` first, then `clr_flag`, then
 * `clr_count` -- and the configuration byte is reached last, through `pac3c`
 * loaded between the two flag stores.
 */
int
VPcmV34RequestDPNotification(void *objp, int *flag, int *count, int *done)
{
	struct v34_object *obj = (struct v34_object *)objp;
	int pending = obj->clr_flag;

	if (pending < 0)
		return 0;

	*flag = pending;
	*count = obj->clr_count;
	*done = obj->clr_done;

	/*
	 * `clr_flag` FIRST, and that is DECODED, not transcribed.  The blob
	 * EMITS these as clr_done, clr_flag, clr_count, and this file used to
	 * say exactly that -- which is the trap: a reconstruction writes the
	 * stores down in the order they come out, so our source order was
	 * already the object's emission order and was therefore the answer
	 * sheet rather than a candidate.  What has to be found is its PREIMAGE
	 * under GCC 3.4.2's scheduling, and the compiler swaps the first two.
	 * All 4! orderings of this tail (these three plus the
	 * `&= ~CFG_FLAG51_CLEAR` below) were compiled on the period toolchain:
	 * 24 cells, 12 distinct emissions, exactly ONE at zero differing
	 * bytes, nearest other cell at two.  A unique preimage.
	 */
	obj->clr_flag = -1;
	obj->clr_done = 0;
	obj->clr_count = 0;

	*((unsigned char *)obj->pac3c + CFG_FLAGS51) &=
		(unsigned char)~CFG_FLAG51_CLEAR;

	return 1;
}

/*
 * Set the transmit scale.
 *
 * NO PARAMETER AND NO CHOICE: 0x16a1 is built in, stored, and then reported
 * through `edprintf` -- which is not behind a debug-level test here, because
 * `edprintf` applies its own.  Every other diagnostic in this file is gated
 * at its call site; this one is not, and that difference is the object's.
 */
void
VPcmV34SetTxScale(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->tx_scale = 0x16a1;
	edprintf("VPcmV34SetTxScale: tx scale set to %d\r\n", 0x16a1);
}

/*
 * ---------------------------------------------------------------------------
 * The two questions the transmitter asks the PCM configuration.
 *
 * Both reach through `pac3c` for what was asked for and through `p3548` for
 * what the PCM receiver actually has, and both take the smaller.  They are
 * next to each other in .text and they share the pattern; nothing else does.
 *
 * ONE FIELD OF THE SESSION GATES BOTH: `p3548 + 0x6120`, an int, tested
 * against zero before either of them will look at the PCM side at all.  What
 * it means is not in this translation unit -- it is read and never written
 * here -- but it is always paired with `v90_receiver`, so it reads as "a PCM
 * modem exists" against "and it has got somewhere".
 */

/*
 * How far the transmit power must be backed off, in dB.
 *
 * THREE THINGS HAPPEN AND THE NAME MENTIONS ONE.  The return value is the
 * configured reduction clamped to [-10, +7]; on the way there the function
 * also sets the echo canceller's three adaptation constants and flips a flag
 * in the PCM receiver.  Both diagnostics say so -- the first names the three
 * constants it just wrote, and it is the object's own words that give
 * `echo_decay_start`, `echo_decay_fact` and `echo_beta` the names "decay start", "decay fact" and
 * "beta", which nothing else in the tree could have supplied.
 *
 * THE CLAMP IS ASYMMETRIC and it is a clamp rather than a saturate-to-zero:
 * -10 dB is a real answer and so is +7.  The comparisons are 16-bit and
 * signed, so a configuration asking for -20 gets -10 rather than 236.
 *
 * WHICH ARM IS TAKEN IS NOT AN `||`, AND THE POLARITY OF +0x4f8 IS THE
 * SURPRISE.  With V.90 up the K56Flex word is consulted only when the PCM
 * object's +0x4f8 is SET; when it is clear the reduction stands on the V.90
 * side alone.  +0x4f8 is the same field `VPcmV34GetMaxUpstreamRateIndex`
 * calls a "sensitive ISP", so the reading is that a sensitive line will not
 * take a V.90-only word for it -- and it is `je`, at 0x7bff, which this
 * reconstruction got backwards first and the sweep caught on the first run.
 *
 * `red` is forced to zero when nothing has the line, which is what makes the
 * sign test below a three-way decision spelled as two.
 */
short
GetVPcmMinimalTxPowerReduction(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *sess = (unsigned char *)obj->p3548;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	unsigned char *pcm = *(unsigned char **)(sess + 0x610c);
	short want = *(const short *)(cfg + 0x44);
	short red;

	if (want < -10)
		red = -10;
	else if (want > 7)
		red = 7;
	else
		red = want;

	if (!(*(const int *)(sess + 0x6120) != 0 && obj->v90_receiver != 0
	      && *(const int *)(pcm + 0x4f8) == 0)
	    && obj->k56flex_receiver == 0)
		red = 0;

	if (red > 0) {
		*(int *)(pcm + 0x4f4) = 0;
		obj->echo_decay_start = 0x7d0;
		obj->echo_decay_fact = 0x7fdf;
		obj->echo_beta = 2;
	} else {
		*(int *)(pcm + 0x4f4) = 1;
		obj->echo_decay_start = 0x7d0;
		obj->echo_decay_fact = 0x7fcb;
		/*
		 * 4 when the configuration's +0x54 is exactly 4 and 6
		 * otherwise -- `cmpl $4; setne; lea 4(%ecx,%ecx,1)`, which is
		 * a two-way choice and not arithmetic on the field.
		 */
		obj->echo_beta = (*(const int *)(cfg + 0x54) == 4) ? 4 : 6;
	}

	edprintf("VPcmV34Main: Due to final MinTXPR = %d, setting echo: "
		 "decay start = %d, decay fact = %d, beta = %d\r\n",
		 (int)red, obj->echo_decay_start, obj->echo_decay_fact, obj->echo_beta);

	/*
	 * The session pointer is RE-LOADED for the second report rather than
	 * kept -- `mov 0x610c(%esi),%ebp` at 0x7b67, after the first call.
	 * Reproduced by reading it again; nothing here can change it, so the
	 * two spellings agree, and it is written this way because the object
	 * is.
	 */
	edprintf("VPcmV34Main: Get Minimal power reduction - returning %d "
		 "(cfg flag set to %d)\r\n",
		 (int)red, *(int *)(*(unsigned char **)(sess + 0x610c) + 0x4f4));

	return red;
}

/*
 * The upstream rate cap, in BITS PER SECOND despite the name.
 *
 * `pac3c + 0x3c` is what was configured and the PCM receiver's own limit is
 * `p610c + 0x4fc` multiplied by 2400, so the second is an index and the
 * first is not -- the function returns the smaller of the two and the units
 * of the answer are the configured field's.  The two diagnostics are how the
 * arms are told apart: "regular ISP" is the configuration unqualified and
 * "sensitive ISP" is the one the receiver has capped.
 *
 * THE V.90 TEST IS `> 1`, NOT `!= 0`, which is the only place in this file
 * that reads `v90_receiver` as a position on its ladder rather than as a
 * flag.  1 is below every value the four Indicate entry points set, so what
 * it excludes is a receiver that has been created and has received nothing.
 */
int
VPcmV34GetMaxUpstreamRateIndex(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const unsigned char *sess = (const unsigned char *)obj->p3548;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	int rate = *(const int *)(cfg + 0x3c);

	if (*(const int *)(sess + 0x6120) != 0 && obj->v90_receiver > 1) {
		const unsigned char *pcm =
			*(unsigned char *const *)(sess + 0x610c);

		if (*(const int *)(pcm + 0x4f8) != 0) {
			int cap = *(const int *)(pcm + 0x4fc) * 0x960;

			if (rate > cap)
				rate = cap;

			edprintf("on get max upstream rate, on sensitive ISP, "
				 "returning %d\r\n", rate);
			return rate;
		}
	}

	edprintf("on get max upstream rate, on regular ISP, returning %d\r\n",
		 rate);
	return rate;
}

/*
 * The upstream rate cap, again.
 *
 * BYTE FOR BYTE THE SAME FUNCTION AS `VPcmV34GetMaxUpstreamRateIndex` above,
 * 123 bytes against 128 and the difference is register allocation: the same
 * two arms, the same five offsets, the same two `edprintf` strings.  The
 * object exports both names, 0x7c10 and 0x7c90, so the source has the body
 * twice -- once under each prefix, which is what the `V34XF_` / `VPcmV34`
 * split means (the direction of the call, not the work).  Transcribed twice
 * for that reason rather than one of them calling the other, which would
 * leave a `call` the object does not have.
 */
int
V34XF_GetMaxUpstreamRateIndex(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const unsigned char *sess = (const unsigned char *)obj->p3548;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	int rate = *(const int *)(cfg + 0x3c);

	if (*(const int *)(sess + 0x6120) != 0 && obj->v90_receiver > 1) {
		const unsigned char *pcm =
			*(unsigned char *const *)(sess + 0x610c);

		if (*(const int *)(pcm + 0x4f8) != 0) {
			int cap = *(const int *)(pcm + 0x4fc) * 0x960;

			if (rate > cap)
				rate = cap;

			edprintf("on get max upstream rate, on sensitive ISP, "
				 "returning %d\r\n", rate);
			return rate;
		}
	}

	edprintf("on get max upstream rate, on regular ISP, returning %d\r\n",
		 rate);
	return rate;
}

/*
 * Two reports with no reader.
 *
 * `objp` is deliberately unused: the object writes the format pointer into
 * the incoming argument slot and tail-jumps into `dsplibs_debug_printf`, so
 * the parameter is the slot rather than an input.  Both are gated at the
 * call site, unlike `VPcmV34SetTxScale` below, which lets `edprintf` do it.
 * That inconsistency is the original's.
 */
void
VPcmV34ReportStartOfEchoAdapt(void *objp)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Echo adapt start " "reported...\r\n");
}

void
VPcmV34ReportMiddleOfEchoAdapt(void *objp)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Echo adapt middle " "reported...\r\n");
}

int *
V34XF_GetInfo0BitsPtr(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return obj->info0_bits;
}

/*
 * ---------------------------------------------------------------------------
 * Two address handouts and one scalar read.
 *
 * The first two are `return &obj->field` and nothing else -- no bounds, no
 * copy, no state.  They exist because the caller is in another translation
 * unit and the V.34 object's layout is private to this one.
 */

double *
V34XF_GetProbeResultsPtr(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return obj->probe_results;
}

/*
 * The round-trip delay, plus 480.
 *
 * READ UNSIGNED, RETURNED SIGNED, AND THE WRAP IS REAL: the object does
 * `movzwl` on the field, adds 0x1e0 in 32 bits, then `cwtl` -- so a stored
 * delay above 0xfe20 comes back as a small negative number rather than as
 * anything clamped.  `v34handshak` both writes and reads +0xaa7e as a signed
 * short elsewhere, so the unsigned load here is this function's alone.
 *
 * 480 samples is 60 ms at 8 kHz.  What it is compensating for is on the
 * caller's side and not visible here.
 */
short
V34XF_GetRTD(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return (short)((unsigned short)obj->rtd + 0x1e0);
}

/*
 * Rebuild the transmitter for a V.90 rate renegotiation.
 *
 * NO HANDSHAKE: this is the one of the three that does not call
 * `v34handshakinit`.  What it does instead is `v34handshakinit`'s mode 2
 * block with the state machines left out -- the same four transmit-queue
 * fields, the same mask on `tx_flags`, the same `preinitdigital`, the same
 * `short_382` pair.  Two independent readings of one block, which is the
 * corroboration that block was read right.
 *
 * IT ALSO WINDS `v90_receiver` BACKWARDS.  That field is documented as a
 * ratchet the phase-3 indications only advance; here it is assigned, so a
 * renegotiation can move it down.  See D48.
 *
 * `rrn_type` is tested against zero only, and `constel_size` likewise -- the
 * two `short_382` values differ by 32 and are the pair `V34XF_IndicateJdReceived`
 * chooses between on its own constellation-size bit.  The parameter names are
 * the object's, from the diagnostic below.
 */
void
VPcmV34SetV90RateReneg(void *objp, short rrn_type, unsigned char constel_size)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"setV90RateReneg called, rrn type = %d, " "constel size = %d\r\n",
			(int)rrn_type, (int)constel_size);

	/*
	 * UNSIGNED, and that is the whole content of the test: the object
	 * compares with `cmp $1` and reads the borrow, so only zero takes the
	 * low arm.  A negative `rrn_type` takes the high one.
	 */
	obj->v90_receiver = (rrn_type != 0) ? 15 : 11;

	obj->prev_quadrant = 0;
	obj->seg_symcount = 0;
	obj->tx_scr_sr = 0;
	obj->tx_flags = (short)((obj->tx_flags & ~0x4018) | 0x2000);

	preinitdigital(obj);

	/* The `[1]` counter every handshake trace prints; see v34hshak.c. */
	*(short *)(m + 0x2aa2) = 0;

	obj->progress = 6;
	*(int *)(m + 0x2218) = 5;

	obj->short_382 = (short)(constel_size != 0 ? 0x89b0 : 0x8990);

	/*
	 * The timer, reset: the same three fields and the same two constants
	 * as `v34handshakinit`'s guard writes when it rejects the span, with
	 * +0x244 left alone here and written there.
	 */
	*(int *)(m + 0x238) = 0;
	*(int *)(m + 0x248) = (int)0xfff15a00;
	*(int *)(m + 0x23c) = 0x69780;
}

/*
 * ---------------------------------------------------------------------------
 * Four indications: the V.34 handshake telling the PCM side that a phase-3
 * message has arrived.
 *
 * Each one moves `v90_receiver` (or `k56flex_receiver`) forward, and the
 * numbers are a ratchet rather than a set of flags -- see the TRN2d case,
 * which is the only one that reads the field before writing it.
 */

/*
 * A Jd has been received.
 *
 * `constel` and `silence_scr` are the two bits the message carried, and they
 * pick between three values for +0x382, which nothing in this object reads.
 * The silence flag wins outright: with it set the constellation size is not
 * consulted at all.
 */
void
V34XF_IndicateJdReceived(void *objp, unsigned char constel,
			 unsigned char silence_scr)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34Main: IndicateJdReceived - constel size = %d, " "silence SCR = %d\r\n",
			constel, silence_scr);

	obj->v90_receiver = 3;

	if (silence_scr != 0)
		obj->short_382 = 0;
	else if (constel != 0)
		obj->short_382 = (short)0x89b0;
	else
		obj->short_382 = (short)0x8990;
}

/*
 * A DIL has been received.
 *
 * Same two values for +0x382 on the same constellation-size test, and one
 * extra store: +0x25c0 is cleared.  That field is the transmit block's, and
 * the object reaches it as `0x3a4(obj + 0x221c)` -- through the transmit
 * queue's base rather than the object's -- which is what says it belongs to
 * the transmitter and not here.
 */
void
V34XF_IndicateDilReceived(void *objp, unsigned char constel)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34Main: IndicateDilReceived - constel size = %d\r\n",
			constel);

	obj->v90_receiver = 6;
	obj->seg_symcount = 0;

	if (constel != 0)
		obj->short_382 = (short)0x89b0;
	else
		obj->short_382 = (short)0x8990;
}

/*
 * A TRN2d has been received.
 *
 * The only one that reads `v90_receiver` first, and it is a RATCHET WITH
 * FOUR RUNGS, not an assignment: whatever the field holds is rounded up to
 * the next of 10, 14, 18, 20 and can never move backwards, because each
 * band's floor is the previous band's value.
 *
 * The last two rungs are one comparison in the object -- `cmp $0x11; setg;
 * lea 0x12(%eax,%eax,1)` computes 18 or 20 branchlessly -- which is the
 * compiler's, not a different rule.
 */
void
V34XF_IndicateTrn2dReceived(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	int v = obj->v90_receiver;

	if (v <= 9)
		v = 10;
	else if (v <= 14)
		v = 14;
	else if (v <= 17)
		v = 18;
	else
		v = 20;

	obj->v90_receiver = v;

	/* Re-read, not `v`: the object loads the field back for the print. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"IndicateTrn2dReceived called, v90Receiver = %d\r\n",
			obj->v90_receiver);
}

/*
 * ---------------------------------------------------------------------------
 * Cap the V.34 symbol rate by doctoring the line probe.
 *
 * `chkForceBaudRate` is the one function in this file that does not touch the
 * V.34 object at all -- it reads two of its fields and writes the DFT bank the
 * caller hands it.  `probeselect` calls it twice, both times with
 * `obj->probe_bins`, and then goes on to pick a symbol rate from the very
 * energies and shifts this has just adjusted.  So the mechanism is indirect:
 * there is no "forced rate" variable anywhere, only a probe measurement that
 * has been made to look as though the high bins were never received.
 *
 * WHICH BIN STANDS FOR WHICH RATE.  The six entries of `allow` are the six
 * V.34 symbol rates in the standard's order -- 2400, 2743, 2800, 3000, 3200
 * and 3429 baud -- and the object maps the top four onto bins whose centres
 * are the band edge each rate needs, one bin being 150 Hz:
 *
 *     allow[5]  3429 baud  ->  bins[24]   3750 Hz   shift := 11
 *     allow[4]  3200 baud  ->  bins[22]   3450 Hz   shift :=  7
 *     allow[3]  3000 baud  ->  bins[21]   3300 Hz   shift :=  7
 *                               bins[ 2]   450 Hz   shift :=  7
 *     allow[2]  2800 baud  ->  bins[20]   3150 Hz   shift :=  7
 *                               bins[19]  3000 Hz   shift :=  7
 *
 * `allow[0]` and `allow[1]` are written and never read: the two lowest rates
 * have no bin to spoil, because nothing about them is out at the edge.  The
 * rate-to-bin assignment is the object's; the frequencies are this
 * reconstruction's arithmetic on the 150 Hz spacing finding F212 establishes,
 * and the pairing of two bins with 3000 and 2800 is not explained by it.
 *
 * WHERE THE LIMIT COMES FROM, AND WHERE IT IS APPLIED, ARE DIFFERENT PLACES.
 * The limit is the top three bits of a configuration byte at `pac3c + 0x50`.
 * What it is applied TO depends on which PCM receiver is running:
 *
 *   - V.90 active (`v90_receiver` set): the six flags live in the SESSION
 *     object, at `p3548 + 0x217`, and this edits them in place -- so the
 *     V.90 side's own idea of which rates are on the table both feeds this
 *     decision and is narrowed by it.
 *   - otherwise: a local array, seeded 1,1,1,1,1,x, so the cap is the only
 *     thing that can clear anything and the decision is made afresh.
 *
 * The `x` is 0 when K56Flex is running and 1 when neither is, which is the
 * only thing the K56Flex arm changes.
 *
 * `allow[5] = 1` on the V.90 arm is a DEAD STORE -- `sel` points at the
 * session object there, and the local is not read again.  Reproduced because
 * it is what the object does; see D49.
 */
void
chkForceBaudRate(void *objp, struct v34_dftbin *bins)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	unsigned char allow[6] = { 0 };
	unsigned char *sel;
	int maxidx;

	maxidx = cfg[0x50] >> 5;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VpcmV34Main: max V34 baud rate index "
				     "= %d\r\n", maxidx);

	allow[0] = 1;
	allow[1] = 1;
	allow[2] = 1;
	allow[3] = 1;
	allow[4] = 1;

	if (obj->v90_receiver) {
		allow[5] = 1;			/* dead -- see the note above */
		sel = (unsigned char *)obj->p3548 + 0x217;
	} else if (obj->k56flex_receiver) {
		allow[5] = 0;
		sel = allow;
	} else {
		allow[5] = 1;
		sel = allow;
	}

	/*
	 * Index 0 CLEARS NOTHING, and it is guarded separately rather than
	 * falling out of the chain: with `maxidx` zero every comparison below
	 * would hold and every rate would be barred.  So zero means "no cap
	 * configured" and not "cap at the lowest rate".
	 */
	if (maxidx != 0) {
		if (maxidx <= 1)
			sel[1] = 0;
		if (maxidx <= 2)
			sel[2] = 0;
		if (maxidx <= 3)
			sel[3] = 0;
		if (maxidx <= 4)
			sel[4] = 0;
		if (maxidx <= 5)
			sel[5] = 0;
	}

	if (sel[5] == 0)
		bins[24].shift = 11;
	if (sel[4] == 0)
		bins[22].shift = 7;
	if (sel[3] == 0) {
		bins[2].shift = 7;
		bins[21].shift = 7;
	}
	if (sel[2] == 0) {
		bins[20].shift = 7;
		bins[19].shift = 7;
	}
}

/*
 * A K56flex Jd has been received.
 *
 * THE FIFTH INDICATION, and unlike the other four it does not move
 * `v90_receiver` or `k56flex_receiver` at all -- it rebuilds the
 * TRANSMITTER.  Four things happen in this order:
 *
 *   1  bit 3 of the receiver's `flags` is set, the same word +0x122 that
 *      v34recv.h describes as the detector-pending and AGC-freeze bit;
 *   2  +0x382 takes the same two constellation constants
 *      `V34XF_IndicateJdReceived` and `V34XF_IndicateDilReceived` use, on
 *      the same test, except that the test here is against 0x10 rather than
 *      against zero -- so the argument is a constellation SIZE and not the
 *      flag those two take;
 *   3  `v34setuptxmit`, which is the transmitter rebuild;
 *   4  the transmit state is forced to 0x12 (and only if it is not already
 *      there -- `cmpw` then a conditional store, which is the object's and
 *      is kept because a store to a state word is not free);
 *   5  three transmit scalars are cleared and adaptecho's DC estimator is
 *      re-seeded.
 *
 * THE DC SEED IS THE SAME EXPRESSION `VPcmV34InitiateRetrain` WRITES, at the
 * same offset: `336 * short_35a4 + 10000`, `short_35a4` read signed, into +0x254 with
 * +0x256 and +0x258 cleared behind it.  v34fsk.h's note on `short_35a4` said that
 * two functions computed it and that "nothing establishes that their
 * destinations are the same field".  This is the second one, it is
 * reconstructed, and the destination is the same -- so that reservation is
 * discharged, and the arithmetic belongs to the DC estimator rather than to
 * whatever else those two functions do.
 *
 * The diagnostic is gated and prints the three transmitter parameters; it is
 * what names `preemp` in v34fsk.h.
 */
void
V34XF_IndicateK56FlexJdReceived(void *objp, unsigned char constel)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);
	const struct v34_ratecfg *cfg =
	    (const struct v34_ratecfg *)(m + V34_RATECFG);

	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_LATE_TRN);

	obj->short_382 = (constel == 0x10) ? (short)0x89b0 : (short)0x8990;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: About to setup v34 txmit, "
			"baudrate = %d, carrier = %d, preemp = %d\r\n",
			cfg->baud, cfg->carrier, cfg->preemp);

	v34setuptxmit(obj);

	if (obj->txstate != 0x12)
		obj->txstate = 0x12;

	obj->seg_symcount = 0;
	obj->prev_quadrant = 0;
	obj->tx_scr_sr = 0;

	*(short *)(m + 0x254) =
		(short)(336 * (int)*(const short *)(m + 0x35a4) + 10000);
	*(short *)(m + 0x256) = 0;
	*(int *)(m + 0x258) = 0;
}

/*
 * K56Flex has settled on a rate.
 *
 * The one indication that touches `k56flex_receiver` rather than
 * `v90_receiver`, and it sets a constant.  Its sibling
 * `V34XF_IndicateK56FlexJdReceived` is not here: it calls `v34setuptxmit`,
 * which is blocked behind `settxlevel`.
 */
void
V34XF_IndicateK56FlexRateDetermined(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"K56Flex rate determined, starting MP " "transmission...\r\n");

	obj->k56flex_receiver = 5;
}

void
VPcmV34LogTimingOffset(void *objp, short offset)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->v90_timing_offset = offset;
}

/*
 * The two renegotiation counters, each an increment and nothing else.
 *
 * `datapumpv34` is the only caller of either in the object.  It calls the
 * local one on both of the renegotiations it starts itself -- the step down
 * at 0x71d12 and the step up at 0x71ba7 -- and the remote one at 0x71c95, on
 * the branch the receiver's +0x122 bit 5 selects.  Which end ASKED is
 * therefore what the two names distinguish; both are counted on this modem.
 *
 * The increment is 16-bit and wraps -- `movzwl`, `inc %eax`, `mov %ax` -- the
 * same arithmetic `VPcmV34InitiateRateRenegotiation` does inline above, on
 * the same field, because that entry point counts the same event where the
 * request arrives from the shell rather than from the datapump.
 *
 * Neither reads the object for anything else and neither is gated on the
 * diagnostics: the two `V34RNEG` messages belong to the caller.
 */
void
VPcmV34IndicateLocalRRN(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->rrn_local = (short)(obj->rrn_local + 1);
}

void
VPcmV34IndicateRemoteRRN(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->rrn_remote = (short)(obj->rrn_remote + 1);
}

/*
 * The remote end has asked for a retrain.
 *
 * Twelve bytes: load the object, store 1 at +0xac17, return.  Nothing
 * reconstructed reads the byte, so the function's own name is the whole of
 * what names the field -- see v34fsk.h's note on `remote_retrain_ind`.  It is
 * never cleared here, which is why it reads as an indication rather than as a
 * request.
 */
void
VPcmV34SetIndicationOfRemoteRetrain(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->remote_retrain_ind = 1;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Same argument as dpsk.c's block: these offsets sit in
 * regions that are otherwise padding, so a field that drifted would compile
 * silently.  Guarded to a 32-bit ABI because `struct v34_object` holds
 * pointers and its member offsets are only the object's on that target.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34PCMIF_ASSERT(name, field, off) \
	typedef char v34pcmif_off_##name[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34PCMIF_ASSERT(txscale, tx_scale,           0x25d4);
V34PCMIF_ASSERT(v90rx,   v90_receiver,     0x024c);
V34PCMIF_ASSERT(k56rx,   k56flex_receiver, 0x0250);
V34PCMIF_ASSERT(short_382,    short_382,             0x0382);
V34PCMIF_ASSERT(seg_symcount,   seg_symcount,            0x25c0);
V34PCMIF_ASSERT(probe,   probe_results,    0xa258);
V34PCMIF_ASSERT(info0,   info0_bits,       0xa8a4);
V34PCMIF_ASSERT(rtd,     rtd,              0xaa7e);
V34PCMIF_ASSERT(timeoff, v90_timing_offset,            0xac0c);
V34PCMIF_ASSERT(progress,   progress,            0x0004);
V34PCMIF_ASSERT(rmin,    rate_min,         0x0220);
V34PCMIF_ASSERT(rmax,    rate_max,         0x0224);
V34PCMIF_ASSERT(rnow,    rate_now,         0x0228);
V34PCMIF_ASSERT(rwant,   rate_want,        0x022c);
V34PCMIF_ASSERT(rrnloc,  rrn_local,        0xac0e);
V34PCMIF_ASSERT(rrnrem,  rrn_remote,       0xac10);
V34PCMIF_ASSERT(p3548,   p3548,            0x3548);
V34PCMIF_ASSERT(pac3c,   pac3c,            0xac3c);
V34PCMIF_ASSERT(pbins,   probe_bins,       0xa320);

/*
 * And the three receiver scalars the two Initiate entry points clear, which
 * they reach as `obj + 0x264 + 0x258`.  Asserted as a sum so that a change to
 * either struct breaks here rather than moving the clear into the queue.
 */
#define V34PCMIF_RXASSERT(name, field, off) \
	typedef char v34pcmif_rxoff_##name[ \
		((int)(__builtin_offsetof(struct v34_object, rxq) \
		       + __builtin_offsetof(struct v34_receiver, field)) \
		 == (off)) ? 1 : -1]

V34PCMIF_RXASSERT(bad_run, bad_run, 0x4bc);
V34PCMIF_RXASSERT(bad_long_run, bad_long_run, 0x4be);
V34PCMIF_RXASSERT(good_run, good_run, 0x4c0);

/*
 * The doubles are the first floating-point member the struct has ever had,
 * and on i386 GCC aligns `double` to 4 inside a struct while a 64-bit target
 * aligns it to 8.  The offset assertions above would catch a shift; this
 * catches the array being the wrong length, which they would not.
 */
typedef char v34pcmif_probe_len[
	(sizeof(((struct v34_object *)0)->probe_results)
	 == V34_PROBE_RESULTS * sizeof(double)) ? 1 : -1];

#endif
