# V8 transmitter shape-only zero cursor

Precompile856c1ecb four full-TU cells: explicit first tx_shape clearing cursor ×
short tx clear-loop counter. Batch50 closed width/receive-component owner family;
this independent cursor was not tested. Original v8_txinit forms tx_shape+0x77c
before scalar stores, uses immediate zero through pointer and advances2; second
ring and third symbol loops stay indexed. Retained allthree indexed. Hold all
scalar/pointer field store order and other functions fixed. No automatic memset,
register choice or arbitrary permutations. Fixed140/460/128 extents fit short.
Source pointer initialized before scalar setup because original LEA precedes
those stores. Audit entire V8global TU, tables, bindings and nonexact bystanders.

First four:163B retained,165B word-only,179B cursor-only,181B combined vs181B
reference, BYTES29. First24 nonpadding instructions reproduce; alpha row25
XOR versus LEA at terminal pointer/availability setup. Original shared symbol
base is consumed by both pointer writes before zero availability, while current
independent pointer statements let availability precede them and base LEA move
before half-pointer store. New bounded expression-owner discriminator: raw
baseline,181B predecessor, chained tx_sym_a=tx_sym_b=tx_symbols. Do not permute
stores or manufacture a zero carrier. Trace initial RTL/CSE/scheduling if changed.

Chained control181B/BYTES29 changes only the order of two MOV destination
operands0x114/0x118, proven by complete instruction comparison. Base LEA/zero
lifetime mismatch persists, so opposite chain/store/zero variants are not a
new owner question. Close cursor/word and chain families, no production edit.

Seven complete-TU controls audit13 canonical symbols and4 named tables, all
allocated nontext bytes/relocations and symbol metadata; only v8_txinit changes,
eight existing exact functions remain exact. Baseline raw reproduces saved
Gentoo object. Static anchors unaffected; no mutation/runtime/phase runs.
