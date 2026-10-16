# Mapping mask clamp source domain

Base 902f47fa, complete V90MappingParamsInt.cpp, unchanged raw control.
The two blob setters use signed SETL, NEG, AND rather than the current conditional branch. Preserve negative `which` as well as values >=6: candidates are boolean multiplication, signed-mask AND, and explicit clamp assignment. No loop/statement permutation, header or flag change. Audit every inlined caller and all TU metadata/data/relocations; no candidate adopted by score alone.
