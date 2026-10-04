# Callprog eager refusal predicates

Pinned856c1ecb fullTU. Original Progress dynamic transition sites 0x79b18..27 and0x79b9a..a9 form separate boolean last_state!=next and next!=0 with SETNE then register TEST; timeout state exclusion0x79c16..26 similarly forms two SETNE then TEST. Current request_state uses short-circuit OR refusal and run_timeouts uses short-circuit AND state exclusion. Predeclare four crossed cells: baseline; request_state refusal using !( (next!=0) & (last_state!=next) ); timeout exclusion using bitwise &; both. All operands pure ordinary int fields/local values, no getter or calls, semantic booleans remain0/1. Preserve pending guard and every diagnostic/store boundary, all widths and helpers. Existing findings describe helper inlining, not this witnessed predicate family. No other boolean transformations, permutations or helper inline controls. FullTU raw baseline/all bodies/nontext/relocs audit required; no runtime/mutation/fuzz.

## Measured closure

Four valid rawcompleteTU cells: Progress SIZE5→14 for requesteager; timeout-eager rawinert; bothsame14. No gain/loss,3/6exact. All nameddata/exports/metadata identical,stringmultiplicity retained, changedanonymous tableedges/stringemissionorder preservedexplicitly in changed-dataledger. No sourceadoption.
