# V8 notch filter paired backward history shift

Declared raw902 full V8Detector baseline plus one paired two-iteration
history-copy loop. Blob loop0x7841d..0x78440 computes source1-i and dest2-i,
copies acc_c then acc_d, ascending i=0..1. Reconstruction has four independent
stores and load-hoists histories before stores. Restore the observed loop,
reuse existing int i; no widths/type/store permutations or closed biquad
families. Values/source roles unchanged, paired order follows object loop.
Two cells then close; full TU initialRTL loop and all data/metadata/nontext/
relocation/bystander audits, mandatorybugdefine and actual retained compiler.
Parent batch period gate only; no harness/fuzz/mutation run.

Result: paired source history loop reproduces notch_filter EXACT161B.
Full TU2/6→3/6 exact, no losses or changed nonexact bystanders. All named
objects/metadata/nontext/relocations preserved. Initial RTL retains paired
indexed loop rather than straight stores; raw902baseline reproduces.
