# CID/DCR compiler and source audit

Audit of the issue-20 branch on 2026-09-09, starting at `4f7875ed`.
All experimental compilations used `dsplibs-tc342-gentoo`, with
`DSPLIB_REPRODUCE_BUGS`, GCC 3.4.2-r2, and the image's assembler and linker
2.15.92.0.2 (20040927). Host ELF readers inspected the resulting period-built
objects; they did not compile or assemble any reconstruction source.

## The DCR acceptance premise was incorrect

The existing `-O2 -fno-rerun-cse-after-loop` exception emits 567 bytes for
`dcr_process`, against 568 in the reference. Two different measurements were
misread:

- `SIZE (1)` reports the absolute difference in function lengths. It is not
  a count of differing bytes at corresponding positions.
- `alpha_why` / `--why` returns on the **first** rejection. A frame-size
  rejection at row zero supplies no evidence about later instructions.

The reference and candidate both have 161 non-padding instructions. Their
inventories nevertheless differ: the candidate has one extra `cmp`, one
fewer `jmp`, three `jg` in place of `jl` with reversed comparison operands,
and a memory-operand `idivl` in place of register-operand `idiv`.
Scheduling, spills and store placement also differ. Removing only the
initial stack-allocation instruction from the diagnostic sequences immediately
exposes another rejection: `xor` versus `mov`. This is an investigative
check, not a modification of the acceptance test.

Consequently the user's conditional acceptance of a stack-only difference
does not establish acceptance of this candidate. The existing per-file flags
remain in place pending a replacement decision, but their description as a
recovered original compiler profile is withdrawn. Neither source nor the
strict completion gate has been changed by this audit.

The active Gentoo driver specs do not enable SSP by default. Explicit
`-fno-stack-protector` leaves the candidate unchanged. `-fstack-protector`
enables protection and changes the three previously exact small DCR functions
as well. The banner's SSP patch version alone does not explain the residue.

## Bounded combined experiments

Each cell compiled the complete translation unit and scored every shared
function with `byteident.py`'s `body` and `verdict`, including the already-exact
constructors/deleters as controls. No alternative source was installed in
`src/`. A failed code-generation candidate was not promoted to behavioural
validation or accepted solely because its size approached the reference.

| Domain | CID cells | DCR cells | Exact process matches |
|---|---:|---:|---:|
| Dispatch shape / result type × 11 flag profiles | 176 | 11 | 0 |
| Selected source forms × 3 levels × 21 optimizer/stack profiles | 189 | 63 | 0 |
| Loop/scope/accumulator forms × 3 levels | 192 | 24 | 0 |
| Source forms × branch-probability/scheduling combinations | 768 | 96 | 0 |
| Selected source forms × 3 levels × 12 processor tunings | 108 | 72 | 0 |
| Total, including repeated control cells | 1,433 | 266 | 0 |

CID dispatch forms include the original success-first test, zero-first
nesting, and all six orders of a zero/success/default switch. Result locals
were tested as `short` and `int`; loop forms included while, for, an explicit
return-status guard and labelled control flow; declarations were tested at
inner and outer scope. DCR forms included local versus field accumulators
and separate summation versus accumulation directly into the field. These
are bounded source domains, not a claim to have exhausted equivalent C.

Flag domains included `-O`, `-O2`, `-O3`, the existing no-rerun-CSE pair,
aliasing, scheduling, register renaming, unit-at-a-time, loop optimization,
GCSE/load motion, CSE path following, block reordering, jump threading,
crossjumping, if conversion, strength reduction, outgoing argument handling,
SSP, register moves, caller saves, and branch-probability heuristics.
Processor tunings were i386, i486, Pentium, Pentium Pro/II/III/4,
K6/K6-2, Athlon/Athlon XP and K8, always retaining `-march=i386`.

The local experiment directories `build/cid-dcr-audit`, followed by suffixes
`2` through `5`, retain generated full sources, `compile.sh`, compiled
objects and `results.json`. These are ignored diagnostic artifacts. A
single container compiles each complete batch; the compiler, assembler and
linker identify themselves before the first compile. All 1,699 cells built
successfully and produced nonempty scores; varied outcomes and the known
exact small functions demonstrate that the experiment actually distinguishes
candidates.

A delegated follow-up added 33 variants: six declaration orders, three scope
or late-declaration forms, and all 24 function-definition orders. Independent
review found all 33 still reserve `0x5c`; 31 retain the one-byte length gap,
one has a 22-byte gap and one an 11-byte gap. Twelve function-order variants
regress `dcr_create`, showing that the ordering experiment affects real
code even though it does not solve `dcr_process`. GCC's reload dump annotates
`count` at stack offset `0x1c` and `over` at `0x3c`; these are real spill
accesses, not merely a different unused frame-size immediate. The artifacts
are under `build/dcr-frame-followup`. This bounds the tested scope/order
explanations; it does not identify the original source or fully explain the
additional 48 bytes of frame allocation.

## Global settings need exact-set comparisons

The earlier observation that the DCR profile changes 187/273 objects did not
measure whether it improved them. The completed comparison does:

| Period-build profile | Compared symbols | EXACT | REGALLOC | UNRESOLVED |
|---|---:|---:|---:|---:|
| Global O3, existing DCR exception | 1,852 | 813 | 52 | 4 |
| Global O2 plus no-rerun-CSE | 1,851 | 737 | 56 | 2 |

The exact-name sets gain **15** and lose **91**. The missing shared definition
is `GetNextDigitAndReturnNextState`, so even the denominator must be compared.
The reports are `build/cid-dcr-audit/baseline-exact.txt` and
`build/cid-dcr-audit/o2-norerun-exact.txt`, produced by `byteident.py
--list-exact` against `build/tc_repro` and `/tmp/tc-gentoo-o2-norerun`.

Three gains are `RD_delete`, `RD_process` and `RD_ring_details`, all in
`src/service/voice.c`. This is a useful follow-up lead, not yet a clean
file-level replacement: although its seven previously exact shared functions
stay exact, `RD_create` moves from a three-byte size gap to twelve and
`RingDetector_Process` from 88 to 98. Inspect inlining and full-TU source
organization alongside O2 before adopting an exception for that file.

An eight-cell follow-up identifies the mechanism. The reference calls
`RingDetector_Delete` and `RingDetector_GetLastRing` from these wrappers;
O3 inlines them and O2 preserves the calls. O3 with
`-fno-inline-functions` gives the same three exact-wrapper gains. Conversely,
the constructor's reference body inlines `RingDetector_Reset`; the existing
source fails to do so at both O2 and O3. Giving that definition explicit
`inline` makes the constructor's size gap fall from 411 to 17 bytes at both
levels, but does not reach exact identity. Thus inline eligibility and
optimization level are separate, measurable questions. This experiment is
recorded in `build/ring-inline-audit`; no source change was retained.

## Reproducibility gap

The main build defaults had been updated to Gentoo, but `flagsweep.py` and
`declorder.py` still defaulted to stock GCC, and `functionorder.py` hardcoded
it. That makes an unqualified future experiment inconsistent with the stated
reconstruction policy. The follow-up tool patch makes those experiments use
the Gentoo compiler path and native container user by default, while retaining
explicit image overrides for deliberately requested compiler A/B work.

The shared `tools/experiment_toolchain.py` also prints the selected compiler
and the assembler actually selected by that compiler, and appends the
reproduction define after user-supplied options. The declaration-order tool
requires a new or empty work directory instead of deleting prior output.

Independent review ran default invocations of all three tools: six generated
objects carried the Gentoo banner and all twelve comparisons of their two
known reference functions were EXACT. The fixture refuses to compile without
the reproduction macro; explicit `-U` attempts still compiled because the
tools restore the mandatory define. A planted invalid compiler option failed
with nonzero status and produced no object. Reusing the declaration work
directory was rejected with all files preserved. Help, Python syntax, image
tag selection and quoted compiler-path checks also passed. No behavioural
suite was rerun: production source and executable period-build flags are
unchanged; the period-build edits only correct explanatory comments.

Full-object completion still requires the strict partial-link comparison.
None of the exploratory results changes what that check accepts.
