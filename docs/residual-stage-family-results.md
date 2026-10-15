# Residual compiler-stage family checkpoint

Merged PR283 as 736cf61c; new branch investigate/residual-stage-families.
[Predeclared domain](residual-stage-family-domain.md). No reconstruction source
or compiler profile changes in this checkpoint; source controls require separate
independent original witnesses. Baseline remains 1077/1852 EXACT, 118337 original
exact bytes; 673 SIZE, 66 BYTES, 31 REGALLOC, five UNRESOLVED.

## Inventory and complete baselines

The inventory reproduces canonical worst-copy grading over all 1852 shared
unique symbols. It checks the expected census, nominates 97 REGALLOC/BYTES
symbols from 59 source TUs, preserves every defining copy, canonical instruction
operands, first alpha rejection, config/census/object hashes. _iir_filter_create
is a known REGALLOC positive; exact V90 session dispatch is a negative. Neither
same size nor a first rejected row establishes the whole residual's cause.

All 59 unchanged complete TUs reproduce production objects raw; metadata,
nontext data/BSS/relocations and all emitted bodies agree. 804 shared body
verdicts across those baseline compilations, including repeated weak copies.
58 TUs have full -da/-dP streams. ADID's full dumps fail with an internal compiler
segmentation fault, including an isolated repeat and an attempt without -dP.
Those three failed attempts are retained and excluded from valid baselines.
The previously used ADID -dr/-dc/-dR/-dS subset succeeds with raw equality;
missing stage windows are unavailable, not zero. A source-list preparation
wrapper initially lacked its json import; no compiler control resulted from
that attempt, and the corrected remaining 27-TU run is separate.

Header nomination resolves explicit GCC [with ...] bindings. 85 targets have
one header; 12 are ambiguous repeated constructor headers. No C1/C2 ownership
is inferred from their order. Stage windows cover lreg/greg, flow2/peephole2,
peephole2/rnreg and rnreg/sched2. They are retained compiler transformations;
there is no original RTL against which to claim first divergence.

## Differing instructions linked to UIDs

Assembly -dP annotations can link a REGALLOC mismatch row to an instruction UID
only when the bounded assembly symbol has one-to-one instruction cardinality
and opcode order versus the padding-stripped object. Original instructions
remain the byte comparator's operand/relocation-aware sequence. Refuse ambiguous
clone headers, missing annotations and multi-instruction mappings.

25/31 register-only targets link successfully. Five constructor targets remain
ambiguous; ADID has no usable annotations. FloatFIR's differing scratch-load
UID appears at peephole2, demonstrating the observer. _iir_filter_create has
8 changed UIDs there despite zero searches in its independently measured prior
trace: zero materializations become XOR forms without a scratch search. Its
6 renaming changes also reproduce. An initial audit incorrectly asserted that
zero scratch searches implied zero peephole changes; the real control rejected
that assumption. It was corrected before any classification was adopted.

## Read-only scratch wave

Eight controls: seven complete TUs and one independent v8 repeat. Every saved,
raw and traced object triple is identical; pinned compiler hashes and unchanged
observer verified. Seven controls validate under the existing HI/SI r-constraint
availability model: 382 searches / 3784 candidate visits / 382 replacement
attempts, including repeats. V90MP's raw triple agrees but its five QI/q searches
are outside that validated model; preserve the refusal without broadening it.
No original cursor, compiler state modification or register forcing is claimed.

17 distinct current REGALLOC targets are covered by validated controls. The
scored Scrambler scalar and block process copies make zero searches. This rules
out direct scratch selection in those bodies; it does not exclude earlier
compiler history or establish a source fix. Other targets make one to eleven
searches; V90 setRdRtSymbols makes seven, so its dead-stack pop alone is not a
complete history accounting. Detailed assembler-name events stay in audit.json.

### Failed replacements also matter

Five rejected replacement attempts occur at three distinct sites: one
V92EchoCanceller::setEchoDelay search, two v8_process searches, and their two
independent-repeat counterparts. The failed searches return no selected
register and reset the persistent cursor to zero. The subsequent event stream
validates that state. A rejected replacement leaves no surviving generated
sequence but can change later scratch choices. An initial overly strict audit
required every attempted replacement to succeed and correctly failed on these
real controls; the repaired audit validates and counts rejection explicitly.
No assertion about the original compiler's failed attempts follows.

## Replay

```
python3 tools/residual_stage_inventory.py --census build/byteident-small-call-result.json --output build/residual-stage-inventory.json
python3 tools/residual_stage_reproduce.py --inventory build/residual-stage-inventory.json --exclude-source src/pump/v90/V90AutoDigitalImpDetector.cpp --domain docs/residual-stage-family-domain.md --output-name residual-stage-full
python3 tools/residual_stage_reproduce.py --inventory build/residual-stage-inventory.json --source src/pump/v90/V90AutoDigitalImpDetector.cpp --focused-dumps --domain docs/residual-stage-family-domain.md --output-name residual-stage-adid-focused
python3 tools/residual_stage_audit.py --inventory build/residual-stage-inventory.json --run build/residual-stage-full --run build/residual-stage-adid-focused --output build/residual-stage-audit.json
python3 tools/residual_register_uid_trace.py --inventory build/residual-stage-inventory.json --audit build/residual-stage-audit.json --output build/residual-register-uid-trace.json
```

The scratch manifest uses the existing label/directory/compiler/input schema.
Pass --scratch-manifest build/residual-stage-scratch-manifest.json to the stage
audit to generate the declared eight controls from its valid baseline directories.
Run gentoo_peep2_search_reproduce.py with that manifest, then
residual_scratch_audit.py --inventory INVENTORY --input TRACE_DIR --output REPORT.
Keep failed attempts in separate directories and include their results.json in
the stage audit to preserve refusal counts. No dump-only failure becomes source
or compiler-profile evidence until a raw baseline succeeds.

## Next discriminating work

Do not turn 97 nominations or 25 UID links into 97 source hypotheses. Use the
links to distinguish changed scratch-generated rows from earlier allocated
ones, then examine original use boundaries and branch-result sharing. Treat
failed-search history as part of any complete-TU carrier comparison. Existing
instantiation/definition-order permutations remain closed absent a new witness.
Clone ownership and the QI/q availability model remain explicit observer limits.
This checkpoint establishes reusable evidence; it claims zero new exact gains,
zero source adoptions, no fuzzing/mutation execution and no portability result.
