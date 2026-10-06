# V29 epoch sample index capture boundary

Four completeTUs at6b4509bd: baseline/late-index/streamed-control/both.
End-to-end trace identifies original signed n_out read9b698 after state lms_on
clear9b685 and decoder sym_count increment/store9b691. Source snapshots n_out
atentry; retained final load9b985 precedes both stores. Candidate defers only
that existing index assignment until immediately before the first sample
load, after both writes. No declaration ordering or type change, no other
memory/arithmetic/callback/store/phase/average/counter/return changes.
This is a precisely witnessed read boundary, independent of prior pair-only
pressure controls; cross existing streamedpair family without new widths.
Potential owner/index overlap is a synthetic component-alias question, not
proven modem lifecycle reachability. Don't invent an admissible overlapping
fixture or make a semantic adoption from narrower size. Rawbaseline and old
streamedcontrol must reproduce their previous complete objects. FullTU
all body/binding/data/BSS/relocation audit; miss closes this read family.
No runtime/fuzz/mutation; no arbitrary adjacent variants after closure.
