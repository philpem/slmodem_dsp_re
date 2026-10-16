# Byte-exact unblocking review

Reviewed at `426814ea`, 16 October 2026. All 300 live period-object hashes
match `build/recovery-packers-next/object-audit.json`. The recorded baseline
remains 1,091/1,852 exact functions; the preceding 120-cell packer campaign
gained none. This review changes no reconstruction source or comparator and
runs no new differential gate. The experiments below are proposed.

## 1. Complete the RxNextStateV17 table proof

This 730-byte function is UNRESOLVED because its anonymous jump-table target
identity has not been established by the bounded prover. Inspection of both
objects identifies `SETE` as the instruction outside the pushed-frame
checker's accepted set. An in-memory diagnostic admitting that instruction
lets the existing proof succeed on both objects: seven ordered destinations
at function offsets 106, 151, 227, 279, 320, 385 and 437; 168 reachable
instructions, eight returns, eleven direct ABI calls and an eight-byte frame.

Implement a narrow SETcc destination classifier in
`tools/toolchain/jumptable.py`. Prove its register/byte-memory effects and
reject writes corrupting saved registers, the return address or tracked
stack state. Preserve existing bounds, incoming-edge, instruction-boundary,
table-extent and ordered-target checks. Add real positive controls and
negative controls for unsafe destinations, altered table targets, bypassed
guards and unsupported encodings. Exercise the shared body/verdict path.

Regrade the entire unchanged object set. Credit any resulting gain as a
completed identity proof for existing emitted code, separately from source
recovery. The diagnostic is promising evidence, not an adopted EXACT verdict.

Next inspect `V90ConstellationPower::getConstellationInfo` (227 bytes): its
proof needs a flag/index-preserving MOV between the comparison and branch,
and a push-only ABI frame. Declare each supported form and falsifier before
extending the prover. Keep the other unresolved functions separate.
Two subsequent targets share a larger missing proof: `V32LocLoopNextState`
(794 bytes) saves EBX/ESI in a 28-byte fixed frame and contains calls and
data relocations; `V90Resampler::setBllState` (693 bytes) uses a 12-byte fixed
frame plus a nonadjacent taken-edge guard. Their 34 and 16 ordered table
destinations respectively match on inspection. Establish save/restore,
outgoing-argument disjointness, relocation-field ownership and guard
dominance before accepting either. Equal target lists alone are insufficient.
`getSegmentPointer` already has a matching immutable-pool/read-extent audit;
that is insufficient to prove canonical destination identity. Do not mask
ordinary anonymous data relocations to obtain a gain.

## 2. Recover Tone IIR interval ownership

The strongest new source witness crosses a producer and consumer:

* Original `cadence_create` loads `buflen` at `0x7d767` and stores all 32 bits
  to configuration offset `+0x10` at `0x7d77a`.
* Original `toneiir_progress` compares that full word at `0x7c7f1`.
* Current `include/dsplib/toneiir.h` splits it into `short interval` and
  `short pad12`; `Cadence.c` explicitly casts the producer to short.
* The constructor's short load at `0x7c3cc` feeds
  `GetFP_Value(short, short)`. A narrowed use does not establish narrow storage.

There is a separate arithmetic witness: original `0x7c820` uses SAR 2,
while the current signed `/4` includes a correction for negative values.

Use the previously measured four-stage/short-counter source as one fixed
structural seed. Cross seed versus current structure, full-width interval
versus current field, and arithmetic shift versus division: eight declared
Tone IIR cells, including unchanged reproduction. This reuses a closed
control only to test interaction with new evidence; it does not repeat its
spelling search. Compile every header consumer for the field change and
audit `cadence_create` as a second potential beneficiary. Preserve the
short-argument boundary at GetFP_Value and all configuration offsets/extents.

Run `t_toneiir`, `t_cadence` and affected callers under the period compiler.
Audit constructor-created coverage before adding wide-interval cases. If
negative envelope values cannot be reached by a justified lifecycle, label
any planted negative-value probe exploratory. Do not change the source to
appease an impossible fixture or infer reachable behaviour from SAR alone.

The old expansion/counter domain reached 1,126 versus 1,141 bytes without
identity. Its sibling shift-capture control reached 799 versus 802 bytes.
Both remain closed except for the explicitly stated new interaction above;
see `batch20-toneiir-section-controls.md` and
`batch20-toneiir-shift-capture.md`. No exact gain is promised.

## 3. Recover CID circular-filter wrap ownership

`CID_FSD_demodulate` has two independent terminal-cursor witnesses: original
`0x9234b` derives correlation wrap with LEA 0xa from the first loop's final
cursor; `0x923d8` derives low-pass wrap with LEA 0x22. The old pointer controls
instead reset to `cid->ac_hist + 4` and `cid->lpf_hist + 16`.

On the archived duplicated-rate-arm/pointer seed, cross terminal-derived
wrap ownership independently for the two filters: four cells, plus unchanged
current-TU reproduction. Preserve coefficients, integer counters, rate
factoring and read order. Use valid one-past/predecrement cursors where
needed; a machine address before an array does not justify undefined C
pointer arithmetic. Run `t_cid_fsd` and affected CID lifecycle tests.

The old factoring/reset-pointer domain reached 1,012 versus 1,049 bytes and
is closed (`batch50-service-cid-fsd-factoring.md`). The new discriminator is
which value produces the wrap pointer, not pointer syntax or size distance.

## 4. Expand call-boundary coverage after these bounded targets

`recovery_return_mode_screen.py` calls `first_return_use` without supplying
its supported `direct_edges` argument. Production branch traversal therefore
refuses. The archived 4169cb3b screen proves extension modes for only 147 of
4,703 aligned calls, refusing 4,556. Its overlapping refusal counts include
991 branch-ownership, 1,826 memory-write and 1,674 other-carrier-use cases.
These are historical measurements, not newly counted opportunities.

First retain a per-call refusal ledger: the current output preserves aggregate
reasons and nominations but discards detailed rejected pairs. Then prove
relocation-free in-function branch destinations and add bounded return-value
tracking through registers/private stack slots. Keep aliases, clobbers,
indirect targets and loops explicit. Match repeated callees by CFG and
argument ownership, not physical order: `receiver`'s two V34demodulate calls
already demonstrated reversed layout. Add positive and refusal controls for
each capability and report newly covered calls separately from nominations.

## Acceptance and stopping rules

Keep the complete Gentoo profile and bug-reproduction define. Reproduce each
unchanged complete TU, review all bodies/data/bindings/relocations, and keep
the complete existing exact set. Run focused period differentials followed
by `make phase` for retained changes. Broader diagnostic controls may lose
matches, but production adoption must account for every loss. The strict
whole-object gate remains a separate, failing obligation.

Stop a finite domain after its declared controls unless a new independent
operand, owner or lifetime witness identifies another test. CP/MP/DIL/FixedRC
spill and scheduling work remains deferred under the closures in
`recovery-packers-next-results.md`; near-sized bodies alone are insufficient
grounds for another campaign.
