# V90Parameters floating-absolute-value experiment

> SUPERSEDED: these generated candidates retained the old integer absolute
> value after adding floating `fabs`, so they did not test the intended
> replacement. The member-input candidate also read the member before its
> assignment and is behaviorally invalid. Preserve this matrix as diagnostic
> history only; do not adopt its store-middle result. Corrected controls and
> full-object consequences are in `params-abs-replacement.md` and
> `v32-params-scope.md`. No Parameters source change is retained.

Question: does the reference `loadModemParamsData` encode a floating absolute
value before converting the displayed whole part to `int`, rather than the
current integer absolute value after conversion?

The reference executes `fabs` while the value is in x87, stores the signed
value to member `+0x380`, converts the signed value for the fractional path,
and converts the retained absolute value for the whole-part argument. The
current source converts first and implements absolute value with integer
`sar/xor/sub`. The independently established input path is
`powerReductionTenths` (unsigned), integer division by 5, cast to `int`, then
multiply by `0.5f`; therefore the two forms are behaviorally equal over every
reachable input. NaN and negative values cannot arise, and the largest result
fits in signed `int`, so this is source/code-generation recovery rather than a
behavior repair. This is consistent with D148/F879 but not answered by it.

The first retained-profile batch has three cells: unchanged control,
`(int)fabs(pr)`, and `(int)fabs(DIGITAL_POWER_REDUCTION)`, with the required
`<math.h>` declaration in the two candidate cells. The latter separates
local-versus-member access because the reference later reloads the member for
its sign test. The fractional part remains `abs((int)(...))` in all cells.
The local form improved the target and exposed a distinct store-order residual,
so a bounded second batch adds two cells placing the existing member assignment
before both conversions or between the whole and fractional conversions.

Artifacts and complete commands are recorded in
`build/params-abs-experiment/results.json`; compiler and selected assembler
identity are in `build/params-abs-experiment/toolchain.txt`. The run uses the
published Gentoo GCC 3.4.2-r2 image natively, the retained full-TU profile, and
appends `DSPLIB_REPRODUCE_BUGS` last. The generated filename changes STT_FILE;
the unchanged control must otherwise reproduce the retained TU before the
candidate cells are interpreted.

## Result

The corrected five-cell run is valid over 9 shared symbols. The control
reproduces every retained body and relocation: 7/9 symbols are exact against
the blob, the target is `SIZE 12` (344 reference bytes, 332 candidate bytes),
all 20,596 positioned non-NOBITS bytes and all 626 relocations match, and only
the generated STT_FILE name differs structurally.

`fabs-local` changes only the target and improves it to `SIZE 8` (352 bytes).
`fabs-member` is worse at `SIZE 16` (360). Moving the member assignment before
both conversions is `SIZE 8` (352); placing it between the whole and fraction
declarations is closest at `SIZE 4` (348). Every candidate retains 7/9 exact
symbols, changes only the target body, and leaves non-text contents unchanged.

The finite family supports `(int)fabs(pr)` and rejects member-based `fabs`
under this profile, but does not close the function. The closest cell still
converts the absolute whole part before the signed fractional integer; the
reference keeps the absolute x87 value live, converts the signed original,
then converts the absolute value. The evidence-backed integration candidate is:

```cpp
float pr = (float)(int)(tempPR / 5) * 0.5f;
int whole = (int)fabs(pr);
DIGITAL_POWER_REDUCTION = pr;
int frac = (int)((pr - (float)(int)pr) * 100.0f);
```

This requires `<math.h>` and removal of the later duplicate assignment. Keep
the fractional integer absolute value unchanged. Re-anchor the existing
whole-sign equivalent mutation to the floating absolute expression; F879's
proof still applies because the reachable value is finite, nonnegative, and
within signed-int range.
