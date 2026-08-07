/*
 * v34hshak.h -- ITU-T V.34: the handshake's state names.
 *
 * `v34handshak` is 61,541 bytes holding one state machine, and this is its
 * eighty-seven states in the AUTHOR'S names, in index order.
 *
 * They come from `StateName`, a table of string pointers at .data+0x6c00,
 * and it is LIVE: `v34handshakinit` and `v34handshak` index it 533 times
 * between them, to print state transitions.  An earlier note here said
 * nothing indexed it, which was a relocation search missing an addend
 * against a section symbol -- see findings 144 and 176.
 *
 * The transitions name THREE concurrent machines, not one: a receive state,
 * a transmit state and a "microstate", each trace printing its own change
 * and the other two's current value.
 *
 * AND SO DOES THE DISPATCH.  The two jump tables at .rodata+0x2da0 and
 * +0x2ee8 are fed by the transmit state, the one at +0x3000 by the
 * microstate, and the receive state has no table at all -- it is dispatched
 * by compare-and-branch chains.  So a state NUMBER means different things to
 * different machines: 51 is `TX_L1` to both the transmit machine and the
 * microstate machine and they run different code for it.  Finding 213, which
 * retracts the part of finding 77 that read the three tables as one machine.
 *
 * `v34handshak` itself is not reconstructed; the split above is what its
 * task list is organised around, because "the signal generator" and "the
 * protocol engine" are driven by different things and tested by different
 * means.  The point of writing the names down first is that planning against
 * "state 47" and against "TX_PHASE3_ANS" are very different exercises, and
 * cfgsplit's per-state byte counts become readable the moment the states
 * have names.
 *
 * The gaps are the author's: NOSTATE0, NOSTATE2, NOSTATE3 and NOSTATE36 are
 * placeholders, so the enum has four holes and eighty-three real states.
 */

#ifndef DSPLIB_V34HSHAK_H
#define DSPLIB_V34HSHAK_H

#ifdef __cplusplus
extern "C" {
#endif

#define V34HS_NOSTATE0                0
#define V34HS_TXRENEG                 1
#define V34HS_NOSTATE2                2
#define V34HS_NOSTATE3                3
#define V34HS_RECEIVE                 4
#define V34HS_SILENCE                 5
#define V34HS_ANSAM                   6
#define V34HS_TONE_2100               7
#define V34HS_TONE2225                8
#define V34HS_AA_TX                   9
#define V34HS_CC_TX                  10
#define V34HS_AC_TX                  11
#define V34HS_CA_TX                  12
#define V34HS_SXMIT                  13
#define V34HS_XMIT1                  14
#define V34HS_XMIT2                  15
#define V34HS_XMIT3                  16
#define V34HS_XMITV22                17
#define V34HS_SSEG                   18
#define V34HS_SBARSEG                19
#define V34HS_PPSEG                  20
#define V34HS_TRNSEG4                21
#define V34HS_TRNSEG16               22
#define V34HS_TX_JM_CM               23
#define V34HS_TX_DPSK                24
#define V34HS_DET_2100               25
#define V34HS_DET_2250               26
#define V34HS_DET_2400               27
#define V34HS_DET_1200               28
#define V34HS_DET_AC                 29
#define V34HS_DET_AC_RTN             30
#define V34HS_DET_AC_END             31
#define V34HS_DET_AA                 32
#define V34HS_PHASE1                 33
#define V34HS_PHASE2                 34
#define V34HS_WAIT                   35
#define V34HS_NOSTATE36              36
#define V34HS_RECEIVE1               37
#define V34HS_RECEIVE2               38
#define V34HS_RECEIVEV22             39
#define V34HS_DET_CM_JM              40
#define V34HS_DET_SYNC               41
#define V34HS_DET_CJ                 42
#define V34HS_RX_DPSK                43
#define V34HS_DET_INFO               44
#define V34HS_TONE_AB_ANS            45
#define V34HS_TX_PHASE1_ANS          46
#define V34HS_TX_PHASE2_ANS          47
#define V34HS_TX_PHASE3_ANS          48
#define V34HS_RX_PHASE1_ANS          49
#define V34HS_RX_PHASE2_ANS          50
#define V34HS_TX_L1                  51
#define V34HS_TX_L2                  52
#define V34HS_DET_AB                 53
#define V34HS_SILENCEINFO            54
#define V34HS_TX_PHASE1_CALL         55
#define V34HS_TX_PHASE2_CALL         56
#define V34HS_TX_PHASE3_CALL         57
#define V34HS_RX_PHASE1_CALL         58
#define V34HS_RX_PHASE2_CALL         59
#define V34HS_TONE_AB                60
#define V34HS_TONE_AB_CALL           61
#define V34HS_RX_PHASE3_CALL         62
#define V34HS_INFODONE               63
#define V34HS_JTXMIT                 64
#define V34HS_XMIT0                  65
#define V34HS_TRNSEG4A               66
#define V34HS_XMITMP                 67
#define V34HS_J1TXMIT                68
#define V34HS_EXMIT                  69
#define V34HS_DATAXMIT               70
#define V34HS_TXLEVEL                71
#define V34HS_RX_L1                  72
#define V34HS_RX_L2                  73
#define V34HS_SILENCERETRAIN         74
#define V34HS_RX_RETRAIN_CALL        75
#define V34HS_RX_RETRAIN_ANSWER      76
#define V34HS_TX_RETRAIN_ANS         77
#define V34HS_JaTXMIT                78
#define V34HS_MOH_TONE               79
#define V34HS_MOH_TONE_DROP          80
#define V34HS_MOH_SILENCE            81
#define V34HS_MOH_ON_HOLD            82
#define V34HS_MOH_FRR                83
#define V34HS_MOH_CLEARDOWN          84
#define V34HS_K56JaTXMIT             85
#define V34HS_TXMD                   86

#define V34HS_STATE_COUNT		87

/*
 * THE STATE NAMES ARE NOT AN ENUM OVER ONE MACHINE.  `v34handshakinit`'s
 * thirteen diagnostics name THREE concurrent machines, and the three words
 * they live in are settled -- read off each format string against its
 * arguments, not guessed:
 *
 *      obj + 0x3592    microstate
 *      obj + 0x3594    rxstate
 *      obj + 0x3596    txstate
 *
 * See src/pump/v34/v34hshak.c for the derivation.  It matters for #39-#45:
 * eighty-seven states over three machines is not seven slices of one, and
 * `tools/cfgsplit.py` should be pointed at `v34handshak` before that split is
 * planned (docs/fastpass.md).
 */

#define V34HS_MICROSTATE_OFF	0x3592
#define V34HS_RXSTATE_OFF	0x3594
#define V34HS_TXSTATE_OFF	0x3596

/*
 * A halfword of the object by offset, and one state transition with the
 * diagnostic the object prints for it.
 *
 * These are `v34hshak.c`'s and they are shared rather than copied because
 * `v34handshak` is in the same translation unit and is being reconstructed
 * one dispatch arm at a time in files beside it.  Its arms change the same
 * three words through the same three format strings, and the two context
 * arguments each string takes are in an order that differs per string -- so
 * a second copy of `hs_setstate` is a second place to get that order wrong
 * while every byte of the object still matches.  See the comment on the
 * definition.
 *
 * `hs_setstate` prints only when the value CHANGES and only when
 * `DSPLIB_DEBUG_ON()`, and it prints before it stores.  Both are the
 * object's; an arm that needs the store without the compare uses `hs_put`.
 */
struct v34_object;
short hs_get(const struct v34_object *obj, unsigned off);
void hs_put(struct v34_object *obj, unsigned off, short v);
void hs_setstate(struct v34_object *obj, unsigned off, short next);

/*
 * Which of `v34handshak`'s unwritten paths ran.
 *
 * The blob's function is 61,541 bytes over forty microstates, twenty
 * txstates and an rxstate chain, and it is being taken a few arms at a time.
 * Table 3's forty arms are all in; most of table 1 and four of table 2 are
 * not, and a path with no reconstruction has to do something DEFINITE,
 * because "nothing" is the one answer a differential test cannot tell from a
 * wrong answer.
 *
 * So every such path records a code and then aborts -- unless the test has
 * called `v34handshak_unwritten_reset`, which says "I am going to read the
 * code afterwards" and turns the abort into a return.  `t_v34hst3mid.c` is
 * that test and checks the code after every step.  The codes are numbers
 * rather than strings because the strings firewall rejects any literal in
 * src/ that is not the object's own (findings 180 and 201);
 * `t_v34hst3mid.c` names them.  See src/pump/v34/v34hshak.c, and finding 547
 * for why one mechanism had to serve both of the reconstructions that were
 * merged into it.
 */
#define T3M_WRITTEN			0
#define T3M_UNWRITTEN_TBL1		1
#define T3M_UNWRITTEN_RXIDLE		2
#define T3M_UNWRITTEN_RXSTATE		3
#define T3M_UNWRITTEN_FSKGATE		4
#define T3M_UNWRITTEN_TBL3_DEFAULT	5
#define T3M_UNWRITTEN_TBL3_ARM		6
#define T3M_UNWRITTEN_TBL2_ARM		7
#define T3M_UNWRITTEN_OTHER		8

int v34handshak_unwritten(void);
void v34handshak_unwritten_reset(void);

/*
 * Bring the handshake up.  `mode` selects one of five entries, and the names
 * below are the CALL SITES' -- every caller passes a literal, so the modes
 * are sourced rather than inferred:
 *
 *      0   VPcmV34Create                        cold start
 *      1   VPcmV34InitiateRetrain, v34handshak  retrain
 *      2   VPcmV34InitiateRateRenegotiation,    rate renegotiation
 *          VPcmV34InitiateHangUp                and hang-up
 *      3   -- no caller anywhere in the object; shares 2's jump-table body
 *      4   VPcmV34InitMOH                       Modem-on-Hold
 *
 * Anything outside 0..4 is NOT an error: the range check jumps to the common
 * tail, which every mode also falls into.  The tail clears +0xaa3c and puts
 * 0x10 in +0x2aa0.
 */
void v34handshakinit(void *obj, int mode);

/*
 * The handshake itself, one block at a time.
 *
 * PARTIAL, AND IT HALTS RATHER THAN GUESSING.  61,541 bytes over three
 * dispatches are being landed one arm at a time against
 * `test/harness/v34hsstep.c`; an arm nobody has written yet calls `abort`,
 * so this is safe to call only for a state some test has landed.  Nothing in
 * this tree calls it but that fixture.  docs/v34handshak.md has the map and
 * says which arms exist.
 */
void v34handshak(void *obj);

/*
 * `v34handshak`'s once-per-block transmit dispatch on its own -- the
 * seventy-entry table at .rodata+0x2ee8 selected at 0x62af1, its seven
 * targets, and the shared tail at 0x62a40 that every one of them falls into,
 * down to the `ret`.
 *
 * FOR ONE TEST, AND SAID SO RATHER THAN IMPLIED.  `t_v34hstbl2.c` drives this
 * against the whole blob function on an object the fixture has steered into
 * this dispatch and no other; over that domain the two are the same function
 * (finding 361).  It is not a second reconstruction -- it calls the same
 * static dispatch every microstate arm calls.  Nothing in `src/` calls it.
 * The txstate halfword at +0x3596 selects the arm, exactly as the object's
 * own three routes in leave it.  Finding 591.
 */
struct v34_object;
void v34handshak_txblock(struct v34_object *obj);

/*
 * The datapump's per-block entry point, and the last function of V34hshak.c.
 *
 * The int at +0x2218 decides: above 1 it drives `v34handshak` until the
 * transmit block is full and the receive queue drained, and does nothing
 * else; at 0 or 1 it runs `modulatevector` and `receiver` instead, and then
 * supervises the line -- retraining or renegotiating on the receiver's three
 * consecutive-error counters at +0x258, +0x25a and +0x25c.
 *
 * IT INHERITS `v34handshak`'s PARTIALNESS on the first of those two paths:
 * an arm nobody has written calls `abort`.  The second path does not reach
 * `v34handshak` at all.
 */
void datapumpv34(void *obj);

/*
 * ---------------------------------------------------------------------------
 * The handshake's support functions -- everything in V34hshak.c that is not
 * `v34handshak` itself.  See src/pump/v34/v34hshak.c.
 */

#define V34_SCALE_ENTRIES	28	/* two rows of fourteen */
#define V34_CARRIER_DESC	8	/* four zeroes and two coefficient pairs */
#define V34_BPV22_TAPS		60

/*
 * The transmit power scales, indexed by pre-emphasis index; the receive
 * carrier descriptors; and the phase-2 DPSK band-pass pair.  All fifteen are
 * global in the object and referred to only from this translation unit.
 *
 * `bpv22high` and `bpv22low` are NOT const: they live in .data rather than
 * .rodata, which is the original's own statement about their storage class.
 */
extern const short scale2400[V34_SCALE_ENTRIES];
extern const short scale2800[V34_SCALE_ENTRIES];
extern const short scale3000[V34_SCALE_ENTRIES];
extern const short scale3200[V34_SCALE_ENTRIES];
extern const short scale3429[V34_SCALE_ENTRIES];

extern const short c1600[V34_CARRIER_DESC], c1680[V34_CARRIER_DESC];
extern const short c1800_[V34_CARRIER_DESC], c1829[V34_CARRIER_DESC];
extern const short c1867[V34_CARRIER_DESC], c1920[V34_CARRIER_DESC];
extern const short c1959[V34_CARRIER_DESC], c2000[V34_CARRIER_DESC];
extern const short c1200_[V34_CARRIER_DESC], c2400_[V34_CARRIER_DESC];

extern short bpv22high[V34_BPV22_TAPS];
extern short bpv22low[V34_BPV22_TAPS];

/*
 * Re-arm the FSK demodulator to look for INFO1.  Clears its working state
 * and reloads the V.21-rate slicer configuration; touches nothing else.
 */
void dpskDetectInfo1Init(void *obj);

/*
 * Bring the phase-2 DPSK link up.  `mode` zero selects a 1200 Hz transmit
 * carrier and anything else 2400; `high` non-zero selects the upper receive
 * band.  The two are independent -- the directions occupy different bands.
 */
void dpskinit(void *obj, short mode, short high);

/*
 * Turn the negotiated MP bit-fields at +0xa9de..+0xa9e3 into transmit and
 * receive symbol rates, carriers, power scales and a pre-emphasis index.
 * Rate codes 1, 6 and 7 set nothing at all.
 */
/*
 * Set the whole object up for phase 2.  Two configurations in one function,
 * chosen by `f359c == 0x65`: the originate end signals on 1200 Hz through
 * the upper receive band, the answer end on 2400 through the lower.
 */
void v34modeminit(void *obj);

void setfinalrate(void *obj);

/*
 * Configure the demodulator for the rate `setfinalrate` chose, and arm the
 * tone detector on the carrier descriptor it selected.
 */
void setupreceiver(void *obj);

/*
 * How many multiplications by a per-rate ratio it takes for a per-rate
 * measurement to pass a limit: the pre-emphasis index, 6..10.
 *
 * The object supplies no default arm, so a baud rate other than 2400, 2800,
 * 3000, 3200 or 3429 runs the loop on uninitialised registers there and on
 * zero here.  See D37.  What the first argument points at is not known --
 * nothing in the object calls this -- so it stays a `void *`.
 */
short preempindex(void *obj, short baudrate);

/*
 * ---------------------------------------------------------------------------
 * Three DFT banks the handshake arms, and the detector it polls.
 *
 * All four work on `struct v34_dftbin` banks -- see v34det.h.  The three
 * initialisers take the bank as an argument rather than finding it in the
 * object, so where their caller keeps it is not recoverable: NOTHING IN THE
 * OBJECT CALLS ANY OF THEM.  No relocation names them and no direct call
 * reaches them, which is the shape `cosread` has and the shape four of the
 * seven functions already in v34hshak.c have.  They are global, so they are
 * testable regardless; it only means the bin counts and the argument have to
 * be read out of the code rather than off a call site.
 *
 * The bin numbers below are in units of the phase step, and one unit is
 * 150 Hz at V.34's 9600 Hz rate (docs/rate_assumptions.md R-1): 0x4000 of
 * phase is a full turn and the step is `bin << 8`, so a bin repeats every
 * 16384 / (bin * 256) = 64 / bin samples.
 */

struct v34_dftbin;

/*
 * Twenty-five bins, 150 Hz to 3750 Hz, which is the V.34 line probe's tone
 * spacing.  Clears both accumulator pairs AND the double result; leaves
 * `energy`, `shift` and the two thresholds alone.
 */
void dftfreqinit(struct v34_dftbin *bins);

/*
 * Four bins at 1050, 1350, 1950 and 2550 Hz, and four at 900, 1200, 1800 and
 * 2400 Hz.  Each noise bin sits one 150 Hz step below its signal partner,
 * which is what makes the pair a measurement rather than two frequencies.
 *
 * `nl` is the author's; V.34 phase 2 measures nonlinear distortion from the
 * probe, and that is the obvious reading.  Which bank is the reference and
 * which the product is NOT settled by anything in the object -- neither
 * initialiser has a caller -- so nothing here reads across.
 *
 * Both clear only the integer accumulators, not the double pair
 * `dftfreqinit` also clears.  That difference is the object's.
 */
void dftnlinitSignalBins(struct v34_dftbin *bins);
void dftnlinitNoiseBins(struct v34_dftbin *bins);

/*
 * Arm the retrain-request detector: three bins at 900, 1200 and 1500 Hz in
 * the object's own `retrain_bins`, both thresholds on each, and the five
 * scalars at +0xa24a.  See `struct v34_object`.
 */
void dftRetrainDetInit(void *obj);

/*
 * Feed `nsamples` samples to that detector and answer whether the far end is
 * asking for a retrain.
 *
 * Returns 1 only on the measurement that completes the second run; every
 * other call returns 0, including the ones that merely accumulate.  `nbins`
 * is the caller's and not the three the initialiser wrote, so a prefix of
 * the bank can be polled, and the clear after each measurement covers
 * exactly the bins the caller named.
 */
int detectRetrainReq(void *obj, short nbins, const short *samples,
		     short nsamples);

/*
 * ---------------------------------------------------------------------------
 * The handshake's two symbol emitters, and the constellations they map onto.
 *
 * Both scramble the bits handed to them, encode the result differentially
 * against the quadrant the object is carrying, and TAIL-CALL `txmit` -- so
 * calling either of these transmits a symbol rather than merely computing
 * one.  Everything they touch is in `struct v34_object`.
 *
 * Each table entry is a packed complex int, real part in the low half.
 */
extern const int vect4[4];
extern const int vect16[16];

/*
 * The line probe's one period, 64 signed shorts at `.rodata + 0x2c00`, and
 * NOT a constellation: `v34handshak`'s txstate 51 `TX_L1` indexes it with the
 * low six bits of `vect_idx`, scales each sample by `f25d4` and hands four at
 * a time to `txwritequeue`.  Sixty-four is what the mask `0x3f` admits; the
 * object checks no length anywhere.
 */
#define V34_PROBE_SAMPLES	64
extern const short probe[V34_PROBE_SAMPLES];

/* Two bits -> one quadrant, differentially against the last. */
void txmitdibit(void *obj, short bits);

/*
 * Four bits -> one of sixteen points.  The low dibit picks the quadrant,
 * differentially; the high one selects within it and is not.
 */
void txmitquadbit(void *obj, short bits);

/*
 * One symbol of the K56flex phase 3/4 transmit sequence.  Declared here
 * rather than beside the other K56flex material because it drives the two
 * emitters above and their two tables; it is defined in a .cpp -- see
 * `src/pump/v34/v34k56.cpp` for why, and for the three ways its idle symbol
 * is NOT one of those emitters.
 *
 * ALWAYS RETURNS 0, at all three `ret`s.  That is as far as the return type is
 * recoverable: its one caller (inside `v34handshak`) discards it, so nothing
 * distinguishes `int` from `short` or `unsigned` -- `xor %eax,%eax` before
 * every `ret` is the whole of the evidence, exactly as for the
 * `K56FlexFloModem` members it calls.
 */
int k56FlexPhase34(void *obj);

/*
 * One symbol -- or, in four of its arms, two -- of the V.90 phase 3/4
 * transmit sequence.  The twin of `k56FlexPhase34` above, and declared beside
 * it for the same reason: it drives the two emitters and their two tables.
 * Defined in `src/pump/v34/v34pcmmain.cpp`, which is where the object puts
 * it; the head of that block gives the evidence, and sets out the ways this
 * differs from the K56flex twin.
 *
 * ALWAYS RETURNS 0, at both `ret`s, and on the same evidence as above.
 */
int v90Phase34(void *obj);

/*
 * Send whichever PCM receiver is past phase 2 into phase 3, at the point in
 * the handshake where the JA is about to go out.  Two tail calls and nothing
 * else: `VPcmFloModem::enterPhase3` when `v90_receiver > 1`, otherwise
 * `K56FlexFloModem::enterPhase3FullDuplex` when `k56flex_receiver > 1`, and
 * otherwise nothing at all.  Both tests are signed and both are `> 1`.
 *
 * Declared here rather than in `v34pcmif.h` for the same reason as the two
 * above: its only two callers are inside `v34handshak` and it is not one of
 * VPcmV34Main.cpp's `V34XF_`/`VPcmV34` interface exports.  Defined in
 * `src/pump/v34/v34pcmmain.cpp`, because both callees are C++ members.
 *
 * RETURNS NOTHING.  %eax is never set on the falling-through path and both
 * callees are themselves `void`, which is as far as this is recoverable.
 */
void indicateJaTransmission(void *obj);

/*
 * Turn the line probe's twenty-five bins into a power-reduction request, a
 * set of offered symbol rates or one chosen one, and a pre-emphasis index
 * per rate.  Writes the rate config at +0xaa84 and the outgoing message at
 * +0xa9ac, and nothing else; takes no arguments beyond the object.
 *
 * `f359c == 0x65` -- the originating side -- offers every rate the probe
 * allows.  Any other value picks one and fills the rate config in.
 */
void probeselect(void *obj);

/*
 * ---------------------------------------------------------------------------
 * Bringing the data-mode transmitter up.
 */

/*
 * Apply the far end's requested power reduction to the transmit scale.
 *
 * `mp` is the received MP message; only its first short is read, and the one
 * call site passes `obj + 0xa9dc`.  Writes `f25dc` with the reduction in dB
 * and `f25d4` with the scale that comes out of it.
 */
void settxlevel(void *obj, const short *mp);

/*
 * Configure the transmitter for the negotiated rate: power scale, modulator,
 * two state words to WAIT and SSEG, two transmit flags, then `txinit`.
 * Takes nothing but the object -- everything else comes out of the rate
 * config `setfinalrate` filled at +0xaa84.
 */
void v34setuptxmit(void *obj);

/*
 * ---------------------------------------------------------------------------
 * The two routines the object keeps FILE-LOCAL, and the struct one of them
 * walks.  Both are reachable only from `v34handshak` in the object; our
 * copies have external linkage and the ordinary calling convention, and the
 * blob's are reached as `ref_getbit` and `ref_ApplyBulkDelay` -- which needs
 * `objcopy --globalize-symbols` first (finding 221).  Both take their
 * arguments in registers there, so a declaration of the *reference* has to
 * say `regparm`; see the tests.
 */

#define V34_BITSOURCE_WORDS	10

/*
 * A message being clocked out one bit at a time, MSB first, with the CRC-16
 * the V.34 sequences carry appended once the message itself runs out.
 *
 * THE WORD ARRAY IS INLINE AT OFFSET ZERO and `getbit` indexes it with `idx`
 * -- `movzwl (%esi,%ebp,2),%edx`.  ITS LENGTH IS NOT IN THE CODE: ten is
 * what fits between offset 0 and `crc` at +0x14, and nothing in `getbit`
 * bounds `idx` against it, so ten is adjacency rather than a bound.
 *
 * WHERE THE OBJECT KEEPS ONE.  `v34handshak` passes whatever `obj + 0xaa6c`
 * holds, thirty-six times, and `getMPrecvdBits` is the writer that says what
 * that is: it stores `obj + 0xaa3c` there.  The struct is declared standalone
 * rather than embedded in `struct v34_object` at +0xaa3c because that region
 * is already named from the other direction -- `info_caps` and `caps_flags`,
 * the nibbles the handshake reads straight out of the same two words -- and
 * neither reading is more correct than the other.
 *
 * The field names are the code's: `crc` is initialised to 0xffff and folded
 * with 0x1021 MSB-first, `crc_on` gates both the folding and the 16-bit
 * flush, `nbits` and `pos` bound the message, `wordbits` is how many bits
 * come out of one word, `repeat` enables the restart and `repeats` counts
 * them, and `avail0`/`acc0` are what the restart reloads `avail`/`acc` from.
 */
struct v34_bitsource {
	short	word[V34_BITSOURCE_WORDS];	/* +0x00 */
	short	crc;				/* +0x14 */
	short	crc_on;				/* +0x16 */
	short	nbits;				/* +0x18 */
	short	pos;				/* +0x1a */
	short	wordbits;			/* +0x1c */
	short	idx;				/* +0x1e */
	short	repeat;				/* +0x20 */
	short	repeats;			/* +0x22 */
	int	acc;				/* +0x24 */
	short	avail;				/* +0x28 */
	short	avail0;				/* +0x2a */
	int	acc0;				/* +0x2c */
};

/*
 * The next bit of `b`, as 0 or 1, or -1 when the message is exhausted and
 * `repeat` is clear.  RECURSIVE: the restart arm reloads the whole reader
 * and calls itself for the first bit of the repeat.
 *
 * `short`, not `int`, and all four call sites say so: every one of them --
 * the recursion at 0x5ec47 and `v34handshak`'s three -- follows the `call`
 * with `cwtl`, which is the sign extension of a 16-bit return.
 */
short getbit(struct v34_bitsource *b);

/*
 * Set the bulk-delay ring to `delay` samples and clear it, then decide
 * whether the FAR echo canceller can run at that delay.
 *
 * `delay` at or below zero becomes 144, and `delay` at or past `bulk_len`
 * -- compared UNSIGNED -- becomes zero.  Both announce themselves with the
 * SAME diagnostic, which is why the transcript has to be compared rather
 * than counted.  With
 * neither PCM receiver running and the far canceller already armed, a delay
 * of 29 or less disarms it and pulls the DMA delay back by `delay + 15`,
 * capped at 30.
 */
void ApplyBulkDelay(void *obj, short delay);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSHAK_H */
