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
This rejects a flag-only replacement on the **current source**, not O2 as
an original compiler setting. Some source reconstructions may compensate for
the wrong optimization profile; an exact match under O3 does not establish
that their source form is original. Audit lost matches by translation unit,
source history and emitted mechanism, and test source/flag combinations
before deciding between those explanations. GitHub issue
[#22](https://github.com/philpem/slmodem_dsp_re/issues/22) tracks this inquiry.
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
The combined ring-detector follow-up is tracked in
[#21](https://github.com/philpem/slmodem_dsp_re/issues/21). Remaining DCR and
CID work is tracked in [#23](https://github.com/philpem/slmodem_dsp_re/issues/23)
and [#24](https://github.com/philpem/slmodem_dsp_re/issues/24), respectively;
these are task records, separate from the findings and deviations logs.

### Follow-up: source and flags can recover lost matches together

The first reviewed sample supports this possibility. In `v32seq.c`, O2 plus
no-rerun-CSE leaves calls to the plain `static` helper `v32_common_rate`,
where the reference contains its expanded rate ladder. This initially looked
like evidence against O2, but the absence of a helper symbol and repeated
ladder do not distinguish automatic inlining under O3 from an explicitly
`inline` helper under O2.

A four-cell full-TU experiment measured both source forms under O3 and
O2/no-rerun-CSE. Both O3 forms have 9 EXACT of 13 shared functions. Plain
static under O2/no-rerun has 5; explicit `static inline` restores the same
9-function exact set, recovering `SeqToRate`, `DecodeRateSeq`,
`CodeFinalRateSeq` and `CodeRateSeq`. The remaining functions still differ;
matching exact sets is not full-TU identity. The parent independently rescored
the generated objects in `build/lost-exact-audit/objects`.

Thus at least four of the 91 losses are recoverable through a source/flag
combination. This neither identifies the original global setting nor proves
all 91 losses have the same cause. No source or production flags were changed.
Issue #22 remains open for broader inline/declaration and optimizer controls.

For ring detection, independent review scored all 60 generated objects in
`build/ring-followup`, 21 shared functions each (1,260 verdicts). The domains
cover explicit Reset inlining, initial and reset-store order, the debug-call
sample-rate expression, and six local-temporary forms, under O2/O3. All 30
O2 cells retained 10 exact functions; all 30 O3 cells retained 7. None made
`RingDetector_Reset` or `RingDetector_Create` exact. Selected candidates emit
a 451-byte constructor against 452 in the reference, but this is only a
length gap: the constructor still differs, and Reset is 433 versus 449 bytes.
No candidate was retained or promoted to differential acceptance. Issue #21
tracks the remaining work.

### Issue 22: literal `-O` and attribution controls

The preserved application Makefile uses `-O`, but links an already-built
`dsplibs.o`; it does not establish the library's flags. On the recovered
Gentoo compiler, direct `-O`/`-O1` comparisons produced ten byte-identical
whole-object pairs: five C/C++ TUs under minimal and current explicit-option
profiles. Subsequent historical-profile experiments use literal `-O`.

An ownership map resolves all 91 lost exact symbols across 51 TU owner
groups, and all 15 gains across nine groups. This is a worklist, not a vote
for an optimization level. Its script and output are
`build/issue20-followup/map-profile-sets.py` and `profile-map.txt`.

The ring extension compiled 29 source forms under four literal-O profiles:
minimal, current explicit period options, and each with
`-foptimize-sibling-calls`. All 116 objects built; all 2,436 shared-symbol
verdicts were scored. Every cell has zero EXACT except current explicit
options plus sibling-call optimization, which has one: `RD_ring_details`.
Disassembly shows precisely the reference's four-instruction tail jump in
that case, versus a call/return without the option. No Reset/Create source
form closes. This identifies a relevant option without identifying the
original optimization level. Artifacts: `build/issue20-followup/o-ring*`.

V32's explicit-inline recovery also works under plain O2, without disabling
the CSE rerun: the same nine exact functions out of thirteen. A separate
twelve-cell literal-O domain tested plain/inline source with minimal or
current explicit options, plus sibling-call and unit-at-a-time controls.
Minimal profiles have zero exact functions; all full-option profiles have
two (`GetSequence`, `RateToSeq`). The parent independently rescored all 156
verdicts. Literal O therefore does not recover this TU in the tested source
domain, but is not excluded for other forms or additional options. Artifacts:
`build/o-v32`, including twenty earlier O2/O3 control cells.

No production source or executable flags changed in these experiments. The
strict partial-link baseline is unchanged, and no differential acceptance
is claimed for rejected variants. Open tests and further loss clusters
remain on GitHub issue #22, separate from this measured record.

The small-C control separates another confound: `B103LocLoopNextState` is
EXACT under plain O2 and loses that status only with no-rerun-CSE (15
differing bytes, same 138-byte length). The full B103 TU has 5/17 exact
under O2, 4/17 under O2/no-rerun, and 3/17 under O3. Plain O2 retains
the O3 exact set and also `TxHdxDataB103` and `RxHdxStartB103`.
This is not a claim that its non-exact functions all improve. Seven profiles
over B103 and PCM produced fourteen objects, independently rescored by the
parent (161 verdicts). Both fresh O3 objects are byte-identical to their
`build/tc_repro` controls. Artifacts: `build/o-small-controls`.

Two C++ controls independently confirm that the causes are mixed:

| TU | Shared functions | O3 EXACT | O2/no-rerun | O2 | O2 + web | O2 + inline-functions |
|---|---:|---:|---:|---:|---:|---:|
| ResamplerTiming | 14 | 11 | 4 | 6 | 6 | 11 |
| V90BitsToSymbol | 11 | 10 | 7 | 7 | 7 | 10 |

Plain O2 restores `ResamplerTiming::invertPhase` and `SdHalfBaudDft`;
their loss was the no-rerun option. Enabling automatic inlining restores
the other five ResamplerTiming losses and the three V90BitsToSymbol losses,
with the exact sets matching O3. The parent rescored all six new objects
(75 verdicts), preserved in `build/o-cpp-losses`. This establishes the
option mechanism, not whether a different original source could achieve it
without that option.

The PCM control also exposed an omitted option in the experiment, rather
than a reason to change its source. Gentoo GCC's `-Q -v` enabled-option
reports differ by `-funswitch-loops` between O3 and O2 plus web/automatic
inlining. A standalone O2+unswitch cell does not restore `alaw2linear`, but
O2+web+automatic-inlining+unswitch does: the **entire PCM object** is
byte-identical to faithful O3, independently checked with `cmp`.
The two additional objects and actual compiler-option logs are under
`build/o-small-controls`. This proves the combined setting reproduces the
control; it does not prove which combination the original author passed,
or that unswitch acts locally inside this loop-free function.

Two C++ source-inline probes bound the V32 analogy. The reference exports
`ResamplerTiming::reset(unsigned)` and `V90BitsToSymbol::nofBitsForNextTime`
as strong GLOBAL DEFAULT functions, of 106 and 134 bytes respectively.
Adding `inline` to the former's definition under O2 emits a WEAK 89-byte
COMDAT body; adding it to the latter emits no standalone definition at all.
The parent checked those symbol tables. These particular source edits cannot
reproduce the reference's binding surface, regardless of any caller gains.
Unlike the static C helper in V32, they are not successful source-inline
recoveries. Their isolated sources and objects are in `build/o-cpp-losses`;
other source organizations remain untested.

### Global plain-O2 control: fourteen losses were the extra CSE option

A fresh isolated build of all 273 TUs succeeded with plain O2, current
explicit options and `DSPLIB_REPRODUCE_BUGS`. The DCR exception was
explicitly overridden too, so this is genuinely global O2:

```
make -f tools/toolchain/period.mk -j6 \
  TC_OUT="$PWD/build/issue22-o2" \
  TC_EXTRA='-O2 -DDSPLIB_REPRODUCE_BUGS' TC_DCR_FLAGS=-O2
TC_OUT=build/issue22-o2 python3 tools/toolchain/byteident.py --list-exact
```

| Profile | Compared | EXACT | Gains vs baseline | Losses vs baseline |
|---|---:|---:|---:|---:|
| O3, provisional DCR exception | 1,852 | 813 | — | — |
| O2, no-rerun-CSE globally | 1,851 | 737 | 15 | 91 |
| Plain O2 globally | 1,851 | 754 | 19 | 78 |

Plain O2 restores **14 of the original 91 losses**, retains all 15 previous
gains, and adds four gains. It also loses `fComputeRMSValueShortBuf`, which
was exact in both earlier profiles: 77 old losses remain, plus this one new
loss. Thus the original 91 were not an isolated optimization-level test.
The missing shared-definition denominator remains important; neither O2
profile shares the baseline's `GetNextDigitAndReturnNextState` definition.

The four additional gains are `create_cid`,
`V90Phase4Modulator::recivedE2u`, `recivedCPtag`, and
`V90AutoDigitalImpDetector::applyPadGainToLinMapp`. The restored set includes
the three independently investigated B103/ResamplerTiming cases above;
the full fourteen-name set is in
`build/issue20-followup/three-profile-summary.txt`, alongside all directional
set differences. `compare-three-profiles.py` verifies the exact-name counts
against each report's denominator-bearing headings before producing it.
The complete plain-O2 report is `global-o2-exact.txt` in the same directory.

This is an experimental control, not an adopted global profile. No production
source/flags or completion criteria changed. Source-plus-options recovery
of the remaining losses and validation of the new gains remain issue #22.

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
