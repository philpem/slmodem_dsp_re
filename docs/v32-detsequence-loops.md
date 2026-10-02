# DetSequence loop and lifetime controls (2026-10-02)

Baseline e98ea74b, after landed PR #242: 860/1852 exact, 83,604 exact bytes.
DetSequence is 275 bytes in both objects but BYTES214 under canonical
comparison. This is a bounded negative result: no production change adopted.

## Source-loop domain

The blob decrements a remaining unsigned-short word count and keeps a
separate signed-short shift counter, while retained source increments an
unsigned-short index and computes nbits-1-bit. Both retain an ascending
short bit bound. [Four cells declared before compilation](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5943662708):

| Cell | Outer loop | Shift source | Result |
| --- | --- | --- | --- |
| Baseline | Ascending index | Computed nbits-1-bit |275B BYTES214 |
| Countdown | while(count-- !=0) | Computed |280B SIZE5 |
| Shift counter | Ascending index | short shift, word >> shift-- |285B SIZE10 |
| Both | Countdown | Separate short shift |290B SIZE15 |

All four cells preserve eight functions and eight global definitions,
six/eight exact, zero gains/losses; only DetSequence changes. Raw unchanged
full-TU control reproduces the retained object. Input-word width, short
nread/bit counters, bitwise match predicate, last-match-in-word behavior,
register writeback, call interface and all other functions remain unchanged.
No nearest-size adoption, profile changes or spelling permutations.

Initial execution carried an incorrect issue-comment URL in its metadata.
Its four objects/log are preserved at build/playbook-detsequence-invalid-domain-url/
and /tmp/playbook-detsequence-invalid-domain-url.log, explicitly excluded.
The same four declared cells were rerun with the correct URL; only those
accepted results are reported. This was provenance repair, not a new source
domain or flag experiment.

## Independently observed found-flag lifetime

Pass/disassembly review identifies a third source difference. The blob zeros
found once in the prologue (0x83782 xor EDI,0x8378b store to ESP+0x10), sets it
on a match and tests it at word end; no word-entry reset exists. The combined
candidate initializes found inside each word iteration. Moving that
initialization outside the loop is behavior-inert: a match returns after
the word, so every continuing iteration already has found==0.

[A new two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5943715142)
compares the previous combined source as a raw control with found=0 moved
before nread=0 at function scope, matching blob initialization order. The
control raw-reproduces the previous 290-byte object; the alternative removes
the per-word reset but remains 290B/SIZE15. Both preserve eight functions/
globals and six/eight exact; only the target changes. No source adopted.

The initial RTL now has found 69=0 at insn 24 and nread 70=0 at insn 26 before
the outer loop, with the found setter/test preserved. This establishes the
predicted lifetime change; exactness is separately refuted. No new declaration,
register or volatile-variable spelling domain follows from that result.

## Where the remaining spill first appears

In the two-loop and found-once candidates, .24.lreg retains nbits as
pseudo 64, with no new bound-memory spill. .25.greg first spills it: new
insn 208 stores to ESP+0x1c and insn 212 reloads it for nbits-1. The loop bound
then reads that stack slot. Reg pseudo 66 stays in EBX; the blob keeps nbits
in EBP and reg at ESP+0. The found-once change removes a separate entry-bound
reload present in the combined control, but does not change this spill choice.

Both controls show allocation priority beginning 66,75,63,76,64 and estimated
memory costs 3653 for nbits versus 21120 for reg. Those measurements are
consistent with retaining reg; they do not by themselves recover the original
command line or uniquely identify a source spelling. Remaining nread/word
scheduling and match-store order can follow allocation/scheduling; initial
match stores already have the original source order. No further independent
source-boundary hypothesis was established by the parent/independent audit.
The six valid cells close here. Future work needs a new pass/profile or
source-boundary observation, not a declaration-order search around the miss.

## Reproduction and validation

```
python3 tools/playbook_detsequence.py --domain <four-cell-comment-URL>
python3 tools/playbook_detsequence_found.py --domain <two-cell-comment-URL>
python3 tools/detsequence_analysis.py --loop-domain <four-cell-comment-URL> --found-domain <two-cell-comment-URL>
```

Results and analysis JSON: build/playbook-detsequence/ and
build/playbook-detsequence-found/. Full source/header/local-header hashes,
actual commands, Gentoo 3.4.2-r2 and executed assembler 2.15.92.0.2 identities,
full retained flags/mandatory DSPLIB_REPRODUCE_BUGS, object hashes, complete
function/global inventories, all verdicts and changed bodies are recorded.
New analysis fires on **4/4 loop plus 2/2 lifetime controls**, including both
spilling and nonspilling objects, and extracts initial/local/global RTL.
Replay validates object hashes and the explicit domain metadata.

All 300 retained compiler objects are raw-identical to the saved 860-exact
baseline (build/playbook-v32-count-adoption/partial/current); none of the
experimental objects replaced them. Exact counts/bytes remain 860/1852 and
83,604. No source/header/fixture edits or anchor retargeting. Existing fixed
t_v32seq has zero-count, streaming state/writeback, hit/miss and all-ones
controls; these are component boundaries, not arbitrary modem reachability.
No new oracle fixtures, fuzzing or mutation execution.

The fixed reconstruction gate passed: **385 period tests, 0 failures**,
with structural checks clean and new files included in the tracked-file census.
Reference checking resolved 14,223 references and 2,675 finding headings;
static anchor checking covered 285 suites / 10,038 anchors, all clean.
Upstream drift was not checked because the upstream source checkout is absent;
the seven manifest files were checked against the manifest only.

## Allocation follow-up

[The evidence checkpoint](v32-detsequence-allocation.md) compares all six saved
controls: reg ranks before nbits in every cell, but their final spill choices
reverse when the short shift is introduced. Priority alone is insufficient;
the baseline compiler already creates an int running shift counter. The next
trace concerns counter lifetime, ECX constraints and allocation versus reload.
