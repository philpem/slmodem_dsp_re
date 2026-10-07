# EIA6 expansion recovers the copy structure, but not a complete function

At `548b5edd`, four valid complete-TU controls yield 64 emitted-body comparisons and no byte-exact gain or loss. All cells remain 6/16 exact. The raw unchanged baseline reproduces archived production. Only `V90PreFilter::setParamEia6` changes; all 15 bystander bodies, symbol bindings/imports/exports, BSS and nontext relocation identities remain unchanged. The unequal predicate adds one anonymous +0.0f pool entry (four zero bytes); all other allocated data remains identical.

| Copy | Predicate | Target bytes | Conditional backedges |
| --- | --- | ---: | ---: |
| Retained loop | Relational pair | 604 | 1 |
| Retained loop | Unequal | 598 | 1 |
| Literal 18 entries | Relational pair | 802 | 0 |
| Literal 18 entries | Unequal | 792 | 0 |
| Original | Original single compare | 790 | 0 |

The backedge detector fires on the retained loop and confirms its elimination, separately from the byte-exact grades. The combined candidate's two-byte size difference is **not a two-byte residual**: substantial operand, x87 and allocation differences remain.

The original graph has three diagnostics, reloads `params` after each relevant diagnostic, and copies the same 26 preliminary integer fields and four final integer fields. The conditional arm writes the narrowed deviation float at +0x84 and copies eighteen integer entries from +0x110..+0x154 into +0x88..+0xcc. There is no copy loop, and these two ranges do not overlap. The original single-zero comparison controls precisely that arm. No callback, pointer reload, store, width, arithmetic or bug reproduction was removed by the controls.

Important remaining witnesses: original first integer truncation uses `fistl` without popping x, while the combined candidate duplicates x and uses `fistpl`. Original loads the 10000.0f scale then multiplies through x87 registers; candidate uses a memory `fmuls`. Original float-zero test uses `fldz; fcompp`; candidate uses `fcomps` against the additional pool entry. Stack homes and copy scheduling/register roles also differ. Thus one cannot infer a complete source preimage from the close size. These observations are diagnostic, not permission to vary types, declarations, registers, stack slots or expression ordering by score.

No production source/header change was adopted. Close the declared four-cell expansion/predicate domain. Further work requires an independent operand/use/lifetime witness for the arithmetic or copy graph; the domain does not prove the author's spelling uniquely or imply global exhaustion.

Reproduce:

```sh
python3 tools/batch_cpp_prefilter_expand_reproduce.py \
  --domain docs/batch-cpp-prefilter-expand-domain.md \
  --baseline-dir build/production-before
python3 tools/batch_cpp_prefilter_expand_audit.py
```

Complete recorded commands, Gentoo compiler/selected assembler identity, header/source/object hashes and body verdicts are in `build/batch-cpp-prefilter-expand/results.json`; full audit and backedge locations in `audit.json`; original disassembly in `original-eia6.dis`; all compiler dumps and annotated assembly in each cell. The reproduction define is last in actual commands. No mutation/fuzz/runtime was executed. The audit's initial whole-blob metadata traversal was stopped and replaced with direct target-symbol decoding; the completed audit covers every candidate object's metadata/data and the original target graph without auditing unrelated original functions.

Follow-up F11852–F11853: independent combine/use, early magnitude lifetime
and actual tail-reload edge evidence closes setParamEia6 strict EXACT790B;
see [the later full-TU controls](eia6-scheduler-results.md). The earlier
finite-domain measurements above remain historical, not a global ceiling.
