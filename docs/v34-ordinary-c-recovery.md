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
The native sample loop carries its fill target across some paths and reloads
it at specific joins; the ordinary C currently reloads it at the loop test.
This is a remaining value-lifetime/code-generation question, with no measured
differential failure in the tested input domain.
