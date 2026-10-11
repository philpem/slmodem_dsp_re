# Echo append cursor and next-length ownership

Baseline5b552950, fixed Gentoo profile and current headers. Original updateEchoHistory
244B: fast path copies count into a remaining counter, advances input by4 before
FloatARMA call, decrements remaining and reuses original count for echoLength.
Retained fast source indexes in[i] with ascending i. Independent slow witness:
after each call original increments the length, compares it with historyAlloc,
and compaction starts at history[incremented-length-1]. Retained source tests
length+1, then compacts from unincremented member and increments only in else.

Four complete-TU cells cross fast advancing-input/guarded countdown with slow
preincrement/member last-sample index. Fast remaining count is unsigned count's
existing type; original count is preserved for the existing final +=. Guard
zero before do loop; retain wrapping addition in fast selection. Slow preincrement
has no intervening opaque call; zero count makes no store. Compaction uses
incremented length-1, preserving old last sample and unguarded filterLength-1
do copy. Set compacted length and w exactly as before. No width/layout/ABI,
flags, equality threshold, alias workaround, rounding or return changes.

Require raw baseline and full-TU body/metadata/data audit. Inspect actual loop
and length-store paths, not aggregate SIZE score. Misses close this cross, no
nearby declarations or permutation sweep. Adopt only supported complete exact
output with production census and phase gate; no fuzzing/mutation execution.
