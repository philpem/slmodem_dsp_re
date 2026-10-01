# Small period-GCC controls for the V34 handshake

Branch: `investigate/v34-polyvalue-rtl`, based on `086ec898`.
Experiment declared before compilation in [issue #22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5928763753).
This is compiler diagnosis and object comparison. No production source was
changed; no fuzzing, mutation, or differential harness was run.

## Domain and controls

Three expressions, each cast to `short` at return:

```c
-21 * (int)k * k + 837 * k - 354
-21 * ((int)k * k) + 837 * k - 354
(int)k * (837 - 21 * (int)k) - 354
```

Cross full `V34hshak.c` / standalone `polyValue` with register renaming on/off:
12 cells, plus one repeated unchanged full-TU compile. Alternatives are
exploratory compiler inputs: reassociation is not a claim of equivalent C
signed-overflow domains, and none is adopted.

Compiler: GCC 3.4.2-r2, banner `Gentoo Linux 3.4.2-r2, ssp-3.4.1-1,
pie-8.7.6.5`; selected assembler GNU as 2.15.92.0.2 20040927.
The saved retained `.build-config` supplies all flags; the helper appends
`DSPLIB_REPRODUCE_BUGS`. The sole profile control appends
`-fno-rename-registers`. All cells request `-da`. Include paths are made
absolute because each compiler invocation runs inside its own cell directory.

The first run compiled objects but put dumps in a shared cwd. Its artifacts
remain explicitly invalid for RTL interpretation in
`build/v34-polyvalue-invalid-dump-path/`. The corrected complete rerun stores
31 dump files per cell in `build/v34-polyvalue-rtl/`.

## Measurements

| Expression | Full TU bytes, rename on/off | Blob verdict, on/off | Full vs standalone |
| --- | --- | --- | --- |
| Retained | 31 / 31 | SIZE(3) / SIZE(3) | EXACT / EXACT |
| Square first | 31 / 31 | SIZE(3) / SIZE(3) | EXACT / EXACT |
| Factored | 28 / 28 | BYTES(22) / BYTES(22) | EXACT / EXACT |

Blob `polyValue` is 28 bytes. The factored expression reaches that length
through a different computation graph; matching length is not recovery.
For retained and square-first forms the on/off function bodies are exactly
identical; the factored form changes bytes when renaming is disabled.

The repeated baseline is byte-identical across the whole object. All full-TU
cells preserve 55 global names/bindings; standalone cells preserve their one
global. The canonical `byteident.verdict` scores 29 shared full-TU functions.
All source cells have zero exact gains at their respective profiles.
Disabling renaming loses `dftRetrainDetInit` from the retained four-name exact
set, leaving three exact functions. Other nonexact bodies are recorded in the
manifest, rather than assumed unchanged from equal grade counts.

## What the RTL establishes

In retained `polyValue`, `.01.rtl` already contains a multiplication of `k*k`,
followed by shift/add synthesis of multiplication by 21, then negation.
Thus the instruction-selection distinction exists at RTL expansion, before
local/global allocation, reload and post-reload renaming. Allocation alone
cannot turn that initial graph into the blob's two multiplies and final LEA.
The standalone/full-TU byte identity validates this reduction for all six
tested source/profile combinations.

`dftRetrainDetInit` supplies the useful allocation control in the same TU:
131 bytes, EXACT against the blob with retained flags. Its `.25.greg` dumps
are identical with renaming on/off. In the on-cell `.30.rnreg`, the compiler
explicitly reports four chains renamed:

```text
Register ax in insn 145, renamed as dx
Register dx in insn 147, renamed as cx
Register cx in insn 149, renamed as ax
Register ax in insn 151, renamed as dx
```

Final assembly differs in both register choice and ordering of independent
stores to the retrain fields. Therefore this small control exposes the
renaming/scheduling interaction, not merely a global register permutation.
The blob follows the renaming-enabled result exactly. Inspect `.rnreg`,
`.bbro`, and `.sched2` before attributing the reordered stores to source order.

## Next discriminator

Keep the source fixed and explain this exact control's four renaming choices
from the period `regrename.c` decision rules and hard-register liveness.
Trace which ordering differences first appear in `.sched2` and why changed
register dependencies permit them. Then identify a short handshake tail with
the same dependency pattern and check whether this mechanism explains its
register-coloured duplication/merging. Do not transfer conclusions merely
because another function has similar length or mnemonics.

Further `polyValue` source/profile experiments require a new declared domain
aimed at the measured expansion distinction. The completed domain has no blob
preimage. Neither an arbitrary flag search nor the 28-byte factored near-match
is a reason to edit reconstruction source.

## Reproduction

```sh
python3 tools/v34_polyvalue_rtl.py \
  --config /path/to/retained/build/tc_out/.build-config \
  --blob /path/to/ref/slmodemd/dsplibs.o > /tmp/v34-polyvalue.log 2>&1
```

The artifact manifest records source/header/object hashes, revision, complete
compiler argv, exports, per-function verdicts and dump names. Focused dump
extracts include `polyValue`, `dftRetrainDetInit` and `detectRetrainReq`.
The run log records compiler and executed assembler identity. No harness
results are claimed for this apparatus-only investigation.

## Follow-up: renaming and scheduling separated

The [four-cell control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5928882513)
kept the complete TU/source fixed and crossed register renaming with
post-reload scheduling. `tools/v34_rename_schedule.py` reproduces it with the
same `--config` and `--blob` arguments as the first experiment. All four cells
compiled; 29 shared functions were compared and all 55 global bindings were
preserved. The unchanged cell reproduced the prior baseline object byte for
byte. Artifacts: `build/v34-rename-schedule/`.

| Renaming | Scheduling | `dftRetrainDetInit` blob verdict |
| --- | --- | --- |
| on | on | EXACT |
| on | off | BYTES(87) |
| off | on | BYTES(15) |
| off | off | BYTES(87) |

BYTES numbers are the canonical tool's byte discrepancy, not instruction
counts. Equal discrepancy counts do not imply the two scheduling-off bodies
are equal; their registers still differ.

Before scheduling, the five scalar definition/store pairs remain in the same
instruction-ID order in all four cells, through `.31.bbro`:

```text
150 151 148 149 146 147 144 145 142 143
```

`.30.rnreg` substitutes hard registers without reordering those instructions.
In `.33.sched2` the order becomes:

```text
renaming on:  150 148 146 151 144 149 142 147 145 143
renaming off: 150 146 148 151 144 145 142 149 147 143
```

Disabling scheduling leaves the pre-scheduling order intact. This establishes
the observed ordering difference at the scheduling pass, downstream of the
measured register substitutions. Source-store reordering is unnecessary to
explain this exact standalone control.

### Why those four registers were chosen

The [upstream GCC 3.4.2 source](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/regrename.c)
provides the decision rule; the actual Gentoo dumps confirm this example's
behavior. `regrename_optimize` initializes a `tick` array to zero per function,
processes closed definition/use chains, excludes overlapping live registers
and incompatible classes/modes, and avoids unsaved call-preserved registers.
It initializes the best candidate to the existing register and replaces it
only when another eligible register has a strictly smaller tick. It then
increments the selected register's tick. Ties preserve the existing choice.

For these contiguous constant/store chains, the remaining useful choices are
AX, DX and CX; BX holds the object base, SP is fixed, and other call-preserved
GPRs were not saved. Processing chains in the dump's order yields:

| Last use | Value | Original | Chosen | Tick after choice: AX, DX, CX |
| --- | ---: | --- | --- | --- |
| 143 | 9 | AX | AX | 1, 0, 0 |
| 145 | 3 | AX | DX | 1, 2, 0 |
| 147 | 0, runs | DX | CX | 1, 2, 3 |
| 149 | 0, phase | CX | AX | 4, 2, 3 |
| 151 | 1 | AX | DX | 4, 5, 3 |

The scan considers hard-register numbers in order; AX=0, DX=1, CX=2 in these
dumps. At the second choice, DX and CX both have tick zero, so DX wins the
first strict improvement. This reproduces all five recorded outcomes,
including the unchanged first chain, without adding a source carrier or
forcing registers. Tick state is per function, not carried across TU function
emission; earlier emitted functions cannot affect this pass through tick
history itself. Other passes or changed inline bodies remain separate causes.

### Transfer to the handshake: a bounded source-complete island

The blob's initializer at `0x69392..0x69425` contains the same three-bin loop
and five scalar stores. The strongest saved post-Horner diagnostic candidate
contains it at `0xa3c9..0xa436`. The loop operations match in order through
the comparison/backedge. Both scalar tails store identical widths/values at
`0xa24a`, `0xa24c`, `0xa250`, `0xa252`, and `0xa254`.

The blob interleaves those stores with reads of `0x3588` and `0x3592` and the
OR/compare feeding subsequent state transitions. The candidate emits the five
scalar stores before those later reads. The original object base is reloaded
into EAX in the blob, whereas the candidate retains it in EBP and also keeps
a separate receiver pointer in ECX. These are concrete differences in live
values and scheduling context. The standalone exact initializer consequently
cannot dictate the inlined register choices or ordering.

Next discriminator: compile the complete saved strongest handshake source
with isolated RTL dumps and map this initializer plus its following flag/state
instructions. Locate the first stage introducing the different pointer/live
range structure, then apply the demonstrated tick and scheduling rules to the
actual inlined chains. A difference already present before allocation requires
a source/dataflow or earlier-pass explanation; a difference appearing only in
renaming/scheduling requires a liveness/choice explanation. Do not force
registers or permute stores merely to approach the blob.

### Conclusion from the actual inlined island

The four-cell full-TU experiment is complete, followed by two narrowly scoped
pointer-provenance controls. Both domains were recorded before compilation:
[original four cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929138077)
and [common-pointer control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929231084).
The unchanged cell reproduces the saved post-Horner object **byte for byte**.
This is the historical diagnostic snapshot, not a proposed production profile.
All six cells used the recovered Gentoo compiler, its selected assembler, all
five recorded localonly parameters and the mandatory reproduction define.

The concrete source-level obstacle is **address provenance**, present before
register allocation. In initial RTL the initializer writes through an inlined
`obj` copied from `vobj`, while the flag statement loads its base from
`frame.m`. At combine/local-allocation these are still distinct pseudos:
`vobj` 58 and the flag base 15860. Global allocation/reload assigns them BP
and CX respectively. Register renaming does not reunify these live pointers.

In the final scheduled dump, flag-load instruction 13373 depends on all five
scalar stores (35566, 35564, 35562, 35560, 35558). Its memory-dependence list
also includes the three bin-increment stores. Turning register renaming off
retains the memory constraints; turning scheduling off retains source order.
Thus adjusting renaming alone cannot produce the blob's interleaving here.

The causal control changes exactly one statement in the saved diagnostic
source: the two `+0x3588 |= 2` accesses immediately after the initializer use
`obj` directly, with the same widths and arithmetic, instead of `frame.m`.
This leaves the five initializer assignments unchanged. The scheduled flag
load (now instruction 13372) loses its memory dependencies on all five scalar
stores. Hard-register constraints and conservative bin-store dependencies
remain. The resulting assembly reads the flag after the state store and
before phase/runs/quiet/tone stores. With scheduling disabled it stays after
all five stores. This proves the predicted address-provenance mechanism.

The blob also uses one object base for these accesses. The common-pointer
control does **not** reproduce its exact register choices, complete ordering,
or its unsigned state load. It establishes a missing optimization opportunity,
not a unique original-source preimage. Global allocation already chooses BP
for the candidate's object base; the blob reloads its base from the stack into
AX. The blob has no RTL dumps from which to prove its earlier history.

| Diagnostic source/options | Shared blob functions | Exact | Handshake size deficit |
| --- | ---: | ---: | ---: |
| Saved snapshot, rename on / schedule on | 29 | 4 | 4,083 B |
| Saved snapshot, rename on / schedule off | 29 | 0 | 4,083 B |
| Saved snapshot, rename off / schedule on | 29 | 3 | 4,262 B |
| Saved snapshot, rename off / schedule off | 29 | 0 | 4,262 B |
| Common pointer, rename on / schedule on | 29 | 4 | 4,083 B |
| Common pointer, rename on / schedule off | 29 | 0 | 4,083 B |

All six preserve the same 55 global names/bindings. The common-pointer
scheduled control changes only `v34handshak`'s extracted body/relocation records
among the candidate TU's defined functions; all others remain identical to the
saved diagnostic baseline. An `UNRESOLVED` canonical verdict is not itself a
changed-body verdict: even self-comparison can retain unresolved section
relocations. The whole handshake remains non-exact, with no aggregate gain.

Reproducer: `tools/v34_inline_rtl.py`; use `--common-pointer` for the second
domain. Artifacts: `build/v34-inline-rtl/results.json`, `analysis.json`, per-cell
RTL/disassembly and `build/v34-inline-common-pointer/results.json`. Source and
header hashes, complete commands and compiler identity are retained. No
production source, differential, fuzzing or mutation harness was changed/run.

**Conclusion:** the small-example approach was useful. It separates early
instruction selection, address provenance/alias analysis, global allocation,
register renaming, and downstream scheduling. A shape match is insufficient
to classify the residual as register allocation alone. This island provides a
specific source-structure lead inside the budget-bound handshake; it does not
establish either an exactness ceiling or a route around the entire inline wall.

The next worthwhile source investigation is an audit of the synthetic
`t3m_frame` pointer carriers: establish which helpers can change `frame.m`,
which paths preserve `frame.m == (unsigned char *)obj`, and compare accesses
through that carrier with the blob's common-base accesses. Only then test a
bounded original-source hypothesis using direct field/object access. Adoption
still requires the period differential gate. Do not replace all accesses or
force register choices based on this one block, and do not generalize this
sample to the other approximately 1,000 non-exact functions.
