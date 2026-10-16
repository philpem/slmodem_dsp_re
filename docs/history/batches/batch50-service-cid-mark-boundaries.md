# CID mark detector input cursor and eager predicate

Four cells: raw902, advancing sample input, bitwise combination of boolean
predicates, both. Blob reads (%ebx) then advances by2 before energy/calls;
ours reloads base and addresses by signed-short index. Preserve short counter
and replace only samples[i] with *samples++. Blob evaluates wide/2>narrow
and wide>150 with seta then test, not short-circuit branches. Replace boolean
&& with &: operands pure unsigned comparisons; exactly equal over full input
domain. These source boundaries independent and directly object-witnessed.
No widths/flags/permutations; fullTUraw902 historical headers, data/nontext/
reloc/metadata/allbystanders required. No runtime harness/mutation/fuzz.

Measured result: 4cells cursor bestSIZE7, bitwise eager spelling samebody as shortcircuit source, no gain. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
