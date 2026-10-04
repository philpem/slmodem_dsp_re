# CID mark eager predicate carriers

Raw902, advancing input seed, named int predicates, and named short predicates.
Blob evaluates both unsigned predicates into registers (zeroedfull EAX plus
seta, setaDL), bitwise tests them, then sete and zeroextends return. Direct
boolean& source is folded to inverted predicateOR before final emission.
Named independent predicate captures before conjunction are a bounded normal
source alternative; preserve input cursor seed and everythingelse. Types
int/short represent the two available preimages for boolean locals, not
formal changes. Inspect actual earlyRTL boundaries; no further synonyms if
whole-map inert. FullTU data/nontext/relocs/metadata/bystanders and raw902.

Measured result: 4cells int/short predicate maps identical257B/SIZE2; no gain. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
