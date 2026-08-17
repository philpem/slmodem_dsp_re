/*
 * v34pcmif.h -- the V.90/K56Flex side's hooks into V.34.  See v34pcmif.c.
 */

#ifndef DSPLIB_V34PCMIF_H
#define DSPLIB_V34PCMIF_H

#include "dsplib/v34det.h"	/* struct v34_dftbin: chkForceBaudRate's arg */

/*
 * `VPcmV34GetDiagnostics`'s second argument.  A forward declaration, so this
 * header does not drag TAG_DiagnosticResults.h in and neither definition can
 * collide with the other (CLAUDE.md, "One type, one home"); the elaborated
 * `struct` spelling is what lets one declaration serve both languages.
 */
struct TAG_DiagnosticResults;
struct int_complex;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fill in the diagnostics record for whichever datapump is running -- V.34,
 * V.90 or V.92, chosen by `v34_object::status`.  An external API entry point:
 * the caller is the application and it owns the record.  See
 * src/pump/v34/v34diag.cpp, which is a .cpp because the two PCM arms call a
 * C++ member.
 */
void VPcmV34GetDiagnostics(void *obj, struct TAG_DiagnosticResults *results);

/*
 * Fill an array of points with one of nine per-selector diagnostics -- the
 * constellation, the linear equaliser, the DFE, the resampler's phase and
 * offset, either echo canceller, or the decision errors -- and return how
 * many points were written.  Another external API entry point; the caller
 * owns the array and says how long it is.
 *
 * `maxCount` IS NOT HONOURED BY SELECTORS 3 AND 4.  Deviation D710: both
 * write `points[0]` and return 1 whatever it says, including zero.
 *
 * Selector 7 answers nothing, and so does anything above 8.
 */
unsigned long VPcmV34GetVisualDiagnostics(void *obj, int what,
					  struct int_complex *points,
					  unsigned long maxCount);

/* Record the timing offset the V.34 receiver has settled on. */
void VPcmV34LogTimingOffset(void *obj, short offset);

/*
 * Set the transmit scale to the one constant the object ever uses, and say
 * so through `edprintf`.  No parameter: the value is built in.
 */
void VPcmV34SetTxScale(void *obj);

/*
 * Where the V.34 object keeps the two arrays the PCM side fills in and reads
 * back.  Both return interior pointers and neither copies anything.
 */
double *V34XF_GetProbeResultsPtr(void *obj);
int *V34XF_GetInfo0BitsPtr(void *obj);

/*
 * The measured round-trip delay plus 480 samples.  Truncated to 16 bits, so
 * a large stored delay comes back negative -- see the note in v34pcmif.c.
 */
short V34XF_GetRTD(void *obj);

/*
 * Phase-3 arrivals.  Each advances the object's `v90_receiver` counter;
 * TRN2d ratchets it rather than setting it.  `constel` and `silence_scr` are
 * carried bits of the message, not sizes.
 */
void V34XF_IndicateJdReceived(void *obj, unsigned char constel,
			      unsigned char silence_scr);
void V34XF_IndicateDilReceived(void *obj, unsigned char constel);
void V34XF_IndicateTrn2dReceived(void *obj);
void V34XF_IndicateK56FlexRateDetermined(void *obj);

/*
 * The three requests the shell makes of a running connection.  Each forks on
 * `status`: 1 and 2 mean a PCM receiver has the line and the request goes to
 * the C++ side down a chain of three pointers instead.
 */

/* Tear the connection down.  Clears the rate request and both bounds. */
void VPcmV34InitiateHangUp(void *obj);

/*
 * Ask for a different rate.  `req` is 0, 2 or 5 for one index down, 3 for one
 * up and anything else for "no particular rate"; a step that would leave
 * [rate_min, rate_max] leaves the request unchanged rather than clamping.
 * On the PCM arm the code is forwarded verbatim and means something else.
 */
void VPcmV34InitiateRateRenegotiation(void *obj, int req);

/*
 * Count a rate renegotiation, by which end asked for it.  Each is a wrapping
 * 16-bit increment of one short and nothing else; `datapumpv34` is the only
 * caller of either in the object.  `VPcmV34GetDiagnostics` reads the local
 * one back signed.
 */
void VPcmV34IndicateLocalRRN(void *obj);
void VPcmV34IndicateRemoteRRN(void *obj);

/*
 * Rebuild the transmitter for a V.90 rate renegotiation -- the only one of
 * the three that does not go through the handshake.  Both parameters are
 * tested against zero only: `rrn_type` selects 15 or 11 for `v90_receiver`
 * and `constel_size` selects 0x89b0 or 0x8990 for `f382`.  The names are the
 * object's own, from the diagnostic this prints.
 */
void VPcmV34SetV90RateReneg(void *obj, short rrn_type,
			    unsigned char constel_size);

/*
 * Tear the connection down and start the V.34 handshake again, having first
 * re-read the configuration and -- if asked -- switched modulation.
 *
 * `requestedDp` is a datapump code and it is a BYTE: the object loads the
 * argument slot with `movzbl`, keeps it in one and prints it as "%d".  The
 * five values it acts on are the modulation numbers themselves --
 *
 *      0      keep whatever is running
 *     34      V.34
 *     56      K56Flex
 *     90      V.90
 *     92      V.92
 *
 * -- and anything else leaves neither PCM receiver running, which is the same
 * state 34 ends in.  A request the configuration forbids is DEMOTED TO 0
 * rather than refused, and 0 is not "leave everything alone" either: it winds
 * a running V.90 receiver back to 1.  See the two switches in v34pcmmain.cpp.
 *
 * NOT gated on `status` the way the three requests above are: this one acts on
 * the V.34 object whichever modem has the line, and it always ends in
 * `v34handshakinit` mode 1.
 *
 * It lives in v34pcmmain.cpp rather than v34pcmif.c because four of its calls
 * are C++ members; it is `extern "C"` on both sides, which is why it is
 * declared here with the rest of the translation unit's exports.
 */
void VPcmV34InitiateRetrain(void *obj, unsigned char requestedDp);

/*
 * Two progress reports the handshake makes, and nothing acts on.
 *
 * Each is a debug gate and one string; neither touches the object or reads
 * its argument.  The argument exists all the same: the object overwrites its
 * own first argument slot with the format pointer and tail-jumps into
 * `dsplibs_debug_printf`, which a function with no parameters would have no
 * slot to do.  So the transcript is the whole observable behaviour, and a
 * test that compared only state would pass on an empty body.
 *
 * `v34handshak` calls one of each and nothing else calls either.
 */
void VPcmV34ReportStartOfEchoAdapt(void *obj);
void VPcmV34ReportMiddleOfEchoAdapt(void *obj);

/*
 * ---------------------------------------------------------------------------
 * `VPcmV34Progress`, 0xb3c0 and 7,278 bytes: one block of samples through
 * whichever of V.34, V.90, V.92 and K56flex owns the line.  It is the whole
 * of `vpcm_run`'s work between the two sample conversions, and finding 1454
 * measured that a real 33,600 V.34 connect executes no other unwritten
 * symbol.  Defined in `src/pump/v34/v34pcmmain.cpp` because seven of its
 * callees are C++ members; the head of that block is the map.
 *
 * Declared in `include/dsplib/vpcm.h` as well, WEAK, because `vpcm_run` is a
 * C file that must link whether or not this one is present.  The two
 * declarations describe one ABI and never meet in a translation unit -- and
 * this one is the definition's, so it is not weak.
 */
int VPcmV34Progress(void *obj, float *in, float *out, int nin, int *rxbits,
		    int *nrx, int *txbits, int *nbits);

/*
 * ---------------------------------------------------------------------------
 * The unwritten-path record for `VPcmV34Progress`'s seven unreconstructed
 * callees.  `vpcm.h`'s block of the same shape says why it exists and why the
 * default is to abort; the codes below are that file's, one level down.
 *
 * Everything named here belongs to the V.90 and V.92 arms.  A V.34 call
 * reaches none of them, which is what makes `t_vpcmrun`'s four-way comparison
 * meaningful with them absent.
 */
#define V34PCM_WRITTEN			0
#define V34PCM_UNWRITTEN_RUNPCM		1	/* runPcmModem          */
#define V34PCM_UNWRITTEN_V90RUN		2	/* v90RunDemodulator    */
#define V34PCM_UNWRITTEN_QCLINE		3	/* qcLineVerification   */
#define V34PCM_UNWRITTEN_RESETP3	4	/* vPcmResetPhase3Modem */
#define V34PCM_UNWRITTEN_TONEPROC	5	/* GenericToneDetector  */
#define V34PCM_UNWRITTEN_RRN		6	/* v90RateReneg         */
#define V34PCM_UNWRITTEN_RRNSILENCE	7	/* v90RateRenegSilence  */

/* Which unwritten callee was reached, or V34PCM_WRITTEN for none. */
int v34pcm_unwritten(void);

/*
 * "I am going to read the code afterwards."  Clears the record AND turns the
 * abort off; without this call an unwritten path stops the process, because
 * an arm that returns quietly is indistinguishable from an arm that correctly
 * did nothing.
 */
void v34pcm_unwritten_reset(void);

/*
 * Cap the V.34 symbol rate the line probe is allowed to choose, by writing
 * `shift` on the bins that stand for the rates the configuration bars.
 *
 * `bins` is always `obj->probe_bins` at both of `probeselect`'s call sites,
 * but it is a parameter in the object and is kept one here.  Nothing is
 * returned and the V.34 object is not written: the effect is entirely in the
 * bank, and only `probeselect` looks at it afterwards.
 */
void chkForceBaudRate(void *obj, struct v34_dftbin *bins);

/*
 * The transmit power back-off in dB, clamped to [-10, +7].
 *
 * SHORT, and the caller says so: `settxlevel` does `movswl %ax` on the
 * result.  It has the side effect of setting the echo canceller's three
 * adaptation constants -- see v34pcmif.c.
 */
short GetVPcmMinimalTxPowerReduction(void *obj);

/*
 * The smaller of the configured upstream rate and the PCM receiver's own
 * cap, in bits per second despite the name.  One caller, `v34handshak` at
 * 0x63457, and it is the only thing that fixes the return type as an int.
 */
int VPcmV34GetMaxUpstreamRateIndex(void *obj);

/*
 * ---------------------------------------------------------------------------
 * The public accessor surface: what the layer above the datapump calls.
 *
 * All of them are `extern "C"` exports of `VPcmV34Main.cpp` and all of them
 * are defined in `v34pcmif.c`.  The four "current" getters are declared as a
 * block because they read as four copies of one function and are not; see the
 * comment on them in the `.c`.
 */

/* The same body as `VPcmV34GetMaxUpstreamRateIndex` under the other prefix. */
int V34XF_GetMaxUpstreamRateIndex(void *obj);

/*
 * Tear the datapump down.  Always zero, and the argument is a convention:
 * three instructions that read nothing cannot fix an arity.
 */
int VPcmV34Delete(void *obj);

/* Set the datapump's block length; the field is `ptc`.  See D380. */
void VPcmV34SetMaxBlockLength(void *obj, int len);

/* Non-zero if this connection came up on a short phase 2. */
int VPcmV34GetQuickConnectIndication(void *obj);

/* Symbol rate in baud, or 8000 with a PCM receiver running. */
int VPcmV34GetCurrentRxBaudRate(void *obj);
int VPcmV34GetCurrentTxBaudRate(void *obj);

/* Carrier in Hz, or 0 with a PCM receiver running. */
int VPcmV34GetCurrentRxCarrier(void *obj);
int VPcmV34GetCurrentTxCarrier(void *obj);

/* The equaliser's signal-to-noise ratio in whole dB, 0 when unavailable. */
int VPcmV34GetSNR(void *obj);

/* Tell the datapump something happened: 0/1 samples, 2 CAS, 3 three-way. */
void VPcmV34NotifyDP(void *obj, int what);

/*
 * Collect the pending output-sample-clear request.  Returns 1 and fills all
 * three when there is one, 0 and writes nothing when there is not.
 */
int VPcmV34RequestDPNotification(void *obj, int *flag, int *count, int *done);

/* A K56flex Jd has arrived: rebuild the transmitter.  `constel` is a size. */
void V34XF_IndicateK56FlexJdReceived(void *obj, unsigned char constel);

/* The remote end has asked for a retrain. */
void VPcmV34SetIndicationOfRemoteRetrain(void *obj);

/*
 * Reset the sample clock and set a deadline `secs * 9600` samples out.
 *
 * MANGLED, so it is C++ and lives in `v34pcmmain.cpp` --
 * `_Z17VPcmV34SetTimeOutP12tagV34Objecti`.  Declared with the rest of the
 * surface all the same, below the `extern "C"` block.
 */

#ifdef __cplusplus
}
#endif

/*
 * ---------------------------------------------------------------------------
 * The one entry point of VPcmV34Main.cpp reconstructed so far that is NOT one
 * of its `extern "C"` exports, and so is C++ on both sides of the
 * declaration.
 *
 * OUTSIDE the block above, deliberately.  Everything above is reachable from
 * C because the object exports it unmangled; this is not, and putting it in
 * an `extern "C"` block would emit `getMPrecvdBits` where the object has
 * `_Z14getMPrecvdBitsP12tagV34Object`.  The mangling is the only thing that
 * makes our definition a replacement for the blob's, so the declaration has
 * to be C++ and the translation unit that defines it has to be a `.cpp`.
 *
 * `tagV34Object` IS THE OBJECT'S OWN NAME for what v34fsk.h calls
 * `struct v34_object`; it survives only inside this symbol, which is where
 * that header's note about the name comes from.  It stays INCOMPLETE here: a
 * `typedef` to `v34_object` would mangle as `P11v34_object` and produce a
 * different symbol, so the two names have to remain distinct types and the
 * definition casts between them.
 */
#ifdef __cplusplus
struct tagV34Object;

/*
 * Copy the V.90 MP sequence the session has received into the V.34 object's
 * INFO fields, then rebuild the capability word at +0xaa3c around the maximum
 * upstream rate the configuration allows.
 *
 * Reads the session at `p3548 + 0x1744` -- six flag bytes and seven shorts --
 * and the configuration at `pac3c + 0x3c`; announces the rate it chose
 * through `edprintf`, in one of two messages according to whether the PCM
 * receiver's "sensitive ISP" word gets a say.
 */
void getMPrecvdBits(struct tagV34Object *obj);

/*
 * ---------------------------------------------------------------------------
 * AND THE SIX ACCESSORS THAT ARE MANGLED TOO, so C++ on both sides for the
 * same reason `getMPrecvdBits` is.  All six take `tagV34Object *`, which is
 * the whole of why they are mangled: an `extern "C"` export of the same file
 * takes `void *` and these take the object's own type.
 *
 * They are declared here rather than in a private header because there is no
 * private header -- `VPcmV34Main.cpp` is split across a `.c` and a `.cpp` in
 * this tree, and this is the header both halves already include.
 */

/* Two instructions: `ret`.  Every parameter is unread. */
void SetUpstreamModulationInfo(struct tagV34Object *obj);

/*
 * Push the configured rate bounds down to whichever modem is running: the
 * V.90 constellation designer, the K56flex modem, or the V.34 rate group at
 * +0x220/+0x224 by dividing both by 2400 and capping at 14.
 */
void VPcmV34SetMinMaxBitRates(struct tagV34Object *obj);

/*
 * Set `rx_energy_floor` from `V34DisconnectThreshTable`, indexed by the
 * configuration's +0x60 biased by 48 and defaulting to entry 3.
 */
void VPcmV34SetMinimumSigLevel(struct tagV34Object *obj);

/*
 * Compute `filtdelay` and `dmadelay` from the configuration's +0x64 and
 * +0x68, and hand the echo canceller its delay.
 */
void VPcmV34SetDelays(struct tagV34Object *obj);

/* Restart the sample clock and set a deadline `secs * 9600` samples out. */
void VPcmV34SetTimeOut(struct tagV34Object *obj, int secs);
#endif

#endif /* DSPLIB_V34PCMIF_H */
