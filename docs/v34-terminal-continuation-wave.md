# V.34 terminal-continuation boundary experiment

The eight-cell experiment establishes that ordinary placement of the existing
transmit-dispatch body restores the root's third indirect jump. Merging a
bounded set of terminal exits alone does not. It does not recover the complete
native root or establish the original helper interfaces. No reconstruction
source change is retained.

## Native witness and bounded question

The baseline is `4317aa3edb4739bb9d1695330818533cacc94f19`. Its complete
`V34hshak.c` object reproduces raw through the experimental compile path under
Gentoo GCC 3.4.2-r2 and the retained flags, including bug reproduction. The
compiler image, cc1 hash, selected assembler version, source/header hashes,
actual commands and emitted-object hashes are recorded with the artifacts.

A fresh relocation-preserving disassembly of `v34handshak` confirms:

* `0x62af1` sign-extends CX into EAX to form the table2 index. The actual
  indirect jump is at `0x62b00`, through `.rodata+0x2ee8`.
* The common completion begins at `0x62a40`.
* The per-sample table1 loop rejoins at `0x629e7`; its control flow must remain
  separate from the once-per-block completion.

The earlier [body-placement audit](v34-handshake-body-placement-audit.md)
retains the full-closure macro and shared-dispatch controls. This experiment
uses a smaller ordinary-source boundary, with no macro expansion, forced
inlining, compiler-option change or register carrier.

## Components and domain

A merges five root-local `t3c_txblock` exits into the existing
`micro_txblock` continuation. Each loads `obj->txstate` where the old shim
would have loaded it, then jumps to the root continuation.

B changes three private leaf helpers for microstates 62, 79 and 80 to return
the short transmit-state value. Their only caller is the root, which consumes
that value and joins the continuation. The eight removed dispatch sites are
terminal; the old void return follows the dispatch or the function ends.
There are no direct fixture callers or exported ABI changes. All original
calls, stores, diagnostics and state reloads precede the returned value.
This transport interface is a source hypothesis, not a recovered original
signature.

C places the existing ordinary table2 body at the root continuation. A single
include body serves the private helper facade and the root; its early returns
terminate the root only after the original completion. The root's frame and
the separate short transmit state stay live through the dispatch. This tests
body placement independently of A and B. It is an experiment, not an approved
production include structure.

The original four-cell A/B cross and a second A/B cross with C fixed were
compiled. The generator checks the exact replacement counts and caller closure.
All eight generated sources are distinct; the table1 source prefix is unchanged
in all eight, including under `python3 -O`.

## Measured complete-TU results

| Cell | Root bytes | Root calls | Root indirect jumps | Changed surviving bodies | Emitted functions |
| --- | ---: | ---: | ---: | ---: | ---: |
| Native root | 61,541 | 394 | 3 | — | — |
| Baseline | 12,867 | 113 | 2 | 0 | 53 |
| A | 12,516 | 113 | 2 | 1 | 53 |
| B | 12,806 | 117 | 2 | 18 | 52 |
| AB | 13,048 | 117 | 2 | 9 | 52 |
| C | 13,030 | 113 | 3 | 16 | 53 |
| AC | 13,055 | 113 | 3 | 16 | 53 |
| BC | 12,570 | 111 | 3 | 21 | 53 |
| ABC | 13,171 | 117 | 3 | 20 | 52 |

Every cell preserves all nine EXACT bodies among 29 native-common functions,
with zero EXACT or REGALLOC gains or losses. Strong exports and bindings are
unchanged. All surviving bodies, extra/missing helpers, data and relocation
consumers are recorded, including changes that leave their grade unchanged.

B removes both emitted Modem-on-Hold helpers but newly outlines
`t3m_micro49`. AB removes only one helper. Every C cell replaces the emitted
`t3m_txblock` with `t3c_txblock`; BC and ABC also emit `t3m_micro49`. The
restored root dispatch therefore moves an artificial boundary rather than
eliminating the unsupported helper closure.

Nontext equality is not claimed. A/B/AB change 65 positioned data relocation
fields; C-family cells change 280. Table addition/reordering changes positional
owner correspondence, so the complete data audit remains attached to each
cell. No table-target or control-flow equivalence is inferred from equal owners,
matching table counts or a smaller length gap.

## Differential and apparatus controls

All eight cells pass three existing period fixtures:

| Fixture | Checks per cell | Boundary |
| --- | ---: | --- |
| `t_v34hst3core` | 6,253 | Synthetic/component shared-arm and microstate 62/79/80 cases |
| `t_v34hsrxch` | 16,060 | Component receiver-dispatch/guard cases |
| `t_v34hshak` | 14,368,945 | Existing handshake component probes |
| Total | 14,391,258 | Three fixtures per cell |

The 24 initial fixture executions pass. Their combined recorded wall time is
61.258 seconds; compile CPU time was not captured. The ledger independently
reruns the same 24 binaries, with the same per-cell checks. These are repeated
measurements of the same fixture domains, not additional independent input
coverage or public modem-lifecycle evidence.

Each selected period object is hashed before and after the gate and is unchanged.
The ledger runner also checks every nonrelocated literal byte of the linked
root against its selected object before executing a binary. It explicitly
excludes and counts relocation fields; complete linked-body evidence remains
unset. A corrupted linked root is rejected before execution under normal Python
and `python3 -O` (2/2 controls).

One deliberately wrong terminal-state control replaces the eight B leaf-state
returns with zero. The period `t_v34hst3core` run fails 88 of 6,297 checks,
including the microstate-79 progress/result assertions. This is 1/1 selected
negative controls, no exclusions. The failure's different denominator is
preserved; it is not relabelled as a 6,253-check run. No mutation campaign or
snapshot refresh was run.

The initial reporting-key error and an unavailable in-container `sha256sum`
identity attempt are preserved as invalid apparatus attempts. The valid reruns
use the corrected report key and hash the compiler bytes on the host.

## Decision and reopening condition

No source candidate is adopted. A native shared table motivates the boundary
experiment but does not establish a particular return-value transport or helper
partition. The restored third jump still has unsupported surrounding helper
boundaries and no independently verified native-complete graph. Neither root
length nor the absence of exact losses supplies that missing evidence.

This finite domain is closed. Reopen it when an independently established
region live-in/live-out correspondence identifies which remaining helper
boundaries belong inside the root, including diagnostic and receiver-setup
continuations. The next discriminator must account for the whole affected
closure and any newly outlined helper; another return-spelling palette is not
justified by this result.

## Artifacts and replay

Artifacts are retained at the investigation's compatibility path,
`build/six-step-wave/continuation-dispatch/`, with native disassembly and
compiler identity in its parent directory. Two immutable records under
`build/six-step-wave/ledger/` are named `v34-terminal-join` and
`v34-terminal-join-fixed-dispatch`. Both records have no freshness errors.
They import hashed compiled artifacts and preserve actual compile commands;
that import does not independently establish compilation provenance.
The worker Git checkout was removed after integration. The consolidated wave's
`worker-cleanup.json` maps the retained path to `worker-evidence/v34/`, preserving
recorded input paths. Parent freshness queries still pass after this cleanup.

Compile a fresh matrix with an explicit baseline and reference:

```sh
python3 tools/recovery_v34_continuation.py \
  --out build/v34-terminal-continuation-fresh \
  --baseline-dir /absolute/path/to/period-comparison-objects \
  --reference /absolute/path/to/dsplibs.o
```

The baseline directory must contain `.build-config` and the complete baseline
`src_pump_v34_V34hshak.c.o`. The unchanged compile must reproduce that object
raw before any candidate is interpreted. The retained artifact
`build/six-step-wave/run_period_cells.py` records the initial period commands,
object copies and fixture selection. After those binaries exist, the tool's
`--run-fixtures LABEL --result FRESH_JSON` reruns and checks their bounded
provenance, and `--ledger FRESH_DIRECTORY` records both four-cell crosses.

Only this report and the experiment tool are retained. Reconstruction source,
headers, compiler options, fixtures, mutation anchors and ratchets are unchanged.
