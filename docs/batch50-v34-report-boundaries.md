# V34 coefficient report observable source boundaries

Declared nine-TU domain: raw902 baseline, plus signed scan locals, coefficient
pointer capture and per-print positive debug guards crossed on exact energy
seed (eight cells). Blob unsigned taps/6 arithmetic is converted into signed
n (test/jle) and signed k (cmp/jl). coeff+8 is read before scan guard and kept
across debug callbacks. Original header skips printing if debug false, but
still enters row loop; each row condition skips its print rather than exits
function. Reconstruction unsigned scan, repeated e->coeff reads and debug
false early returns do not express these boundaries. All original strings,
144-row extent and unsigned input division retained. Print callbacks can
change debuglevel or coeff owner, so scope/lifetime is observable fidelity.
No arbitrary statement or register permutations. Audit full TU, all nonexact
bystanders/data/relocs, metadata. Root final phase once; no harness/fuzz/mutation.
