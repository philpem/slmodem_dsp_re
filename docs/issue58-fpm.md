# Issue 58: FPM initializer investigation

Worktree: `issue58-fpm`, base `591e99402828205a1eede6d898d5a7086dda8d0f`.
Scope: complete `src/dsp/fpm_sre.c` and `src/dsp/fpm_fse.c`.

## Predeclared first domain

The reference copies each 56-byte configuration with `rep movsl`, interleaved
with scalar state stores. All individual store widths agree with the declared
fields; a first mnemonic mismatch does not establish a field-width defect.

Competing explanations: aggregate-assignment versus memcpy alias/dependency
information; post-reload scheduling; a broader O2/O3 pass interaction.
Prediction: changing copy form can change which independent stores cross the
copy; disabling scheduling should expose the pre-scheduling sequence. A
constant emission across copy forms falsifies that distinction under the tested
profile. O2 is a separate bundled profile control, not an isolated pass claim.

Domain: 2 TUs x 2 copy spellings (retained aggregate assignment, explicit
`__builtin_memcpy` of exactly `sizeof(cfg)`) x 3 profiles (retained O3,
O3 plus `-fno-schedule-insns2`, O2) = **12 full-TU compilation cells**.
Explicit builtin is a mechanism probe, not automatically recovered source.
Two unchanged-source helper controls precede the matrix. No score-based selection.

## Invalid preliminary control

The initial focused `period.mk` build omitted `DSPLIB_REPRODUCE_BUGS`, since
the Makefile does not append it. Its two objects and configuration remain in
`build/tc_out/`; they are **invalid for reconstruction** and excluded from the
matrix. The helper appends the required define after configurable flags.
Valid independent Makefile controls will use explicit `TC_EXTRA` and a separate
output directory. No default full-tree gate was run.

## Predeclared second domain

The valid baseline isolates SRE's `adapt` store versus the four short filter
resets, and FSE's `freq`/`phase_acc` stores crossing the copy. The 12-cell
first domain has zero exact initializers. Copy spelling changes scheduled O3
output but assignment/memcpy converge with scheduling disabled.

Hypothesis: relative source order of the independently initialized fields
explains these scheduling choices. Prediction: a finite interleaving can
recover the complete initializer, including operands, at retained O3.
Falsifier: no exact body in the declared domain. Do not select a near miss.

SRE: interleave the ordered three stores `active, acquiring, adapt` with the
ordered five `mode, pll_acc, err_avg, mag_avg, taps`, preserving each chain:
`C(8,3) = 56` cells. Keep the configuration copy and all subsequent stores fixed.
FSE: remove `freq` and `phase_acc` from its 17 scalar initialization stores;
insert them at every pair of distinct final positions, leaving the other 15
in their current order: `17*16 = 272` cells. Keep the configuration copy fixed.
**328 full-TU cells**, each compiled at retained O3 only. This tests a bounded
interleaving family, not every permutation of every initializer statement.

## Results and disposition

**SRE has an exact source-order equivalence class; FSE remains unresolved.**
No `src/` or header change is retained. There are two new files: this report
and `tools/issue58_fpm.py`. No commit or push. No differential, phase, period,
portability, or partial-link gate was run; adoption and final gates belong to
the parent. Issue 51's normal-build allocation safety is untouched.

The two valid unchanged-source helper controls are **whole-object byte-identical**
(`cmp`, exit 0 for both) to independently compiled focused Makefile controls
with explicit reproduction enabled. This includes STT_FILE, every body, data,
relocation and symbol table, not just the initializer. Both issue diagnostics
reproduce: SRE BYTES(44), 574 bytes; FSE BYTES(56), 458 bytes.

### First matrix: 12/12 compiled, zero compiler rejections

| Source form/profile | SRE init | FSE init |
|---|---:|---:|
| assignment / O3 | BYTES(44) | BYTES(56) |
| builtin memcpy / O3 | BYTES(75) | BYTES(82) |
| assignment / O3 no-sched2 | BYTES(213) | BYTES(163) |
| builtin memcpy / O3 no-sched2 | BYTES(213) | BYTES(163) |
| assignment / O2 | SIZE(4) | SIZE(4) |
| builtin memcpy / O2 | SIZE(4) | SIZE(4) |

SIZE is a length gap, not a differing-byte count. Zero initializer hits.
Generated copy forms demonstrably differ; the scheduled O3 bodies change in
both TUs. Under no-sched2, both copy spellings emit the same whole object
within each TU. Under O2 their equal SIZE scores hide different initializer
bodies: assignment versus memcpy differs by 65 bytes in SRE and 60 in FSE;
all other emitted bodies agree. This is a scheduling interaction, not a
recovered flag profile. The final pairwise audit caught and corrected an
earlier draft's unsupported claim of O2 whole-object equality.

### Store-order domain: 328/328 compiled, zero compiler rejections

* **SRE: 36/56 exact hits, five distinct emitted objects/initializer bodies.**
  Every hit emits the **same entire object**, SHA-256
  `6bd88b8e088c19edf623cb839b61ce43bf3c84236999e2ac76457f4c530af98d`.
  Checking the complete domain establishes **EXACT iff `mag_avg` precedes
  `adapt`**, 56/56 cells, with the other declared chain constraints held.
  This recovers that ordering fact, **not a unique total source order**.
* **FSE: 0/272 exact hits, 30 distinct objects/initializer bodies.**
  This excludes the declared placement domain at retained O3. No nearest
  cell is selected or proposed for source retention.

The minimal SRE representative is `sre-order-004` (only one store moved):

```diff
 sre->active = 0;
 sre->acquiring = 1;
-sre->adapt = 1;
 sre->mode = 0;
 sre->pll_acc = 0;
 sre->err_avg = 0;
 sre->mag_avg = 0;
+sre->adapt = 1;
 sre->taps = (short)(sre->cfg.coeffs / FPM_SRE_BRANCHES);
```

It is preserved as an experimental complete source/object for parent review,
not applied to `src/`. There is no near-match selection: all 36 full matches
and the common ordering predicate were enumerated before disposition.

### Full-TU accounting, including losses and nonexact collateral

Shared function denominator is **3 for SRE, 4 for FSE**, in every cell.
Across 2 helper controls + 12 first-matrix + 328 order cells: **342 recorded
cells, 1,305 shared-function verdicts**. These are comparison counts, not tests.
Two independent valid Makefile compilations are additional controls.

Baseline exact sets are `{FPM_SRE_free}` and `{FPM_FSE_free}`. Baseline nonexact
bodies besides the targets are `FPM_SRE_recover` SIZE(202),
`FPM_FSE_receive` SIZE(35), and `FSE_getdiag` SIZE(16).

* Every SRE exact cell gains **`FPM_SRE_init` only**, loses nothing, and keeps
  both other bodies and their canonical relocations exactly baseline-identical.
  All three symbols remain `FUNC GLOBAL DEFAULT .text`. Allocated nontext,
  export/undefined inventory, function sizes and layout are unchanged.
* All **328** order cells change **only their initializer body**. No nonexact
  bystander changes, exact losses, new/missing functions, binding changes,
  relocated-target multiset changes, or allocated-nontext changes.
* Copy-form-only O3 controls change only their initializer, with no gains or
  losses. Both no-sched2 controls change SRE init/recover and FSE
  init/receive/getdiag. The frees remain exact. Their same-SIZE nonexact bodies
  do change: they are recorded, not treated as unchanged on a score alone.
* O2 changes those same shared nonexact functions. SRE recover becomes
  SIZE(173); FSE receive/getdiag become SIZE(29)/SIZE(60). O2 additionally emits
  **LOCAL DEFAULT `.text` helpers `iabs` and `sre_ingest`**, with **two direct
  calls to each from `FPM_SRE_recover`**. These unrelocated internal calls are
  not captured by a relocation-only census. No existing export is removed or
  weakened. Complete helper bodies are included in the disassembly artifacts.
* All 342 cells preserve allocated nontext section contents, sizes and
  alignment against their TU baseline. All shared functions preserve the
  multiset of canonical relocated destinations, but some relocation **sites**
  move under source/profile changes. `audit.json` distinguishes the two.

The audit fires on known experimental changes: 2/342 cells add local helpers,
and the order generator produces 5/30 distinct emissions rather than a constant
map. No new comparator was written: scoring uses `byteident.body/verdict`.

## Identity, configuration and reproduction

Compiler: `/usr/i386-pc-linux-gnu/gcc-bin/3.4/gcc`,
`gcc (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)`.
Target `i386-pc-linux-gnu`. The compiler-selected assembler was **executed**:
GNU assembler `2.15.92.0.2 20040927`, target `i386-pc-linux-gnu`.

Image `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`:

* image ID `sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16`
* repository digest `sha256:14efe17550e390798c81d78a612b538af6c8bd70b8b6fc75ee19115147ef5d96`
* reference blob SHA-256 `1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`

Complete baseline compiler flags (C TUs; no C++-only flags):

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4
-include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS
```

The matrix appends its single alternative option before the mandatory define.
`compile_shell` appends that define; the complete expanded commands are in each
batch's `manifest.json` and `compile.sh`. `docker_prefix` mounts this isolated
worktree at `/src`, this worktree's artifact directory at `/work`, and uses the
image's native user and explicit Gentoo compiler path. Source/header hashes
are in `inputs.json`; tool/blob hashes in `reference-identity.json`.

Baseline object SHA-256:

* SRE `3066ebe549dbf27fe418b7f6520f0c178aa45e439376dafdfb75c30464576928`
* FSE `793d19208b8971af18f02b0fb4001ee04c47c56a8f1b5ab1e57391e6e8725f0d`

Run from the isolated worktree root (focused targets only):

```sh
make -f tools/toolchain/period.mk \
  TC_OUT="$PWD/build/issue58-valid-make" TC_EXTRA=-DDSPLIB_REPRODUCE_BUGS \
  "$PWD/build/issue58-valid-make/src_dsp_fpm_sre.c.o" \
  "$PWD/build/issue58-valid-make/src_dsp_fpm_fse.c.o"
python3 tools/issue58_fpm.py control
cmp build/issue58-valid-make/src_dsp_fpm_sre.c.o build/issue58-fpm/control/baseline-sre/fpm_sre.o
cmp build/issue58-valid-make/src_dsp_fpm_fse.c.o build/issue58-fpm/control/baseline-fse/fpm_fse.o
python3 tools/issue58_fpm.py matrix
python3 tools/issue58_fpm.py orders
python3 tools/issue58_fpm.py audit
```

Initial source/reference readings used `python3 tools/dis.py
ref/slmodemd/dsplibs.o FPM_SRE_init` and the corresponding FSE invocation.
A preliminary invocation missing the object argument was rejected by argparse.
A first helper control compiled successfully but its JSON reporter rejected a
bytes-valued canonical string target; serialization was fixed and both controls
rebuilt before the matrix. Neither reporting failure contributes a matrix score
or a compiler-rejection count. The two earlier non-reproduction Makefile
objects remain explicitly invalid in `build/tc_out/`.

## Artifact map and next discriminator

All generated artifacts are under **`build/issue58-fpm/`** in this worktree:

* `identity.txt`, `inputs.json`, `reference-identity.json` — provenance.
* `control/`, `matrix/`, `orders/` — complete source/object per cell;
  `manifest.json`, `compile.sh`, `compile.log`, `results.json` per batch.
* Per cell `bodies.json`, `symbols-relocs.txt`, `sections.txt` — body/relocation,
  binding and section evidence. One `disassembly.txt` per distinct object;
  duplicate objects share the same hash and need no second disassembly.
* `reference-sre.txt`, `reference-fse.txt` — complete shared reference bodies.
* `audit.json`, `copy-form-comparison.json`, `sre-common-precedence.json` —
  gain/loss/collateral, pairwise copy-form results, and the complete-domain
  ordering predicate.

**SRE next:** parent independently rescore `sre-order-004`, review acceptance
of the uniquely distinguished *ordering fact* despite 36 total-order preimages,
then run the normal-build boundary and Gentoo differential/partial-link gates
if adopting its one-store move. This is a source candidate, not a validated
retained change.

**FSE next:** inspect scheduling RTL/dependencies for the configuration copy
and all four enable stores, rather than repeating `freq`/`phase_acc` placements.
The current domain fixed `lms_force, pll_on, tilt_on, lms_on` ahead of every
short store and fixed the copy at the beginning. A new bounded cross should
vary that fixed assumption (e.g. the enable-store block position against the
short-reset chain, crossed with assignment/memcpy). First identify which
dependence or scheduling priority prevents the reference's `rep` placement.
No global flag inference follows from these two TUs.

## FSE continuation: predeclared RTL controls

After the parent's review request and issue update, retain all earlier artifacts.
First compile **two diagnostic full-TU controls**, assignment and builtin memcpy
at retained O3 with `-da -fsched-verbose=5`, explicitly naming dump paths under
`build/issue58-fpm/fse-rtl/`. These flags request observation, not a new source
or optimization hypothesis. Require the two generated objects to match their
earlier O3 counterparts before interpreting the RTL. Inspect copy/enable/reset
dependencies and scheduling choices before declaring the next source cross.

### RTL observation and next predeclared cross

The initial driver invocation with `-dumpbase PATH` rejected the source as
duplicate global definitions; no object from it is interpreted. Preserve
`fse-rtl/` as the failed diagnostic invocation. Corrected controls in
`fse-rtl-v2/` use each cell's container working directory for dump placement
and absolute paths to the same include directory/compat header. Both objects
are `cmp`-identical to their prior O3 controls (2/2), with all dumps available.

In `.27.flow2` the assignment's copy (insn 19) is `mem/s:BLK`, alias set 6,
56 bytes; builtin memcpy is `mem:BLK`, alias set 0, same size. The SI state
stores are alias set 3, the HI stores set 7. The `.33.sched2` dependency table
is the decisive observation: assignment copy 19 has edges to HI resets but
not SI stores 21/23/25/27/29/45 (`lms_force/pll_on/tilt_on/lms_on/freq/phase_acc`).
Those six stores have priority 6 against the copy's 5 and are all scheduled
before it. The `fresh` argument reload (390) follows the SI stores. With
memcpy, copy 19 has edges to all six SI stores as well, and priority 8.
Thus moving only `freq/phase_acc` in an otherwise fixed assignment cannot
withdraw the typed-copy independence that made them ready early.

**Continuation source batch A: 56 full-TU cells at retained O3.** Treat the
four ordered enable stores as one contiguous block. Place that block in all
14 slots of the remaining 13-store chain (11 short stores plus `freq` and
`phase_acc` retained in their existing relative positions). Cross these slots
with **2 copy forms** (assignment/builtin memcpy) and **2 copy positions**
(first statement of this region/immediately after the enable block):
`14 * 2 * 2 = 56`. No new freq/phase placement enumeration. All allocations,
loops, functions, field types and normal-build branches stay unchanged.

Prediction: a dependency-preserving copy after the enable block may reproduce
the reference's four-enables / copy / other-resets sequence. First-copy cells
control the effect of moving just the enable block. Falsifier: no exact full
initializer in the complete domain. Any matching builtin form is a mechanism
candidate pending an ordinary-source spelling and independent review.

### Batch A result and predeclared continuation batch B

Batch A: **56/56 compiled, zero rejections, zero exact hits, 32 distinct
objects**. The preselected mechanism anchor `memcpy-after-enable-00` restores
the reference's four enables then copy then `freq`, unlike the corresponding
assignment control. It still schedules `phase_acc` ahead of the `fresh` stack
reload and short resets. This is not a retained near-match; the observation
isolates a second dependency after the copy mechanism moved.

**Batch B: 18 full-TU cells at retained O3.** Test the field-type/alias
explanation with three phase forms: retained `int phase_acc` and zero literal,
`int phase_acc` with `0L` literal (negative control), and `long phase_acc` with
zero literal (same 32-bit width on this compiler, different C type). Cross
those with three copy spellings (assignment, builtin memcpy, ordinary `memcpy`
with `<string.h>`) and two copy positions (first / after the four enables).
`3 * 3 * 2 = 18`. Keep the enable block at its original slot and every other
store in the original order. This does not repeat the freq/phase placements.

The header experiment is an **isolated per-cell diagnostic overlay** beside
each source, reached by its existing quoted include. All 18 cells get a full
header copy, six change only `int phase_acc` to `long phase_acc`; source-tree
headers remain untouched. Record overlay hashes. A match would motivate
review of a distinct 32-bit declared type, not license an LP64-breaking header
edit. The literal control distinguishes a field's alias set from a spelling of
zero. Prediction: only the declared type can remove `phase_acc`'s dependency
on the `int fresh` stack reload while a byte copy retains the copy boundary.
Falsifier: the type change leaves that dependency/order, or no full body matches.
This is the second and final source batch of the requested continuation.

Batch B completed: **18/18 compiled, zero rejections, zero exact initializers**.
Before reporting the type mechanism, emit two more RTL diagnostic copies of
already-tested ordinary-memcpy-after-enables cells (`int-zero`, `long-zero`),
with the same dump-only flags, and require whole-object equality with their
batch-B objects. These add no source/profile candidates. Stop source exploration
after this second batch; no near-match will be retained.

### Continuation conclusion: mechanism resolved, initializer still different

**No source or header candidate is retained.** Two source domains completed:
56 enable/copy cells and 18 copy/type cells, all compiled successfully, no
exact initializer. The normal allocation arms and the SRE packet are intact.
The new valid diagnostic controls also compare whole-object equal to their
non-dump counterparts, 4/4. The invalid `-dumpbase` invocation is separate and
its three accidental root dumps were moved to
`fse-rtl/invalid-driver-dumps/`, not discarded or interpreted.

The second batch gives the following complete target ledger; each entry is
BYTES(N), each function remains 458 bytes:

| Copy | Position | int/0 | int/0L | long/0 |
|---|---|---:|---:|---:|
| assignment | first | 56 | 56 | 56 |
| assignment | after enables | 56 | 56 | 56 |
| builtin memcpy | first | 82 | 82 | 39 |
| builtin memcpy | after enables | 49 | 49 | 20 |
| ordinary memcpy | first | 82 | 82 | 39 |
| ordinary memcpy | after enables | 49 | 49 | 20 |

Equal scores are not equal code: the assignment's long-field cells change
initializer bytes against baseline even while keeping BYTES(56). The audit
measures bodies directly. The zero-literal pairs are **whole-object equal,
6/6**, as are the ordinary/builtin memcpy pairs **6/6**. The unmodified local
header overlay reproduces the original whole object, 1/1.

The type prediction is confirmed narrowly by RTL, but **the complete-source
hypothesis fails**. In both final diagnostic copies, insn 45 is the store to
`phase_acc`, and 390 reloads `fresh` from the stack. With `int phase_acc`, the
store has alias set 3 and a forward edge to 390, priority 6. With `long`, it
has alias set 12, the edge to 390 disappears, its priority falls to 1, and 390
has 7 dependencies rather than 8. Its size remains S4 and alignment A32.
The copy is still a 56-byte alias-set-0 block operation. See
`fse-type-rtl/*/fpm_fse.c.27.flow2` and `.33.sched2` (dependency table starts
at line 12449 in both sched2 dumps). These are original C alias classes
affecting scheduling, not a load/store-width discrepancy.

The long-field, ordinary-copy-after-enables diagnostic emits `phase_acc` in
the reference's scalar-store location, but **still differs by 20 bytes**:
the `fresh` reload, `%esi` zeroing, condition test and nearby short stores
are scheduled differently. It is not an adoption candidate, and the numeric
improvement does not recover `long` as an original declaration. In particular,
no LP64 header change is licensed by this 32-bit mechanism probe.

#### Full-TU and provenance accounting for the continuation

**78 valid recorded full-TU compilations** = 74 source cells + 4 dump-only
controls; **312 shared-function verdicts**, four per cell; 35 distinct full
objects across these records. The failed first diagnostic has one rejected
invocation and no valid object, excluded from that denominator.

* No exact gains or losses. `FPM_FSE_free` remains the sole exact function.
* **Only `FPM_FSE_init` changes** in every changing cell, including the
  field-overlay controls. `FPM_FSE_receive`, `FSE_getdiag`, and free remain
  byte/relocation-identical to baseline; their reference gaps remain SIZE(35),
  SIZE(16), and EXACT respectively.
* No defined/undefined symbol inventory, binding, allocated-nontext content,
  alignment, or canonical relocation-target multiset changes. All four
  exports stay `FUNC GLOBAL DEFAULT .text`. Complete symbol tables, relocation
  sites and bodies are retained per cell, and disassembly covers all four
  bodies for every distinct object. No new call edge or helper is introduced;
  ordinary memcpy inlines as the same `rep movsl`, not an external call.
* `src/` and `include/` remain Git-identical to the worktree base. Previous SRE
  sources and objects are checked against their recorded hashes (126/126).
* Same Gentoo compiler/image/assembler and retained O3 profile as above.
  Matrix commands append the reproduction define through the shared helper.
  Dump-only commands additionally use `-da -fsched-verbose=5`; their working
  directory is the cell folder and include paths name the same files with
  `/src/` prefixes. Actual commands and local-overlay hashes are in manifests;
  image ID/digest, tool/source/header/blob hashes and flags are also recorded
  in `fse-continuation-audit/identity.json`.

Reproduce the continuation after the earlier valid controls exist:

```sh
python3 tools/issue58_fpm.py fse-rtl
cmp build/issue58-fpm/fse-rtl-v2/fse-rtl-assign/fpm_fse.o build/issue58-fpm/matrix/fse-assign-O3/fpm_fse.o
cmp build/issue58-fpm/fse-rtl-v2/fse-rtl-memcpy/fpm_fse.o build/issue58-fpm/matrix/fse-memcpy-O3/fpm_fse.o
python3 tools/issue58_fpm.py fse-enables
python3 tools/issue58_fpm.py fse-types
python3 tools/issue58_fpm.py fse-type-rtl
cmp build/issue58-fpm/fse-type-rtl/fse-type-rtl-int-zero/fpm_fse.o build/issue58-fpm/fse-types/fse-types-library-after-enable-int-zero/fpm_fse.o
cmp build/issue58-fpm/fse-type-rtl/fse-type-rtl-long-zero/fpm_fse.o build/issue58-fpm/fse-types/fse-types-library-after-enable-long-zero/fpm_fse.o
python3 tools/issue58_fpm.py fse-audit
```

New artifact directories, all beneath `build/issue58-fpm/`:
`fse-rtl-v2/`, `fse-enables/`, `fse-types/`, `fse-type-rtl/`, and
`fse-continuation-audit/`. Earlier `control/`, `matrix/`, `orders/`, and SRE
evidence are not regenerated by continuation commands. No full gates,
differential test, partial-link comparison, commits, or delegation were run.

**Stopping point / next discriminator:** The formerly fixed copy/enable
boundary and phase-field alias mechanism now have measurements. Before another
source matrix, examine the remaining `freq` store → `fresh` reload dependency
and the incoming argument's declaration/materialization, together with any
independent evidence for the original accumulator/config field types. Compare
their RTL ready times against the exact reference instruction schedule. Do not
take the long-field cell as an accepted starting source or repeat either store
placement enumeration. Any new experiment must explicitly distinguish a
declaration hypothesis from a copy-alias/source-order interaction and still
match the complete initializer, not merely move the remaining reload.

## Final requested discriminator: fresh argument materialization

Parent reports SRE/SGD retained in its integration worktree with phase 375/0;
that is parent-reported status, not a gate run from this worktree. This worktree
remains at `591e9940` with no source/header edits.

### Reference-bounded domain, declared before compilation

Reference `FPM_FSE_init+0x41` loads the argument from `0x28(%esp)` with a
32-bit `mov` into `%ecx`; `+0x53` tests `%ecx` at full width. No `movswl`,
`movzwl`, `cwtl`, narrow register test, or narrowing store intervenes on this
argument. A short formal or short receiving local is therefore **not licensed**:
it could turn a value whose only set bits are high into zero, unlike the blob.
At this compiler's ABI `int`, `long`, and `unsigned long` are all 32 bits;
the full-width truth test alone does not distinguish their signedness or C
alias type. Test these as hypotheses, not public ABI changes.

**One batch of 20 full-TU cells:**

* five argument/receiving forms: direct `int fresh` (baseline), direct
  `long fresh`, direct `unsigned long fresh`, `int fresh` received into a
  `long fresh_value` at function entry, and `int fresh` received into an
  `unsigned long fresh_value` at function entry;
* two copy forms: aggregate assignment and ordinary `memcpy` with `<string.h>`;
* two copy boundaries: first statement of the reset region / immediately after
  the four enable stores.

`5 * 2 * 2 = 20`. Preserve every state/config field type, including **int
phase_acc**; the earlier long-field probe remains a separate diagnostic.
Only direct long-formal cells change the prototype in their per-cell header
overlay, consistently with the definition. Receiving-local cells retain the
int prototype and use the local only for the existing `!fresh` condition.

Prediction: a different formal argument alias class can withdraw the SI
`freq`/`phase_acc` store dependencies of the stack reload; a widened receiving
local may retain the incoming int-slot dependency or force earlier argument
materialization. The crossed copy boundary distinguishes that effect from the
already-measured copy scheduling mechanism. Falsifier: no exact initializer
over the complete domain; equivalent receiving forms emitting identical objects
establish no type recovery. No source type is adopted on a partial match.

All 20 cells use retained Gentoo O3 flags, helper-appended bug reproduction,
and dump-only `-da -fsched-verbose=5` so the same batch records dependencies.
Previously measured int-form/copy controls must reproduce whole objects before
new cells are interpreted. Artifact directory: `build/issue58-fpm/fse-fresh/`.
No additional source batch, field retyping, full gate, or commit is planned.

### Final batch result: bounded negative, with a declaration ambiguity witness

**20/20 full-TU cells compiled, zero rejections, zero exact initializers,
six distinct full objects.** Each initializer is 458 bytes. Complete results:

| Copy / boundary | int formal | long formal | unsigned long formal | int → long local | int → unsigned long local |
|---|---:|---:|---:|---:|---:|
| assignment / first | BYTES(56) | BYTES(49) | BYTES(49) | BYTES(56) | BYTES(56) |
| assignment / after enables | BYTES(56) | BYTES(49) | BYTES(49) | BYTES(56) | BYTES(56) |
| memcpy / first | BYTES(82) | BYTES(50) | BYTES(50) | BYTES(82) | BYTES(82) |
| memcpy / after enables | BYTES(49) | BYTES(20) | BYTES(20) | BYTES(49) | BYTES(49) |

All four int-form controls are whole-object equal to the corresponding earlier
`fse-types` int/zero controls, 4/4. Each receiving-local form emits the same
whole object as its int-formal control, 8/8 comparisons. Signed and unsigned
long formals emit the same whole object at each copy/boundary combination,
4/4 comparisons. These **16/16 control comparisons** are assertions in the
final audit, not inferences from equal BYTES scores.

The RTL confirms the formal-versus-local distinction. For ordinary memcpy
after the enable block:

* Int formal: reload 390 has 8 dependencies. `freq` store 29 and `phase_acc`
  store 45 both lead to it, each priority 6. The parameter memory slot is
  alias set 3, S4/A32.
* Long formal: the parameter slot becomes alias set 42, still S4/A32. Reload
  390 has only 2 dependencies (stack setup 370 and block copy 27). Neither
  store leads to it, and both stores fall to priority 1. The four enable-store
  edges to that reload disappear too. Source fields remain **int**.
* Int formal with long receiving local: reload 392 still has 8 dependencies;
  the corresponding `freq`/`phase_acc` stores 31/47 retain their edges and
  priority 6. The local declaration does not force earlier materialization.

See the named cells' `.33.sched2` first-basic-block dependency tables,
beginning at line 12449, and the final reload RTL in the direct-long cell
around line 13906. All cells include complete dumps and `disassembly.txt`.

**A stronger negative than another near-match:**
`fse-fresh-library-after-enable-long/fpm_fse.o` is whole-object byte-identical
to the earlier `fse-types-library-after-enable-long-zero/fpm_fse.o`.
The former changes the *argument* to long and keeps `phase_acc` int; the latter
keeps the argument int and changes *phase_acc* to long. Their RTL dependencies
differ, but the emitted object does not. Thus the observed partial match
cannot distinguish these declarations, and neither can be adopted as recovered
source. The same emitted residual still has the parameter load/zeroing/test
and nearby short stores in a different order from the reference.

#### Full-TU result and final disposition

Final-batch denominator: **80 shared-function verdicts**, four per cell.
No exact gains or losses, no changed bystander bodies, no symbol inventory or
binding changes, no allocated-nontext changes, and no canonical relocated-target
changes. `FPM_FSE_free` stays exact; receive/getdiag stay exactly baseline
objects with their existing reference gaps. Four C exports stay
`FUNC GLOBAL DEFAULT .text`. The prototype overlays and receiving locals do
not introduce another exported symbol or call.

All 126 prior SRE source/object hashes are still intact. This worktree's
`src/` and `include/` are Git-identical to `591e9940`. **No FSE source/prototype
or field type is retained.** This investigation makes no normal-build or LP64
type change, does not claim differential validation, and ran no full gate or
commit. The earlier long-field overlay remains diagnostic only.

Reproduction, using the previously validated configuration:

```sh
python3 tools/issue58_fpm.py fse-fresh
python3 tools/issue58_fpm.py fse-fresh-audit
```

Each compilation is a complete FSE TU through `experiment_toolchain` using
the same Gentoo image/compiler/assembler identity and complete retained flags
recorded above; `-da -fsched-verbose=5` are dump-only additions, and the final
define remains `-DDSPLIB_REPRODUCE_BUGS`. Commands and header hashes are in
`build/issue58-fpm/fse-fresh/manifest.json`; all sources, consistent prototype
overlays, objects, RTL, disassembly, sections, symbol tables and relocations are
in its cell directories. `fse-fresh-audit/{audit,controls,identity}.json` record
full-TU comparisons, the 16 exact control comparisons, the cross-declaration
object equality, SRE integrity, image digest and source/header/tool/blob hashes.

**Next action:** leave FSE's original BYTES(56) source in place and keep its
part of issue 58 open. Retire this formal/local-type domain as exhausted.
Any future reopening needs independent declaration/copy evidence or a different
compiler-dependency/resource hypothesis for the remaining schedule. The new
discriminator is the **same final object from different RTL dependency graphs**:
compare the two preserved diagnostic schedules against the reference before
choosing another source family. Another isolated argument/field retype or a
smaller byte count cannot resolve that ambiguity. Parent SRE/SGD integration
does not depend on this unresolved FSE inquiry.
