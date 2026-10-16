# Call-boundary lead ledger

At 66087521, the historical eight-instruction argument screen has 39 nominated
bodies. Eleven were reviewed in earlier PR #284 batches; this pass reviews the
remaining 28 and closes the supported local experiments below. A nomination is
an instruction-window difference, not evidence of a wrong prototype. These
counts include repeated emitted template copies and are not the strict
1,852-symbol byte-identity denominator.

## Earlier reviews

FPM_FSE_receive recovered paired error lifetimes; FPM_MTD_detect has the
original extension outside the window; FPM_TONE_detect's conversion serves
energy arithmetic; settxlevel's conversion does not supply bitreverse.
getV90Decision and scalar Descrambler<unsigned char,int>::process were reviewed
with the scalar return correction. v23_process recovered the imported int
count contract. V34GiveINFO1aBits recovered early word caching and both
pointer captures. FAXVMI_create recovered the promoted capacity comparison.
reset_cid already has the original signed word loop; FDSP_DP_Create's nominated
conversion serves a success predicate, and its stage-flag domain is closed.
See F11975–F11984 and their linked complete-TU records.

## Remaining 28 bodies

| Body | Evidence and disposition |
| --- | --- |
| DemodDataV17 | AGC receives the unsigned caller count at a50bd; the candidate's BX sign extension prepares later signed uses. AGC-return and MRF-count domains are already measured separately. No new formal witness. |
| V17RX_status | Original TESTB/SETE at a093d/a0941 supplies the quality word; the candidate extension belongs to that mask result, not GetSNRV17. Both source operands already express the same predicate. |
| DemodDataV21 | Original a57e8 MOVZWL AX,EDX is immediately outside the eight-instruction FSD window: no missing result narrowing. The AGC nomination is the separate signed count preparation, not a new return witness. |
| RxNextStateV21 | Original default reload at a1e30 and diagnostic owner reloads were crossed in the closed V21 next-state publication domain. No new context or operand witness. |
| RxHdxStartV21 | The same inlined next-state default conversion; not another independent source statement. |
| RxHdxWaitV21 | Next-state default conversion plus countdown conversion in a neighbouring block; existing publication domain covers the former. |
| RxHdxDataV21 | The same next-state default conversion; no new local source experiment. |
| V21RX_status | Original TESTB/SETE at a2487/a248b computes the quality word; the extra conversion belongs to the predicate rather than GetSNRV21. |
| GenEQTrnSequenceV27 | Signed fill index, postdecrement output walk and per-output rate reload are already the eight-cell batch100-fax-gen-eq27 domain. Its explicit high-count nontermination limitation must not be lost. No repeat. |
| TxHdxDataV27 | Extensions supply the selected callback budget. Existing V27 count/transition controls remain closed; call proximity does not establish a new formal type. |
| TxHdxSCR1V27 | Selected budget extension, not an independently missing source statement. No width sweep. |
| TxHdxEQCondV27 | Count/member reload, rate reread, output walk and before-call lifetime crosses are recorded in fax-eqcond-reload-domain.md. No repeat. |
| TxHdxAltV27 | Selected budget extension; no new producer/callee boundary evidence. |
| TxHdxQuietV27 | Selected budget extension; no new producer/callee boundary evidence. |
| DemodDataV29 | AGC argument at a600d retains the unsigned count; candidate sign extension prepares separate signed uses. Closed AGC/MRF domains remain distinct. |
| _tx_data_state | Candidate CWTL is before the conditional jump; FIFO_read's own count is a separate fresh MOVZWL in both bodies. The window crosses a block boundary. |
| DemodDataB103 | Original 8fbe7 already zero-extends the MRF result before AGC; it is outside the eight-instruction window. No missing narrowing. |
| DemodDataV22 | Extra signed count preparation near AGC and FSE; not evidence of a different imported AGC prototype. A fresh source claim needs the full producer/consumer contract. |
| V22_FSE_receive | Extra extension serves updated history index; memcpy's size is separately doubled and stored. No memcpy prototype hypothesis. |
| V22_MRF_filter | Extra extensions serve other history/count carriers; both memcpy sizes already follow their separate doubled count. No memcpy prototype hypothesis. |
| setInitialPhase | Window crosses the negated-short cold arm and jump before the diagnostic block. The nominated conversions do not supply the displayed argument. |
| VPcmV34InitiateRetrain | Original 69a0 MOVZBL from a stack byte versus candidate MOVZBL AL: both normalize the byte requestedDp. The following edprintf nomination crosses another call. No wider formal witness. |
| V90AutoDigitalImpDetector::updateAltRbsPhaseInDil | Nominated MOVZWL follows a loop increment and jumps back before the debug block; not a diagnostic argument conversion. |
| V90ConstellationDesigner::determineDminForRrn | Nominated byte normalization publishes a byte print argument; the original cold block is laid out differently. No type information beyond the existing byte value. |
| displaySpectralParams | New 24-cell result/cache-width × evaluation-boundary × fractional-mode cross: no exact gain. See spectral-sign-carrier-results.md. |
| V90TRN2Designer::V90TRN2Design | Byte extensions form table indices. Both calls ultimately receive the same signed-word sample absolute value; the debug nomination crosses a return/cold-loop block. No compander prototype mismatch. |
| vpcm_create | Extension forms the mute predicate from qcFlags, not modem_get_param's argument. Original shift/mask/SBB and candidate TESTB/SETNE differ in lowering; current source already states the original bit test. No unique alternate preimage. |
| voice_online | Extension follows an output-clear loop increment, then a jump before the diagnostic block. Existing voice-online carrier and eager-predicate domains are closed. |

Refer to the original object for all addresses in the table.

## Fresh measurements and widening

Replaying the argument screen on current objects produces **36 bodies /53
calls**. The three removed nominations are scalar Descrambler, v23_process
and V34GiveINFO1aBits; no new body enters. FPM_FSE_receive's remaining nomination
is its diagnostic window, not the corrected paired LMS error boundary.

The new `tools/call_result_boundary_screen.py` instead examines fresh EAX
results after equal ordered named-call sequences. It stops at a call/control
transfer or an intervening explicit EAX/AX/AL/AH write. This is a bounded
straight-line nomination screen, not full def/use or ABI inference. It excludes
unequal call sequences and cannot certify branches, inlined boundaries or
conversions outside its window.

At both six- and twelve-instruction bounds it examines **300 objects, 1,934
emitted bodies, 1,886 shared bodies, 1,744 equal call sequences and 4,671 calls**:
**zero nominated bodies /zero calls**. It reports 142 unequal-sequence and 48
unmatched bodies separately. The historical pre-fix getV90Decision detects
its Descrambler return extension; original identity and an intervening EAX
write are negative controls. This is a measured null, not an untested detector.

```
python3 tools/narrow_call_boundary_screen.py --output build/narrow-call-boundary-screen-current.json
python3 tools/call_result_boundary_screen.py
python3 tools/call_result_boundary_screen.py --window 12 --output build/call-result-boundary-screen-12.json
```

The supported queue from this screen is exhausted. No source is changed in
this pass. New work requires a different independent witness: an actual
caller/callee width contradiction, a value loaded on the wrong side of an
opaque call/store, or a demonstrated earlier compiler-stage difference.
Neither this finite null nor the rejected local domains establishes a global
byte-exactness ceiling or proves every residual is register allocation.

## Later partial-adoption review

F11987–F11988 supersede the no-adoption decision for the three explicitly
reviewed boundaries in v23FP_rx_progress, RxNextStateV21 and HDLC unframe.
The historical finite measurements and this ledger's e8393850 screening result
remain valid. This is a separate partial-fidelity review, not a new exact win
or a reopening of neighbouring source/profile domains.
See [current controls and validation](../compiler/partial-boundary-recoveries-results.md).
