# AGC cached predicates, partition and RMS boundary

Pinned6eaeaedd, retained Gentoo profile/mandatory bug define.
[Predeclared domains](agc-predicate-domain.md). Scope is original operations and
value boundaries, not minimum size or another declaration-order/flag matrix.

## Ranked leads and results

1. Original HI gain and level zero tests are the strongest operand witnesses.
   Nine cells separate int/short cached carrier/use-only narrowing for each.
   Short gain restores both gate word tests with the surrounding graph intact.
   Short level cache does not recover the original full guard. Use-only level
   under && has SIZE3 but branchifies the original eager boolean combination;
   reject despite smaller size. Four extra eager/logical × use-only/retained
   level cells restore original SETE, AX TEST, SETNE, MOVZBL, combined TEST/branch.
   Keep the int level carrier and wide alpha term; adopt eager boolean & with
   narrow predicate and short cached gain. One ordinary freeze read and an
   already-computed local have no evaluation side effects; comparisons give0/1.
2. Original partition JBE targets cold block increment, with small-tail fold as
   fallthrough. Two structured comparison/arm cells restore this branch by
   reversing both condition and arms. No hints, gotos or layout forcing.
3. Full chosen block length is distinct from the sample countdown. Two cells
   restore SI cached block_len/tail loads with unsigned len while retaining the
   unsigned-short countdown. Narrow RMS formal then exposes an argument
   extension absent from original; widen causal scope instead of hiding it.
4. Full2x2 RMS formal/countdown versus AGC length over10 TUs preserves RMS's
   entire62-byte body, while full/full recovers original AGC argument use.
   The only changed bystander is SRE_recover's argument load: its signed field
   becomes sign-extending, whereas original is zero-extending. An explicit
   unsigned-short normalization at that one call restores SRE's entire original
   candidate object, without changing the field's other signed uses. A second
   two-cell/all10-TU cross verifies it. No other caller source is edited.

The public source declaration and definition agree in all controls. RMS receives
unsigned length and explicitly initializes a word countdown, preserving modulo
65536 count semantics. Existing callers see the same low16 count; this is a
supported source/interface family, not proof the author's formal type was wide
or that the old declaration caused a runtime bug. Do not generalize a callee
word loop into a whole-interface word type. Retain normalization at the consumer
that the original actually narrows.

The smoother's MOV/add versus original LEA and private intermediate home remain
unresolved. Source already sequences alpha before beta. Saved local/global
reload comparison identifies132 shared instructions/12 stack substitutions,
37 inserted patterns/39 unclassified restructures in the partition control;
these are compiler observations, not a reconstruction of original liveness or
reason to add volatile/stores/register hints. Close this line without inventing
an intermediate merely to obtain its stack slot.

## Cell accounting

| Family | Complete TUs | Shared body verdicts |
| --- | ---: | ---: |
| cached predicates | 9 | 36 |
| eager guard | 4 | 16 |
| partition | 2 | 8 |
| chosen length | 2 | 8 |
| RMS boundary | 40 | 388 |
| RMS call normalization | 20 | 194 |
| total | 77 | 650 |

All cells retain exact sets and complete metadata/named/allocated data. Baseline
objects raw-reproduce their stated controls: predicate raw canonical; guard
short-gain candidate; partition eager/narrow candidate; length inverse-partition
candidate; RMS boundary inverse partition for AGC/canonical for others; final
normalization prior full/full cells. Distinct losing controls are preserved.
No uniquely recovered spelling, original allocator state, near-exact or complete
object identity claim. A SIZE1 body can still have a different frame and graph.

## Replay

```
python3 tools/gcc3_agc_predicate.py --domain docs/history/dsp/agc-predicate-domain.md --historical-headers --baseline-dir build/agc-predicate-before-objects
python3 tools/gcc3_agc_guard.py --domain docs/history/dsp/agc-predicate-domain.md --historical-headers --baseline-dir build/agc-guard-before-objects
python3 tools/gcc3_agc_partition.py --domain docs/history/dsp/agc-predicate-domain.md --historical-headers --baseline-dir build/agc-partition-before-objects
python3 tools/gcc3_agc_length.py --domain docs/history/dsp/agc-predicate-domain.md --historical-headers --baseline-dir build/agc-length-before-objects
python3 tools/gcc3_agc_rms_boundary.py --domain docs/history/dsp/agc-predicate-domain.md --historical-headers --baseline-dir build/agc-rms-before-objects
python3 tools/gcc3_agc_rms_normalize.py --domain docs/history/dsp/agc-predicate-domain.md --historical-headers --baseline-dir build/agc-rms-normalize-before-objects
python3 tools/gcc3_agc_predicate_audit.py
```

Use each prior selected object under its flattened production name plus retained
.build-config for the next baseline directory. RMS families include all10 source
objects. Drivers pin headers/source and preserve commands/compiler/selected
assembler identity, dumps, assembly, source/object hashes and results.json.
Archive all300 comparison objects and faithful partial object before adoption.
F11973 records batch-end gates and integration. No fuzz/mutation execution,
fixture edits, tolerance widening or modern portability claim.

The class1 complete-TU audit exposed an inspector bug: its76-byte,32-aligned
COMMON class1_state_functions is an object without section bytes. Treat COMMON
as storage metadata/alignment, not a numeric section index; all6 applicable
cells explicitly compare it. No skip or invented contents. Initial inspection
failed before conclusions; repaired audit reruns all77 cells. The deciding
initial phase passes388/0 but one static SRE locator is detached by the explicit
count cast. Retarget find/replace to retain the same threshold mutation; do not
execute mutations or rerecord snapshots. Final repaired gate is recorded below.

## Integration result

Final make phase J=8 exits0:388 passed/0 failed;285 suites/10038 static anchors
green. Unchanged AGC10 groups/51643 hard checks,476 return trials34 zero/442 one;
V23 lifecycle22 groups/4656926 checks pass. RMS's existing2049 result checks
remain hard; its whole62-byte object is unchanged. No new fixture is needed for
that exact callee. One/300 production objects changes,299/config raw-identical;
all300 faithful inputs raw-match canonical. Shared source interface and SRE
normalization edits leave their actual objects unchanged.

Census1082/1852 EXACT/119759 original exact bytes unchanged;669 SIZE/65 BYTES/
31 REGALLOC/5 UNRESOLVED; ratchet green. AGC567 bytes versus original566, but
179 nonpadding instructions versus175 and frame0x4c versus0x3c. The SIZE1 score
is an absolute function-size difference, not one differing byte or near identity.
Original guard, cached length, RMS argument and partition categories are now
recovered; prologue, spills, smoother and epilogue still differ. Partial .text
702396 and allocated918404 unchanged; positioned equal69567→69558/943398.
Both strict comparisons remain DIFFERENT/exit1. No new exact gain or byte-count
improvement, whole-object identity or modern portability claim.

Next promising reusable lead is the helper/caller argument boundary: audit
narrowed argument uses separately from a helper's internal word loops across
all callers. For AGC itself, obtain an independent value-lifetime or source
factoring witness before changing the smoother's stack home or LEA/add form;
these finite type/guard/partition/length/signature families are closed.
