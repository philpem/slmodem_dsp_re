# Verifier process ABS census false lead

Fresh batch20 ABS-gap census classifies V90SpectralVerifier::process:
blob268B/FABS0, ours419B/FABS1. This is diagnostic inlining, not a source
absolute-value conditional difference. Blob has a relocation to printSpectrum
at0x466b6; ours has its spectrum-print strings and one fabs from the existing
SV_PRINT_WHOLE(v) inside the inlined print loop. Blob has5 relocation sites,
ours12. process source has no direct abs/sign arithmetic. Initial RTL and
combine already carry exactlyone abs:SF in its inlined diagnostic; there is
no newly introduced late optimization ABS to undo.

This is Playbook lever10/F5802's already closed printSpectrum inlining family
(same268/419B): definition-order and400-function TU-growth controls were
negative historically. No source rewrite/compile repeated, no noinline hint,
no complemented-sign rewrite of a correct diagnostic. Counterexample to
interpreting a per-function FABS gap as an arithmetic preimage: classify
inlined callees first. Exact count unchanged.

Evidence existing raw-reproduced baseline full TU:
build/gcc3-batch20-verifier-native/V90SpectralVerifier/baseline/;
V90SpectralVerifier.cpp.01.rtl process named section one abs:SF, .20.combine
same. Both printSpectrum and process contain the same spectrum format string.
