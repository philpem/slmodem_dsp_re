# Issue #40: load-bearing V.90 square-root wrappers

`V90ConstellationDesigner.cpp` and `V90TRN2Designer.cpp` each contain a
diagnostic helper with the same essential shape:

```
static inline double helper(double x) { return sqrt(x); }
```

Their callers first promote a `float` power to `long double`; the helper then
forms the reference's double argument and result boundary before the x87
square root. The diagnostics deliberately evaluate the root repeatedly.

The bounded recovered-Gentoo GCC 3.4.2-r2 candidate domain was:

1. Keep the helper and replace its builtin body with standard `sqrt`.
2. Remove the helper and substitute `__builtin_sqrt` directly.
3. Remove the helper and retain the explicit double conversion through a
   function-like macro, `__builtin_sqrt((double)(x))`.

Candidate 1 leaves both complete translation units byte-identical to clean
master. Candidates 2 and 3 move both: each diagnostic-bearing function loses
0x20 bytes and the resulting register allocation, branches, offsets and
relocations move with it. An explicit conversion therefore does not replace
the compiler-visible inline-function source shape.

The accepted result is deliberately narrow: both named `static inline double`
wrappers, their parameter type, their call sites and repeated evaluations stay
unchanged. Only their body spelling becomes standard `sqrt`, using the
existing `<math.h>` declarations. The full recovered-Gentoo object comparison
and differential suite are the acceptance evidence; no style-driven removal
is claimed.
