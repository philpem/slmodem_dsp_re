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

## Issue 22: follow-up on the four new gains and the new loss

The Beepgen RMS loss is independently recoverable under O2 with `-fweb`.
All nine corrected full-TU cells were independently rescored (207 verdicts).
O2+web retains the same seven exact functions as O3, including both
`fComputeRMSValueShortBuf` and `detector_delete`; O2/no-rerun/no-web retains
six. Inlining and unswitching without web do not recover RMS. The reference
retains a preheader comparison which plain O2 deletes; changed padding keeps
both functions at 85 bytes. This is an optimizer interaction, not a newly
identified arithmetic defect. The first nine cells omitted register renaming
and are explicitly invalid controls; only the `*-rename` objects under
`build/issue22-rms` support these conclusions.

For `V90AutoDigitalImpDetector::applyPadGainToLinMapp`, the entire target
body differs by one byte under O3: a dead epilogue `pop` uses ECX where the
reference uses EDX. Plain O2 uses EDX; automatic inlining and the CSE option
also perturb this register choice through the TU. There is no observed
calculation change in this target. Ten recorded profiles were independently
rescored over all 34 shared functions (340 verdicts). Plain O2 has 13 exact
functions versus O3's 12. The full records are in
`build/issue22-adid/identity.json`; this does not certify the TU's remaining
non-exact functions or justify a source rewrite to steer a dead register.

### Rxcid: a source change can recover the O3 constructor too

The reference `create_cid` calls `reset_cid`. O3 inlines the current reset
body and emits a 612-byte constructor versus the reference's 183 bytes.
Plain O2 preserves the call and reproduces the constructor exactly. The
two-byte O2/no-rerun mismatch is only EDX versus ECX materializing the
diagnostic's constant argument, with earlier reset code also changed.

But this is not evidence for an O2-only original. The reference reset
branches between initialization arguments zero and one; our boolean argument
expression compiles to `sete`. Testing an explicit conditional with two
literal-argument calls gives GCC a different inlining-cost estimate and
recovers the exact constructor under O3 as well. A sixteen-cell domain
tested boolean/ternary/two branch orders, pointer versus direct-array clear,
and O2/O3. These are observations about compiled source candidates, not proof
of the author's unique spelling.

The original also expands `pack_next_bit` inside `cid_modem`. Explicit
GNU-C inline preserves its strong exported definition and restores that
expansion under O2, unlike the C++ weak-binding probes above. It does not
close the remaining function. Twenty-two source/option cells and eight
if-conversion controls scored all four shared symbols; disabling either
if-conversion pass does not reproduce the reset branch from the boolean
expression in this domain.

A further completed 384-cell source-order domain tested all 24 orders of
the four initial clears and all 16 combinations of four independently
differing store pairs, using the nonnull-first/direct-array candidate at
O3. Every constructor remained exact, but **no reset matched exactly**.
Thus the constructor recovery does not identify the reset's store order.
No source candidate was retained, and no behavioural acceptance is claimed.
The complete sources, commands, domains and scores are in
`build/issue22-rxcid` (430 objects, 1,720 symbol verdicts).

### Phase 4: exact-set gains conceal mismatched call boundaries

All twelve corrected option cells were independently rescored over 53 shared
functions (636 verdicts). O2 and O3 without automatic inlining have the same
43-function exact set; O3 and O2 with automatic inlining have the same 37.
Web and unswitching do not change this split in the tested domain. The six
gains include `recivedE2u`, `recivedCPtag`, `setRfSymbols`, `setRdRtSymbols`,
`recivedFirstRrnE2u` and `recivedPartTwoSilenceRrnSUVtag`.

That is **not** a clean file-level replacement. The reference's two large
pumps expand internal state helpers, as O3 does. O2 leaves seven such call
relocations in `generateV90Symbol` and thirteen in `generateV92Symbol`.
Their lengths are respectively reference/O3/O2: 2235/2228/1894 and
3922/3919/3305 bytes. The important evidence is the mismatched call
boundaries, not a preference for the smaller size gap. Selective inlining,
through options and/or original source organization, remains unresolved.
The initial macro-free batch is explicitly invalid and preserved under
`build/issue22-phase4-invalid-no-repro`; only the corrected
`build/issue22-phase4` matrix is evidence.

The threshold follow-up found a combined profile, after resolving a coarse
sweep's gap. Thirteen initial controls covered automatic-inline limits
20/40/60/80/100/150, six `-finline-limit` values and default O3. Limits
40–80 retained the six gains and fully expanded the V90 pump, but left one
unwanted `enterRepeatedCPd` call in the V92 pump. A second complete domain
tested **every integer 81–99**, plus 200:

- **81 and 82:** 43/53 exact, both pumps fully expanded, and the five-byte
  wrapper still tail-calls `recivedSUVtag`, as the reference does.
- **83–99 and 200:** 37/53 exact; both pumps expand but the wrapper is also
  expanded incorrectly.

The parent independently rescored all 33 threshold cells (1,749 symbol
verdicts), and checked the 81 pump bodies against the O3 control. Both raw text
bodies and their relocation dictionaries are unchanged; `byteident` still
reports UNRESOLVED for each comparison because each dictionary includes an
unproved section-relative target. This is not promoted to an EXACT claim.
The constructors change within BYTES; the remaining non-exact functions
retain their O3 bodies. The reference itself is still not fully reproduced.

Thus 81–82 satisfies the tested **call-boundary combination**, not proof
that the original build specified either number. Reconstructed source can
change the compiler's inline-cost estimates. Artifacts are under
`build/issue22-phase4-threshold{,-gap}`.

The global 81 control completed: **273/273 objects built successfully**, with
the existing DCR exception retained and bug reproduction enabled. EXACT rises
from **813/1852 to 815/1852**, but the set is **six gains and four losses**.
The gains are the six Phase4 functions above. The losses are both
`V92Modulator` constructors and `V92Jd::unPackJdReset` /
`V92Jd::unPackJdPhaseReset`. This is not an adopted global profile.

Independent full-TU rescoring gives V92Modulator 16/24 to 14/24 exact and
V92Jd 11/21 to 9/21. Both constructors change from the reference's exact
734-byte body to 581 bytes: auto81 leaves a call to `V92Modulator::reset`
where the reference and baseline expand it. Both Jd resets remain 20 bytes
but change from EXACT to BYTES(2). The next inquiry is selective expansion
and source inline-cost effects for the constructors, separately from the Jd
reset emission differences; a net gain cannot settle either question.
Artifacts: `build/issue22-auto81` and
`build/issue20-followup/global-auto81-{build.log,exact.txt}`.

No production source or flags changed, and no experimental candidate is
claimed differentially accepted. The strict partial-link result remains
DIFFERENT, with the unchanged production baseline of 54,109/943,398
positioned bytes, 905/18,317 relocations and 222/2,907 symbols exact.
Rxcid's outstanding source recovery is separately tracked in GitHub #25,
linked from #22; the new global-profile losses remain inquiries in #22.

### Global inline-limit follow-up: inspect the non-exact functions too

A complete 273-object length census of auto81 finds 26 shared functions with
changed lengths: 11 increase their absolute reference length gap and 15
decrease it. Length is a locator, not an accuracy score. Inspecting actual
relocations confirms additional call-boundary regressions invisible to the
six-gain/four-loss exact-set summary:

| Function | Reference / baseline / auto81 bytes | New call under auto81 |
| --- | --- | --- |
| V90CP::bitsToInfo | 2391 / 2350 / 1650 | evaluateCRC |
| V90Phase3Modulator::generateV90Symbol | 1790 / 1759 / 1081 | generateDIL, twice |
| V90Phase3Modulator::generateV92Symbol | 2044 / 1938 / 1278 | generateDIL, twice |
| V90Jd::getBitVector | 537 / 537 / 22 | packData |

These helpers are expanded in the reference and baseline. In particular,
`getBitVector`'s equal baseline length is not byte identity: it is BYTES(224).
Recovering only the four lost EXACT functions would not settle this profile.
The local census is reproducible with
`python3 build/issue22-global-inline-audit.py`; its complete rows are saved in
`build/issue20-followup/global-auto81-lengths.json`.

The experimental partial link uses the same recovered 273-input ordering
(172 anchored, 101 source-order retained) and Gentoo period linker:

| Strict partial-link census | Production baseline | Global auto81 |
| --- | ---: | ---: |
| Positioned bytes / 943398 | 54109 | 54676 |
| Exact relocations / 18317 | 905 | 904 |
| Exact symbols / 2907 | 222 | 222 |
| Candidate content size delta | -41996 | -50936 |
| Completion | DIFFERENT | DIFFERENT |

`partialcmp.py --require-exact` exits **1** for the auto81 partial link, as
required. The 567 additional matching positioned bytes are not a completion
claim or justification for accepting the call-boundary regressions. JSON and
text reports are `build/issue22-auto81/partial.{json,txt}`, with the baseline
JSON at `build/issue20-followup/partial-baseline.json`. No production changes
or differential acceptance are claimed for this experiment.

#### V92Modulator: expansion is recoverable, but plain inline loses an export

Fourteen full-TU cells combine O3/auto81 with seven source/option controls;
twelve compile and two `extern inline` C++ forms are rejected by GCC 3.4.2.
The parent independently rescored all **284 shared-symbol verdicts** and
verified both unmodified control objects are byte-identical to their existing
global-build counterparts.

An explicit `inline` declaration recovers the two 734-byte exact constructors
at auto81 but removes the strong exported `reset` definition entirely, even
with `-fkeep-inline-functions`. The denominator drops from 24 to 23, so
15/23 exact is not a valid replacement for either baseline. Moving the reset
definition after the constructor has no effect at either profile: the
compiler still sees its body across the whole TU.

Two diagnostic controls recover the constructors and retain the reset export:
`always_inline`, and duplicating reset's body at its call site. The former
produces an object **byte-identical to the complete O3 baseline**, independently
confirmed by the parent. This isolates the expansion mechanism, but gives no
evidence that the author wrote the attribute; the latter introduces unjustified
duplication and changes emission order. Neither is retained as reconstructed
source. Artifacts: `build/issue22-v92mod-inline/{run_matrix.py,results.json,run.log}`.

#### V92Jd: a preceding-emission carrier, not wrong reset stores

A complete 16-cell product tests pack-function swap, getters-last, unpacker
swap, and reset-function swap, under auto81. Parent rescoring confirms all
**336 verdicts**. Two cells give 13/21 exact, up from 9/21, without exact
losses: move both getters after the existing reset/data-unpack/phase-unpack
tail, leaving reset and unpack order unchanged. Either pack order works.
The four gains are both resets and both getters. This decodes a shared
ordering property in that domain, not a unique authorial order.

Equal scores are not unchanged bodies: parent raw-body/relocation comparison
finds an additional one-byte change inside `unPackJdData`, hidden behind its
unchanged SIZE(413) result. The other 16 shared bodies/relocation dictionaries
are unchanged. At the unchanged O3 profile, the useful getters-last candidate
leaves **all 21 function bodies and relocation dictionaries unchanged**, with
11/21 exact; this is not an independent function-identity improvement under
the retained flags.

Blindly copying the full reference emission order is different again: it
gives 11/21 under auto81 rather than 13, and changes non-exact unpackers.
At O3 it retains 11 exact, but parent inspection also finds 25-byte changes
in each packer, despite their unchanged SIZE scores. Source definition order
and final emission order are not interchangeable. No source change is retained
on these experiments alone. Records are in `build/issue22-v92jd-order`,
including `results.json`, `o3-control.json` and `o3-best-control.json`.

#### The other successful Phase4 threshold, 82

The global 82 build also completes **273/273 objects, zero failures**.
Whole-object comparison against 81 finds **272/273 byte-identical objects**;
only V90Demodulator changes. Its 26-symbol TU retains the same 16 exact
functions; `progress` changes from 7471 to 7750 bytes (reference 7276).
The complete census confirms **815/1852 exact**, the identical six-gain,
four-loss set measured at 81 against the 813-function baseline.
Thus 82 does not recover the four lost exact functions or the call-boundary
regressions discussed above.

Its strict partial link is still DIFFERENT: 54578/943398 positioned bytes,
908/18317 exact relocations, 222/2907 exact symbols, content delta -50648.
Artifacts: `build/issue22-auto82`,
`build/issue20-followup/global-auto82-{build.log,exact.txt}`. Neither endpoint
of the successful Phase4 interval is a clean global replacement.

## Issue 22: source visibility versus whole-TU analysis

A twelve-cell full-TU matrix crosses four source forms with O3, auto81 and
O3/no-unit-at-a-time: unchanged, explicitly qualified wrapper call, wrapper
definition before its callee, and a diagnostic noinline callee. The parent
independently rescored all **636 shared-symbol verdicts** and compared every
shared body/relocation dictionary with the existing O3 and auto81 objects.

Both unmodified controls reproduce all 53 shared bodies and relocations.
Strict object comparison also confirms every allocated section/content and
relocation record matches their respective baseline; the only defined-symbol
difference is the experimental STT_FILE name `baseline.cpp` instead of
`V90Phase4Modulator.cpp`. This is an explained experimental filename difference,
not a waiver for the eventual reference filename or partial-link gate.

The qualified-call form does not change any shared body at O3 or auto81.
Moving the wrapper before its callee likewise has no effect under those
profiles. But combining the move with `-fno-unit-at-a-time` preserves the
reference's five-byte wrapper tail-call: definition order becomes relevant
when deferred whole-TU analysis is disabled. Neither that move nor the option
alone recovers the wrapper from the O3 baseline.

The option has its own bystanders. Unchanged source at O3/no-unit stays at
37/53 exact but gains the Scrambler constructor and loses
`resetRRNSecondSection`. The moved-wrapper combination has 43/53 exact:
**seven gains and one loss**, not the same exact set as auto81. All non-exact
body/relocation changes remain in the recorded matrix. Equal aggregate counts
do not make these profiles equivalent.

Diagnostic noinline at O3 gives all 53 shared bodies/relocation dictionaries
identical to auto81. It isolates the callee-inlining decision as sufficient
to reproduce that TU's auto81 function results, without asserting that the
author wrote an attribute or that every resulting difference has the same
internal compiler cause. It remains diagnostic, not retained source.

Artifacts: `build/issue22-phase4-source-cost/{run.py,results.json,sources,objects,logs}`.
The associated agent workflow is now maintained in
`docs/method/experiment-design.md`, linked from AGENTS/CLAUDE and refinement.
No source or production flags were changed by this matrix.

### Global no-unit control: compiler rejection exposes a source constraint

The unchanged-source global `-fno-unit-at-a-time` build initially stopped at
Queue. A subsequent `make -k` attempted the remaining inputs and finished with
**271/273 objects present, two failed TUs**, exit 2. The failed TUs are
`src/dsp/Queue.cpp` and `src/pump/v90/V92Modulator.cpp`. GCC reports forced-inline
Queue helper bodies unavailable: `count` in both TUs, and `isEmpty`/`isFull`
in V92Modulator. This is a measured source/profile incompatibility, not proof
that the original global option is excluded independently of source.

There is **no global accuracy or partial-link result** for this incomplete
profile. Do not substitute the 271 objects for a complete build or quietly
borrow the two missing baseline objects. Logs are
`build/issue20-followup/global-no-unit-{build.log,keep-going.log}`; outputs
remain isolated in `build/issue22-no-unit`.

A bounded four-cell Queue control crosses retained O3/no-unit with original
header/removal of **only count's always_inline attribute**. The parent
independently rescored the three successful objects (18 shared-symbol
verdicts); the fourth reproduces the compiler rejection.

- Under O3, both original and attribute-removed Queue objects are **entirely
  byte-identical** to `build/tc_repro`'s Queue object, independently verified.
- Under no-unit, removing the attribute permits compilation and retains
  3/6 exact shared functions, but introduces three definitions absent from
  the baseline/reference: `count`, `copy1<float>`, and
  `dsplib_assign<float>`. It is not an accepted replacement.

This bounds the header comment's attribute rationale to its tested historical
context; it does not establish that removing it is neutral in other consumers.
Next work in #22 is to test Queue helper visibility/instantiation and header
consumers as a source/profile combination, preserving the reference's export
surface. No attribute is removed from production on this single-TU result.
Artifacts: `build/issue22-queue-inline/{run.py,results.json,overlay,objects,logs}`.

## Issue 22: Queue attribute and instantiation follow-up

The period dependency records identify exactly two production consumers of
`Queue.h`: Queue.cpp and V92Modulator.cpp. Source includes agree with that
dependency census. A Queue-only result must therefore be checked in the second
consumer before calling an attribute redundant in the reconstruction.

Historical commit `6ad802b6` added `count`'s always_inline attribute **and**
replaced explicit class instantiation with individual member instantiations.
Its missing-count-symbol rationale is not an isolated experiment establishing
the attribute's necessity: the source organization changed in the same commit.
This is a reason to cross the two hypotheses, not proof that the attribute
was wrong or that an alternative compiler profile is correct.

### Both consumers: all three attributes are neutral under retained O3

Eight cells cross original/all-three-attributes-removed headers, O3/no-unit,
and the two consumers. Six compile and the two original/no-unit controls
reproduce the body-unavailable errors. Parent rescoring covers **90 shared
symbol verdicts**. At retained O3, removing all three attributes produces
**whole-object byte identity for both consumers**, with no symbol changes.
This is broader than the earlier Queue-only/count-only observation.

The attribute-free no-unit cells both compile, but Queue introduces count and
two local copy helpers; V92Modulator introduces weak count/isEmpty/isFull,
loses its C1 exact match (16/24 to 15/24), and changes progress from 1094 to
1059 bytes. These are not accepted source/profile replacements.
Records: `build/issue22-queue-consumers/{run.py,results.json,REPORT.md}`.

### Complete diagnostic profile, with explicit object provenance

To measure rather than extrapolate, a separate complete candidate combines
the **271 successfully compiled no-unit objects** with the **two no-unit
attribute-free consumer objects**. No O3 fallback objects are used. The
dependency census confirms only those two production TUs include Queue.h;
the original manifest supplies the source inventory only. Every object has
its origin and SHA-256 recorded in
`build/issue22-no-unit-queue-plain/provenance.json`, generated by
`build/issue22-queue-consumers/assemble_diagnostic.py`. The unsuccessful
unchanged-header build remains separately recorded as 271/273, not relabelled
successful.

This complete diagnostic has **792/1852 EXACT**, versus retained O3's
813/1852: **15 gains and 36 losses**. The sets were independently checked
against their report counts. Gains include three ring wrappers (`RD_delete`,
`RD_process`, `RD_ring_details`), V90Phase3Demodulator constructors and
V92Phase3Modulator::generateTRN1u; losses include Resampler constructor
families and other bystanders. The entire sets are in
`build/issue20-followup/global-no-unit-queue-plain-exact.txt`.

| Partial-link census | Retained O3 baseline | Diagnostic no-unit/plain Queue |
| --- | ---: | ---: |
| Positioned bytes / 943398 | 54109 | 55673 |
| Exact relocations / 18317 | 905 | 897 |
| Exact symbols / 2907 | 222 | 220 |
| Exact content sections | 57 | 52 |
| Candidate content delta | -41996 | -32633 |
| Completion | DIFFERENT | DIFFERENT |

The improved positioned-byte count is not a reason to overlook lost symbol
and relocation identity. This profile remains a diagnostic, not an adopted
global setting. Link order is recovered by the same tool, with 273 inputs,
172 anchored and 101 source-order retained; the period linker is unchanged.

### Visibility and copy organization are distinct mechanisms

A fixed twelve-cell domain crosses six source forms with O3/no-unit:
retained; plain-count control; helpers earlier in class; inline template
definitions after class; inline template definitions before TU consumers;
explicit inline float specializations before TU consumers. Eight compile,
four reject, with **48 independently rescored shared-symbol verdicts**.
All six O3 forms reproduce the complete retained Queue object.

Generic-template placement changes do not fix no-unit's forced-inline failure.
Explicit float specializations do, and avoid a count export, but still leave
copy1/dsplib_assign local definitions and calls. No cell gains an exact member.
This identifies specialization availability as distinct from physical source
definition order, rather than excluding all source-order explanations.

The remaining copy calls justify one additional **two-cell** discriminator:
retain explicit float specializations, replace the seven copy1 call sites by
plain assignments with their increments preserved, and compile at O3/no-unit.
Both compile and retain the baseline symbol names/bindings without helper
definitions or calls. Parent rescoring covers **12 verdicts**; all defined
function bodies and canonical relocations agree between the two profiles.
The same 3/6 reference members are exact: scalar write remains 71 versus
reference 85 bytes, block write is 213 versus 211, read is 215 versus 197.
These are length gaps, not differing-byte counts or recovered source.

Whole objects still differ: no-unit emits the reset COMDAT after constructor
and destructor sections. The copy-helper organization is thus a separate
source/profile influence, and function agreement does not settle section
order or the three nonexact members. Neither specialization nor copy form is
adopted on this diagnostic alone. The domains are complete and stopped;
records are `build/issue22-queue-visibility/{results.json,copy-axis-results.json,REPORT.md}`.

### Cleanup validation found a pre-existing period failure

Before applying the O3-neutral attribute cleanup, the unchanged working branch
`744d1bb6` was gated with `make period J=3`: **374 suites passed, one failed**.
`t_v34hstx1` reports 7/25850 failed checks at case 6739, the XMITMP exit calling
initdigital. An isolated `make period J=3 T=t_v34hstx1` repeats the failure
(0 passed, 1 failed). First differing object bytes are +0xa42 through +0xa44;
diagnostic count is four versus five. Both make runs exit 2.

No production source edits had been made; the failure is not attributed to
Queue or to clean master without further reproduction. It is tracked in
GitHub **#26** with complete logs and next checks. The cleanup is deferred,
not committed despite its object-neutral controls. `make phase` and candidate
after-gates are not claimed: there is no applied source candidate. Baseline
logs and partial-link JSON are under
`build/issue20-followup/queue-attributes-*`.

### The baseline failure was an unowned fixture lookup (#26)

A fresh `PERIOD_OUT=build/issue26-fresh-period make period J=2
T=t_v34hstx1` repeated the same seven failures, excluding stale objects.
The working branch and master had identical tracked source, headers, tests,
tools and Makefile. Tracing the first differing fields established an
apparatus defect, not evidence for changing `initdigital` or its flags:

- The varied fixture left `rxbits = -30181` and `rx_use_max = -21730`.
  Rate reconciliation reaches zero, then both implementations read
  `rx_divtab[0 + 14 * (-21730) - 1]`, or entry **-304221**.
- This is 608,442 bytes before the dummy table, outside its owned arena and
  padding. Equal nominal table contents do not make those reads congruent.
  The existing diagnostic table probe skips dummy pointers, hiding this input.
- Ours read a value halving to 1038; the reference read zero and took its
  existing fallback to one. This explains divisor +0xa42, width +0xa44,
  and the extra ZERODIV diagnostic without changing the source or comparator.

The fixture repair bounds the negotiated inputs and aims each side's receive
table sixteen shorts into its own dummy block. It retains the original's
zero-rate `table[-1]` read in owned storage: case 6739 uses predecessor zero
and asserts divisor 1/width 8 plus the once-only latch; companion 6740 uses
512 and asserts divisor 256/width 9. Whole-object, signature, transcript and
address-independence checks remain intact. Focused Gentoo validation passes
**25,940 checks**, versus seven failures out of 25,850 before (+90 checks,
not a removed failure or relaxed tolerance). The full Gentoo gate now passes
**375 suites, zero failed** (before: 374/1). An isolated negative-control test
changes only side B's zero-case predecessor to 512: the existing detector
reports eight failed checks out of 25,943 and exits 1, all in case 6739,
including the expected divisor/width, arena, signature and transcript changes.
The nonzero companion remains green. Its additional three checks are existing
per-differing-byte reports, not a different test selection. Commands and output
are in master worktree `build/issue26-negative/run.{py,log}`; the full gate is
`build/issue26-full-period.log`. No production source, flags or partial-link
bytes change in this fix.

### Independent review of the no-unit ring gains

The complete `voice.c` TU has **21 shared function definitions** with unchanged
binding/visibility. Independent `byteident.py` rescoring confirms **7 -> 10
EXACT**, gaining `RD_delete`, `RD_process`, and `RD_ring_details`, losing none.
No-unit retains the reference calls to `RingDetector_Delete` and
`RingDetector_GetLastRing`; retained O3 substitutes those callees. Existing
plain-O2 and no-inline controls already discriminate this mechanism, so the
116-cell literal-`-O` source domain was not repeated.

The nonexact `RD_create` needs a more careful reading than its length gap
**3 -> 12 bytes** suggests. Parent review found that the reference DOES call
`RingDetector_Create`, as no-unit does; retained O3 substitutes allocation and
reset there. No-unit restores the reference call boundaries and offsets through
the diagnostic call at +239. Its twelve-byte deficit instead comes from three
switch arms sharing one EAX-to-stack store where the reference emits a separate
four-byte store per arm; the local config-address register also differs.
Thus the larger size gap is not evidence of a worse call reconstruction.
Do not confuse this wrapper with `RingDetector_Create`, whose reference body
contains reset work. Neither constructor nor reset
is recovered; `RingDetector_Process` remains SIZE(88) under both current
profiles (an older audit's SIZE(98) is not the current result). Therefore the
three wrapper gains establish call preservation, not original source order
or a justified production profile. Artifacts: `build/issue22-ring-no-unit/`,
`build/tc_repro/src_service_voice.c.o`, and the complete diagnostic profile's
`build/issue22-no-unit-queue-plain/src_service_voice.c.o`. Follow-up is #21;
global source/profile acceptance remains #22.

### RD_create switch domain: eight cells, no recovered body

The preceding switch residual was not covered by the 116-cell Reset domain.
A newly declared four-form by two-profile cross compiled the full `voice.c`:
direct assignment/break, direct assignment/goto, arm-local temporary/break,
and arm-local temporary/goto, each at retained O3 and O3/no-unit. All eight
compiled under Gentoo with reproduction enabled. Parent independently
rescored **168 function verdicts** and checked every body/relocation against
the appropriate profile control. Baseline controls reproduce all 21 functions
and bindings; their raw-file differences are confined to the generated
STT_FILE name in `.symtab`/`.strtab`, with every other section byte-identical.

Locals erase, leaving four distinct normalized emissions across both profiles.
Every O3 cell stays **7/21 EXACT**, every no-unit cell **10/21**: no source-form
gain or loss. Only `RD_create` changes between break and goto; all other
functions retain their full bodies/relocations. Goto reaches SIZE(2), 291 bytes
against 293, but still shares the EAX store and moves no-unit's constructor
call from the correct +193 to +209. The closer size is not accepted.
The codec-to-threshold map is preserved in every cell, but physical switch
targets differ. This excludes these four spellings under these two profiles,
not source/flag combinations generally. Do not repeat local/goto synonyms;
reopening needs a mechanism that separates the stores without sacrificing
known call positions. Artifacts: `build/issue22-rd-create-switch/`.

### Queue forcing removed without an object change

With the #26 fixture fixed, the unchanged-header master period baseline is
**375/0**. The actual working-branch removal of `always_inline` on `count`,
`isEmpty`, and `isFull` also passes the full Gentoo period gate **375/0**.
The reset attribute, function bodies and optimization flags are unchanged.
The header's earlier assertion that ordinary definitions necessarily emit an
extra helper was corrected to the measured source/profile scope.

A real `make partial-link` after the edit leaves **all 273 object SHA-256s**
and the entire partially linked file unchanged. The strict comparator's JSON
is identical before/after: 54,109/943,398 positioned bytes, 905/18,317 exact
relocations, 222/2,907 exact symbols, 57 exact content sections, and
**DIFFERENT**, exit 1 with `--require-exact`. This is maintainability cleanup,
not an accuracy increase or adoption of no-unit. Source commit `4504e0ba`;
artifacts `build/issue20-followup/queue-attributes-*`. Full phase completion
is tracked separately: its baseline also exposed an unused duplicate CID
wrapper type left in the core TU, now tracked as #27.

### Crossjumping explains the merged stores; source group order does not explain block placement

The next control tested a distinct compiler mechanism, not more source
synonyms: break/goto x unit/no-unit x crossjumping enabled/disabled, **eight
full-TU cells and 168 independently rescored verdicts**. Four existing controls
reproduce their previous objects byte-for-byte. The compiler accepts and
executes `-fno-crossjumping`: eight nonexact TU bodies change, with no exact-set
gain/loss or binding change.

The direct/break source with **both no-unit and no-crossjumping** restores
`RD_create`'s 293-byte size, separate arm stores, config-address register and
reference call offsets, including constructor +193 and final free +285.
It remains BYTES(11): physical arms are 650,850,1000 rather than the reference's
1000,650,850. No-unit alone leaves SIZE(12), no-crossjumping alone SIZE(9), and
the goto combination SIZE(18). The interaction is established; the flag is
not adopted. Other nonexact bodies and section layout also change.

That new observation justified a complete **six case-group permutations x
four flag profiles**, **24 full-TU cells / 504 independently rescored
verdicts**, with the default arm fixed last and each group's labels unchanged.
All four unchanged-order controls reproduce their predecessor bodies,
relocations and bindings; generated STT_FILE names explain the confined raw
symbol/string-table differences. The domain has fourteen distinct normalized
emissions. Every unit cell remains 7/21 EXACT and every no-unit cell 10/21.

With crossjumping enabled, all permutations collapse. With it disabled,
source order changes registers but **the physical order remains 650,850,1000
in all six cases**. No-unit/no-crossjumping's six results are BYTES(11,9,9,5,11,9),
not an exact preimage. The five-byte result matches registers by physical
position, NOT by threshold value. Parent decoding of all twelve jump-table
entries verifies the same codec-to-threshold meaning, but case 4/12 reaches
function offset +270 rather than +248, case 13/15 +248 rather than +259, and
case 14 +259 rather than +270. Both instruction placement and table targets
remain different, so that near-match is not retained or relabelled.

This completes and stops the case-group permutation line under these profiles.
The next distinct question in #21 is compiler block placement, not another
permutation: any future block-order/pass control must preserve the recovered
store separation and call offsets and report all other changed TU bodies.
No global/per-file flag change is justified by these diagnostic cells.
Artifacts: `build/issue22-rd-create-crossjump/` and
`build/issue22-rd-create-arm-order/`.

### Cleanup checkpoint: full phase green, PR #28

The unused `struct CID` copy in the core TU has no users; its used definition
belongs to `src/service/cid.c`. Removing only that duplicate in `5e6d70ae`
clears #27's gate: **301 types / 168 files / one existing registered duplicate,
OK**. All 273 object hashes, the complete partial link and strict-comparison
JSON remain identical to both the pre-cleanup and Queue-only outputs. The
full Gentoo period suite again passes **375/0**.

Finally `make phase J=3 -j3` on the combined cleanup branch passes, **exit 0**:
differential, 64-bit, interop, structural, coverage and debug tiers all OK,
with **49,031/51,416 source lines across 233 files, 1,425 debug sites and 35
anchored deviation sites**. The modern tier uses its existing compiler
divergence register; the deciding period tier has no allow-list. Log:
`build/issue20-followup/queue-cid-phase-after.log`. PR **#28** contains only the
Queue attribute cleanup and unused CID type removal; apparatus/guidelines
landed on master and were merged into that branch. Strict completion remains
DIFFERENT, and #22 remains open.

### No-unit Resampler losses: full-family review

Parent independently rescored **102 verdicts** across both profiles for all
**51 shared functions** in `Resampler`, `ResamplerTiming`,
`ResamplerTimingOffset`, and `V90Resampler`. Retained O3 is **40/51 EXACT**,
no-unit **27/51**, with zero gains and thirteen losses. All eleven previously
nonexact functions retain their bodies and canonical relocations. Per-TU
EXACT counts are 9/14 -> 9/14, 11/14 -> 6/14, 11/11 -> 7/11, and
9/12 -> 5/12 respectively. FUNC/OBJECT inventories and bindings are unchanged.

These account for **13 of the global no-unit profile's 36 losses**: twelve
constructor ABI symbols (six overloads, C1/C2) and `ResamplerTiming::reset`.
Reference/O3 expand later-defined reset or timing-offset helpers; no-unit
retains their calls. In particular the four TimingOffset constructors remain
91 bytes but become BYTES(18), replacing x87 instructions with calls to
`setTimingOffset`; equal size is not equal code. Timing reset gains a call to
`resetSdHalfBaudDft`; derived constructors replace expanded reset work with
calls to their own reset methods. These are actual reference call-boundary
constraints, not an inference from lengths.

The profile also changes TU emission order and section sizes: Timing `.text`
2281 -> 1945, V90Resampler `.text` 2486 -> 2134, and TimingOffset's constant
pool 24 -> 8 bytes. Therefore repairing caller instructions alone would not
settle partial-link layout. No new matrix or source change is justified by
this read-only batch; a future source/visibility explanation must preserve
these expansions, exports and layout, not merely recover thirteen names.
This does not rule out all no-unit/source combinations. Pinned objects and
complete provenance are in `build/issue22-resampler-no-unit/REPORT.md` and
`build/issue22-no-unit-queue-plain/provenance.json`.

## Original O2 losses: direct Resampler inlining control

The [symbol-by-symbol ledger](issue22-loss-ledger.json) partitions the original
91 losses into **91 controlled recoveries and 0 without a recovery route**.
This closes the name-set accounting, not whole-profile validation. Parent review
checked its 91 names against the original exact-set subtraction and verified
the recovery groups are disjoint:

| Recovery control | Original losses accounted for |
| --- | ---: |
| Plain O2, undoing the bundled no-rerun-CSE option | 14 |
| V32 static-helper inline source variant under O2 | 4 |
| O2 plus automatic inlining: ResamplerTiming / V90BitsToSymbol | 5 + 3 |
| O2 plus web, automatic inlining and unswitching: PCM | 1 |
| O2 plus automatic inlining: the two TUs below | 8 |
| O2 plus automatic inlining: V92 follow-up below | 5 |
| O2 plus web: Beepgen detector_delete (earlier omission corrected) | 1 |
| O2 plus web: V22/V32 crossed follow-up | 6 |
| V34 helper inline source or O2 automatic inlining | 1 |
| O2 plus automatic inlining and web: V92 crossed follow-up | 4 |
| Full-build O2 + automatic inlining + web: additional combination recoveries | 31 |
| Add unswitching to that combination: remaining isolated owner controls | 8 |

This is **not 91 proven source mistakes**. Five have a demonstrated plausible
source-inline recovery; the others have compiler-profile recovery controls.
All 91 now also have flag-only routes on unchanged source: the full combined
build recovers 82 together, and the residual nine owner controls below recover
the rest. Neither proves the author's source spelling. All 91 have owners (51 owner
groups), but ownership is not an explanation. Later auto81 and no-unit
experiments are overlapping, different-profile evidence, not extra completed
members. The missing `GetNextDigitAndReturnNextState` definition is a separate
coverage regression: alternative profiles compare 1851 symbols, versus 1852
at baseline. Plain O2 also introduces a new loss, `fComputeRMSValueShortBuf`,
outside these original 91. #22 remains open.

The six-cell unchanged-source screen in
`build/issue22-resampler-o2-inline/` crosses retained O3, plain O2 and
O2 plus `-finline-functions` for `ResamplerTimingOffset.cpp` and
`V90Resampler.cpp`. These are controls on the **original 91-loss question**,
not additional counts borrowed from the later no-unit profile.

All compilations used Gentoo GCC 3.4.2-r2, its selected period assembler,
the complete retained flags and `DSPLIB_REPRODUCE_BUGS`. Fresh O3 and O2
objects reproduce their respective retained controls byte-for-byte.

| Complete TU | O3 EXACT | O2 EXACT | O2 + automatic inlining EXACT |
| --- | ---: | ---: | ---: |
| ResamplerTimingOffset | 11/11 | 7/11 | 11/11 |
| V90Resampler | 9/12 | 5/12 | 9/12 |

All eight lost C1/C2 constructor symbols recover. More strongly, both
O2-plus-inlining **complete objects are byte-identical to retained O3**:
no bystander, relocation, data, binding or export difference remains.
TimingOffset expands `setTimingOffset` while retaining the qualified
`Resampler::reset` call. V90Resampler expands its own reset, exposing the
`ResamplerTiming::reset` and `setBllState` calls found in the reference.
Its three pre-existing non-exact symbols remain non-exact, including the
UNRESOLVED `setBllState`; this result does not upgrade them.

Parent review independently rescored all 69 function verdicts and verified
all six complete objects against their claimed retained controls. This
establishes an automatic-inlining mechanism for eight original losses; it
neither proves the original source spelling nor justifies global flag adoption.

## V92 original-loss controls and ledger correction

The bounded `build/issue22-v92-o2-inline/` experiment ran four unchanged
complete TUs at O3, O2 and O2 plus `-finline-functions`: 12 cells and 147
shared-symbol verdicts. All eight fresh O3/O2 control objects reproduce their
respective retained objects byte-for-byte. Parent review independently
rescored all 147 verdicts, checked control hashes and compared every shared
body/relocation map. Gentoo, period binutils and complete reproduction flags
were used throughout.

| TU | O3 EXACT | O2 EXACT | O2 + automatic inlining EXACT |
| --- | ---: | ---: | ---: |
| V92BitsToSymbol | 9/10 | 6/10 | 7/10 |
| V92Modulator | 16/24 | 14/24 | 16/24 |
| V92Parameters | 7/7 | 5/7 | 7/7 |
| V92Precoder | 6/8 | 4/8 | 4/8 |

Five original losses recover. The V92Modulator constructors expand `reset`
again (581 -> 734 bytes); V92Parameters constructors expand `init`, exposing
the reference's `setToDefault` and `loadParams` calls (15 -> 53 bytes).
V92Parameters' entire object reproduces retained O3. V92Modulator does not:
the already-nonexact `progress` body/relocations change; its other shared
bodies/relocations match O3. Exact-set equality alone would hide this.

The fifth recovery is precisely
`V92BitsToSymbol::process(unsigned char *, unsigned)` (`...processEPhj`),
not either overload taking `unsigned &`. Its length and call-target list
already agreed under O2; the pass restores its instruction/register choices
through the TU. The other two overloads lose the extra `nofBitsForNextTime`
call, but remain nonexact (468/reference468 bytes and 363/reference359).
Those are the only two shared bodies/relocation maps still different from O3.

V92Precoder's O2 and O2-plus-inlining objects are byte-identical. Its two
constructor losses retain the same four calls and 127-byte lengths as O3;
automatic inlining does not explain their remaining operand differences.
The already-nonexact `process` body also differs from O3. These controls stop
after 12 cells; they do not justify guessing inline thresholds for a mismatch
whose relevant helper call is already gone.

Separately, the 35-count checkpoint omitted an existing controlled recovery:
`detector_delete` is EXACT under the corrected O2+web Beepgen experiment
(`build/issue22-rms/o2_web-rename/src_service_Beepgen.c.o`). Parent rescoring
confirmed 133/133 bytes, zero differences. This adds one ledger member, not
new experimental progress. The invalid earlier non-rename controls remain
excluded. Five new recoveries plus this correction make **41/91**, with
**50 pending**. The original name set and measurement model have not changed.

## C-loss triage: one inline boundary, eight other differences

A read-only pass over the three complete owner objects examined nine pending
names: `ResetRx`, `SetRxRate`, `SetTxRate`; `RxHdxEpoch`,
`RxHdxRateSequence`, `RxHdxSequence`; `V34EchoPreFilter`,
`V34EchoUpdateDelayLine`, `V34EqualizerUpdateDelayLine`. Parent rescoring
confirmed all nine EXACT at O3 and nonexact at **plain O2** (not the bundled
no-rerun-CSE profile). No name is promoted to recovered by this inspection.

O2's `V34EchoPreFilter` calls exported `V34Filter2` where the reference and
O3 expand its 42-tap loop. This identifies a specific inline-boundary
discriminator. The other eight keep their call boundaries; observed changes
are instruction/register choices, store ordering, and LEA versus ADD/INC
forms. These are not evidence of eight source defects or justification for
rewriting their statements. Changes elsewhere in a TU can affect them.

The detailed read-only artifact is `build/issue22-c-loss-triage/REPORT.md`.
Remaining experiments are tracked in #22, separate from this measured record.

## Crossed follow-up: inline source versus passes, then web versus unswitch

Three bounded experiments add eleven local recovery routes to the preceding
41-count checkpoint. Parent review independently rescored **234 verdicts**
across 17 complete-TU compilations and checked the unchanged controls against
the retained object bytes. All use Gentoo GCC 3.4.2-r2, its selected period
assembler and the complete bug-reproduction flags. None changes production
source or flags.

`build/issue22-v34-inline-cross/` crosses retained source / explicit GNU-C
inline `V34Filter2` with plain O2 / O2 plus automatic inlining, plus one O3
control: five cells, 130 verdicts. Either explicit inline at plain O2 or
automatic inlining recovers the 139-byte `V34EchoPreFilter` caller (O2 alone
is 107 bytes with a helper call). The explicit source-only change affects
**only that caller** among 26 shared bodies/relocation maps; its other 25
are unchanged. The exact 61-byte helper remains strongly exported. This is
another plausible source/profile interaction, not proof of an original
keyword. All O2 cells still lose the two delay-line exact matches relative
to O3 and retain the unrelated `V34EqualizerCleanUp` gain. Source-only
collateral must not be confused with profile-wide collateral.

`build/issue22-c-web-control/` crosses two unchanged C TUs with plain O2 /
O2 plus web: four cells, 32 verdicts. `v22rate` rises 0/4 -> 3/4 EXACT,
recovering `ResetRx`, `SetRxRate`, `SetTxRate`; `V32rxhdx` rises 5/12 -> 8/12,
recovering `RxHdxEpoch`, `RxHdxRateSequence`, `RxHdxSequence`. Both O2+web
**complete objects are byte-identical to retained O3**, including the
remaining nonexact bodies, data, relocations and exports. Enabled-option
reports verify web off/on. No missing helper call was involved in these six
losses; source statement rewriting was not needed for their recovery.

`build/issue22-v92-pass-cross/` holds O2 plus automatic inlining and crosses
web off/on with unswitching off/on for two TUs: eight cells, 72 verdicts.
Web recovers both pending V92BitsToSymbol reference-taking `process`
overloads and both V92Precoder constructors. BitsToSymbol rises 7/10 -> 9/10
EXACT and both web cells reproduce its complete O3 object. Precoder rises
4/8 -> 6/8 with web, but **web without unswitching still changes the
already-nonexact `process` body**. Only web plus unswitching reproduces its
whole O3 object. Unswitching changes that nonexact body without moving the
EXACT count: it is not an inert option. No symbol/binding changes occur.

These results separate a source-inline hypothesis from web-driven operand
choices, and demonstrate why the global combination must inspect nonexact
bodies too. The local domains are complete; no threshold search was added.

## Complete combined-profile control: 82 losses recover together

After the local controls, the unchanged tree was built uniformly at
`-O2 -finline-functions -fweb`, including DCR (no retained no-rerun exception),
with the full reproduction flags. The isolated build completed **273/273
translation units, zero failures**. It is not a mixture of candidate objects
and baseline fallbacks. Reproduction commands:

```sh
make -f tools/toolchain/period.mk -j2 \
  TC_OUT="$PWD/build/issue22-global-o2-inline-web/objects" \
  TC_EXTRA='-O2 -finline-functions -fweb -DDSPLIB_REPRODUCE_BUGS' \
  TC_DCR_FLAGS='-O2 -finline-functions -fweb'
TC_OUT="$PWD/build/issue22-global-o2-inline-web/objects" \
  python3 tools/toolchain/byteident.py --list-exact
```

The authoritative whole-tree census is **804/1852 EXACT**, versus baseline
813/1852: **zero gains, nine losses**. All nine belong to the original91;
therefore **82/91 recover together on unchanged source**. PCM's ninth loss
already has a separate unswitching combination recovery, bringing the union
of demonstrated routes to 83 at this stage, not a hypothetical globally exact profile.
Thirty-one of those routes were not in the 52-member local ledger. They are
labelled *combined-profile controls*, not individually isolated mechanisms.

The remaining nine losses in this complete profile are `SetAdaptEqV22`,
`SetAdaptEqV32`, `SetEncoderV17`, `VPCMXF_Create`,
`VPcmV34GetCurrentSessionDP`, `V90SpectralShaper::applyFrameAction`,
`V90SignBitsExtractor::applyFrameAction`, `FloatIIR::reset`, and
`alaw2linear`. The missing Dialer definition is restored; no function-name
inventory or FUNC/OBJECT binding/visibility change remains versus baseline.

**236/273 whole objects reproduce baseline bytes; 37 differ**, carrying 56
changed common function body/relocation occurrences, not merely the nine exact
losses. Seven objects also change non-executable PROGBITS contents, including
switch tables, one string section and V34hshak's 8-byte constant pool. The
complete hash/inventory comparison is
`build/issue22-global-o2-inline-web/comparison.json`; ambiguous COMDAT/section
targets keep the existing comparator treatment.

Both partially linked objects were measured with `partialcmp.py
--require-exact`; both return **DIFFERENT, exit 1**:

| Strict census | Retained baseline | Combined profile |
| --- | ---: | ---: |
| Positioned bytes equal / 943398 | 54109 | 53675 |
| Exact relocation records / 18317 | 905 | 897 |
| Exact defined-symbol records / 2907 | 222 | 220 |
| Exact content sections | 57 | 57 |

The candidate was linked from its complete recovered-order manifest using
period binutils, without replacing `build/partial/dsplibs.o`. Reports and
JSON are `before-partial.*` and `after-partial.*` in the experiment directory.
No source or profile is adopted, and no differential acceptance is claimed.
This answers coexistence for 82 recovery routes; it does not establish the
author's command line or certify the changed nonexact bodies.

## Residual unswitch isolation: the original91 ledger is accounted for

The nine exact losses from the complete combined profile have nine distinct
owner TUs. A fixed 18-cell experiment compiles each unchanged owner at that
profile and with `-funswitch-loops` added; PCM is the existing positive
control. All nine fresh controls reproduce the complete global-build objects.
All nine unswitch variants reproduce their **complete retained O3 objects**,
including nonexact functions, data, relocations and bindings. The parent
recomputed all **304 shared-function verdicts** and independently checked
all 18 whole-object comparisons.

Artifacts are `build/issue22-unswitch-verified/{run.py,results.json,REPORT.md}`
and its command/option logs. Earlier preliminary delegated attempts failed
control/domain requirements and are excluded; only this fresh fail-closed
parent-run experiment supports the result. Every accepted control uses the
source-build flags, not the test harness dialect/flags, and Gentoo with the
final bug-reproduction define.

Eight newly accounted-for losses plus the already-known PCM control close
the original91 recovery ledger. **None requires a source change to recover
its exact reference function under the measured option combinations.** This
does not prove that the source is uniquely original, that O2 was the original
level, or that all91 are source defects. It establishes that the original
15-gain/91-loss observation alone cannot adjudicate those claims: source
changes and named passes must be separated.

The remaining #22 validation is now specific: build **all273** TUs with
O2 + automatic inlining + web + unswitching together, compare all functions
and nonexact residue, then validate that actual profile rather than a mixture
of objects. Nine isolated matching owners cannot certify its other264 TUs.
No global setting has been adopted; the existing O3 baseline and provisional
DCR exception remain in production. The last measured combined partial link
is still DIFFERENT, and the unchanged-source differential suite has not been
run at the proposed final combination.

## Dialer: missing-definition control, not an exact recovery

Three unchanged full-TU controls in `build/issue22-dialer-o2-inline/` test
O3, O2 and O2 plus automatic inlining, with both fresh controls reproducing
their retained objects. Shared reference definitions are 6 -> 5 -> 6;
all 17 scored function verdicts are nonexact.

Plain O2 expands `GetNextDigitAndReturnNextState` into `begin_next` and
emits seven helpers absent from O3. Adding automatic inlining removes those
helper definitions, restores the LOCAL parser definition and two calls to
it from `DialerProgress`. This is an inline-graph interaction, not missing
source behavior. The restored parser body/relocation map equals retained
O3, but remains 777 versus reference895 bytes; ambiguous section targets
are not upgraded to EXACT. `AnalyseDialString` and `DialerProgress` still
differ from O3, so restoring the symbol inventory does not restore the TU.
This result adds **zero** to the original91 ledger.

No production source or flags changed in this batch. Differential and strict
partial-link gates were not rerun for these diagnostic-only objects; no
acceptance is claimed, and the last strict completion verdict stays DIFFERENT.
The common-profile/source question and global combination validation remain
open in #22.

## Ring block-placement control: stop after four cells

The predeclared screen in `build/issue21-block-placement/` crossed source
case-group orders `abc` and `bca` with default block reordering and
`-fno-reorder-blocks`, holding O3, no-unit and no-crossjumping. Here `a`,
`b`, `c` select thresholds 1000, 650, 850 respectively. Both default controls
reproduce the preceding arm-order experiment's whole objects exactly.

Disabling block reordering makes physical arm order follow source: `abc`
now emits the reference order. However, the constructor call moves from
instruction +192 (relocation +193) to +248, and `RD_create` shrinks from
293 to 290 bytes. The full-TU exact set falls from 10/21 to 3/21, with
seven losses and no gains; 17/21 bodies change. All twelve switch entries
retain the correct semantic mapping, but their physical targets and other
sections move. A SIZE(3) result is a length gap, not three differing bytes.

Parent review independently rescored all 84 verdicts. The required call
placement was not preserved, so the conditional four-permutation extension
was **not run**. This identifies block reordering as the cause of the
invariant arm placement, but rules out simply disabling it as a recovery
in this domain. No production source or flags were adopted. Outstanding
questions remain in #21 and #22; the strict completion check is unchanged.

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
