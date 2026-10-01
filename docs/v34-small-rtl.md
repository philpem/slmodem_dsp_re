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

### Pointer audit and idiomatic field refinement

The follow-up audit closes the identity question **at this action site**.
`v34handshak` initializes its local frame through `t3m_frame_init`, which sets
`frame.m = (unsigned char *)obj`. Before the post-retrain flag statement,
`&frame` is passed only to that initializer. All microstate helpers receiving
`&frame` and the final transmit helper occur later. Object/receiver callees
on the earlier path receive object-derived addresses, not this local frame's
address. They therefore cannot change this carrier on a valid execution.
This is a local proof, not a blanket alias assertion about arbitrary callers.
The only direct assignment to `t3m_frame::m` in the TU is its initializer.

The existing `short_3588` field is defined at the measured offset `0x3588`,
with a live `HS_OFF_ASSERT` here. Updating its low 16 bits with
`obj->short_3588 |= 2` has the same result as the prior unsigned-read,
OR, cast-to-short and store, including sign-bit-set values under the deciding
period compiler. This ordinary field spelling preserves the blob's common
object base without a synthetic volatile, explicit register, or store order.

[Declared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929464711):
source baseline/field crossed with the retained and historical diagnostic
profiles. Both unchanged baselines reproduce their prior objects byte for
byte. All cells retain the same 55 global names/bindings and the same four
exact functions among 29 shared blob functions. In both profiles **only
`v34handshak` changes** among the candidate TU's defined function bodies and
relocation records. Size deficits remain 54,433 bytes under retained flags
and 4,083 under the historical diagnostic flags. The diagnostic field spelling
has the identical extracted handshake body/relocation records to the earlier
explicit-cast common-pointer control. No aggregate exactness improvement is
claimed, and the historical inline profile is still not adopted.

The branch source now uses the direct field expression at this one site.
The four existing affected source anchors are retargeted (including the large
nbits-order anchor); no mutation harness is executed. Reproducer
`tools/v34_flag_field.py` retrieves the original retained source/header from
revision `ff2b5e8d`, so adopting the field expression does not silently replace
its baseline. It was replayed successfully after the source edit. Complete
commands, hashes, inventories, canonical blob verdicts and RTL dumps live in
`build/v34-flag-field/results.json` and its four cell directories.

The period differential passed **385 tests, 0 failed** on the edited source.
The initial combined gate also exposed four detached source anchors; these
were repaired. The final `make phase J=8` exited zero: **385 passed, 0 failed**,
14,199 references checked with no live mutant, and 10,038 existing anchors
across 285 suites with no detached, non-unique or misplaced anchor. This does
not use a mutation score as reconstruction evidence.

A next discriminator is visible in the same block: the candidate's generic
`hs_setstate` eagerly sign-extends and caches `now` for both equality and
logging, whereas the blob first loads the halfword with zero extension for
its equality comparison, then obtains signed indices only on its debug path.
This should be investigated as compare/logging source factoring and live
ranges, with the debug branch included; changing the state field to unsigned
or globally rewriting the helper would overstate this one-site evidence.

### State equality and debug indexing: value helper versus direct storage

The comparison/logging investigation produced a second source-factoring lead.
The blob's microstate check at `0x69401` loads with `movzwl`, compares the
halfword at `0x6941a`, and only on the debug branch sign-extends that **same
cached register** at `0x6946c` for `StateName`. It does not reread the subject
state from memory there. The cached `short now = hs_get(...)` source emits
`movswl` before equality in the diagnostic candidate.

Two domains were declared before compilation:
[comparison/read placement, six cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929650860)
and [direct comparison access, four cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929689120).
Both cross retained and saved diagnostic profiles; all ten preserve recovered
compiler/assembler identity, complete flags, and bug reproduction. The two
unchanged baselines reproduce their prior objects byte for byte. The second
domain reuses those validated baseline objects and verifies their hashes.

The first domain compares the existing cached short with direct `hs_get`
equality followed by either a second getter in the debug argument or a
`short now` read local to the debug branch. Neither restores the target
pattern: all three emit early `movswl`. Initial RTL already promotes the
signed getter return into SI before the HI comparison. Combine retains the
sign-extended memory load. Moving the local's scope is insufficient.

The next control bypasses the value-returning getter **only for equality**,
leaving the existing signed getter and index in the debug block. Signed and
unsigned direct-memory comparisons were both tested; unsigned comparison also
casts `next` to unsigned short to preserve all halfword equality cases.
The signed and unsigned versions produce byte-identical entire objects in
both profiles. Thus the object's zero extension does **not** establish that
its state field was unsigned.

Direct signed access produces HI load/compare RTL at combine, rather than
an SI signed value whose low bits are compared. The diagnostic candidate now
has `movzwl 0x3592(%ebp),%eax`, `cmp $0x2e,%ax`, and a debug-only
`movswl %ax,%edi`. GCC reuses the comparison halfword despite the debug-local
source getter. No additional subject memory load is introduced. This recovers
the observed value-width and extension placement; register choices, surrounding
store order, branch layout and the full function remain different.

| Source family | Retained size deficit | Diagnostic size deficit |
| --- | ---: | ---: |
| Cached short baseline | 54,433 B | 4,083 B |
| Getter comparison / getter in debug argument | 54,582 B | 4,191 B |
| Getter comparison / debug-local short | 54,582 B | 4,079 B |
| Direct signed comparison / debug-local short | 54,546 B | 3,638 B |
| Direct unsigned comparison / debug-local short | 54,546 B | 3,638 B |

Every cell retains the same 55 global names/bindings, the same four exact
functions among 29 shared blob functions, and all baseline local function
symbols. No byte-exact count increase is claimed. The 445-byte narrowing of
the diagnostic size gap is not a recovered byte count. The retained function
shrinks by 113 bytes because this also changes inline decisions; improvement
must be assessed by bodies and operations, not a monotonic size score.

Under the diagnostic profile the direct-access variants change three defined
function bodies/relocation records: `v34handshak`, `v34handshakinit`, and
`v34setuptxmit`. Their external PC32 target counts inside `v34handshak` are
unchanged. Under retained flags, 35 defined function bodies/relocation records
change, and the handshake's direct debug-printf references go from 31 to 30;
this profile contains additional out-of-line helper copies. Full inventories,
changed bodies and target counts are recorded, rather than attributing every
change to the target comparison. No global flag change is proposed.

The signed direct predicate and debug-local read are selected on the branch:
comparison of the stored halfword is separate from its use as a signed index.
This does not force a hard register or change the state-field type. The
comparison/storage operation has the same semantics as `hs_get == next`;
the signed subject value is read again in source only across nonvolatile
reads, with no intervening call or write. The compiler's reuse of its halfword
is confirmed in the deciding period output. Selection is based on the actual
extension-placement recovery, not the near-size results of the earlier cells.

Reproducer: `tools/v34_state_factor.py`, optionally `--direct-load-domain`.
Retained input revision is `a35ed924`; diagnostic input is the saved snapshot
with the already-validated direct flag statement. Commands/hashes, complete
verdicts, inventories and RTL are in `build/v34-state-factor/results.json` and
`build/v34-state-load/results.json`. No fuzzing or mutation harness runs.
The deciding `make phase J=8` exited zero: **385 period tests passed,
0 failed**, with 14,199 references and 10,038 existing anchors across 285
suites clean. The helper source is committed only after that validation.

### Duplicate frame carrier: bounded negative result

[Six-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929814439)
tested the current source against two broader pointer-carrier explanations,
under retained and historical diagnostic profiles. The macro-only variant
changes the four `T3M_*` integer-access macros to derive bytes from `f->obj`
instead of `f->m`, keeping the duplicate member and other uses. The complete
variant also replaces the remaining byte-pointer reads with casts of `f->obj`
and removes the `m` member/initializer. These are diagnostic variants only.

The source audit finds exactly one assignment each to `f->obj` and `f->m`,
both in `t3m_frame_init`. Frame pointers are passed only among the TU's
private frame helpers; object/receiver callees receive object-derived addresses.
This explains the intended source equivalence, but no broader source rewrite
is adopted on that argument alone.

One preliminary run is invalid: a generator substring replacement of `f->m`
also altered `f->mst`, and GCC rejected the resulting token sequence.
This is a generator defect, not an original-source/compiler finding. The
whole partial run is excluded and retained at
`build/v34-frame-carrier-invalid-generator/INVALID.json`, including the compiler
log. Identifier-boundary replacement fixes the generator; all six cells were
rerun, with both unchanged baselines reproducing their prior direct-signed
objects **byte for byte**.

| Source variant | Retained size deficit | Diagnostic size deficit |
| --- | ---: | ---: |
| Current source | 54,546 B | 3,638 B |
| Common provenance in four macros | 54,615 B | 3,927 B |
| Remove duplicate byte-pointer carrier | 54,698 B | 4,119 B |

All six retain the same 55 globals/bindings and four exact functions among
29 shared blob functions. No local function symbols are added or removed.
The two retained alternatives each change 18 defined function bodies/relocation
records; the diagnostic alternatives change only `v34handshak` and
`v34handshak_txblock`. Retained handshake external PC32 target counts stay
unchanged. Both diagnostic alternatives reduce direct debug-printf references
from 289 to 288, so even a call-count change is not itself a recovered inline
profile or a reason to adopt.

The decisive target observation is stronger than these scores: all three
diagnostic initializer/scalar/flag/state windows have the same instruction
order, registers and operands after excluding jump destination addresses.
Their windows start around `0xa409`, `0xa1bc`, and `0xa179` respectively;
all retain the object in BP. Combine still uses `vobj` pseudo 58 for the
post-initializer flag access. The blob instead reloads its object base from
`0xc0(%esp)` into AX at `0x693c6`. Neither broader carrier change recovers
that reload, its register choices, or the remaining store order.

**Conclusion:** eliminating `frame.m` is insufficient to explain the residual
at this island. The earlier targeted flag provenance and direct state equality
recoveries remain supported; that does not justify wholesale removal of the
synthetic frame. No production source changed in this domain, no differential,
fuzzing or mutation harness was run, and neither larger rewrite was adopted.
Close this particular pointer-carrier explanation for the island rather than
forcing a spill or continuing nearby spelling changes.

Reproducer: `tools/v34_frame_carrier.py`, retained input revision `eb6fa5b4`.
The diagnostic input is the saved direct-signed cell from the preceding domain.
Complete compiler/assembler identity, commands, hashes, exports, verdicts,
changed bodies, local-symbol changes and PC32 target counts are retained in
`build/v34-frame-carrier/results.json`. Per-cell target windows and
`retrain-island-analysis.json` record the instruction/operand comparison; that
comparison explicitly excludes destinations and is not a byte-exact verdict.

A next discriminating investigation should trace **where the blob's object
argument is spilled and reloaded across the surrounding predecessor paths**,
and compare it with the candidate's pseudo-58 live range before global
allocation. A single reload is not proof of a source scope boundary. Require
matching nearby call boundaries and live values before proposing a source
lifetime or inlining-context change; the two tested carrier variants do not
supply it.

### Incoming argument homes and a small allocation-pressure control

The predecessor/prologue trace corrects the earlier shorthand "object-pointer
spill". The blob pushes four registers and reserves `0xac` bytes, so its
incoming first argument is at `4 + 16 + 172 = 192`, **`0xc0(%esp)`**. Across
`v34handshak` there are **845 static read sites, zero write sites, and zero
address-taking sites** for that slot. It is an incoming argument home, not a
locally created spill slot. Counts are instruction sites, not execution counts.

The current diagnostic candidate instead reserves `0xec` bytes. Its incoming
argument is at `4 + 16 + 236 = 256`, **`0x100(%esp)`**, and has one read site:
loading the pointer into BP. Its `0xc0`/`0xc4` object/byte-pointer frame copies
are separate locals. Comparing equal-looking offsets across these two stack
frames would conflate distinct objects.

The aligned predecessor path preserves the same `dftenergy` boundary and
three-bin clearing loop. The blob uses BP for constants/zeroing, then reloads
its argument to read retrain state at `0x6930d`, again on the run-count path,
after the debug call, and after the initializer loop. The candidate retains
its object pointer in BP across these operations. Before allocation its root
pseudo 58 has `REG_EQUIV` to the incoming argument; its `fskin` pseudo 60 also
remains live across the energy call/clear loop and ultimately occupies SI.
The bins base occupies BX at the target. Thus the candidate has an extra
resident object base consuming a call-preserved register in this island.

The candidate global allocation dump orders 709 allocation candidates, with
root pseudo 58 at position 96. Its recorded GPR conflicts include
AX, DX, CX, BX, SI, DI and SP, leaving BP as the usable GPR. This explains the
candidate's choice at that stage; later renaming is not the decision that
placed this long-lived pseudo in BP. The blob has no allocation dump, so its
original conflicts/priorities cannot be reconstructed uniquely from the
reloads. Source scopes, volatility, and a particular flag profile do not
follow from these operands alone.

[Declared small mechanism domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929958161)
uses five ordinary unsigned-integer examples with 0..4 independent accumulators,
a loop count, and an object pointer live across `pressure_tick()`. Each is
compiled with renaming on/off: ten cells on the recovered compiler, retained
flags, mandatory bug-reproduction define and isolated RTL dumps. There is no
volatile, explicit register, assembly, manual spill, inline-budget parameter,
linking, execution or blob-oracle fixture.

| Accumulators | Pointer allocation | Incoming argument reads | Writes |
| ---: | --- | ---: | ---: |
| 0 | SI | 1 | 0 |
| 1 | DI | 1 | 0 |
| 2 | BP | 1 | 0 |
| 3 | Incoming stack home | 2 | 0 |
| 4 | Incoming stack home | 3 | 0 |

These outcomes are identical with renaming on/off. In the three-accumulator
case, global allocation prioritizes the loop-count pseudo, a2, a1, a0, then
the pointer. BX carries the count, SI/DI/BP carry the accumulators, and the
pointer is reloaded from its incoming slot for the output store. In the
four-accumulator case one accumulator also spills. Its prologue contains two
adjacent incoming-pointer loads into different registers despite having no
volatile source. This demonstrates how reload-generated accesses can survive
into final code after earlier CSE has run.

**Conclusion:** a small ordinary-C example reproduces the mechanism that
makes repeated incoming-argument loads compatible with normal allocation.
The blob's reloads are not evidence that the author repeatedly assigned the
pointer, declared it volatile, or created an explicit stack temporary. The
current island's residual has a concrete **global allocation/residency**
component. This does not imply that all remaining non-exact functions are
allocation-only, and does not recover the original inline profile or identify
which original live values caused its different choice.

Reproducers: `tools/v34_argument_pressure.py` and
`tools/v34_argument_homes.py`. The home detector was demonstrated both on the
known three-accumulator spill and on the resident zero-accumulator control;
it reports the static-site denominator and argument-slot derivation.
Artifacts are `build/v34-argument-pressure/results.json`, per-cell RTL and
assembly, and `handshake-homes.json`. Ten cells compile; zero execute. No
production source, differential, fuzzing or mutation harness changes/runs.

Further allocator work must compare whole-function conflict/prioritization
inputs rather than force a spill at this one island. A separate source lead
visible in the same predecessor is the retrain-state dispatch: the blob
sign-extends the state then compares as SI, while the candidate's if-chain
uses a zero-extended halfword and HI comparisons. Test an ordinary state
`switch` against the if-chain in the smaller standalone detector before
inferring that difference is allocation; do not add casts just to force width.


## Retrain-state dispatch: ordinary switch versus if-chain

[Declared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5930139031)
compares the unchanged detector with an ordinary switch on its signed-short
state. Case 1 retains the quiet arm and return; case 2 breaks to the existing
tone arm and shared equality return; default returns zero. Both spellings are
compiled as complete translation units under retained flags and the historical
diagnostic inline profile. The controls reproduce their saved objects exactly.

The blob standalone dispatch at `0x5e99c` and inlined dispatch at `0x69314`
use `movswl` followed by two full-width comparisons against 1 and 2. The
if-chain emits `movzwl` and two halfword comparisons. The switch emits the
blob's signed extension and full-width comparisons in both the exported
function and the inlined handshake under both profiles. This is an ordinary
source-structure explanation for the width difference, independent of the
argument-home allocation difference. Initial RTL already has a HI comparison
for the if-chain and an explicit sign-extension plus SI comparison for the
switch, so this difference precedes allocation and register renaming.
It establishes a compatible source family,
not a unique original spelling.

| Profile | Source | Handshake size deficit | Exact shared functions |
| --- | --- | ---: | ---: |
| Retained | If-chain | 54,546 | 4/29 |
| Retained | Switch | 54,548 | 4/29 |
| Diagnostic | If-chain | 3,638 | 4/29 |
| Diagnostic | Switch | 3,638 | 4/29 |

Every cell has the same 55 global definitions/bindings. Only `detectRetrainReq`
and `v34handshak` bodies/relocations change; no function symbols are added or
removed, and handshake external-call target counts are unchanged. The standalone
detector remains one byte smaller than the blob in all four cells; that is a
size measurement, not a claim of a one-byte mismatch. Its candidate equality
return uses `sete`, whereas the blob branches to zero/one return paths. Other
operand and instruction differences remain. Neither function becomes byte-exact.

Adopted the switch because it recovers observed dispatch semantics/codegen in
both contexts with ordinary idiomatic source, despite no exact-count gain and
a two-byte retained size regression. Retargeted two existing source anchors to
the equivalent switch spelling; no mutation or fuzzing harness was run.
Reproducer: `tools/v34_retrain_switch.py`; complete commands, hashes, canonical
verdicts, exports and body comparisons are in
`build/v34-retrain-switch/results.json`, with per-cell RTL and disassemblies.

The next bounded source question is the detector's boolean return: compare a
shared direct equality return with an ordinary explicit `if` returning one,
then zero. It is a distinct CFG hypothesis suggested by the blob's branch
sequence, not a reason to force registers or spills. For allocator work,
the ten pressure controls above already give the requested small example and
show why the incoming argument can remain on the stack. Recovering the whole
handshake still requires matching its whole-function live ranges and inline
choices; local dispatch recovery does not resolve that larger question.

Validation: `make phase J=8` passed: 385 period differential tests, zero
failures; structural checks clean, including 14,199 resolved references and
10,038 source anchors over 285 suites, zero detached/non-unique anchors.
Only anchor metadata was checked; no mutation harness was executed.


## Retrain boolean return and quiet-arm field stores

[Return domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5930273094)
compiled direct equality return versus ordinary explicit if/one/zero under
retained and diagnostic profiles. Both unchanged controls reproduce the saved
switch objects exactly. The explicit-if spelling leaves the entire standalone
detector body and relocation records identical in both profiles, including
`sete`. Only the inlined handshake changes. Its size deficit moves
54,548 -> 54,538 under retained flags and 3,638 -> 3,678 under the diagnostic
profile; neither is evidence of recovery. This return spelling is not adopted.
The blob's branch-based boolean return remains unexplained by this source pair.

The [next eight-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5930293342)
crosses that return pair with local `short runs` versus direct field writes
in the quiet arm, under the same two profiles. All four local-store controls
reproduce the preceding return experiment objects exactly. The direct-store
variant increments `obj->retrain_runs` in the quiet branch and clears it in
the other branch, retaining the same state-update/store order and return.
The tone arm and its shared comparison, including behavior at a zero tone
limit after reset, are unchanged.

| Profile | Quiet stores | Return | Handshake size deficit |
| --- | --- | --- | ---: |
| Retained | Local | Equality | 54,548 |
| Retained | Local | Explicit if | 54,538 |
| Retained | Direct | Equality | 54,549 |
| Retained | Direct | Explicit if | 54,539 |
| Diagnostic | Local | Equality | 3,638 |
| Diagnostic | Local | Explicit if | 3,678 |
| Diagnostic | Direct | Equality | 3,638 |
| Diagnostic | Direct | Explicit if | 3,678 |

The local is represented as an SI pseudo in initial RTL, despite its short
source type. The direct-store form has no such pseudo. In the standalone
function, direct stores remove exactly one instruction, the quiet increment's
`cwtl`, under either profile or return spelling. Other standalone instructions
and operands remain the same apart from shifted branch destinations. The blob's
quiet increment at `0x5ea18` also stores without `cwtl`. Candidate instruction
count falls 96 -> 95 versus blob 94; candidate byte-size deficit increases
1 -> 2. Neither count alone assesses accuracy.

For equality-return controls, inlined handshake mnemonic comparison removes
one `cwtl` under retained flags; the diagnostic comparison removes that `cwtl`
and one alignment `nop`. This is a shared source-level explanation for an
extra conversion in both contexts, not an allocator-only explanation.
All eight cells retain 55 global definitions/bindings and four exact functions
among 29 blob-shared functions. Direct-store variants change only the detector
and handshake bodies/relocations; no function symbols or handshake call targets
are added/removed. The return-spelling-only variants change just the handshake.

Adopted direct quiet-arm field writes with the original equality return. This
recovers an observed instruction detail without forced registers, volatility,
manual spills, flags or a score-based choice. It supports a compatible source
family; an int temporary could be a separate original-source hypothesis, so
this does not uniquely identify the author's spelling. No exact-function gain
is claimed. No existing source-anchor edits were needed for direct stores.

Reproducers: `tools/v34_retrain_return.py` (four compilations) and
`tools/v34_retrain_stores.py` (eight compilations). Complete commands, hashes,
canonical verdicts, binding/call/body comparisons, RTL and disassemblies are
under `build/v34-retrain-return/` and `build/v34-retrain-stores/`. All compilations
use recovered Gentoo GCC and the final bug-reproduction define; none execute
a fuzzing or mutation harness.

Stop these spelling tests here. A next discriminating branch-return study
should cross source with the period compiler's if-conversion mechanism in a
small function and the full detector, inspect where branches become setcc,
and review whole-TU effects before drawing profile conclusions. Repeatedly
rewriting the same boolean expression is not justified by the negative result.

Validation of adopted direct stores: `make phase J=8` passed, 385 period
differential tests and zero failures. Structural checks clean: 14,199 references
and 10,038 anchors over 285 suites, zero detached/non-unique anchors.
No fuzzing or mutation harness was executed.
