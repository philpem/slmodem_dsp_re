# Issue 37: x87 wrapper audit

The reconstruction had ten file-local helpers whose names contain `x87_`.
Every one simply forwarded to `log10l` or `__builtin_sqrt`; none is a
reference symbol or an independently recovered call boundary.

Eight are removed: the six logarithm helpers in `psd.cpp`,
`V90TRN2Designer.cpp`, `VpcmFloModem.cpp`, `V90Demodulator.cpp`,
`V90Phase4Demodulator.cpp` and `V90ConstellationDesigner.cpp`, plus the
square-root helpers in `V90Equalizer.cpp` and `V90Phase4Demodulator.cpp`.
Their callers now name the underlying operation directly. Repeated calls
remain repeated; no arithmetic, casts or evaluation order changed.

Two are retained: `x87_fsqrt` in `V90ConstellationDesigner.cpp` and
`trn2_x87_fsqrt` in `V90TRN2Designer.cpp`. Replacing either with the same
`__builtin_sqrt` expression changes its complete recovered-Gentoo object.
The wrapper is therefore load-bearing source shape, not a cosmetic alias, and
is left in place rather than fitting source to a cleanup preference.

The two non-`x87_` square-root wrappers, `agc_fsqrt` and `v92mapper_fsqrt`,
are out of this issue's name audit and remain for issue #34's type-specific
standard-math review.

Validation used `dsplibs-tc342-gentoo` with `DSPLIB_REPRODUCE_BUGS`:

- the eight affected differential fixtures passed, 8 passed and 0 failed;
- both candidate and master compiled all 273 faithful objects successfully;
- all seven changed translation-unit object files were byte-identical between
  the candidate and master after retaining the two measured square-root
  wrappers.
