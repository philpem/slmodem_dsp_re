# V.34 handshake recovery as ordinary C

The replacement for PR300 starts from master `28ccee066`. Its objective is
maintainable C translation units whose Gentoo GCC 3.4.2-r2 output approaches
the reference object, including the final partial link.

## Retained evidence and rejected implementation

PR300 (`cf17e289a`) and its uncommitted T72 checkpoint remain preserved in
the `ghidra-assisted` worktree. The staged and unstaged patches, source pin,
and hashes of the experiment records are archived locally under
`build/v34-ordinary-checkpoint/` in the main checkout. The replacement does
not incorporate PR300's continuation macros, extracted body includes, or
mutation-source resolver.

The experiments provide useful observations: shared native dispatch targets,
call and reload ordering, the T72 measurement/reinitialisation sequence, and
compiler inlining collateral. Their passing component fixtures do not prove
the inferred original source architecture. Their unchanged exact-function
count and declining positioned partial-link matches are retained as limits.
The body-length gap reported as `SIZE` is not a count of differing bytes.

Master already contained a shared implementation header and test-facing arm
helpers. Those boundaries are also reconstruction hypotheses. An absent
out-of-line symbol can reflect compiler inlining; it does not establish
whether the original author wrote a helper.

## Recovery and comparison boundaries

The source recovery uses ordinary switch cases, scoped locals and shared
joins. Tests exercise the compiled handshake entry rather than requiring
production code to expose the historical per-arm decomposition. Seeded
component states remain labelled as such.

At native addresses `0x62a40..0x62ad8`, the status tail consumes the cached
transmit state in ECX. The mode-1 path explicitly reloads it at `0x62b5f`.
Receiver exits reload it before the common transmit dispatch, including
`0x62aea` and `0x62b8a`. These distinctions constrain local-value lifetimes
in the recovered C.

Compiler investigations use the recovered flags and Gentoo compiler, saved
preprocessing and assembly, and supported callgraph/RTL dumps. Complete-TU
comparisons include non-exact functions, data, symbols and relocations.
Partial-link comparisons retain the ordered input manifest. Function
identity, body-length differences and link-layout differences are reported
separately.

## Baseline

A fresh `make tc-repro J=8` in the isolated `v34-ordinary-baseline` worktree
reproduced all 300 archived master objects byte-for-byte: 300 compared,
zero missing and zero different. The archive is the pre-PR300 input set in
`ghidra-assisted/build/v34-continuation-partial/before/`.
The fresh baseline comparison records are under that worktree's `build/`.

## Ordinary-C checkpoint

The candidate places the nineteen sample-transmit arms and nine microstate
arms in ordinary switches, removes `t3m_frame`, and makes transmit dispatch
and the common status tail caller-owned joins. RX_L1 is a labelled receiver
arm. PPSEG and TXMD share their segment-counter join. The obsolete shared
implementation header and arm-exit enum header are removed. PR295's normal
mode INFO0 carrier guard is retained.

The complete Gentoo build contains 300 objects; only `V34hshak.c` differs
from the fresh master baseline. All 300 runtime source objects match their
canonical counterparts. The V34 object changes from 53 emitted functions to
40, retaining all nine exact native-common functions and the same strong
bindings and canonical nontext data. The whole-tree exact set remains
1,109/1,852, with no gained or lost names.

The root changes from 12,867 to 34,029 bytes (native: 61,541). Non-exact
bystanders also change, including `V34SetupDemodulator`, `getbit`,
`probe_preemp_shape` and `v34handshakinit`; the full body/relocation audit is
recorded in `build/ordinary-tu-audit.json`. This checkpoint does not claim
instruction identity for the root or unchanged code generation for the TU's
non-exact bystanders.

| Partial-link measurement | Master | Ordinary C |
| --- | ---: | ---: |
| Positioned equal bytes / 943,398 | 72,202 | 71,554 |
| Exact relocations / 18,317 | 1,002 | 995 |
| Exact symbols / 2,931 | 457 | 457 |
| Candidate `.text` bytes (native 728,304) | 712,329 | 703,769 |

Both strict partial-link comparisons remain different. The recovered source
structure is the progress in this checkpoint; the layout shifts are not
reported as a whole-object identity gain.

The full period tier passes 393 fixtures with zero failures. The adapted
transmit fixture compares compiled `v34handshak` against the blob and retains
explicit queue-progress, state-transition and one-shot-clear checks. It runs
26,368 checks, including its Table 17 controls. A no-op entry negative control
fails 12/108 checks, including the transmit cursor (0 versus reference 16).
Some failures also involve initial state and pointer observations, so this
control establishes detector activity without attributing every failure to
transmit dispatch.

The initial structural run fails on obsolete mutation anchors after source
movement. The separately requested mutation-system removal supersedes their
maintenance; the original failed log is retained. Validation after that
integration passes: after updating to master `8e5325f81` (PR301),
`make phase J=8` reports 393 period fixtures passed, zero failed, and all
structural checks passed. The final log is retained at
`scratch/ordinary-phase-after-removal.log`.

## Remaining source questions

Saved Gentoo callgraph/RTL diagnostics show `t72_ladder` rejected for inlining
because its inline limit is reached. The retained T41, T44, T46, RECEIVE,
DET_AB and MOH helpers likewise require further source-boundary investigation.
The sample-loop fill-target review narrows the earlier lifetime question.
The reference captures the target at `0x62925`, then reloads it at five active
arm rejoins: `0x629cf`, `0x62d70`, `0x63948`, `0x640a1` and `0x64326`.
The retained-DX path at `0x629e0` is reached by range rejection and 57 of the
82 table entries, all no-work defaults. All 25 active table entries reach
reload paths. The other two dispatch tables have no targets at these loop
tests. This is a static control-flow finding, not a runtime reachability claim;
no progressing-arm snapshot discrepancy was established.

One isolated initial-if plus do/while trial, built with the same Gentoo flags,
still reloaded on the unsupported-state back edge. It changed only the root
among 40 TU bodies, preserving all nine reference-exact functions, but did not
recover the reference path. The trial was rejected. The ordinary while remains;
only its misleading reload comment was corrected. The baseline diagnostic
object matched the canonical object byte-for-byte. Evidence and compiler dumps
are retained in `scratch/fill-target/`.

## T72 declaration trial

The existing counter ladder, paired bank updates, parameterized measurement
and shared completion sequence remain coherent source units. A single ordinary
`static inline` declaration was tested for `t72_ladder`, with no attributes,
parameter changes or compiler-flag changes. Gentoo GCC considered the ladder
earlier, at 221 estimated instructions, and inlined it; the baseline considers
it later at 330 instructions and rejects it at its inline limit. This measures
the compiler's decision, not the original author's declaration.

The trial removes the ladder's 1,096-byte local body and its root call, but
emits `t41_tone_ab` (790 bytes) and `t44_det_info_restart` (901 bytes), neither
of which is an out-of-line reference boundary. The TU grows from 40 to 41
emitted functions. Besides the root, `t41_info_marks` and
`t44_det_info_accept` change instructions. All nine reference-exact names
survive, but the extra call boundaries prevent treating this as an overall
recovery. The declaration was rejected. Its root-size reduction, from 34,029
to 33,534 bytes, is not an identity gain.

Both rejected trials remain under `scratch/fill-target/` and
`scratch/t72-inline/`, with complete-TU audits, saved compiler dumps and exact
reproduction scripts. Neither requires new tests or production machinery.

## Review of unchanged-source bystanders

Relocation-normalized comparisons separate moved addresses from changed
instructions. `ApplyBulkDelay`, `probeselect`, `settxlevel`, `tx1_moh_hold`,
`t4_receive_body` and `v34FreezeEcho` retain their canonical instruction bytes;
their changed raw body records reflect relocation/layout effects.
The four following unchanged-source functions have real instruction changes:

| Function | Master bytes | PR303 bytes | Observed differences |
| --- | ---: | ---: | --- |
| `V34SetupDemodulator` | 788 | 795 | Register allocation, constant materialization and reordered independent baud-field stores |
| `getbit` | 447 | 451 | Register allocation and counter/accumulator load-store scheduling |
| `probe_preemp_shape` | 1,273 | 1,273 | Integer register allocation and scheduling; all 171 x87 instructions, operands and relocation text retain their order |
| `v34handshakinit` | 2,618 | 2,617 | Register allocation and initialization-store scheduling |

The inspected paths retain their field offsets, constants and predicates.
This is a bounded instruction review, not a proof of equivalence or a causal
explanation of GCC's decisions. A `SIZE` verdict measures a body-length gap;
it never counts differing instruction bytes. Disassembly and the review are
retained in `scratch/collateral-review/`.

`test/unit/t_v34rx.c` directly exercises demodulator setup;
`test/unit/t_v34hshak.c` exercises `getbit` and initialization, with additional
initialization coverage in call, retrain and PCM fixtures.
`probe_preemp_shape` is an optional repaired preemphasis selector, not an
original reference function. Under `DSPLIB_REPRODUCE_BUGS`,
`dsplib_v34_blob_preemp` defaults to one and bypasses it; no test override was
found. The period result therefore does not establish coverage of that
optional repaired path.

## Review disposition

Keep PR303 in draft. It removes test-facing production-body sharing and
explicit continuation state, but the current checkpoint has no demonstrated
whole-object identity gain. The two bounded follow-ups do not justify further
executable edits: the proposed fill-target snapshot is unsupported on active
paths, and the T72 annotation displaces other inlining decisions.

The next source revision needs a coherent explanation of the remaining helper
boundaries across the handshake, supported by the reference control flow and
complete-TU compiler output. Repeated local inline annotations or mechanical
body expansion would not settle that question. The findings above narrow the
remaining work without claiming a global limit on reconstruction accuracy.

Final review validation rebuilt the canonical objects and ran `make phase J=8`:
393 period fixtures passed, zero failed, and all structural checks passed.
All 300 runtime source objects match their canonical counterparts. The V34
object and final partial-link SHA-256 are unchanged from the initial PR303
checkpoint; the exact symbol set remains 1,109/1,852. Final measurements and
logs are in `scratch/pr303-review/`. Rejected variants were not installed in
production and were not submitted as passing differential candidates.
