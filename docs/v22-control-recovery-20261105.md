# V22 control recovery — 2026-11-05

`V22FP_control` advances from SIZE (+6 bytes) to REGALLOC: 145 bytes,
with 11 byte differences confined to register choices. `V22_status` stays
EXACT. Together with the V92CP recovery, the measured census is 1,133 EXACT,
28 REGALLOC, 74 BYTES and 617 SIZE out of 1,852 symbols (previously
1,132 / 27 / 74 / 619).

## What makes the gain

The native body at `.text+0x8c3b0` acquires the low request byte once for
both scrambler switches. It acquires the high byte before publishing the
scrambler result, conditionally reacquires that high byte after the RETRAIN
state stores, and otherwise reuses the old high byte. The late low-byte read
and DSP-owner reacquisition follow the HDX decisions. The parameter bit-9
assignment uses the existing typed view to recover the native byte-wide write
at root +0x11. The former comment claiming five DSP-owner reads was wrong;
the native control body has two.

The late owner read also needs the observed HDX store alias boundary. With
ordinary short members, GCC 3.4.2 removes that read in GCSE: the short stores
and DSP pointer have disjoint alias sets. A signed 16-bit field view emits the
native word accesses while retaining that read. Each conditional pair captures
one HDX owner, as the native body does. The view occupies +0x0c through +0x0f;
substate remains +0x0c, protocol +0x0e, trained +0x10, and HDX size 0x40.
The scalar members remain available in the same union. This is a measured
reconstruction of the access boundary; the original declaration spelling
has not been uniquely recovered. Signed-short and signed-int diagnostic field
views produced the same instructions. The neighboring V32 diagnostic naming
`options.retrain` supports original option bitfields, but does not identify
this V22 declaration.

The decisive fixed-context matrix varies HDX alias view and conditional
request recapture. Ordinary/no-recapture gives BYTES 54; ordinary/recapture
BYTES 50; field-view/no-recapture BYTES 72; both REGALLOC (raw BYTES 11).
These cells already contain the independently supported request captures and
parameter write. Earlier capture × parameter-write cells, capture widths,
whole control-word overlays and DSP-local placement did not yield a gain.
Broad compiler switches were diagnostic only; retained compiler flags are
unchanged. Further register spelling searches lack new native evidence.

## Full header closure and collateral

All seven header consumers were rebuilt: v22.c, V22.c, v22stc.c, v22prc.c,
V22int.c, v22mod.c and the data-only v22rxtab.c. Four of the complete tree's
300 objects change, including the separate V92CP recovery. No existing EXACT
or REGALLOC function is lost. Function order and exported bindings are checked
by the full-TU comparator.

The union broadens aliasing even for the scalar view. Eight native-supported
HDX-owner captures in v22mod.c remove extra owner reloads without extending
captures across calls. modem, connect2400 and originate return to their
baseline bodies. Five v22mod bodies still differ: ans_rmloop2 changes BYTES
7 to 18 and org_rmloop2 BYTES 90 to 101; answer, data and retrain retain their
SIZE grades with scheduling/register changes. V22_create also retains its
SIZE grade with scheduling differences. This collateral is accepted explicitly
for the recovered control access structure; the gain does not make every
neighbor closer. The final comparator confirms unchanged allocated data in all three V22
objects, unchanged masked payload/layout in V92CP, and unchanged strict targets
for all 75 allocated-data code relocations (58 V22, 17 V92CP).

The residual ans_rmloop2 ordering first changes in sched2: an alias-zero
state store gains a dependency against a spilled detected-value reload.
Native timer-before-state ordering is lost. org_rmloop2 has the analogous ACK
stack-slot dependency. Capturing the owner does not remove it. Reopen this
tradeoff if original shared declarations or an independently supported narrower
alias representation can preserve control's late owner read without these
neighbor changes. Do not add artificial register carriers or change flags.

## Differential evidence and limits

The new `t_v22ctlalias` fixture has 1,032 synthetic component cases and 7,218
checks. Its request overlaps a DSP scrambler slot in 1,024 cases and the HDX
substate word in eight cases. Constructor-owned graphs are restored before
destruction. This measures representation-level fidelity; no public modem
history is claimed for those overlaps. Existing lifecycle fixtures supply
separate coverage of normal modem operation.

Both selected executable negative controls fail (2/2, no exclusions): replacing
the function with its frozen old 151-byte body fails 1,280/7,218 checks;
removing only the four-byte conditional recapture fails 514/7,218. The linked
145-byte candidate is verified against the comparison object. These are fixture
sensitivity probes, not a mutation campaign. The selected candidate passes
all 7,218 checks under the Gentoo period compiler.

Evidence is retained under `build/v22-v92-grade/`: recapture matrix,
state-overlay comparison, final full-TU comparisons and hashes, negative
controls, comparison census, and final phase log. The independent Astra review
added the GCSE alias diagnosis, native-backed collateral captures and sched2
explanation; it did not recover a unique original declaration or an EXACT
control body.

## Integration gate

`nohup make phase J=8` completed successfully on the retained candidate:
399 period tests passed, zero failed; all structural checks passed. The previous
398-test denominator increased by the one new fixture. No mutation snapshot
was refreshed. The census and final four-object comparison use the retained
Gentoo GCC 3.4.2-r2 image; source/header/object hashes are in
`build/v22-v92-grade/final/hashes.json`.
