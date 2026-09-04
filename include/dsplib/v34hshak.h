/*
 * v34hshak.h -- ITU-T V.34: the handshake's state names.
 *
 * `v34handshak` is 61,541 bytes holding one state machine, and this is its
 * eighty-seven states in the author's own names, in index order.
 *
 * They come from `StateName`, a table of string pointers the object indexes
 * 533 times between `v34handshakinit` and `v34handshak` to print state
 * transitions (findings F144 and F176).
 *
 * The transitions name three concurrent machines, not one: a receive state,
 * a transmit state and a "microstate", each trace printing its own change
 * and the other two's current value. The dispatch follows the same split:
 * two jump tables are fed by the transmit state, a third by the microstate,
 * and the receive state is dispatched by compare-and-branch chains instead
 * of a table. So a state number means different things to different
 * machines -- 51 is `TX_L1` to both the transmit machine and the microstate
 * machine, and they run different code for it (finding F213, which retracts
 * the part of finding F77 that read the three tables as one machine).
 *
 * `v34handshak` itself is reconstructed incrementally, a dispatch arm at a
 * time, organised around this three-machine split -- see docs/v34handshak.md
 * for the map of what exists so far.
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
 * The state names are not an enum over one machine. `v34handshakinit`'s
 * diagnostics name three concurrent machines, in three separate words of
 * the object:
 *
 *      obj + 0x3592    microstate
 *      obj + 0x3594    rxstate
 *      obj + 0x3596    txstate
 *
 * See src/pump/v34/v34hshak.c for the derivation. It matters for planning:
 * eighty-seven states over three machines is not seven slices of one, and
 * `tools/cfgsplit.py` should be pointed at `v34handshak` with that in mind
 * (docs/fastpass.md).
 */

#define V34HS_MICROSTATE_OFF	0x3592
#define V34HS_RXSTATE_OFF	0x3594
#define V34HS_TXSTATE_OFF	0x3596

struct v34_object;

/**
 * @brief Read one of the handshake's three state words.
 * @param obj  The V.34 modem object.
 * @param off  One of #V34HS_MICROSTATE_OFF, #V34HS_RXSTATE_OFF, #V34HS_TXSTATE_OFF.
 * @return The word's current value.
 */
short hs_get(const struct v34_object *obj, unsigned off);

/**
 * @brief Store one of the handshake's three state words, unconditionally
 * and without a diagnostic.
 * @param obj  The V.34 modem object.
 * @param off  One of #V34HS_MICROSTATE_OFF, #V34HS_RXSTATE_OFF, #V34HS_TXSTATE_OFF.
 * @param v    The new value.
 */
void hs_put(struct v34_object *obj, unsigned off, short v);

/**
 * @brief Move one of the handshake's three state machines to a new state.
 *
 * Prints the object's state-transition diagnostic when debugging is on and
 * the value is actually changing, then stores the new value -- in that
 * order, matching the object. hs_put() is the version without the compare
 * and the diagnostic, for arms that need the store alone.
 *
 * This and hs_get()/hs_put() are shared (rather than duplicated per arm)
 * because `v34handshak` itself is being reconstructed one dispatch arm at a
 * time across several files, and all of them read and write these same
 * three words through the same diagnostic strings.
 *
 * @param obj   The V.34 modem object.
 * @param off   One of #V34HS_MICROSTATE_OFF, #V34HS_RXSTATE_OFF, #V34HS_TXSTATE_OFF.
 * @param next  The new state.
 */
void hs_setstate(struct v34_object *obj, unsigned off, short next);

/**
 * Which of `v34handshak`'s still-unwritten paths ran on the last call.
 *
 * `v34handshak` is being reconstructed a few dispatch arms at a time; a
 * path with no reconstruction yet must do something definite, since
 * "nothing" is the one answer a differential test cannot tell apart from a
 * wrong answer. So every such path records one of these codes and aborts,
 * unless the test has called v34handshak_unwritten_reset(), which turns the
 * abort into an ordinary return so the caller can inspect the code
 * afterwards. `test/unit/t_v34hst3mid.c` is that caller, and checks the
 * code after every step. See src/pump/v34/v34hshak.c and finding F547.
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

/**
 * @brief Report which unwritten path (if any) the last call to
 * `v34handshak` took.
 * @return One of the `T3M_*` codes above.
 */
int v34handshak_unwritten(void);

/**
 * @brief Arm v34handshak_unwritten() to return instead of aborting when it
 * hits a path this tree has not reconstructed yet.
 *
 * Intended for tests that want to read the code afterwards rather than
 * treat an unwritten path as a hard failure.
 */
void v34handshak_unwritten_reset(void);

/**
 * @brief Bring the handshake up.
 *
 * @param obj   The V.34 modem object.
 * @param mode  Selects one of five entry points; every call site in the
 *              object passes a literal, so the modes are named from their
 *              callers rather than inferred:
 *              - 0: `VPcmV34Create` (cold start)
 *              - 1: `VPcmV34InitiateRetrain`, `v34handshak` (retrain)
 *              - 2: `VPcmV34InitiateRateRenegotiation`,
 *                `VPcmV34InitiateHangUp` (rate renegotiation and hang-up)
 *              - 3: no caller anywhere in the object; shares mode 2's body
 *              - 4: `VPcmV34InitMOH` (Modem-on-Hold)
 *
 *              Any value outside 0..4 is not an error: it takes the same
 *              common tail every mode falls into, which clears +0xaa3c and
 *              sets +0x2aa0 to 0x10.
 */
void v34handshakinit(void *obj, int mode);

/**
 * @brief The V.34 handshake state machine, one block at a time.
 *
 * This function is 61,541 bytes over three concurrent dispatches in the
 * object, and this tree lands it one arm at a time against
 * `test/harness/v34hsstep.c`; an arm nobody has written yet calls `abort`,
 * so it is safe to call only for a state some test has landed. Nothing in
 * this tree calls it but that fixture. docs/v34handshak.md has the map of
 * which arms exist.
 *
 * @param obj  The V.34 modem object.
 */
void v34handshak(void *obj);

/**
 * @brief `v34handshak`'s once-per-block transmit dispatch, callable on its
 * own.
 *
 * This is the same static dispatch every transmit-microstate arm of
 * `v34handshak` calls -- not a second reconstruction of it -- selected by
 * the txstate word (#V34HS_TXSTATE_OFF) exactly as the object's own three
 * routes into it do (finding F591). `test/unit/t_v34hstbl2.c` drives it
 * directly, against an object the test has steered into this dispatch and
 * no other; over that domain it and the full `v34handshak` are the same
 * function (finding F361). Nothing in `src/` calls it.
 *
 * @param obj  The V.34 modem object.
 */
struct v34_object;
void v34handshak_txblock(struct v34_object *obj);

/**
 * @brief The datapump's per-block entry point, and the last function of
 * `V34hshak.c`.
 *
 * Above 1, the field at +0x2218 drives `v34handshak` until the transmit
 * block is full and the receive queue is drained, and does nothing else.
 * At 0 or 1 it instead runs `modulatevector` and `receiver`, then
 * supervises the line -- retraining or renegotiating based on the
 * receiver's three consecutive-error counters at +0x258, +0x25a and
 * +0x25c. Only the first of those two paths reaches `v34handshak`, and so
 * only it inherits that function's partialness (an unwritten arm aborts).
 *
 * @param obj  The V.34 modem object.
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

/**
 * The transmit power scales (indexed by pre-emphasis index), the receive
 * carrier descriptors, and the phase-2 DPSK band-pass filter pair. All
 * fifteen tables are global in the object and referred to only from this
 * translation unit.
 *
 * `bpv22high` and `bpv22low` are not `const`: they live in `.data` rather
 * than `.rodata`, which is the original's own statement about their
 * storage class.
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

/**
 * @brief Re-arm the FSK demodulator to look for INFO1.
 *
 * The smaller half of dpskinit(): the same clear and configuration,
 * without the modulator, the band-pass filter, or any of the receiver's
 * own state -- so this is the re-arm and dpskinit() is the cold start.
 *
 * @param obj  The V.34 modem object.
 */
void dpskDetectInfo1Init(void *obj);

/**
 * @brief Bring the phase-2 DPSK link up: modulator, receive band-pass
 * filter, FSK demodulator and the receiver's AGC.
 *
 * `mode` and `high` are independent, which is what a full-duplex
 * V.21-style channel needs, since the two directions occupy different
 * bands. 600 baud with the carrier `mode` selects is V.34 phase 2's
 * signalling rate.
 *
 * @param obj   The V.34 modem object.
 * @param mode  0 selects a 1200 Hz transmit carrier, anything else 2400 Hz.
 * @param high  Non-zero selects the upper receive band.
 */
void dpskinit(void *obj, short mode, short high);

/**
 * @brief Turn the negotiated MP bit-fields into transmit and receive
 * symbol rates, carriers, power scales and a pre-emphasis index.
 *
 * Reads the packed rate fields at +0xa9de..+0xa9e3. Rate codes 1, 6 and 7
 * set nothing at all on the side that has them: the object has no default
 * arm, so an unrecognised code leaves the previous rate, scale and carrier
 * in place rather than picking one.
 *
 * The high/low carrier choice is not read from one consistent bit
 * position: the transmit side tests bit 2 of +0xa9de for all four rates
 * that offer a choice, while the receive side tests a different bit of a
 * different byte per rate (+0xa9ae bit 2, +0xa9b2 bit 0, +0xa9b6 bit 7,
 * +0xa9b8 bit 6) -- one bit-field walking through a packed message at a
 * different alignment per rate, not four independent flags. 3429 has a
 * single carrier and tests nothing on either side.
 *
 * @param obj  The V.34 modem object.
 */
void setfinalrate(void *obj);

/**
 * @brief Set the whole V.34 object up for phase 2.
 *
 * Long and almost entirely straight-line: three probe records, thirty-odd
 * scalars, two calls into the modulator, and then the same body twice with
 * four values changed depending on `role` (the originate/answer flag
 * preinitdigital() also reads, finding F177):
 *
 *   |                    | originate | answer    |
 *   |--------------------|-----------|-----------|
 *   | +0x25c2            | 4         | 5         |
 *   | receiver flags     | 0         | 4         |
 *   | phase-2 carrier    | 1200 Hz   | 2400 Hz   |
 *   | receive band-pass  | bpv22high | bpv22low  |
 *   | detector coeff     | c2400_    | c1200_    |
 *
 * The FSK setup is dpskinit()'s, inlined: both arms clear the same shorts
 * and write the same fields dpskinit() would, through the same two shared
 * helpers rather than a second copy.
 *
 * The modulator is configured twice: once at 2400 baud with `reset` clear,
 * then again at 600 baud with it set. The first leaves the shaping tables
 * loaded for a rate phase 2 does not use, which reads as preparing the
 * data-mode configuration ahead of overwriting it with the live one --
 * but nothing here proves that, and the order is simply reproduced as the
 * object has it.
 *
 * @param obj  The V.34 modem object.
 */
void v34modeminit(void *obj);

/**
 * @brief Configure the demodulator for the rate setfinalrate() chose, and
 * arm the tone detector on the carrier descriptor it selected.
 *
 * Two independent switches, on the receive symbol rate and on the receive
 * carrier, and neither has a default: an unrecognised rate leaves the
 * timing constants alone and an unrecognised carrier leaves the carrier
 * table alone. Between them they set four timing constants, a sine table
 * and its half-length, then hand the carrier descriptor setfinalrate()
 * stored to detectorinit().
 *
 * The rate switch has an arm for 2743 baud that setfinalrate() has no way
 * to select -- nothing writes that rate into the field this function reads
 * it from, so the arm is either dead or fed by a writer not yet
 * reconstructed (deviation D35).
 *
 * The 2800-baud timing constants are the odd ones out (0x3e82/0x1f41 where
 * every other rate uses 0x3e80/0x1f40) because `step/wrap` is exactly
 * `2400/baud` at every rate, and 2800 is the one rate whose denominator
 * needs a factor of seven that 16000 does not have -- 16002 does, and only
 * for this one rate (finding F620).
 *
 * @param obj  The V.34 modem object.
 */
void setupreceiver(void *obj);

/**
 * @brief The pre-emphasis index (6..10) for a given baud rate: how many
 * multiplications by a per-rate ratio it takes for a per-rate measurement
 * to pass a limit.
 *
 * The object supplies no default arm, so a baud rate other than 2400,
 * 2800, 3000, 3200 or 3429 runs the loop on an uninitialised register
 * there, and on zero here (see bug-compatibility note D37). Nothing in the
 * object calls this function, so what the first argument actually points
 * at is not established; it stays a `void *`.
 *
 * @param obj       Unidentified; see above.
 * @param baudrate  The symbol rate to look up.
 * @return The pre-emphasis index.
 */
short preempindex(void *obj, short baudrate);

/*
 * ---------------------------------------------------------------------------
 * Three DFT banks the handshake arms, and the detector it polls.
 *
 * All four work on `struct v34_dftbin` banks -- see v34det.h. The three
 * initialisers take the bank as an argument rather than finding it in the
 * object, so nothing in the object calls any of them directly: no
 * relocation names them and no direct call reaches them (the same shape
 * `cosread` has). They are global, so they remain testable regardless; it
 * only means the bin counts and the argument have to be read out of the
 * code rather than off a call site.
 *
 * The bin numbers below are in units of the phase step, and one unit is
 * 150 Hz at V.34's 9600 Hz rate (docs/rate_assumptions.md R-1): 0x4000 of
 * phase is a full turn and the step is `bin << 8`, so a bin repeats every
 * 16384 / (bin * 256) = 64 / bin samples.
 */

struct v34_dftbin;

/**
 * @brief Configure the line-probe DFT bank.
 *
 * Twenty-five bins, 150 Hz to 3750 Hz -- the V.34 line probe's tone
 * spacing. Clears both accumulator pairs and the double-precision result,
 * leaving `energy`, `shift` and the two thresholds alone.
 *
 * @param bins  The 25-bin bank to configure.
 */
void dftfreqinit(struct v34_dftbin *bins);

/**
 * @brief Configure the nonlinear-distortion signal bins.
 *
 * Four bins at 1050, 1350, 1950 and 2550 Hz. Each has a matching noise bin
 * one 150 Hz step below it (see dftnlinitNoiseBins()), which is what makes
 * the pair a measurement rather than two independent frequencies -- `nl`
 * reads as "nonlinear", matching V.34 phase 2's nonlinear-distortion
 * measurement from the line probe.
 *
 * This bank overlays the line probe's own bank: `v34handshak`'s rxstate 72
 * arm inlines both initialisers on `obj->probe_bins`, so the signal bank
 * is `probe_bins[0..3]` (finding F739).
 *
 * Only the integer accumulators are cleared here, not the double-precision
 * pair dftfreqinit() also clears -- a difference the object itself has.
 *
 * @param bins  The 4-bin signal bank to configure.
 */
void dftnlinitSignalBins(struct v34_dftbin *bins);

/**
 * @brief Configure the nonlinear-distortion noise bins.
 *
 * Four bins at 900, 1200, 1800 and 2400 Hz, each one step below its
 * matching signal bin (dftnlinitSignalBins()). This bank is the four bins
 * immediately after the line probe's twenty-five (`obj->nl_noise_bins`,
 * finding F739); the averaging loops that follow read `energy` out of both
 * banks and compute `round(256 * signal / noise)`.
 *
 * Only the integer accumulators are cleared, matching
 * dftnlinitSignalBins().
 *
 * @param bins  The 4-bin noise bank to configure.
 */
void dftnlinitNoiseBins(struct v34_dftbin *bins);

/**
 * @brief Arm the retrain-request detector.
 *
 * Configures three bins at 900, 1200 and 1500 Hz in the object's own
 * `retrain_bins`, both thresholds on each, and the five scalars at
 * +0xa24a (see `struct v34_object`).
 *
 * @param obj  The V.34 modem object.
 */
void dftRetrainDetInit(void *obj);

/**
 * @brief Feed samples to the retrain-request detector and report whether
 * the far end is asking for a retrain.
 *
 * The measurement is taken once every 128 samples (the internal phase
 * counter advances four at a time, so any split of calls that totals four
 * samples per call reaches the same cadence).
 *
 * The energy/threshold comparison is asymmetric: `energy` is widened
 * unsigned and the threshold is read signed, so the two disagree once the
 * accumulator is large enough to make `energy` negative as a `short`
 * (finding F212). That only happens on a seeded accumulator in practice --
 * a real tone puts nearly all of its correlation on one axis -- but it is
 * the object's own comparison and is reproduced as such.
 *
 * @param obj       The V.34 modem object.
 * @param nbins     How many of the detector's bins to poll (a prefix of
 *                  the bank the initialiser configured, not necessarily
 *                  all three); only these bins are cleared once a
 *                  measurement completes.
 * @param samples   The input samples.
 * @param nsamples  Number of samples in @p samples.
 * @return 1 only on the measurement that completes the detector's second
 *         run; every other call, including ones that merely accumulate,
 *         returns 0.
 */
int detectRetrainReq(void *obj, short nbins, const short *samples,
		     short nsamples);

/*
 * ---------------------------------------------------------------------------
 * The handshake's two symbol emitters, and the constellations they map onto.
 *
 * Both scramble the bits handed to them, encode the result differentially
 * against the quadrant the object is carrying, and tail-call `txmit` -- so
 * calling either of these transmits a symbol rather than merely computing
 * one. Everything they touch is in `struct v34_object`.
 *
 * Each table entry is a packed complex int, real part in the low half.
 */
extern const int vect4[4];
extern const int vect16[16];

/**
 * The line probe's one period: 64 signed shorts, not a constellation.
 * `v34handshak`'s TX_L1 state indexes it with the low six bits of
 * `vect_idx`, scales each sample by `tx_scale`, and hands four at a time to
 * `txwritequeue`. Sixty-four is what the index mask (`0x3f`) admits; the
 * object checks no length anywhere.
 */
#define V34_PROBE_SAMPLES	64
extern const short probe[V34_PROBE_SAMPLES];

/**
 * @brief Transmit two bits as one quadrant, differentially against the
 * last.
 * @param obj   The V.34 modem object.
 * @param bits  The two bits to send.
 */
void txmitdibit(void *obj, short bits);

/**
 * @brief Transmit four bits as one of sixteen points.
 *
 * The low dibit picks the quadrant, differentially against the last; the
 * high dibit selects within the quadrant and is not differential.
 *
 * @param obj   The V.34 modem object.
 * @param bits  The four bits to send.
 */
void txmitquadbit(void *obj, short bits);

/**
 * @brief Transmit one symbol of the K56flex phase 3/4 sequence.
 *
 * Declared here, alongside v90Phase34() (its V.90 twin), rather than with
 * the rest of the K56flex material, because both drive the two symbol
 * emitters above and their tables. Defined in `src/pump/v34/v34k56.cpp`;
 * see there for why, and for the ways its idle symbol differs from the two
 * emitters.
 *
 * @param obj  The V.34 modem object.
 * @return Always 0; the one caller (inside `v34handshak`) discards it, so
 *         the true return type is not recoverable beyond that.
 */
int k56FlexPhase34(void *obj);

/**
 * @brief Transmit one symbol -- two, in four of its arms -- of the V.90
 * phase 3/4 sequence.
 *
 * The twin of k56FlexPhase34(): drives the same two symbol emitters and
 * tables. Defined in `src/pump/v34/v34pcmmain.cpp`, where the object puts
 * it; see the comment above that definition for how it differs from the
 * K56flex twin.
 *
 * @param obj  The V.34 modem object.
 * @return Always 0, on the same evidence as k56FlexPhase34().
 */
int v90Phase34(void *obj);

/**
 * @brief Transmit one symbol of the "silence" rate-renegotiation sequence.
 *
 * `VPcmV34Progress` picks between this and v90RateReneg() by how far the
 * V.90 receiver has progressed (`v90_receiver > 14` takes this one,
 * `> 10` the other, and anything lower goes to `modulatevector` or
 * `v34handshak` instead).
 *
 * @param obj  The V.34 modem object.
 * @return Always 0, on the same evidence as k56FlexPhase34().
 */
int v90RateRenegSilence(void *obj);

/**
 * @brief Transmit one symbol of the rate-renegotiation sequence.
 *
 * See v90RateRenegSilence() for how `VPcmV34Progress` chooses between the
 * two. Defined beside v90Phase34() in `src/pump/v34/v34pcmmain.cpp`.
 *
 * @param obj  The V.34 modem object.
 * @return Always 0, on the same evidence as k56FlexPhase34().
 */
int v90RateReneg(void *obj);

/**
 * @brief Send whichever PCM receiver is past phase 2 into phase 3.
 *
 * Called at the point in the handshake where the JA is about to go out.
 * Two tail calls and nothing else: `VPcmFloModem::enterPhase3` when
 * `v90_receiver > 1`, otherwise `K56FlexFloModem::enterPhase3FullDuplex`
 * when `k56flex_receiver > 1`, otherwise nothing at all.
 *
 * Declared here rather than in v34pcmif.h because its only two callers are
 * inside `v34handshak`, not part of `VPcmV34Main.cpp`'s `V34XF_`/`VPcmV34`
 * interface. Defined in `src/pump/v34/v34pcmmain.cpp`, since both callees
 * are C++ members.
 *
 * @param obj  The V.34 modem object.
 */
void indicateJaTransmission(void *obj);

/**
 * @brief Turn the line probe's twenty-five bins into a rate offer or
 * choice.
 *
 * Produces a power-reduction request, a set of offered symbol rates (for
 * the originating side, `role == 0x65`) or one chosen rate (for the
 * answering side), and a pre-emphasis index per rate. Writes the rate
 * configuration at +0xaa84 and the outgoing message at +0xa9ac, and
 * nothing else.
 *
 * @param obj  The V.34 modem object.
 */
void probeselect(void *obj);

/*
 * ---------------------------------------------------------------------------
 * Bringing the data-mode transmitter up.
 */

/**
 * @brief Apply the far end's requested power reduction to the transmit
 * scale.
 *
 * When a V.90 receiver is running, the requested reduction is clamped
 * against `GetVPcmMinimalTxPowerReduction()` rather than applied as-is.
 * The scale is then walked one dB step at a time towards the target: the
 * two directions are not symmetric in the object -- the "reduce" loop
 * keeps its accumulator at full width across iterations, while the
 * "restore" loop truncates it to a `short` on every iteration (deviation
 * D51) -- and a final fixed gain (about +1.4 dB) is applied on top of
 * whichever loop ran.
 *
 * @param obj  The V.34 modem object.
 * @param mp   The received MP message; only its first short is read (the
 *             one call site passes `obj + 0xa9dc`). Writes
 *             `tx_pwr_reduction` with the reduction in dB and `tx_scale`
 *             with the resulting transmit scale.
 */
void settxlevel(void *obj, const short *mp);

/**
 * @brief Configure the transmitter for the negotiated rate.
 *
 * Sets the power scale and modulator, two state words to WAIT and SSEG,
 * two transmit flags, then calls `txinit`. Takes nothing but the object:
 * everything else comes from the rate configuration setfinalrate() filled
 * in at +0xaa84.
 *
 * @param obj  The V.34 modem object.
 */
void v34setuptxmit(void *obj);

/*
 * ---------------------------------------------------------------------------
 * The two routines the object keeps file-local, and the struct one of them
 * walks. Both are reachable only from `v34handshak` in the object; our
 * copies have external linkage and the ordinary calling convention, while
 * the blob's are reached as `ref_getbit` and `ref_ApplyBulkDelay` (which
 * needs `objcopy --globalize-symbols` first, finding F221). Both take their
 * arguments in registers there, so a declaration of the *reference* has to
 * say `regparm`; see the tests.
 */

#define V34_BITSOURCE_WORDS	10

/**
 * @brief A message being clocked out one bit at a time, MSB first, with
 * the CRC-16 the V.34 sequences carry appended once the message itself
 * runs out.
 *
 * The word array is inline at offset zero, and getbit() indexes it with
 * `idx`; its length is not stated anywhere in the code -- ten is simply
 * what fits between offset 0 and `crc` at +0x14, and getbit() does not
 * bound `idx` against it.
 *
 * The object keeps one of these at `obj + 0xaa3c`, read via `obj + 0xaa6c`
 * (`getMPrecvdBits` is the writer). It is declared standalone here rather
 * than embedded in `struct v34_object` at that offset because the region
 * is already named from another direction, as `info_caps`/`caps_flags` --
 * the nibbles the handshake reads out of the same two words -- and neither
 * reading is more correct than the other (finding F634).
 *
 * `crc` is initialised to 0xffff and folded with 0x1021 MSB-first; `crc_on`
 * gates both the folding and the 16-bit flush; `nbits` and `pos` bound the
 * message; `wordbits` is how many bits come out of one word; `repeat`
 * enables the restart and `repeats` counts them; `avail0`/`acc0` are what
 * the restart reloads `avail`/`acc` from.
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

/**
 * @brief Read the next bit of a bitsource.
 *
 * Recursive: the restart arm reloads the whole reader and calls itself for
 * the first bit of the repeat. Returns `short`, not `int` -- every one of
 * the four call sites (the recursion, plus `v34handshak`'s three) follows
 * the call with a sign-extension of a 16-bit value.
 *
 * @param b  The bitsource to read from.
 * @return 0 or 1, or -1 once the message is exhausted and `repeat` is clear.
 */
short getbit(struct v34_bitsource *b);

/**
 * @brief Set the far-end echo canceller's bulk delay, and decide whether it
 * can run at that delay.
 *
 * `delay` at or below zero becomes 144; `delay` at or past `bulk_len`
 * becomes zero -- both cases print the same diagnostic, so the transcript
 * has to be compared rather than counted to tell them apart. With neither
 * PCM receiver running and the far canceller already armed, a delay of 29
 * or less disarms it and pulls the DMA delay back by `delay + 15`, capped
 * at 30.
 *
 * The `bulk_len` bound is an unsigned comparison in the object, which is
 * the wrong way round for safety: a negative `bulk_len` would read as huge
 * unsigned, accept every delay, and let the ring clear below run off its
 * end. Reproduced as the object has it rather than fixed; nothing this
 * tree has reconstructed is known to write a negative `bulk_len`, so no
 * call site is known to reach it.
 *
 * @param obj    The V.34 modem object.
 * @param delay  The requested bulk delay, in samples.
 */
void ApplyBulkDelay(void *obj, short delay);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSHAK_H */
