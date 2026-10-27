# Waves 40–41: zero-result provenance and status prerequisites

Base: `7e9126268c7a1485fda6a8b11d3347def60f5ebf` (PR #416). Issue
[#409](https://github.com/philpem/slmodem_dsp_re/issues/409) records the bounded
compiler trace, apparatus retry and two-cell source contract before execution.
GCC analysis used non-Astra agents; no Astra invocation was needed.

## Wave 40: source values are inputs to layout

Native DFE failures at `0x73a0`, `0x73ae` and `0x7505` join explicit zero at
`0x72cb`. The previous candidate instead reaches an epilogue with its entry-zero
value, already before block reordering. This is a value/join boundary, not merely
a different switch-case arrangement.

### Compiler mechanism and controlled replay

The recovered Gentoo recipe at toolchain revision
`c0e667e8b6d47783d7512e29fe19979781e33e0a` matches the production image:
47 patches applied, zero skipped. The unchanged candidate's 60 preprocessor
inputs and complete raw object reproduce production. Relevant GCC code includes
`rest_of_handle_reorder_blocks`, cleanup/reorder after `reg_to_stack`, and
`connect_traces` in `bb-reorder.c`. Trace selection prefers probability, then
predecessor trace length, then first encounter on a complete tie. Probabilities
are compiler heuristics, not measured modem frequencies.

One completed diagnostic compiler build and one unchanged full-TU replay
reproduce the production raw object. First-reorder equal-ranked epilogue
predecessors arrive `68,67,44`; zero tail `68` wins. Second-reorder predecessors
arrive `69,56,4,32`; shared capture `69` wins. The four-block DFE trace then beats
shorter equal-probability traces. Thirty-nine predecessor rows and six controls
verify the rule. These observations describe the unchanged candidate, not
unavailable original RTL or a uniquely recovered profile.

The first configure attempt failed before C compilation on period Bash temporary
handling in the read-only setup. One declared retry maps container `/tmp` to a
new approved host scratch directory; host `/tmp` is unused. The invalid first
attempt is retained and excluded. There is no further diagnostic build/replay.

### Retained two-line partial

The native zero-definition witness justifies one alternative: add
`else { n = 0; }` after the DFE getter conditions, keeping other source and flags
fixed. **Two full-TU objects / six verdicts**, zero rejections; unchanged control
is raw-identical to production. The alternative zero survives and merges with
selector 8's zero in `.27.flow2`. Final failure branches `0x550`, `0x55e` and
`0x6b5` join clear `0x47b`, recovering the native five predecessor classes.
Final selector order becomes native `5,7,8,0,1,2,3,4,6` after `.34.stack`.

Retention rests on that independent value boundary, not a desired order or size.
The alternative has ordinary RTL logs, not instrumented predecessor traversal;
baseline tie observations must not be attributed to it. Calibration passes
**27/27 value/graph controls**. Ledgers retain 27 selector rows, 63 guard-path
rows/189 static routes and 24 DFE failure stage-edge rows, with zero unsupported
or ambiguous. These are static routes, not feasible histories or equivalence.
All prior local boundary recoveries remain intact.

Only visual diagnostics changes: **1006→1007 bytes**, versus native **1023**,
still non-exact. SNR remains exact at 99; diagnostics remains unchanged at 823. All
three final production canonical bodies match the reviewed candidate. The 25
symbol identities/bindings remain, although two undefined entries exchange
ordinal positions. Both objects have 40 relocation records; 19 ordinal records
change position/addend descriptions. All nine jump-table addends change; other
allocated non-text payloads remain unchanged. This is not raw ledger identity.

Whole exact/grade-0-or-1 gain/loss sets are empty; grade-0-or-1 stays **1197/1852**.
Partial-link comparison remains **DIFFERENT**, raw-different: positioned matches
increase **76774→76779 / 943398**; candidate text stays 711415 bytes. Counts remain
18301 candidate relocations and 1160/18317 exact-position matches, not identity.
Focused period passes **1/0**; final `make phase J=8` passes **394/0** and all
structural checks. Independent review checked the native/RTL/control claims;
parent recounted symbol/relocation/data collateral and final production bodies.

## Wave 41: dependencies are not yet an event history

Four audited status writers (`0xc30d`, `0xc346`, `0xcf54`, `0xcf85`) depend on
seven guards and nine producers. Keep owner `O+0` status separate from engine
`E=O+4` progress, session type and receiver selectors at `O+0x24c/+0x250`.

- TX states 20/21/64/68 can produce progress 2; SSEG/SBAR paths are role-dependent.
  Progress 1 needs matching silence, INFO completion/detection and RX conditions.
  Shared tails may overwrite progress. Consecutive status-2/status-3 stores under
  both receiver predicates are static paths, not validated simultaneous states.
- Delayed phase 2 needs prior progress and counters beyond 95 and 576. INFO
  acceptance requires recovered bits, message length+16, matching CRC and matched
  transmitter/receiver states. Those predicates do not supply a valid transcript.
- Exported retrain 56 can enable K56 under config bit `0x10`, but clears live
  status and does not negotiate. Selector 8 still requires status 3/caller role and
  a call-target observer for its zero-return callee.

Checks pass **10/10 selected captures, 110/110 anchors, 8/8 call relocations, 2/2
static verifier controls**. No new runtime probe, maintained fixture call or
assertion is added. This is a precise dependency blocker, not an impossibility
proof or a claim that the missing visual paths were reached.

## Recommended continuation and evidence

1. Build one minimal valid phase-2 INFO event transcript at a named component
   boundary, including length/CRC and matched progression. Stop if it requires
   planted internal states. Observe selector-8 call reach separately from output.
2. Apply the zero-definition/join discriminator to a bounded fresh residue
   screen. Each alternative needs its own native value witness; do not continue
   case permutations or fit registers/scratch to the remaining 16-byte gap here.
3. Keep INFO0 alias-declaration and completed option/spelling families parked
   pending independent evidence. These packets do not justify another Astra call.

Evidence under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/` includes
`w40-layout/REPORT.md`, compiler provenance/tie ledgers, `dfe-zero/REPORT.md` and
full-TU evidence, `w40-layout/parent-verification-final.log`, partial comparisons,
final census, `w41-status/REPORT.md` and `w40-41-phase.log`. The first parent
verification log records an overly strict symbol-order assumption; the final
check reports the reordered entries explicitly while preserving identity.
