# Issue #20: FDSP and session-flag ownership

Baseline: `01eb6a2a`. This batch restores translation-unit ownership and
method placement without changing algorithm statements or compiler flags.

## Ownership evidence and moves

- `Fdsp.c` owns the reference LOCAL `pGlobalFDSPObj` and
  `uCorrelationReportsNo`. Their reference relocations come from
  `FDSP_DP_Delete` and `FDSP_DP_Create`; these two functions now share that
  file, in reference order. Kernel helpers remain in `Fdspkrnl.c`.
- `TONE.c` owns the 212-byte floating-point `ToneLPF`, its configuration,
  and the six floating-point tone routines. The unrelated 106-byte integer
  table has the same LOCAL name in another original file. A unique-name-only
  ownership census excludes both occurrences and misses this distinction.
- The five methods formerly bundled in `V90SessionFlag.cpp` now reside in
  their corresponding modem, modulator and demodulator files. Per-class
  offset assertions move with them. `V90Modem::setSessionFlag` belongs
  between `reset` and `progress`, not before `reset`.
- The fuller `V90Phase4Demodulator` header receives the existing method's
  declaration so callers in the restored owner can see it. This does not
  change object layout or resolve the separately registered duplicate type.

## Build and measured tradeoffs

The corrected Gentoo GCC 3.4.2-r2 partial build succeeds. Initial extraction
errors were a missing and a leftover comment delimiter, plus the missing
method declaration in the fuller header. Its failed build log is preserved;
no comparison from that failed build is used as evidence.

- Exact-function set: **828 -> 829 / 1,852**, no losses. The gain is
  `V90Demodulator::setSessionFlag`; exact function bytes become **80,002**.
- Positioned matching bytes: **56,970 -> 57,193 / 943,398**.
- Exact relocation records: **991 -> 987 / 18,317**; ordered matches 978.
- Exact symbol records: **261 -> 263 / 2,907**; ordered matches 262.
- Exact section descriptors: **67/92**, unchanged; exact section contents 60.
- Shared-name binding: **2,443/2,443**, no disagreements.
- Inputs: **267 -> 268**; ordering candidates **192 -> 194**;
  unresolved inputs **75 -> 74**. Candidates are not proven original owners.
- Candidate text: **684,840 -> 684,488** bytes against 728,304 reference.
  Total content deficit increases to **45,748** bytes. NOBITS remains
  **2,812 / 2,836**. Strict partial comparison remains `DIFFERENT`.

The affected inventory preserves all 118 unique function names. Two weak
Descrambler methods already appeared in two inputs before the move; neither
is a newly introduced duplicate. Six functions change non-relocated bodies:
`EchoCanceler`, `FDSP_DP_Create`, `FDSP_Kernel_InitObj`, `FDSP_Kernel_Loop`,
and the demodulator/modem session-flag methods. `FDSP_DP_Create` shrinks
786 -> 586 bytes, the modem setter 178 -> 64, and the demodulator setter
97 -> 86. None loses a previously exact match.

Four large decision bodies retain identical raw bytes and relocation maps
but the comparator reports unresolved relocation targets. Demodulator
`progress` has identical canonical relocation maps and zero differences
outside relocated fields; its raw addend bytes change with placement.
These unresolved cases are not promoted to proven canonical body identity.

## Mutation routing and gate status

All **121 mutation cases** are preserved by unique source anchors, including
two documented equivalents. The old kernel suite splits into 29 kernel and
21 tone cases; allocation and tone creation retain 22 and 33 respectively.
The session-flag groups contain 3 phase-3, 1 phase-4, 3 modulator,
3 demodulator and 6 modem cases. Notes remain in the original fixture.

The final period differential passes **375 tests, zero failures**. The
structural mutation-anchor audit passes after the existing `v90demctor`
constructor mutation is narrowed to include the preceding constructor-only
assignment. Its mutation semantics are unchanged.

The modern mutation run records 47 caught cases and two documented
equivalents across tone creation and the five session-flag suites. The
remaining 72 FDSP cases cannot run because modern GCC removes or renames
locals named by the fixtures. No failed run is recorded as passing.

**The combined `make phase` gate is not green:** the newly split
`fdspkrnl_tone` suite has never been recorded. The snapshot reports six
current, 264 stale and one never recorded out of 271 suites. The check is
not disabled and no passing snapshot is fabricated. This is an explicitly
period-validated batch with an outstanding mutation/portability gate, not a
claim that every gate passes.

GitHub #96 records the modern failure and the unsuccessful symbol-retention
controls. Per user direction those repairs are deferred; no experimental
host flags or fixture ABI changes are retained. The period source and flags
remain the authority. Artifacts: `/tmp/issue20-fdsp-session-batch/`.

Issue #20 remains open. Remaining constructor/local-data owner conflicts,
the unresolved-input census and remaining definition-order classification
must be completed before claiming the ownership phase is ready to close.
Byte-fidelity refinement is not part of this batch.
