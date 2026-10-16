# Shared-value follow-up on 04eee73f

Four bounded families compile 26 complete Gentoo translation units and review
166 body verdicts. Replay each `tools/*_reproduce.py` named in the corresponding
domain with `--domain docs/<name>-domain.md`; use an archived baseline directory
for historical replay after adopting source changes. Each raw baseline matches
its retained production object. Commands carry the complete configured flags,
DSPLIB_REPRODUCE_BUGS and saved intermediate/RTL dumps. No fuzz or mutation run.

| Family | TU compiles | Verdicts | Conclusion |
| --- | ---: | ---: | --- |
| shared-counter-results | 8 | 40 | No strict gain; cold arms refute common-store nomination |
| cid-shared-results | 4 | 16 | True common stores; conditional assignments do not reproduce bodies |
| guarded-quotient-results | 6 | 70 | No gain; SignBits reset loses identity, not adopted |
| receiver-field-increment | 8 | 40 | Three original counter components recovered; no strict gain |

`python3 tools/shared_value_batch_audit.py` rechecks retained baselines, current
object hashes, all grades, symbol bindings and allocated nontext data/BSS/
relocations. Only expected text positions may differ. No candidate is adopted
for size alone. The quotient and conditional-assignment controls remain outside
src. This closes the four declared domains, not all source-sharing possibilities.

## Adopted components

V27RX_eq_train, V27RX_decision and V29RX_eq_train use the direct field increment
and unchanged wrap check. Their fields remain unsigned short. In each, the blob
and selected control perform MOVZWL field, INC, CMP against AX, JE, normal word
store; the wrap arm has a separate MOVW immediate store. The baseline adds a
MOVZWL after INC. The first three normal projections now match without forcing
registers. Seven full-TU bystanders remain unchanged. V29RX_decision stays as-is:
its candidate and baseline change the base register before the store, outside
the validated tracer's domain. The refusal is not evidence of semantic error.

Modulo 65536, both source forms compute c+1 and replace exactly 0x8000 with
0x4000. This scalar identity is not lifecycle coverage; the period differential
is the behavioural gate. Existing alias-sensitive reads stay before the block.

## Branch-to-store diagnostic

`tools/branch_store_paths.py OBJECT SYMBOL --branch-offset OFFSET
--field-offset OFFSET --base-register REGISTER` follows both arms to the first
explicit store through a bounded straight-line domain. It compares instruction
sites, not arbitrary aliases. Calls, nested branches, cycles, unknown writes and
base changes cause refusal. `tools/receiver_counter_boundary_audit.py` proves
three known common-store cases (SDMv27 and two CID branches), three distinct
receiver stores, and three malformed refusals. It also checks the three adopted
counter projections and full-TU bystanders. The initial four-counter audit
failed on the unsupported base change; it is excluded, not counted as a pass.

Artifacts live in build/<family>/results.json and build/*audit.json. Findings
F11878-F11880 and the Playbook record the usable observations and limits.

## Validation

`make phase J=8`: 388 period differential tests passed, zero failed; structural
gates pass. Fresh refcheck after documentation edits: 14,389 references and
2,977 finding headings, zero unresolved/pending/stale. Static anchors:
285 suites/10,038 anchors, zero detached or non-unique. `make tc J=8`:
300 sources/300 objects, zero failures. Both production receiver objects are
raw-identical to selected audited cells: V27 eq-1-decision-1 and V29
eq-1-decision-0. New tools compile; both read-only audits pass. Modern
portability was not run. The historical byte-ident floor remains unchanged.

Final whole-tree census: 1,075/1,852 exact, 118,176 exact original bytes;
exact-name set identical to committed04eee73f, zero gains/losses. Remainder:
675 SIZE, 66 BYTES, 31 REGALLOC, 5 UNRESOLVED. The surrounding PR still carries
SDMv27_init's earlier +1/+89-byte strict gain. The initial post-census raw-object
check used an incorrect subdirectory path; its wrapper exit is excluded. The
separate corrected check against flat tc_out names verifies both raw objects.

Next discriminating step: use actual common-store witnesses to nominate a
remaining register-only function, then compare branch-result lifetime at
24lreg/25greg before any source change. CID's signed arithmetic/narrowing
controls and these receiver wrap controls do not justify another spelling
sweep. Base-changing paths need independent alias evidence before extending
the observer.
