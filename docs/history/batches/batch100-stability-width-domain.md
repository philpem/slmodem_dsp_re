# Promoted history word versus HI cache in stability predicates

Common-result controls recover original52/34B sizes and MOV1 verdict, leaving
only comparison operand direction (six/three bytes). Reversing source operand
syntax is inert and closed. Original first history load zero-extends to32bits,
while all comparisons use16bits; signed/unsigned int locals can represent the
promoted word instead of an explicit HI cache. Test this distinct typed-local
boundary crossed with the witnessed common-result graph, not more operand
synonyms. Six complete-TU cells: retained baseline, short-common, int and
unsigned-int cache with literal/common returns. All65536 word values preserve
predicates; API and memory widths stay fixed. Require strict fullbody hits,
no canonical normalization of swapped comparison bytes and no bystander loss.
If widening is inert or adds word loads, close family. No fuzz/mutation.
