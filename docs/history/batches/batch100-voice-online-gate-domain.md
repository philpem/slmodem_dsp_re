# Two eager boolean predicates in voice_online sample loops

Independent original discriminator: both sample loops compute SETA(count>i)
and SETNE(result!=1), then TEST their conjunction (0xabfd9..0xabfe9 and
0xac0d1..0xac0e1). Source currently uses short-circuit &&. Both operands are
side-effect-free nonvolatile reads; bitwise & of boolean comparisons preserves
returned values and expresses the observed eager boolean gate.

Four full-TU cells cross logical/eager gate with baseline/before-kernel ret1
assignment. Earlier four timing-only cells miss; do not repeat timing synonyms.
Keep both comparisons boolean and parenthesized; no loop/count/field/header
changes. Require original508B strict hit and all exact neighbours preserved.
No fuzz/mutation; final major gate at batch end.
