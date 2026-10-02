# GenSequence countdown and narrowing recovery (2026-10-02)

Investigating DetSequence's peephole exposure led back to its one non-exact
preceding emitted function, GenSequence. This is an independent source recovery,
not a DetSequence register-spelling workaround.

The blob 836d7/83707/83728 decrements a remaining unsigned-short count. At
836f7..83700 it preserves the old index for multiplication while narrowing
the decremented index to 16 bits before masking it at 83720..83724. Retained
source used an ascending outer loop and masked index-1 before narrowing.

[Four cells declared before compilation](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944693493):

| Cell | GenSequence result | Full-TU exact |
| --- | --- | --- |
| Baseline |111B/SIZE7 |6/8 |
| Countdown only |128B/SIZE10 |6/8 |
| Index post-decrement only |120B/SIZE2 |6/8 |
| Both |118B/EXACT |7/8 |

The unchanged full-TU control raw-reproduces. All cells preserve 8 functions /
8 global definitions. The combined source uses while(count-- !=0), shifts
using index-- * width, then wraps with index &= gen_index_mask. It matches
all 118 blob bytes and canonical relocation targets; only GenSequence changes.
All six prior exact bodies survive; retained DetSequence stays275B/BYTES214.
No cursor/byte-exactness claim about DetSequence follows from this gain.

Index decrement before mask is equivalent because index and mask are 16-bit;
mask loads remain after the output store, preserving supported aliases.
The count change affects a local parameter only. The source candidate is
selected by its independent loop/narrowing evidence and exact full-TU result,
not by nearest size. One exact cell in this declared family is not a claim
of a unique original C spelling.

Reproduce with tools/playbook_gensequence.py --domain <the URL above>.
Artifacts: build/playbook-gensequence/results.json, sources, all pass dumps,
full objects/inventories/bodies and command/source/header hashes. Adoption
artifacts live in build/playbook-gensequence-adoption/. Four static anchors
are retargeted to retain their labeled faulty behavior; no mutation execution
or snapshot refresh. No fuzzing. Final gate/tree/partial-link measurements follow.

## Retained validation

Complete build: 300/300 objects, 0 failures. The retained full-TU object is raw
identical to the tested winner; only V32prc.c.o changes among all 300. Whole-tree
canonical census: 860/1852 -> 861/1852 exact; 83,604 -> 83,722 exact bytes. The only
exact gain is GenSequence; no losses. All eight function starts relative to
SetToneDetect now match the blob: 0,0x70,0xc0,0x140,0x180,0x2a0,0x2b0,0x2d0.
This recovers local TU layout, not the complete partially linked object.

Fixed make phase: 385 passed, 0 failed; structural checks clean, including
14,223 references / 2,681 finding headings and 285 suites / 10,038 static anchors.
Existing t_v32seq generator coverage runs nine parameter sets, three streaming
calls each with output/state/writeback comparison, plus zero-count controls;
these are initialized component boundaries, not full modem lifecycle claims.
No new fixture or oracle threshold. Static anchors retain four original
fault labels/behaviors; their old verdict snapshot is not refreshed.

Complete same-order 300-object partial links gain positioned equality
68,371 ->68,473 /943,398 bytes. Allocated content 914,174 ->914,190. Exact
section records 70/92 and symbol records 394/2907 remain unchanged; relocation
record equality 1022 ->1019 / 18317, so the source gain also shifts positional
records. Complete canonical callee/data bindings within the changed TU are
preserved. Both strict comparisons remain DIFFERENT(exit1); no measurement
model or ratchet is reblessed. Full before/after records and strict logs are
under build/playbook-gensequence-adoption/partial/.

No modern portability or upstream source-drift claim. The upstream checkout
is absent; its seven manifest files were checked against the manifest only.
