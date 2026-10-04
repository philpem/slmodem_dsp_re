# TONE verdict return ABI versus caller narrowing

Declare int versus short return prototype/definition crossed with explicit
caller-use short cast on detector_progress. Fourcells each on two complete
TUs. Sourcecensus one directcaller Detector.c, no Fdspkrnl directcall. Blob
testsAX aftercall (F8809). Callee only returns0/1/2; Cdecl scalar result
highbits not definitive of formal. Wholecaller and callee proof required;
prototype correction cannot be uniquely concluded because explicit short
caller conversion is an alternative. Header candidateoverlay only. No flags,
othercalls/returns, source mutations outsidecells or runtime experiments.

Measured result: 8completeTUs, shortformal and explicitshortcaller equalmaps, progressSIZE12; callee unchanged, no gain. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
