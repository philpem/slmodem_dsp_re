# Issue #34: standard square-root spellings

The source and header inventory found sixteen `__builtin_sqrt` expressions in
the reconstruction.  This change converts fourteen to the standard `sqrt`
spelling, with `<math.h>` declarations added at the two source/header
boundaries that did not already have one.

The converted sites are the `Agc` and `V92Mapper` double wrappers,
`DspMath::Std`, `V90Resampler::getTimingHistoryStd`,
`ResamplerTiming::SdHalfBaudDft`, `V90Equalizer::process`, and the eight
V90 phase-4 diagnostic roots.  The argument and result types are deliberately
unchanged: C++ overload resolution preserves the original float, double, or
long-double expression type.  A recovered-Gentoo `make tc-repro` comparison
against a clean master worktree found all 273 translation-unit objects
byte-identical, including exports and every wrapper boundary.

Two wrapper boundaries remained for separate investigation: `x87_fsqrt` in
`V90ConstellationDesigner.cpp` and `trn2_x87_fsqrt` in
`V90TRN2Designer.cpp`.  Issue #40 established that each must remain a
`static inline double` boundary, while its body can use standard `sqrt` with
full recovered-Gentoo translation-unit identity.

`fpm_sqrt_table_generate` already calls ordinary double `sqrt` and includes
`<math.h>`; it is a reviewed fixed-point table generator, not a builtin
conversion.  The fixed-point `FPM_sqrt` and `FPM_sqrt_dp` APIs are likewise
separate integer algorithms and are not part of this inventory.

The mutation anchors for the four changed source files use the standard
spelling and retain the same faults.  Differential validation is recorded in
the pull request after the relevant and full Gentoo period suites run.
