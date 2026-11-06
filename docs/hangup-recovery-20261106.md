# V.34 HangUp recovery, 6 November 2026

`VPcmV34InitiateHangUp` advances from SIZE24 (234 bytes) to EXACT258. The
census becomes 1,147 EXACT / 29 REGALLOC / 71 BYTES / 605 SIZE over 1,852
functions, with no protected loss. The
[evidence ledger](evidence/hangup-recovery-20261106.json) records the crossed
controls, frozen inputs, compiler observations and validation.

## Source properties and controls

Native captures the session at `0x6be1`, before diagnostics, after acquiring
an interior `obj+4` owner and before acquiring the receiver view. It clears
`rate_want`, reads status, then clears `rate_min` and `rate_max`. The old source
captures the session only inside the PCM arm after all three clears, uses
complete-object rate accesses, and reads status after the clears.

| Control | Result |
| --- | --- |
| Baseline | SIZE24, 234 bytes |
| Early session capture | SIZE4, 254 bytes |
| Interior rate-owner view | SIZE4, 254 bytes |
| Both | BYTES36, 258 bytes |
| Both plus native acquisition order and status capture among rate clears | EXACT258 |

The complete matrix retains the isolated and combined acquisition/status
controls. Status capture alone leaves the target body literally unchanged but
changes InitMOH; target-body identity alone does not establish an inert TU control.
Native addresses support both extra properties. Status capture is
an observation boundary across conservative same-int alias stores; it does
not prove a runtime bug for the genuine root fields, whose offsets are disjoint.
No barrier, volatile, compiler flag, artificial scratch consumer or function
order permutation was added.

The source already publishes `clr` before reading the demodulator. Baseline
machine code reverses them first at `sched2`. The selected ownership graph
restores native store-before-load order without a new ordering operation.
The byte at session+0x173e and pointer at session+0x175c are disjoint, so this
machine-age difference alone does not demonstrate different behavior.

The selected interior view starts from the complete object plus
`offsetof(v34_object, progress)`, preserving the native +4 anchor. A control
starting at the progress member produces a literally identical TU object;
the complete-object spelling keeps its byte extent explicit. Nineteen period
layout assertions check the changed field addresses and widths. Rate fields
remain at root+0x220/0x224/0x22c, session at root+0x3548, and receiver shorts
at root+0x4bc/0x4be/0x4c0. This access view does not identify a unique original
engine type.

## Collateral and full object evidence

All 56 emitted functions have native counterparts. Fifty-four bodies remain
literal matches to the baseline. HangUp becomes EXACT; unchanged-source
`VPcmV34InitMOH` changes machine code while retaining SIZE32. All exports,
bindings, symbol order and normalized non-target relocation identities remain
unchanged. All 300 objects were checked: 299 are byte-identical, and only
`VPcmV34Main.cpp` changes. The existing baud-cap REGALLOC result is preserved.

All six allocated noncode section layouts and relocation-zeroed payloads
match. Raw rodata changes: 75 of 89 relocated code words increase by 32 bytes.
All 89 retain their ordered function-relative targets at instruction boundaries.
Strings and the other allocated data remain raw-identical. There are 528
relocations at both endpoints. Raw data identity is distinct from normalized
target identity.

The selected ordinary, dump, observed, replayed and integrated objects agree at
SHA256 `212fa16d945b91b7441f1c5cf3c958b8e454d69abb5a59044e6b4ce5b07bc364`.
The target's 242 nonrelocated bytes and four canonical relocation fields match
native; this is a strict function EXACT result, not a whole-TU EXACT claim.

InitMOH normalized RTL is identical through `flow2`; its first differences
appear at `peephole2`. Both full-TU observers record 362 decisions. The 13
InitMOH events retain identical operation inputs but enter with a different
scratch-search cursor, 50 versus 3. Conditional replay accounts for all selected
states, followed by register renaming and scheduling. The selected cursor
predicts 13/13 selected states; the baseline cursor predicts 13/13 baseline
states and 0/13 selected state triples. It still matches 8/13 selected registers,
so lane identity alone would miss part of the state difference. The cursor reconverges
later and no other function body changes. This is a measured compiler-state
consequence of the recovered predecessor, not a reason to reject it or add
scratch-consuming source to fit the neighbor.

Both graph layers admit the HangUp pair, each with two matched anchors and
zero compared relations. Overall owner coverage is 49 admitted/7 refused;
stack coverage is 12 admitted/44 refused, out of 56 pairs. These bounded clean
results cannot establish whole-function equivalence or explain away source
observations. The byte oracle and period differential provide separate evidence.

## Differential domain and validation

The new fixture reuses the registered-create graph and its 126-region projection.
It compares the target on restored blob-created graphs through three histories:
create output and exported V.34 reinitialization for either role. Crossing
levels 0/1/2 and one/two requests gives 18 paired observations and 54 calls, with
524 checks. No internal state is planted. These are valid status-0 component
histories; negotiated PCM status 1/2, audio processing and host reentry remain
unproved. Existing 96 quiet and 8 diagnostic synthetic HangUp cases remain.

Mode 2 installs four of the twelve learned static aliases. The new check validates
those four and compares every other masked byte raw; existing all-twelve checks
remain intact. No new blind pointer hole or relaxed comparison is introduced.
The diagnostic host only records output and cannot replace the session.

Old and selected source both pass the expanded fixtures. `t_vpcmcreate` has six
groups/5,530 checks; `t_v34pcmif` retains 38 groups/63,365 checks. The full
`make phase J=8` passes 399 period tests with 0 failures and all structural checks.
Citation checks are rerun with the new documents staged. Mutation infrastructure
is not used.

## Next transfer

Investigate `VPcmV34InitMOH` using two new native-backed source properties,
separately from its explained scratch collateral. Native acquires `obj+4` at
`0x6cf2` and retains it across `v34handshakinit` for the progress publication
at `0x6d89`; source uses the complete-object member directly. Its message==1
arm zeroes the flag register at `0x6e3b` and joins the common `moh_path_sel`
store at `0x6d27`; source duplicates that store across both arms. Cross the
common flag continuation with the interior progress owner after auditing all
intervening configuration/byte publications, aliases and argument ages. No
follow-up experiment is claimed by this wave.
