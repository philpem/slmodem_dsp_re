# Original CALL boundary inventory

Pinned2001434c and300 immutable production-before objects. Inventory scope69
C++ DSP/V90 TUs; do not interpret emitted-copy counts as strict whole-tree
unique-function counts. No source/header/flag changes, runtime/fuzz/mutation.

Predeclared independent PreFilter domain: original180B enum setFilter has
two JMP tails to FloatFIR::setCoefficients (case2/default) and one CALL
(case3 needs a subsequent gain store); retained178B has default CALL, case2
JMP and case3 CALL. Four complete-TU overlays independently replace the
existing terminal break with return in case2 and default. Keep case3
post-call gain store, source order, helper definitions, flags and headers
fixed. This tests terminal CFG ownership, not a proven incoming-alignment
reduction: one ordinary call to the same callee remains in every cell.
Require raw full-TU baseline reproduction under complete retained Gentoo
flags, mandatory bug define last, executed selected assembler and full-da.
Audit metadata/imports/data/nontext relocations and every canonical bystander.
Stop after these four cells without synonyms/permutations/flag controls.

## Measured inventory

`python3 tools/gcc3_alignment_callee_inventory.py` scans69 C++ DSP/V90
translation units and910 blob-common emitted function copies.633 copies are
strict EXACT; the277 remaining copies are not the whole-tree denominator.
Duplicated COMDAT emissions are counted separately here. Immutable objects,
blob SHA256 and full baseline build configuration are in the JSON inventory.

31 copies differ in direct CALL/tail-call target multiplicities. Six of those
are only CALL-versus-JMP differences to unchanged target families. Five rows
belong to excluded ADID/designer/other-session Demodulator work. Every accepted
edge requires R_386_PC32, a named FUNC/NOTYPE target, inline addend-4, and a
fully decoded five-byte E8/E9 instruction at exactly the relocation boundary.
Ambiguous section/addend controls are recorded, not silently treated as named
calls. This inventory does not infer indirect-call targets or actual incoming
alignment; a compiler/callee witness is still necessary.

The standard-library allowlist detects exactly two extra calls, both nanf:
ADID::porcessSecondStudy and designer::setConstellationToNoise. These are
closed source domains and are excluded from fresh candidates. The inventory
fires on both real positives and reports633 exact-copy negatives, in addition
to two positive/one negative Counter apparatus controls. There are zero fresh
standard-library-call-to-constant/instruction candidates within this scope.
The explicit allowlist is recorded in the tool; it is not an exhaustive libc
or compiler-runtime classifier.

## Original-backed shortlist and limits

* PreFilter::setFilter(enum,unsigned) has a genuine terminal-CFG witness;
  measured four-cell domain below. No incoming-alignment reduction follows
  merely from recovering a tail because the case3 ordinary call remains.
* FloatIIR scalar process398/299 emits compactIn/compactOut helper calls in
  reconstruction and original open-coded loops. Block process584/96 calls
  the scalar method only in reconstruction. These are diagnostic expansion
  candidates, not uniquely recovered original author open-coding; historical
  source-order permutations are closed and must not be repeated.
* Phase4Demodulator::trn2dKnownDemod183/248 has one original linear2alaw call
  versus two reconstructed calls, plus one linear2ulaw and generateSymbol
  call on each side. This is duplicated control flow, not a missing original
  companding call or a leaf-to-calling alignment transition. No new source
  domain is justified by the call count alone.
* ConstellationPower::calcModulusParameters/getPower move division/remainder
  calls across an outlined helper boundary. ModulusEncoder::progress has one
  extra __moddi3 but retains other division/remainder calls. These compiler
  runtime targets appear in the full target inventory, outside the standard
  library allowlist. Eliminating one occurrence alone does not establish a
  lower incoming boundary or a semantics-preserving algebraic source change.
* Reset/debug, packed-JD helpers and large phase methods show ordinary helper
  expansion/outline differences. They supply locations for tracing, not proof
  that a global inlining budget is the only cause. Caller/result width, source
  ownership and selected compiler alignment remain independent hypotheses.

## Four-cell PreFilter result

All four full-da Gentoo controls compile and the baseline object reproduces
its immutable full-TU object raw. Every cell remains6/16 exact, with zero
exact gains/losses. Retained target178B is SIZE2 against original180B. Case2
return alone is object-identical to baseline. Default return, alone or crossed
with case2 return, yields the same179B object and SIZE1 target: no exact gain,
so neither source spelling is adopted.

The explicit default return does recover original one ordinary CALL and two
JMP tails. Initial RTL contains six alternative ordinary call nodes for both
source forms; the sibling pass selects baseline two ordinary/one sibling,
versus candidate one ordinary/two siblings. The emitted call topology confirms
that result. It is a source terminal-CFG lever, with remaining instruction and
register choices; no further spelling/permutation cells are justified here.

The complete-TU audit passes4/4: all15 bystanders have identical raw bodies
and canonical relocation maps. Two constructor copies remain self-UNRESOLVED
under strict byteident because of section-relative selector data; the audit
preserves that classification, checks their bodies/maps literally identical,
and separately verifies all allocated nontext bytes, nontext canonical
relocations, BSS and metadata/imports unchanged. No selector is masked into
an EXACT claim. All other exports/private helpers/named data remain present.
Only enum setFilter changes in the default-return cells.

Replay and audit:

```
python3 tools/gcc3_alignment_callee_inventory.py --prefilter-reproduce \
  --domain docs/gcc3-alignment-callee-inventory.md \
  --baseline-dir build/production-before
python3 tools/gcc3_alignment_callee_inventory.py --prefilter-audit
```

Actual compiler/assembler identities and complete commands (including
DSPLIB_REPRODUCE_BUGS appended last), source/object hashes, full-da stages,
results.json, stage-call-topology.json and complete-tu-audit.json are under
build/gcc3-alignment-callee-inventory-prefilter. Two initial apparatus failures
(import path and sandbox Docker socket access) occurred before compilation;
no invalid compiled cell enters the results. Production source/header and
central findings/Playbook remain untouched. No runtime/fuzz/mutation run.

## Bidirectional all-object extension

`--all-tus` scans all300 production-before objects:299 C/C++ sources plus
src/core/pow.S;72 are C++ TUs. Source/object mapping is checked for exact set
coverage of the archive. The default69-TU mode and four PreFilter cells are
unchanged. Output defaults to build/gcc3-alignment-callee-inventory-all.json.
Each row includes missing as well as added standard-library CALL counters,
and separate CALL+TAIL target-family deltas. Ranked missing-call rows carry
source path and both counters, so a CALL-to-JMP change is distinguishable from
an absent dependency. These are direct relocation counts, not dynamic counts.

All-object result:1886 blob-common emitted copies,1075 EXACT copies,
84 CALL-boundary-gap copies,11 with only CALL/JMP topology changes. This still
counts emitted copies rather than strict worst-copy unique symbols. It does
not replace the parent's1852-symbol strict global census.

Added standard-library occurrences are memmove1, nanf2, sin1, cos1, fmodf1.
Missing standard-library CALL occurrences are zero; original-only library
CALL+TAIL target families are also zero under the explicit allowlist. Thus
this census finds no original-higher-library-boundary reverse lead. The five
library-gap rows (six occurrences) have these histories:

| Function | Blob/retained bytes | Extra retained calls | Original-backed interpretation |
|---|---:|---|---|
| RcFixed_Resample |2640/493|memmove1|Blob has no calls; retained rc_push uses memmove for sliding-history compaction. F11526's0/1 is blob/ours.|
| ADID::porcessSecondStudy |831/767|nanf1|Closed NAN sentinel domain; excludes fresh replay.|
| Designer::setConstellationToNoise |3641/3623|nanf1|Closed four-cell NAN source domain from PR265; excludes fresh replay.|
| beepgen_sample |491/488|sin1,cos1|Original FSIN/FCOS at0xad2a8/0xad2af; F8762 documents period GCC ignoring source's modern scoped pragma.|
| MTK_phasor |271/255|fmodf1|F11783 isolates Gentoo glibc mathinline.h selected by __FAST_MATH__; macro-only/profile diagnostics closed, not source algebra evidence.|

RcFixed is the clearest original-leaf/reconstructed-library-call location,
not an original memmove missing from reconstructed code. Its current
src/core/FixedRC.c rc_push compaction statement is an overlap-safe memmove of
short history elements. An original instruction/loop comparison of that exact
compaction region would be the next discriminator before a bounded literal
copy-loop source control. A copied ascending loop is not justified merely by
no-call evidence: pointer direction, overlap, count signedness/width, original
kind-specific machines and all owner/state writes must first be established.
The large2640/493 residual and historical recorded kind1 deviation mean a
single compaction edit cannot honestly be promised to recover this function.
No new RcFixed source experiment is run here.

Beepgen's extra sin/cos has independent instruction witnesses but currently
points to a known compiler/header profile choice. F8762's historical whole-TU
flag conclusion is not adopted as a new profile on count alone; period GCC
ignores the modern pragma, and the adjacent numerical functions need complete
TU review. PHASOR's header-vs-optimizer distinction is already measured and
closed in F11783. These rows are next-trace locations, not fresh spelling
matrices or justification for repeating closed controls.

## Final ownership review

Equal section/value/size function aliases are accepted only after one full
instruction decode of their shared range, then each alias copy receives that
physical direct edge. Unequal overlapping owners are unsupported. All
executable-section PC32 relocations with no sized owner, no direct E8/E9
boundary, or unproved named target/type/addend now have explicit diagnostic
records and denominators; they are no longer silently dropped. The immutable
blob and both source scopes contain zero equal-range alias physical edges
and zero unsupported executable PC32 controls.

An apparatus-only ELF symbol-range control changes saved PreFilter C2 to
share C1's exact range: eight physical calls are correctly reported in both
alias records, and the old orphaned C2 range reports eight unowned controls.
The negative control moves C2's range one byte into C1, producing16 explicitly
unsupported overlapping/unowned controls and no accepted alias edges. This
alters only symbol ownership metadata in two diagnostic objects; no source,
compilation or runtime is involved. Results and source hash are preserved in
build/gcc3-alignment-callee-inventory-controls/results.json and included in
both census JSON files.

Final default counts are69 TUs/910 common copies/633 EXACT copies/31
call-boundary-gap copies/six CALL-JMP-only copies. The increase29→31 fixes
missing-only tail edges previously omitted by the row filter, not an actual
production alias difference. All-object counts remain300/1886/1075/84 and11
CALL-JMP-only copies. Library direction counters and rankings are unchanged.
The PreFilter audit still passes4/4 with no source changes or new experiment.
