# V32 SequenceE count-memory carrier (2026-10-02)

Baseline5c9f7b61, after PR238/240/241 and upstream V8 integration:
859/1852 exact functions,83,265 exact bytes. Follow-up to F11542's closed
four-cell count/array experiment. This is a new source carrier, not a replay
of chained-assignment spellings. No compiler flags change, fuzzing or
mutation execution.

## Evidence and finite domain

The blob stores DemodDataV32's unsigned-short result through `count` before
zero-extending it for DescrambleDataV32. The retained local `n` expands to
an SI pseudo fed by zero_extend(HI), then stores its lowpart. The earlier
direct-array cell matches all339 bytes except that adjacent store/extension
pair; chained assignment changes other live ranges and does not close.

Hypothesis: store directly through the existing output parameter, then read
`*count` as the descrambler argument. This keeps the initial store sourced by
the returned HI pseudo, and permits store forwarding into the call argument.
Cross that carrier with cached `regs` versus direct `hdx->regs` at the first
rate-sequence store, whose blob instruction keeps the hdx root+0x44 instead
of rebasing it+0x3c. Other uses of `regs` remain unchanged.

[Predeclared four cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5943188101),
[retention control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5943204507).
Prediction: both changes reproduce the target. Falsifier: unmatched store
order/body, changed neighboring functions or failed fixed differential gate.

| Count carrier | First rate store | Verdict | Exact TU functions |
| --- | --- | --- | --- |
| Local n | Cached regs | BYTES49 | 10/14 |
| Local n | Direct hdx array | BYTES6 | 10/14 |
| Direct count store/read | Cached regs | BYTES43 | 10/14 |
| Direct count store/read | Direct hdx array | EXACT | 11/14 |

All four cells preserve fourteen functions and fourteen global definitions.
Only RxHdxSequenceE changes; the combination gains its complete339-byte body,
with no exact losses. All cells retain length339. The unchanged full-TU
control raw-reproduces the baseline. Compiler/selected assembler identity,
full build-config flags, mandatory DSPLIB_REPRODUCE_BUGS, source/header/local
header hashes, function/global inventories, all verdicts and changed bodies
are saved in build/playbook-v32-count/results.json.

The combined initial RTL stores the returned HI pseudo directly to mem:HI;
the local-n control stores a lowpart of its widened SI pseudo. Final code
stores AX through count, then zero-extends AX for the descrambler call, as
the blob does. The cache-only and count-only controls separate both effects.
Known graph controls report4/4: the baseline/array-only cells widen first;
the count-memory cells store first, while only array-direct cells remove the
+0x3c rebase. Tool analysis.json, per-cell disassembly and initial RTL are
reproducible without compiling another domain:

```
python3 tools/playbook_v32_count.py --domain <predeclared-comment-URL>
python3 tools/playbook_v32_count.py --domain <predeclared-comment-URL> --analysis-only
```

This is a matching source family, not proof of unique author spelling. It
shows why a remaining instruction-order difference deserves a carrier/pass
explanation before being classified as an irreducible register-allocation
residual. Neither source change by itself closes this function.

## Retention and validation

Production adopts only the tested combined function. Independent audit
build/playbook-v32-count-adoption/combined.json confirms raw reproduction of
the winning full TU, fourteen functions/globals preserved and only the target
changed. Complete comparison build300/300, zero failed.

Existing fixed t_v32rxhdx fixtures cover SequenceE's counter, decode and
timeout branches, signed counter boundary and count/output behavior. These
are component fixtures with planted histories, not proof of complete modem
reachability. The source preserves the count value, truncation, output
ordering, all later calls and all protocol arms. Whole-tree comparison confirms **859/1852 ->860/1852**, exact bytes
**83,265 ->83,604**, exactly RxHdxSequenceE gained and no losses.
Report: build/playbook-v32-count-adoption/tree-after.json.

Complete300-object partial links use identical recovered TU order.
Positioned equal bytes **68,369 ->68,371 /943,398**; allocated914,174
unchanged. Exact section records70/92, relocation records1,022/18,317 and
symbol records394/2,907 unchanged. Both strict comparisons remain
DIFFERENT(exit1). The baseline includes the landed V8 recovery, so its
layout metrics differ from the pre-integration F11542 baseline. Artifacts
build/playbook-v32-count-adoption/partial/, including all objects/manifests,
link order, attribution and comparison JSON. This one canonical function
gain does not establish complete-object identity. Fixed non-fuzz period/
structural validation passes **385/0**, wrapper exit0, all structural
checks clean. Anchor metadata:10038 entries/285 suites, zero detached or
nonunique anchors; no retargeting needed. Refcheck:14223 references/2674
headings, no dangling/pending/stale entries. Tool syntax, whitespace and
four graph controls pass. New files were added to the tracked-file census
before validation so their references were included. No modern portability
claim, mutation execution or snapshot refresh.

The four-cell domain closes on retention. A parallel read-only reserve audit
also confirms that ModDataV22 and V22FP_modem should stay deferred: F8120's
six local forms and F10228's504-cell shift/alias/declaration/sample domain
already close those source explanations. ModData's call-clobber reload is
already correct; V22FP_modem's remaining argument-order mismatch needs saved
pass evidence. No further matrices, source edits or fixture claims are made
for those reserves.
