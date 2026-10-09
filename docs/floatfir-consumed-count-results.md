# FloatFIR countdown component recovery

The [two-cell domain](floatfir-consumed-count-domain.md) starts at ca84f1cc.
Both complete TUs use the actual Gentoo production flags and bug reproduction;
unchanged raw baseline reproduces production. The candidate uses
`if (count-- == 0) return;` before owner loads and `while (count-- != 0)`
at the tail. It recovers the reference's single DEC/CMP UINT_MAX at each boundary,
without the earlier control's redundant post-load entry test.

Zero count returns before all member/buffer accesses. For unsigned input C>0,
the entry guard leaves C-1; the do-loop executes C times before the tail sees
zero. Count 1 and UINT_MAX retain the same iteration count. The mutated local
count is never exported. Output/index/filter work remains identical to the
previous reconstruction:89 nonbranch, noncounter, nonpadding instructions agree;
all 7 other bodies and all 8 function positions/sizes agree. Symbol records,
allocated nontext data, BSS and canonical nontext relocations agree completely.
The negative baseline has no sentinel pair; candidate and original each have 2.

The processor remains 287B and nonexact:BYTES 244 becomesBYTES 222. All 4 existing
strict matches in this TU stay exact; there are zero gains/losses. This is a
supported counter component recovery, not an exact-function or cursor unlock.
The fixed lifecycle fixture and default period/structural gate validate the
adopted source; final measurements are appended below.

```
python3 tools/floatfir_consumed_count_reproduce.py --domain docs/floatfir-consumed-count-domain.md
python3 tools/floatfir_consumed_count_audit.py
```

Artifacts: build/floatfir-consumed-count, including all 16 body grades, complete
commands, source/header hashes, before/after objects, assembly,RTL and audit.
No new fixture, fuzzing or mutation execution. Finding F11876.

Two additional raw/traced/saved object triples agree. Their14 observed searches
(218 candidate visits) have identical function/generator/cursor/selection history.
Reset still enters 2, selects ECX and leaves 3; this component does not alter the
scratch residual. The audit checks the original-negative/current-positive
counter detector and every replay's provenance.

To populate the scratch controls before the counter audit:

```
python3 tools/gentoo_peep2_search_reproduce.py --manifest build/floatfir-consumed-count-manifest.json --output build/floatfir-consumed-count-traces
```

Both reproduction drivers generate their manifests after compiling their saved
inputs. Generic replay accepts only declared labels, pinned compiler choices
and worktree-contained saved-input directories.

Final production object raw-identical to the audited candidate. `make phase J=8`:
388 passed, 0 failed; all structural checks clean. `make tc J=8`: 300 sources,
300 objects, 0 failures. Static reference/anchor checks: 2,973 finding headings,
14,386 references, 285 suites and 10,038 anchors; no failures.
