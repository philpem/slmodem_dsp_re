# Framed-writer and medium-loop recovery, 2026-11-05

`_send_hdlc_buffer_state` becomes EXACT282. `faxvmi_write_frame` improves
SIZE132→SIZE1 (543 versus native542), and the SIMP/ASYC packers improve
SIZE28→11 and SIZE35→9 while restoring observable cursor publication order.
The frame residual is a pre-allocation memory-availability difference; its
one-byte size distance does not establish register-only or semantic equivalence.

Starting revision: `8f8485fecb7cf1b0ed8ea14db572ec6085d2f115`.
The [evidence record](evidence/frame-loop-recovery-20261105.json) retains input,
compiler and parsed-object identities, matrices, closure, graph denominators
and gates. Full frozen sources, commands, dumps and reports are under
`/mnt/zfs/dev/slmodem/scratch/frame-loop-20261105/`; integrated outputs are in
the validated worktree's `build/frame-loop/`. The compiler remains Gentoo
GCC3.4.2-r2 with the production flags and bug reproduction enabled. No mutation
execution, anchor/snapshot maintenance or modern portability campaign ran.

## Restore the frame writer's helper graph

Native has an outer short postdecrement, reads the frame header, then acquires
the framer. It publishes each incremented write cursor before header, payload
and FCS stores. It contains both the two-nibble CRC walk and the existing FIFO
writer's partial/full/countdown/underrun graph inline. Baseline reconstructed
source calls the external CRC entry and manually copies a payload loop.

| Bounded control | Frame result |
| --- | --- |
| baseline | SIZE132 / 410 bytes |
| A: outer postdecrement | SIZE116 / 426 |
| B: post-header framer source acquisition | SIZE132 / 410 |
| AB | SIZE106 / 436 |
| Native publication/CRC controls | SIZE17 / 525 |
| Supported helper calls and shared CRC implementation | SIZE1 / 543 |

The complete 18-cell frame ledger and bounded CRC controls are retained in the
scratch report and evidence. They separate source hypotheses from diagnostic
controls. In particular, naive in-loop acquisition can still be hoisted before
the header by the compiler. Restoring source order is not proof of an identical
emitted capture graph.

The selected payload uses the existing `faxvmi_write_fifo` with ignored return,
which GCC inlines. Its partial-full exit bypasses normal underrun publication,
matching the native boundary. A guarded private `faxvmi_fcs16.h` holds one CRC
implementation for the public wrapper and frame writer. The public entry stays
in faxvmi_utls.c; original inline declaration/visibility remains non-unique.
Header/FCS writes capture old wr and publish wr+1 before the store. Normal
header placement, guards and comments preserve the selected objects literally.

The complete two-TU review includes five emitted bodies. frame_reverse,
write_fifo and byte_reverse remain literally EXACT. Public gen_fcs16 remains
SIZE1 / 107 against native108, with changed bytes; this is neither an exact gain
nor proof of the original CRC spelling. Both TUs retain exports, function order,
allocated data and data relocation identities. All five production objects for
this wave equal their corresponding frozen ordinary/dump objects.

> 🧠 **From Hindsight memory (Full residual-family screening and DIL/MP dataflow recovery)** — a prior faxvmi_gen_fcs16 investigation was recorded closed. The new native inlined consumer supplies the specific shared-helper reopening witness; it does not invalidate every old negative or reopen unrestricted CRC spelling search.

## The remaining frame byte is not an allocator diagnosis

The candidate retains its second FCS FIFO-pointer load, UID290, through
`.07.addressof`. `.08.gcse` removes it and reuses the first FCS load, UID270.
Current pointer alias-set2 and unsigned-short store alias-set3 permit that reuse.
Native reloads the pointer and cursor after the first FCS write. Other residuals
include CRC counter scheduling and prologue homes; the aggregate byte distance
does not identify their cause.

The primary Sol High investigation establishes the first relevant pass. An
informed Sol Extra High rescue independently confirms it and checks that
existing direct-postfix controls produce the same tail as old-wr temporary
controls. It adds no source control, compile or recovery. Its elapsed report is
about 3m46; token/billing measurements are unavailable. This is an informed
review, not a blind model comparison or efficiency ranking.

Native reloads do not select an original union, volatile annotation or alias
view. No such carrier is added. Close the tested CRC signedness, expression,
postfix and declaration palettes. Reopen with independent original access/alias
visibility evidence or a natural source boundary changing GCSE memory
availability under production flags. This is a bounded source-family negative,
not compiler impossibility.

## Two regressions distinguish observation ages

The frame fixture adds eight cases / 113 assertions. A declared unsigned-short
array shares a C union with a framer: rd supplies a one/two-word frame header,
and following same-type array elements overlap wr/count. Pointer arithmetic
stays inside the declared array; FIFO storage is a separate real 16-word array.
Initial rd==wr==length and occupancy0 form an empty ring. Disjoint sources and
zero requested frames provide controls.

Native CRC sees payload after header publication and before later payload
cursor/count publications. Transmission sees the updated values, so the probe
explicitly checks that earlier CRC against the later transmitted payload.
Exactly two old-source ring comparisons fail; the selected source passes all
113 assertions. Full normalised owners, outputs, input storage and guards are
checked. These are explicitly synthetic component probes, not ordinary VMI
allocation or protocol histories.

The packer fixture adds sixteen cases / 177 assertions: SIMP/ASYC × rd0/1 ×
work0/1 × disjoint/union-backed FIFO-to-rd aliases. The backing array really
accommodates capacity16. Input count0 makes both FIFO-writer calls inert; one
output consumes one sample. Native publishes rd+1 before loading FIFO[oldrd].
The old source consumes oldrd, producing eight failed assertions across four
active alias cases; the selected source passes. Controls, consumed sample,
cursor/occupancy, pointer retention, buffers/guards and complete normalised
root/link/framer storage are checked. This also establishes no ordinary protocol
history for a self-aliasing ring.

## Medium-screen gain and bounded negatives

The independent medium screen covers 300 objects / 1,886 shared defining copies:
316 native extents in 251..600 bytes, 223 inexact and 216 with backward jumps.
Eight copies receive native/source inspection by that worker; 215 remain
uninspected by it. Parent-exclusive frame work is separate. Backward cold
rejoins are not processing loops. MTK_phasor/TONE_detect source navigation is
not counted as native inspection.

The HDLC handler calls FAXVMI_process, then copies seven words of FAXVMI_STS,
then calls FAXVMI_status. Moving template assignment after process restores
EXACT282 from SIZE8 / 274. FAXVMI_STS is immutable; this is source placement,
with no demonstrated behavior defect or template-mutating fixture. All eight
TU bodies are compared; seven neighbours, including two REGALLOC bodies, stay
unchanged. Constructor-backed NULL-slot VMI coverage remains the component gate.

SIMP/ASYC old-rd capture plus preincrement restores native publication before
sample loading. Only the selected packer changes in each complete two-body TU;
exports, order and allocated data stay unchanged. Mask-width × publication
controls give SIMP28/ 28/11/11 and ASYC35/35/9/9. Both mask spellings already
expand to SI-mode locals and SHR, so DWORD machine operations do not justify a
wider source declaration. Keep only the independent publication correction.
Reopen the residuals with a new consumed-value/owner/helper witness.

TONE_generate already models native short postdecrement; dp_wrapper_run has
matching signed-positive int admission; asyc_unpack already consumes its signed
short and captures owners at native admission. No blind signedness transfer
follows. Closed TONE, IIR and V8 palettes remain closed.

## Validation and next work

Focused `t_faxunframe t_faxpack t_faxframing t_class1hdlcctl` passes4/4. The
full `make phase J=8` gate passes 399 period tests / zero failures plus
structural checks. Census: 1,142 EXACT / 28 REGALLOC / 71 BYTES / 611 SIZE out
of 1,852, one EXACT gain and no EXACT/REGALLOC losses. Across all 300 objects
only the five intended objects change, one body in each; all 17 emitted bodies
are compared with exports, order and allocated data preserved.
Graph reports retain supported-relation denominators and refusals separately:
HDLC compares six anchors/eight relations, while the frame/CRC subset and packer
subsets have zero compared relations. Clean reports there are not equivalence
proofs. Exact bytes, native witnesses and period differential remain separate.

Next audit `IsPulseDialerReady`: native unsigned elapsed/break comparisons at
0x32ef..0x32f1 and 0x3371..0x3373 conflict with current signed int comparisons.
Native also reloads elapsed after modem_set_param at 0x3331; source retains a
pre-call local. Check duration setters, callback effects, valid histories and
whole call.c/header collateral before changing types or captures. This is fresh
operand/observation evidence, not a reason to repeat old if-conversion flags.
Then prioritise uninspected medium rows with paired predicates/publications,
keeping the closed frame and packer palettes bounded as above.
