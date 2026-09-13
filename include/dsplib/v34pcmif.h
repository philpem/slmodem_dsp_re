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

/**
 * @brief Fill in the diagnostics record for the running datapump.
 *
 * Reports on whichever of V.34, V.90 or V.92 is running, chosen by
 * `v34_object::status`. An external API entry point: the caller is the
 * application, and it owns @p results. Defined in `src/pump/v34/v34diag.cpp`
 * because the two PCM arms it reports on are reached through a C++ member.
 *
 * @param obj      The V.34 modem object.
 * @param results  Output: the diagnostics record, owned by the caller.
 */
void VPcmV34GetDiagnostics(void *obj, struct TAG_DiagnosticResults *results);

/**
 * @brief Fill an array of points with one of nine visual diagnostics.
 *
 * The available diagnostics are the constellation, the linear equaliser,
 * the DFE, the resampler's phase and offset, either echo canceller, and the
 * decision errors. An external API entry point; the caller owns @p points
 * and says how long it is via @p maxCount.
 *
 * Selectors 3 and 4 do not honour @p maxCount (deviation D710): both always
 * write `points[0]` and return 1, even if @p maxCount is zero. Selector 7,
 * and anything above 8, writes nothing.
 *
 * @param obj       The V.34 modem object.
 * @param what      Which diagnostic to report.
 * @param points    Output array of points, owned by the caller.
 * @param maxCount  Capacity of @p points; see the selector 3/4 exception above.
 * @return How many points were written.
 */
unsigned long VPcmV34GetVisualDiagnostics(void *obj, int what,
					  struct int_complex *points,
					  unsigned long maxCount);

/**
 * @brief Record the timing offset the V.34 receiver has settled on.
 * @param obj     The V.34 modem object.
 * @param offset  The timing offset to record.
 */
void VPcmV34LogTimingOffset(void *obj, short offset);

/**
 * @brief Set the transmit scale to the object's one built-in constant.
 *
 * Reports the value through `edprintf`.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34SetTxScale(void *obj);

/**
 * @brief Get the V.34 object's probe-results array.
 * @param obj  The V.34 modem object.
 * @return An interior pointer to the array the PCM side fills in and reads
 *         back; nothing is copied.
 */
double *V34XF_GetProbeResultsPtr(void *obj);

/**
 * @brief Get the V.34 object's INFO0 bits array.
 * @param obj  The V.34 modem object.
 * @return An interior pointer to the array the PCM side fills in and reads
 *         back; nothing is copied.
 */
int *V34XF_GetInfo0BitsPtr(void *obj);

/**
 * @brief Get the measured round-trip delay, plus 480 samples.
 * @param obj  The V.34 modem object.
 * @return The delay, truncated to 16 bits -- a large stored delay comes
 *         back negative; see the note in v34pcmif.c.
 */
short V34XF_GetRTD(void *obj);

/**
 * @brief Record a Phase-3 Jd arrival.
 *
 * Advances the object's `v90_receiver` counter.
 *
 * @param obj           The V.34 modem object.
 * @param constel       Constellation size, carried in the message.
 * @param silence_scr   Silence/scrambling bits, carried in the message.
 */
void V34XF_IndicateJdReceived(void *obj, unsigned char constel,
			      unsigned char silence_scr);

/**
 * @brief Record a Phase-3 Dil arrival.
 *
 * Advances the object's `v90_receiver` counter.
 *
 * @param obj      The V.34 modem object.
 * @param constel  Constellation size, carried in the message.
 */
void V34XF_IndicateDilReceived(void *obj, unsigned char constel);

/**
 * @brief Record a Phase-3 TRN2d arrival.
 *
 * Ratchets the object's `v90_receiver` counter forward, rather than setting it.
 *
 * @param obj  The V.34 modem object.
 */
void V34XF_IndicateTrn2dReceived(void *obj);

/**
 * @brief Record that the K56Flex rate has been determined.
 *
 * Advances the object's `v90_receiver` counter.
 *
 * @param obj  The V.34 modem object.
 */
void V34XF_IndicateK56FlexRateDetermined(void *obj);

/*
 * The three requests the shell makes of a running connection (the next
 * three functions). Each forks on `status`: values 1 and 2 mean a PCM
 * receiver has the line, and the request is forwarded to the C++ side
 * down a chain of three pointers instead.
 */

/**
 * @brief Tear the connection down.
 *
 * Clears the pending rate request and both rate bounds.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34InitiateHangUp(void *obj);

/**
 * @brief Ask for a different connection rate.
 *
 * A step that would leave `[rate_min, rate_max]` leaves the pending
 * request unchanged rather than clamping it. When a PCM receiver has the
 * line, @p req is instead forwarded verbatim to the C++ side, where it
 * means something else.
 *
 * @param obj  The V.34 modem object.
 * @param req  0, 2 or 5 for one rate index down; 3 for one up; anything
 *             else for "no particular rate".
 */
void VPcmV34InitiateRateRenegotiation(void *obj, int req);

/**
 * @brief Count a rate renegotiation requested by the local end.
 *
 * A wrapping 16-bit increment and nothing else. `datapumpv34` is the only
 * caller in the object; `VPcmV34GetDiagnostics` reads the count back signed.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34IndicateLocalRRN(void *obj);

/**
 * @brief Count a rate renegotiation requested by the remote end.
 *
 * A wrapping 16-bit increment and nothing else. `datapumpv34` is the only
 * caller in the object.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34IndicateRemoteRRN(void *obj);

/**
 * @brief Rebuild the transmitter for a V.90 rate renegotiation.
 *
 * The only one of the three renegotiation paths that does not go through
 * the handshake. Both parameters are tested only against zero. The
 * parameter names are the object's own, taken from the diagnostic this
 * function prints.
 *
 * @param obj           The V.34 modem object.
 * @param rrn_type      Zero or non-zero: selects `v90_receiver` state 15 or 11.
 * @param constel_size  Zero or non-zero: selects constellation size 0x89b0 or 0x8990.
 */
void VPcmV34SetV90RateReneg(void *obj, short rrn_type,
			    unsigned char constel_size);

/**
 * @brief Tear the connection down and restart the V.34 handshake.
 *
 * Re-reads the configuration first and, if asked, switches modulation.
 * Unlike the three requests above, this is not gated on `status`: it acts
 * on the V.34 object whichever modem currently has the line, and always
 * ends in `v34handshakinit` mode 1.
 *
 * A request the configuration forbids is demoted to 0 rather than refused,
 * and 0 does not mean "leave everything alone" either -- it winds a
 * running V.90 receiver back to 1. See the two switches in v34pcmmain.cpp.
 *
 * Defined in `v34pcmmain.cpp` rather than `v34pcmif.c` because four of its
 * calls are to C++ members; it is `extern "C"` on both sides, which is why
 * it is declared here with the rest of this translation unit's exports.
 *
 * @param obj          The V.34 modem object.
 * @param requestedDp  The modulation to switch to, passed as a byte: 0
 *                      keeps whatever is running, 34/56/90/92 select
 *                      V.34/K56Flex/V.90/V.92, and anything else leaves
 *                      neither PCM receiver running (the same state a
 *                      request of 34 ends in).
 */
void VPcmV34InitiateRetrain(void *obj, unsigned char requestedDp);

/**
 * @brief Report that the handshake has started echo-canceller adaptation.
 *
 * A debug-log call: gated on the debug level and prints one fixed string,
 * touching neither the object nor its own argument otherwise. The argument
 * still exists in the object's code (it is loaded into the printf call's
 * slot), so it is kept here for byte-identical calling convention, even
 * though nothing reads it. `v34handshak` is the only caller.
 *
 * @param obj  The V.34 modem object (unused; see above).
 */
void VPcmV34ReportStartOfEchoAdapt(void *obj);

/**
 * @brief Report that the handshake has reached the middle of echo-canceller
 * adaptation.
 *
 * Same shape as VPcmV34ReportStartOfEchoAdapt(): a debug-log call that
 * otherwise does nothing. `v34handshak` is the only caller.
 *
 * @param obj  The V.34 modem object (unused; see above).
 */
void VPcmV34ReportMiddleOfEchoAdapt(void *obj);

/**
 * @brief Run one block of samples through whichever datapump owns the line.
 *
 * Dispatches to V.34, V.90, V.92 or K56Flex as appropriate. This is the
 * whole of `vpcm_run`'s work between the two sample-format conversions; a
 * real 33,600 V.34 connection executes no other unwritten symbol (finding
 * F1454). Defined in `src/pump/v34/v34pcmmain.cpp` because seven of its
 * callees are C++ members.
 *
 * Also declared, `weak`, in `include/dsplib/vpcm.h`, so that `vpcm_run` (a
 * plain C file) links whether or not this translation unit is present; the
 * two declarations describe one ABI and never both appear in one
 * translation unit. This declaration is the definition's, so it is not weak.
 *
 * @param obj      The V.34 modem object.
 * @param in       Input samples for this block.
 * @param out      Output samples for this block.
 * @param nin      Number of input samples.
 * @param rxbits   Output: received bits.
 * @param nrx      Output: number of received bits.
 * @param txbits   Output: transmitted bits.
 * @param nbits    Output: number of transmitted bits.
 * @return Status code (see the object's calling convention).
 */
int VPcmV34Progress(void *obj, float *in, float *out, int nin, int *rxbits,
		    int *nrx, int *txbits, int *nbits);

/*
 * The unwritten-path record for VPcmV34Progress()'s seven unreconstructed
 * callees. `vpcm.h`'s record of the same shape explains why this exists and
 * why the default behaviour is to abort; the codes below are one level
 * down from that file's.
 *
 * Everything named here belongs to the V.90 and V.92 arms -- a V.34 call
 * reaches none of them, which is what makes `t_vpcmrun`'s four-way
 * comparison meaningful with these still unwritten.
 */
#define V34PCM_WRITTEN			0
#define V34PCM_UNWRITTEN_RUNPCM		1	/* runPcmModem          */
#define V34PCM_UNWRITTEN_V90RUN		2	/* v90RunDemodulator    */
#define V34PCM_UNWRITTEN_QCLINE		3	/* qcLineVerification   */
#define V34PCM_UNWRITTEN_RESETP3	4	/* vPcmResetPhase3Modem */
#define V34PCM_UNWRITTEN_TONEPROC	5	/* GenericToneDetector  */
#define V34PCM_UNWRITTEN_RRN		6	/* v90RateReneg         */
#define V34PCM_UNWRITTEN_RRNSILENCE	7	/* v90RateRenegSilence  */

/**
 * @brief Report which unwritten VPcmV34Progress() callee, if any, was reached.
 * @return One of the `V34PCM_UNWRITTEN_*` codes, or #V34PCM_WRITTEN if none was.
 */
int v34pcm_unwritten(void);

/**
 * @brief Acknowledge an unwritten-callee abort and allow the process to continue.
 *
 * Clears the unwritten-path record and turns off the abort-on-unwritten-path
 * behaviour. Without this call, reaching an unwritten path stops the
 * process -- an arm that silently returns is otherwise indistinguishable
 * from one that correctly did nothing.
 */
void v34pcm_unwritten_reset(void);

/**
 * @brief Cap the V.34 symbol rates the line probe is allowed to choose.
 *
 * Writes `shift` onto the probe bins that stand for the rates the
 * configuration bars, so the caller (`probeselect`) skips them. Nothing is
 * returned and the V.34 object itself is not written -- the effect lives
 * entirely in the bin bank.
 *
 * @param obj   The V.34 modem object.
 * @param bins  The probe bin bank to constrain (always `obj->probe_bins`
 *              at both call sites, but kept as a parameter here).
 */
void chkForceBaudRate(void *obj, struct v34_dftbin *bins);

/**
 * @brief Get the transmit power back-off, in dB.
 *
 * Has the side effect of setting the echo canceller's three adaptation
 * constants -- see v34pcmif.c.
 *
 * @param obj  The V.34 modem object.
 * @return The back-off, clamped to [-10, +7].
 */
short GetVPcmMinimalTxPowerReduction(void *obj);

/**
 * @brief Get the effective maximum upstream rate.
 * @param obj  The V.34 modem object.
 * @return The smaller of the configured upstream rate and the PCM
 *         receiver's own cap, in bits per second (despite the name).
 */
int VPcmV34GetMaxUpstreamRateIndex(void *obj);

/*
 * The public accessor surface below: what the layer above the datapump
 * calls. All of these are `extern "C"` exports of `VPcmV34Main.cpp`,
 * defined in `v34pcmif.c`. The four "current" getters are declared as a
 * block because they read as four copies of one function and are not --
 * see the comment on them in the .c file.
 */

/**
 * @brief Get the effective maximum upstream rate.
 *
 * The same implementation as VPcmV34GetMaxUpstreamRateIndex(), under this
 * header's other naming prefix.
 *
 * @param obj  The V.34 modem object.
 * @return The maximum upstream rate, in bits per second.
 */
int V34XF_GetMaxUpstreamRateIndex(void *obj);

/**
 * @brief Tear the datapump down.
 * @param obj  The V.34 modem object; unused.
 * @return Always 0.
 */
int VPcmV34Delete(void *obj);

/**
 * @brief Set the datapump's block length.
 * @param obj  The V.34 modem object; the field is `ptc`. See deviation D380.
 * @param len  The new block length.
 */
void VPcmV34SetMaxBlockLength(void *obj, int len);

/**
 * @brief Report whether this connection came up on a short Phase 2.
 * @param obj  The V.34 modem object.
 * @return Non-zero if a short Phase 2 was used.
 */
int VPcmV34GetQuickConnectIndication(void *obj);

/**
 * @brief Get the current receive symbol rate.
 * @param obj  The V.34 modem object.
 * @return Symbol rate in baud, or 8000 while a PCM receiver is running.
 */
int VPcmV34GetCurrentRxBaudRate(void *obj);

/**
 * @brief Get the current transmit symbol rate.
 * @param obj  The V.34 modem object.
 * @return Symbol rate in baud, or 8000 while a PCM receiver is running.
 */
int VPcmV34GetCurrentTxBaudRate(void *obj);

/**
 * @brief Get the current receive carrier frequency.
 * @param obj  The V.34 modem object.
 * @return Carrier in Hz, or 0 while a PCM receiver is running.
 */
int VPcmV34GetCurrentRxCarrier(void *obj);

/**
 * @brief Get the current transmit carrier frequency.
 * @param obj  The V.34 modem object.
 * @return Carrier in Hz, or 0 while a PCM receiver is running.
 */
int VPcmV34GetCurrentTxCarrier(void *obj);

/**
 * @brief Get the equaliser's signal-to-noise ratio.
 * @param obj  The V.34 modem object.
 * @return SNR in whole dB, or 0 when unavailable.
 */
int VPcmV34GetSNR(void *obj);

/**
 * @brief Get the receiver's timing offset.
 *
 * Reached by no caller anywhere in the object; see v34fsk.h's field
 * comments for the naming derivation of the field this reads.
 *
 * @param obj  The V.34 modem object.
 * @return The timing offset.
 */
int getTimingOffset(void *obj);

/**
 * @brief Get the receiver's timing phase.
 *
 * Reached by no caller anywhere in the object; see v34fsk.h's field
 * comments for the naming derivation of the field this reads.
 *
 * @param obj  The V.34 modem object.
 * @return The timing phase.
 */
int getTimingPhase(void *obj);

/**
 * @brief Notify the datapump of an event.
 * @param obj   The V.34 modem object.
 * @param what  0 or 1 for samples, 2 for CAS, 3 for three-way calling.
 */
void VPcmV34NotifyDP(void *obj, int what);

/**
 * @brief Collect the pending output-sample-clear request, if any.
 *
 * @param obj    The V.34 modem object.
 * @param flag   Output: the request's flag, if there is one.
 * @param count  Output: the request's count, if there is one.
 * @param done   Output: the request's done indicator, if there is one.
 * @return 1 and fills all three outputs if a request is pending, 0 and
 *         writes nothing otherwise.
 */
int VPcmV34RequestDPNotification(void *obj, int *flag, int *count, int *done);

/**
 * @brief Record a K56Flex Jd arrival and rebuild the transmitter.
 * @param obj      The V.34 modem object.
 * @param constel  Constellation size.
 */
void V34XF_IndicateK56FlexJdReceived(void *obj, unsigned char constel);

/**
 * @brief Record that the remote end has asked for a retrain.
 * @param obj  The V.34 modem object.
 */
void VPcmV34SetIndicationOfRemoteRetrain(void *obj);

/*
 * VPcmV34SetTimeOut() -- reset the sample clock and set a deadline
 * `secs * 9600` samples out -- is declared further down, alongside the
 * other C++-mangled entry points: its symbol is mangled
 * (`_Z17VPcmV34SetTimeOutP12tagV34Objecti`), so it has to live outside the
 * `extern "C"` block below and is defined in `v34pcmmain.cpp`.
 */

#ifdef __cplusplus
}
#endif

/*
 * The remaining entry points of VPcmV34Main.cpp are C++ on both sides of
 * their declaration, unlike everything above: the object exports each of
 * them under its mangled C++ name rather than an unmangled one, so an
 * `extern "C"` declaration -- which would name the unmangled symbol --
 * would not match the object's own definition of it.
 *
 * All of them take `struct tagV34Object *`, which is the object's own name
 * for what v34fsk.h calls `struct v34_object` (and the only place that
 * name survives). It is kept incomplete and distinct from `v34_object`
 * here deliberately: a `typedef` between the two would still mangle
 * differently, so the definitions cast between the two named types instead.
 *
 * Declared here, rather than in a private header, because `VPcmV34Main.cpp`
 * is split across a `.c` and a `.cpp` in this tree and this is the header
 * both halves already include.
 *
 * `getMPrecvdBits` is deliberately NOT among them any more: the reference
 * records it LOCAL, so it is `static` in `v34pcmmain.cpp`, where its one
 * consumer was merged in, and a test that names it declares it itself and
 * resolves against the test tier's globalized copy of that object
 * (`tools/testvisible.py`).
 */
#ifdef __cplusplus
struct tagV34Object;

/**
 * @brief Push upstream modulation information to the running modem.
 *
 * Currently a no-op in the object (its body is just `ret`); every
 * parameter is unread.
 *
 * @param obj  The V.34 modem object; unused.
 */
void SetUpstreamModulationInfo(struct tagV34Object *obj);

/**
 * @brief Push the configured rate bounds to whichever modem is running.
 *
 * Reaches the V.90 constellation designer, the K56Flex modem, or the V.34
 * rate group, depending on which is active; the V.34 case divides both
 * bounds by 2400 and caps the result at 14.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34SetMinMaxBitRates(struct tagV34Object *obj);

/**
 * @brief Set the receive energy floor from the configured signal level.
 *
 * Looks up `V34DisconnectThreshTable` (file-local in `v34pcmmain.cpp`) by
 * the configuration's signal-level setting and writes the result to
 * `rx_energy_floor`.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34SetMinimumSigLevel(struct tagV34Object *obj);

/**
 * @brief Compute the echo canceller's filter and DMA delays from the
 * configuration.
 *
 * Sets `filtdelay` and `dmadelay`, and hands the echo canceller its delay.
 *
 * @param obj  The V.34 modem object.
 */
void VPcmV34SetDelays(struct tagV34Object *obj);

/**
 * @brief Restart the sample clock and set a deadline.
 * @param obj   The V.34 modem object.
 * @param secs  Seconds until the deadline; converted to samples at 9600 Hz.
 */
void VPcmV34SetTimeOut(struct tagV34Object *obj, int secs);
#endif

#endif /* DSPLIB_V34PCMIF_H */
