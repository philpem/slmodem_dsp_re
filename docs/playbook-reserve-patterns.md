# Second small-function playbook pass (2026-10-02)

Baseline 5632087b, following [the first pass](playbook-small-patterns.md):
856/1852 exact functions, 83,111 exact bytes. The twelve screened reserves
are revisited object-first. Five finite source families, six functions and
seventeen compile cells (including five unchanged controls) are tested.
No compiler-profile changes, fuzzing or mutation execution.

## Domains and dispositions

| Family | Cells | Result | Retention |
| --- | --- | --- | --- |
| V22 fill loops: countdown crossed with output cursor | 4 | Both combined loops EXACT; single changes miss | TxNOP and RxClampV22 |
| Silence converted threshold named before comparison | 2 | Named int threshold EXACT | silence_is_more_then |
| CID receiver cache crossed with direct destination | 4 | SIZE1 baseline/cache; BYTES9 direct/both | None |
| Calling-tone amplitude loaded before TONE_read | 3 | SIZE1 baseline; SIZE14 short/int locals | None |
| V32 count assignment crossed with array root | 4 | BYTES49/61/6/28 | None |

Predeclared domains on issue #22:
[V22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942781483),
[V32](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942778885),
[CID/tone/silence](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942798361),
[combined retention](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942842276).

RxHdxStartB103 is deferred because its call/inlining mismatch overlaps prior
profile controls. v23FP_tx_create's apparent width lead is already refuted by
F8122. TxHdxTRN's memory subtraction versus cached update does not yet give
an independent source family. ModDataV22, DetSequence and V22FP_modem remain
screened, uncompiled reserves. A shortlist is not a proof of recoverability.

Later TxHdxTRN controls (F11553) distinguish a fresh unsigned-input fold lead
from that subtraction hypothesis: explicit unsigned-short conversion recovers
one extension instruction, without exact identity; unsigned mask changes
nothing. GCSE PRE first introduces the cached subtraction. Disabling load
motion changes nothing; disabling all GCSE restores memory RMW but loses
two exact neighbors. Both finite domains are now closed, no adoption.
[Pass-boundary record](v32-txhdxtrn-pass-boundary.md).

## Exact source recoveries

The blob's TxNOP and RxClampV22 initialize signed-short counters to 159 and
11, write through an advancing pointer, and terminate by decrementing the
counter. Source uses `do { *out++ = value; } while (i-- != 0);`. This preserves
160/12 ascending writes and the final output count. Countdown with a computed
ascending index yields BYTES6 for each; cursor with an ascending counter
leaves SIZE2. Together they recover both complete 44-byte functions.
The complete v22prc.c TU preserves ten functions and ten global definitions,
gaining two exact bodies (1/10 -> 3/10) with only those bodies changed.

The blob's silence comparison loads the count after the x87 conversion.
A named `int threshold = (int)(10.0f * t);` followed by the existing comparison
recovers the complete 66-byte function. The constant, arithmetic, conversion,
comparison and rounding remain the same. This establishes a matching source
carrier, not unique recovery of the author's spelling. Complete silence.c
preserves five functions/globals, 2/5 -> 3/5 exact, only the target changed.

## Closed negatives and controls

CID's blob loads the DTMF receiver before the sixteen-byte copy and stores
through the context root. Caching that receiver alone leaves SIZE1; direct
`ctx->strings[i]` produces equal-length BYTES9, also with the cache. No exact
candidate; all ten functions/globals and five exact bodies preserved.
The 600-byte clear, mode routing and renderer calls are untouched.

Calling-tone blob loads amplitude before TONE_read. Per-iteration short or
int locals test this source lifetime explicitly, preserving phase/cadence
bugs and amplitude reload frequency. Both leave SIZE14, with identical target
canonical verdicts; two functions/globals and the exact reset preserved.
Neither candidate is adopted merely because it recovers a load boundary.

RxHdxSequenceE stores Demod's count before widening and uses the hdx root
for its first rate store. Separate/chained assignment crossed with cached
regs/direct array preserves fourteen functions/globals and ten exact bodies;
only the target changes. Direct array recovers every instruction/register
except the adjacent count store/zero-extension order (BYTES6). Chaining
restores that order but changes live ranges elsewhere. Neither the nearest
cell nor their non-exact combination is retained. Initial RTL distinguishes
lowpart-of-SI local from returned-HI store carriers. Graph detector controls
fire 4/4 on known rebase/order cells. A possible next domain, **not run here**,
is direct `*count = Demod(...);` and passing `*count` to Descramble instead of
an intermediate local; this needs its own source/control declaration.

All five raw full-TU controls reproduce retained objects. Gentoo 3.4.2-r2,
selected assembler 2.15.92.0.2, full .build-config flags and mandatory
DSPLIB_REPRODUCE_BUGS are recorded, with source/header hashes, complete
function/global inventories, all changed bodies and relocation-aware verdicts.
The existing three-family replay engine is parameterized without changing its
default domain. New replay tools:

```
python3 tools/playbook_v22_fill.py --domain <V22-comment-URL>
python3 tools/playbook_reserve_carriers.py --domain <V32-comment-URL>
# Re-score the four saved graph controls without compilation:
python3 tools/playbook_reserve_carriers.py --domain <V32-comment-URL> --analysis-only
python3 tools/playbook_reserve_patterns.py --domain <CID-tone-silence-comment-URL>
```

Artifacts: build/playbook-v22-fill/{results,analysis}.json,
build/playbook-reserve-carriers/{results,analysis}.json,
build/playbook-reserve-parent/results.json. Raw baseline/candidate objects,
commands, initial RTL and disassemblies sit beside each cell. Replays keep
saved baseline objects rather than comparing old source to adopted objects.
Independent production audit: build/playbook-reserve-adoption/combined.json;
all three winning raw objects reproduce and only three bodies change.

## Final validation

Complete comparison build: 300/300 objects, zero failures. Whole-tree exact set: **856/1852 -> 859/1852**, exact bytes
**83,111 -> 83,265**, exactly the three named gains and no losses.
Initial fixed period run passes385/0 but exposes three detached static
silence anchors. Each is retargeted to the same scale/comparison/rounding
change on the named threshold; no mutation execution or snapshot refresh.
Final fixed non-fuzz make phase passes **385/0**, with structural checks
clean and wrapper exit0. All10038 anchors/285 suites match uniquely;
refcheck reports14218 references/2671 headings with no unresolved entries.
Tool syntax, whitespace and the four saved V32 graph controls pass.

Complete 300-object before/after partial links use the same recovered order,
overriding only the two unchanged baseline TUs. Positioned equality
68,215 -> 68,222 /943,398 bytes; candidate allocated bytes 914,158 unchanged.
Exact section records70/92, relocation records1,018/18,317 and symbol
records394/2,907 unchanged. Both strict comparisons remain DIFFERENT (exit1).
Artifacts: build/playbook-reserve-adoption/partial/, including manifests,
objects, attribution, link-order and comparison JSON. Canonical function
identity does not establish complete-object identity.

All five domains close here. No mutation verdict is refreshed and no modern
portability claim is made. These finite results neither prove a whole-tree
ceiling nor classify every remaining non-exact function as register allocation.

F11545 executes the previously unrun direct-count carrier hypothesis in
[a new four-cell follow-up](v32-sequence-count.md). Combined with direct-array
access it recovers RxHdxSequenceE exactly; the previous four-cell family
remains closed. ModDataV22/V22FP_modem's older exhausted source domains are
confirmed by a separate read-only audit, with no new compilation there.

DetSequence's proposed loop transfer and a separately observed found lifetime
are tested and closed by F11546: [six valid cells](v32-detsequence-loops.md),
no adoption. The remaining measured discrepancy begins at allocation/reload;
these local graph recoveries do not justify another spelling matrix.


F11549's word-home pair recovers DetSequence's slots but is non-exact and closed;
F11550 certifies peephole exposure without adopting flags. The independent
predecessor GenSequence lead (F11551–F11552) crosses countdown with narrow-before-
mask index update; combined source is exact 118B. [Ledger](v32-gensequence-recovery.md).

The fresh non-exact screen at74cda31e has159 eligible bodies from493 shared
symbols after the declared path exclusions (300 objects). FPM_rms is a new
object-backed loop lead, not a nearest-size assumption: its four crossed
countdown/cursor cells recover62-byte exactness only together (F11554).
[Domain and validation](fpm-rms-countdown.md).

Dual_TONE_create supplies a fresh common-result case (F11555): guarded
successful initialization followed by return st recovers64-byte exactness
in a two-cell full-TU domain. Notch's independently observed addition grouping
is tested separately (F11556); it recovers the operation tree but staysSIZE2,
so the closed domain contributes evidence and no adopted source.
[Constructor](dualtone-create-common-return.md), [notch](notch-addition-tree.md).
