# V8 cosine formal width and narrowing-at-use

Declared six-cell full-caller domain on902f47fa: unsigned-char/int/unsigned-int
formal crossed with retain/remove explicit caller byte casts. Owner byte
narrowing stays at table lookup for widened formals. All three caller TUs
(V8.c, V8Dftc.c, V8Fsk.c) compiled, including owner/callee; no other caller
found in scoped src search. Header is candidate overlay only; ABI Cdecl scalar
slot and short result unchanged. Every candidate still indexes by low8bits.
Blob ansam calls pass full-register (phase+32)>>6 without movzbl; callee reads
argument lowbyte. Caller materialization can distinguish formal boundary from
byte-limited callee semantics. Signed versus unsigned formal may remain an
indistinguishable family; do not claim unique original prototype from callee.
Source cast removal alone on byte formal is a negative control, widened formal
with retained casts separates caller source from declaration. Full production
header breadth at parent integration if adoption justified. No other V8Interface
changes. Raw baseline, actual compiler/bugdefine/assembler and complete caller
TUs/data/metadata/nontext/relocs/bystanders checked; no fuzz/mutation/harness.

Measured result: 18 complete TU controls: signed and unsigned full-formal maps identical; owner14B exact in every cell, no losses. Full formal plus removed caller casts changes nonexact caller bodies. Original signedness remains ambiguous.
