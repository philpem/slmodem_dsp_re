# TxHdxTRN: input extension and GCSE are separate leads

Baseline 3cbe7d52, Gentoo GCC 3.4.2-r2 and selected assembler
2.15.92.0.2. Every cell uses the saved complete `.build-config` through the
shared experiment helper, including `DSPLIB_REPRODUCE_BUGS`. The blob's
237-byte function spans 0x7ff70..0x8005d. Retained source also emits 237 bytes,
but has BYTES45 under canonical byte/relocation comparison.

## Bounded input-fold domain

The blob zero-extends the input word at 0x7ffe0 before masking with 3;
retained source sign-extends it. Explicit unsigned-short conversion and an
unsigned mask are distinct source controls. Both preserve the low two bits
for every short value; neither changes the function's parameter declaration.

| Input fold | Size | Verdict | TU exact |
| --- | --- | --- | --- |
| `data[i] & 3` | 237 | BYTES45 | 4/9 |
| `(unsigned short)data[i] & 3` | 237 | BYTES44 | 4/9 |
| `data[i] & 3u` | 237 | BYTES45 | 4/9 |

The explicit cast changes only the observed extension instruction and only
TxHdxTRN's body. The unsigned mask changes no body. Nine function names and
nine global definitions/bindings survive all three cells; no gains or losses.
No source candidate is adopted for this partial result.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944920282).
Replay with `python3 tools/playbook_txhdxtrn.py --domain URL`; artifacts live
in `build/playbook-txhdxtrn/V32TXHDX/`.

## First pass that caches the subtraction operand

The blob uses `sub %eax,0x78(%edx)` at 0x7ffb7. Retained code subtracts from
a cached register and stores the result. Initial RTL and first CSE still
load `state_left` separately for the comparison and subtraction. The GCSE
dump explicitly reports redundant load insn48, reaching register102, and
replacement of register79 in subtraction49 with register102. Thus this
observable difference first appears before allocation. It does not by itself
identify a source statement or the original optimization profile.

Cross the two input forms above with retained flags, `-fno-gcse-lm` and
`-fno-gcse`: six cells, two raw-reproduced source controls. The narrow
load-motion control reproduces each entire source-control object byte for
byte. Disabling GCSE restores memory RMW in both source forms, but produces
244 bytes/SIZE7. It changes seven bodies: TxHdxCarrierState, TxHdxData,
TxHdxFinishFrame, TxHdxNoCarrier, TxHdxScrSequence, TxHdxTRN and V32TxHdxModem.
Nine functions/globals are preserved, exact4/9 falls to2/9, losing
TxHdxFinishFrame and V32TxHdxModem; no exact gains. Changed canonical bodies
and relocation-bearing disassemblies are saved for full-TU review.

[Predeclared pass domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944965892).
Replay with `python3 tools/txhdxtrn_gcse.py --domain URL`; artifacts live in
`build/txhdxtrn-gcse/V32TXHDX/`. `python3 tools/txhdxtrn_analysis.py` verifies
the saved known transition and controls: 2/2 raw source controls, 2/2 unchanged
load-motion controls, 6/6 inventories, 1/1 GCSE transition, 2/2 RMW exposure,
2/2 input-extension controls, zero gains across six cells.

## Disposition and next discriminator

Both finite domains are closed; source and compiler flags remain unchanged.
Do not infer a volatile member, change parameter types, permute locals or
adopt the nearest byte score. Reopening this lead requires independent evidence
for a source effect that prevents PRE reuse, or a broader original-profile
experiment that explains neighboring bodies as well as this one. Memory RMW
alone is not evidence for a recovered source preimage. No fuzzing or mutation
execution; retained tree remains 861/1852 exact.

Fixed Gentoo `make phase`: 385 passed, zero failed; structural checks clean.
Reference census: 14,223 references and 2,682 finding headings. Static anchor
check: 285 suites, 10,038 anchors, no detached or non-unique anchors. These
are static checks, not mutation execution. Three new Python files compile,
the saved diagnostic fires on its known controls, and whitespace checks pass.
