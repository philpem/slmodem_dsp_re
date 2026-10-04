# Circular dot history cursor

Baseline856c1ecb. Reference initializes descending history pointer at hist+widx
before first-loop guard, consumes it with postdecrement, then adds taps to
that resulting pointer for second wrap segment. Retained indexed second-loop
starts from taps-1 independently. Three-cell domain: retained; descending
history cursors with independent second start; descending shared cursor with
wrap +=taps. Preserve short loop counters, coefficient stride and products.
For valid0<=widx<taps history accesses equal. Negative widx is a distinct
reference domain: skipped first loop leaves hist+widx, then +=taps, versus
retained hist+taps-1. No internal blob callers; do not assert reachability or
universal equivalence. Object second pointer LEA(base,taps,2) uses first
cursor after loop at+110; width follows existing API, no header/flags edits.

Three valid complete TUs: retained179B, both cursor bodies195B versus
reference195B. Independent restart differs92 bytes, shared wrap carry63.
Instruction alpha fails early scheduling (third reference PUSH versus candidate
XOR), not simply register colour. No exact gains/losses; divider unchanged.
Initial loop dump declines indexed strength reduction with negative GIV
profitability (-108/-144 vs12); cursor control explicitly expresses accesses,
not a pass flag. Close cursor/carry family; no scheduling/declaration fitting.
