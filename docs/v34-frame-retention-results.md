# V34 fixed callbacks, sequential extraction and buffer lifetime

Pin f420cd89; retained full Gentoo profile and bug define. Original observations
and finite controls are in v34-frame-sequence-domain.md. This retains supported
partial recovery, with complete function mismatches still explicit.

putFrame's four fixed groups recover sixteen distinct group calls,18 total
static sites versus six. Each frame word is read immediately before its callback;
callback capture already existed and remains. Candidate577B/SIZE7 against584B.

getFrame acquires its callback once before field reads; all six static calls
use that saved pointer. Four fixed groups recover the original independent
regions. Source extraction now destructively shifts one, then small, then w;
the blob has four same-register chains (57ab6/57acf/57ae7 in the first group).
The old absolute sum-shift expressions did not recover this graph.

Original first group acquires unshifted bitbuf at57a96 after its position
publication, retaining it at stack1c. Later no-refill groups use that snapshot.
Their callback arms reload buffer before publishing new position. The candidate
recovers those boundaries: first acquisition after publication, refresh after
each later callback, no extra owner read in normal later group blocks. The
initial refill loop and wide split callback (whose return is discarded) remain
unchanged. Seven static owner reads on both sides conceal this read-age gain.

Plain fixed/captured getFrame1041B/SIZE13; sequential form1092B/SIZE64; adding
observed buffer lifetime1064B/SIZE36. The latter recovers four destructive
chains, callback refresh ages and original0x5c frame. Retain that supported graph
despite the smaller byte-distance of the incomplete graph. Word-width controls
were observed but not retained; nb/pos conversion residuals remain independent.
Sequential shifts agree over legal field widths0..16 and documented small
widths. Arbitrary malformed widths/callbacks/concurrent aliasing are not claimed.

Eleven opening/follow-up TU cells/132 body verdicts, then four combination
cells/48 verdicts. Baselines reproduce production raw; each remains3/12 exact.
Combining candidates changes exactly putFrame/getFrame and decodeDepth. Both
targets equal their independent selected cells canonically. put expansion
changes ten decodeDepth rows through register ownership and scheduling while
its SIZE239 remains unchanged; this collateral is reviewed, not labelled as an
improvement. All nine other bodies, bindings and allocated/named data stay fixed.
Production source and complete object equal the combined candidate raw.

```
python3 tools/gcc3_v34_frame_sequence.py --domain docs/v34-frame-sequence-domain.md
python3 tools/gcc3_v34_frame_sequence.py --audit-only
python3 tools/v34_frame_retention.py --domain docs/v34-frame-sequence-domain.md
python3 tools/v34_frame_retention_audit.py --integrated
```

Final period388/0 includes existing get/put component, round-trip and initialized
modulatevector coverage; all structural gates pass. Static anchors remain clean
without edits. No new candidate runtime boundary or universal invalid-input
claim; no fuzz/mutation execution. Combined census/partial-link integration is
recorded in getpower-stack-results.md. Future work must isolate remaining word
conversions and their read ages; no declaration or size hill-climbing follows.
