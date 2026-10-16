# Quality-mask recovery and bounded transfer pass

PR #267 merged at `8af3af53`. The next pass started with 1,052 exact names
out of 1,852, representing 113,867 original bytes. It sought twenty additional
matches and found three complete source recoveries, each 226 bytes:

| Function | Independent boundaries needed |
| --- | --- |
| `RxHdxDataV17` | Used boolean quality value, wide mask, terminal short return |
| `RxHdxDataV27` | Same mask plus unsigned-short public count and signed-short consumption |
| `RxHdxDataV29` | Same mask plus original byte flag writes through the existing union |

The direct algebraically equivalent mask is **not** enough. Initial RTL already
turns it into branches. A separately captured, used quality verdict retains the
original SETNE/NEG/AND value. V27 and V29 needed independent original width
witnesses too; neither of those axes alone reproduces its function. See
[next20-quality-mask-domain.md](next20-quality-mask-domain.md).

Three production objects reproduce the independently compiled winners raw.
The remaining 297 of 300 objects and the complete build configuration are raw
unchanged, including the V27 callee and all other direct header consumers.
The V27 protocol caller shares the changed API boundary, remains non-exact,
and is explicitly included in the audit. No structures or field layouts change.

## What limited this pass

The transfer checks found instruction mechanisms, rather than more complete
functions. These finite domains are closed with no source adoption:

| Area | Compiler cells | Complete emitted-body comparisons | Result |
| --- | ---: | ---: | --- |
| Three quality-return families and V27 consumers | 102 | 704 | Three gains; no losses |
| Fax no-carrier masks, owner captures and result widths | 24 | 128 | Mask partly recovered; complete bodies miss |
| DSP normalization helpers, FIR captures and tone owners | 30 | 240 | Output/publication or spill boundaries recovered; bodies miss |
| Fax symbol-coder masks, traversal and halfword transition | 11 | 55 | SETL/NEG/AND and CMPW recovered; bodies miss |
| V90/V92 helper, index, spill, sample, CFG and copy boundaries | 22 | 182 | Demapper reaches BYTES1; no complete gains |
| V32/V23/V22 receive/transmit boundaries and PPS traversal | 29 | 177 | All miss; no source adoption |

Total: 218 valid complete-TU compiler cells, 1,486 emitted-body comparisons
(1,438 blob-common verdicts). Repeated unchanged controls are included in these
execution denominators; this is not a count of distinct source candidates.
Every package reproduces its unchanged baseline and audits symbol metadata,
exports/imports, named/allocated data, BSS, canonical nontext relocations and
all emitted bodies. No exact losses occur in the bounded experiments.

The near misses are specific. Demapper's remaining byte commutes SIB base/index
registers; it is not permission to permute expressions. The normalization helper
recovers original word outputs but keeps different loop layout/allocation.
A delayed tone-energy cast is hoisted before its call, refuting that proposed
mechanism. The precoder block-copy control emits REP MOVSL absent from the
original; historical loop unrolling still leaves profile as a competing cause.
The old empty FSE tap loop remains declined under F1604/F6605: an empty emitted
loop does not establish an empty source loop.

## Reproduction

Use the pinned `8af3af53` production objects/config as `build/production-before`.
All replay drivers obtain complete flags through shared helpers, append
`DSPLIB_REPRODUCE_BUGS` last and execute the selected compiler/assembler.
After adopting the V27 header, baseline replay must use the historical headers:

```sh
python3 tools/next20_count_mask.py --domain direct-quality-mask --baseline-dir build/production-before --historical-headers
python3 tools/next20_count_predicate.py --domain used-quality-verdict --baseline-dir build/production-before --historical-headers
python3 tools/next20_count_cross.py --domain quality-mask-cross --baseline-dir build/production-before --historical-headers
python3 tools/next20_count_audit.py next20-count-mask next20-count-predicate next20-count-cross
python3 tools/next20_no_carrier_mask.py --domain captured-ring-mask-owner-width --baseline-dir build/production-before --historical-headers
python3 tools/next20_count_audit.py next20-no-carrier-mask
```

Other finite domains and their predeclared controls are in the `next20_data_*`,
`next20_dsp_*` and `next20_v90_*` replay tools. Machine artifacts under
`build/next20-*` retain actual commands, hashes, period identities, all verdicts,
full audits and RTL dumps. No fuzzing or mutation execution occurred. One
sandbox Docker refusal preceded compilation; it is excluded, with the valid
rerun retained. Failed diagnostic locators are apparatus errors, not compiler
verdicts.

Final census: **1,055/1,852 exact**, 114,545 original bytes, up three names
and 678 bytes with zero exact losses. `make phase J=4` exits zero with
**388 period differential passes, zero failures**, and all structural checks
passing. Static anchor checking reports 285 suites / 10,038 unique anchors;
none needed retargeting. No mutation or fuzzing harness was executed. Other-session PR #263 structure
work remains untouched. Issue #22 was checked read-only; this pass makes no
claim of recovering the original whole-object optimization profile or exhausting
all possible source families.
