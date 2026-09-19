# Issue #30: GCC 14 x87 portability — census, register reconciliation, and the sinc domain

Work on [issue #30](https://github.com/philpem/slmodem_dsp_re/issues/30), the
GCC 14 follow-up to #19.  Branch `improve/issue30-modern-x87`, based on
`origin/master` at `db6eee3e` (the #82 merge).  `make period` — the deciding
tier — is **376 passed, 0 failed**.  The modern tier is red, and this document
records its current state, reconciles `tools/gccdiverge.json` against it, and
bounds the one candidate fix.

This is an investigation result, not a green portability fix.  No tolerance is
widened, no `src/` file is edited, and no instruction-forcing shim is added.

## 1. Census — measured, with denominators

Compiler: **Debian GCC 14.2.0-19** (`gcc (Debian 14.2.0-19) 14.2.0`).  Command:

    make -j1 -k test > /tmp/opencode/issue30-census.log 2>&1

376 fixtures are registered.  Before reconciliation **34 run targets exited
non-zero**: 342 green, 34 red.  After the register changes below, **347 green,
29 red**.

Three of the red fixtures were already declared and excused by the register
(`t_v90p4dnan`, `t_v90specproc`, `t_v90connevalnan`) and are counted green.
Two more were red only because their declaration had gone STALE
(`t_v90adidnan`, `t_v92ecnan`); three more because a declared fixture had grown
an undeclared check (`t_psd`, `t_v90equ`, `t_v90equproc`).

### (a) Declared and green

| fixture | register entry | checks excused |
|---|---|---|
| `t_v90p4dnan` | F4812 | 1 |
| `t_v90specproc` | F5804 | 1 |
| `t_v90connevalnan` | F2410 | 1 |

### (b) STALE declarations — removed

`t_v90adidnan` (F6001) and `t_v92ecnan` (F6000) now **pass**.  Both are the
NaN-compare class: the object compares with a single ordered `fcom` and no
parity test, so a NaN takes the equal/zero arm.  The recovered source profile
is `-ffast-math` (`CXXMATHFLAGS`), which implies `-ffinite-math-only`; under it
GCC 14 is entitled to assume no NaN and folds `v == 0.0f` / `out[0] == 177.0f`
to a plain ordered compare, which is what the object has.  The divergence the
two findings describe reappears the moment the profile stops assuming no NaN,
so the entries must be re-declared if `-fno-finite-math-only` is ever adopted.
A stale entry is an error (the register must not lie), so both are deleted.
Finding F11363.

### (c) Uncovered divergences — 29 fixtures

Failing checks / total checks per fixture, from a clean per-binary run (the
`make` log interleaves a passing fixture's output into the preceding failing
fixture's block, so this list is taken by running each binary directly):

| fixture | failing / total | cause |
|---|---:|---|
| `t_dspmath` | 4 / 630272 | sinc/transcendental return narrowing (`fcos` windows) |
| `t_floatarma` | 3957 / 24368 | x87 excess precision |
| `t_gtonedet` | 1703 / 23361 | x87 excess precision |
| `t_resampler` | 3174 / 23067 | sinc/transcendental return narrowing |
| `t_v27fax` | 8 / 272 | other — FAX, outside #30's x87 scope |
| `t_v34hshak` | crash (rc 245 = SIGSEGV) | other — not a check divergence |
| `t_v34info1a` | 988 / 68561 | x87 excess precision |
| `t_v90adid` | 42 / 2432 | x87 excess precision |
| `t_v90cdadjust` | 26 / 977 | x87 excess precision |
| `t_v90cdesign` | 71 / 496 | x87 excess precision |
| `t_v90cdnoise` | 288 / 5354 | x87 excess precision |
| `t_v90dataph` | 16 / 13444 | x87 excess precision |
| `t_v90demctor` | 32 / 6499 | x87 excess precision |
| `t_v90demod` | 116 / 218992 | x87 excess precision |
| `t_v90demprog` | 197 / 2010 | x87 excess precision |
| `t_v90eqdata` | 2 / 1256 | x87 excess precision |
| `t_v90leaves` | 303 / 9709 | x87 excess precision |
| `t_v90modprog` | 2 / 60 | x87 excess precision |
| `t_v90p3ddec` | 43687 / 2111618 | x87 excess precision |
| `t_v90prefilter` | 2 / 21420 | sinc/transcendental (filter design) |
| `t_v90rundemod` | 1 / 953618 | x87 excess precision |
| `t_v90specialcond` | 4 / 1826 | x87 excess precision |
| `t_v90spectral` | 304 / 40274 | x87 excess precision |
| `t_v90trn2design` | 4 / 2490 | x87 excess precision |
| `t_v92dec` | 588 / 149177 | x87 excess precision |
| `t_v92modstate` | 1957 / 36848 | sinc/transcendental return narrowing |
| `t_vpcmflomodem` | 27 / 2004 | x87 excess precision |
| `t_vpcmqcline` | 1 / 188544 | x87 excess precision |
| `t_vpcmrunpcm` | 27933 / 1321588 | x87 excess precision |

Every one of these is a **modern-only** divergence: the period compiler, at
the project's own flags including `-ffast-math`, passes every group
(`make period` 376/0).  The assignment above is by the failing group's own
function and the source-level class already documented for it, not by a fresh
per-fixture RTL investigation; the boundary between the two x87 sub-classes is
the only soft one.

The dominant class is unchanged from #30's prior finding: the modern compiler
keeps intermediates the object narrows.  `-fexcess-precision=fast` (the C++
default and what `-ffast-math` selects) is the mechanism.

## 2. Register reconciliation

`tools/gccdiverge.json` went from 8 entries / 13 checks to **6 entries / 14
checks**, all ALLOWED, 0 stale, 0 uncovered:

| entry | change | evidence |
|---|---|---|
| `t_psd` | `+ Psd::getFrequencies` | object `fildll; fmul; fmul; fstps` (three extended steps, one rounding); modern `-ffast-math` reassociates to a reciprocal and one `fmul`, 1 ULP on bin 39; period passes all 8169 |
| `t_v90equ` | `+ the shift at an exact power of two` | `convertEqualizerToMmx`'s float accumulators stored to stack each iteration (`fstps 0x4c(%esp)` / `0x48(%esp)`); modern keeps them in x87; period passes all 105 |
| `t_v90equproc` | `+ the phase 4 state arms` | same `float err = soft - fdec` subtraction as the declared RESET arm (F6203); period passes all 18023 |
| `t_v90adidnan` | **removed** | now passes under `-ffast-math`'s `-ffinite-math-only` fold |
| `t_v92ecnan` | **removed** | now passes under `-ffast-math`'s `-ffinite-math-only` fold |

Each addition names a check and the object's own instruction, and each is
green on the period compiler at the same flags — which is the register's
precondition.  Verified after the edit:

    t_psd                ALLOWED (rc=0)
    t_v90equ             ALLOWED (rc=0)
    t_v90p4dnan          ALLOWED (rc=0)
    t_v90equproc         ALLOWED (rc=0)
    t_v90specproc        ALLOWED (rc=0)
    t_v90connevalnan     ALLOWED (rc=0)

## 3. The sinc discriminator — measured domain

The blob's `sinc<float>` (`.gnu.linkonce.t._Z4sincIfET_S0_`, 46 bytes) is

    flds x; fcoms 0.0f; fnstsw; sahf; je <one>
    fldl <pi double>; fmulp st,st(1)      ; y = pi*x, EXTENDED
    fld %st(0); fsin; fdivp st,st(1)      ; sin(y)/y, EXTENDED
    fstps (%esp); flds (%esp); ret        ; the binary32 RETURN NARROWING

No modern flag profile reaches `fsin` + a single double pi load + the return
narrowing.  Compiled with the project's own base flags plus each variant, and
the emitted `sinc<float>` disassembled (`/tmp/opencode/sinc_probe.cpp`):

| math flags | `fsin` | return narrow | pi load | notes |
|---|---|---|---|---|
| `-ffast-math` (current) | yes | **no** | `fldl` | the witness |
| `-ffast-math -fexcess-precision=standard` | yes | **no** | `fldt` | also changes pi |
| `-ffast-math -ffloat-store` | yes | yes (5 stores) | `fldl` | rounds every intermediate |
| `-ffast-math -fno-finite-math-only` | yes | **no** | `fldl` | parity compare, `fucomi` |
| `-funsafe-math-optimizations` (restricted) | yes | **no** | `fldl` | parity compare, `fucomi` |
| restricted `+ -fexcess-precision=standard` | yes | **no** | `fldt` | |
| ordinary flags | no (library `sin`) | yes | `fldl` | the 49/4019 divergence |
| `+ -ftrapping-math` / `-fsignaling-nans` / `-frounding-math` / `-O1` / `-O3` | yes | **no** | `fldl` | no change |

`-ffloat-store` is the only flag that supplies the narrowing, and it does so by
storing **every** float/double intermediate — four to six stores in the body
instead of the object's one — which rounds `y` to `double` before `fsin`.  The
object keeps `y` extended.  So it is not the object's sequence and it changes
the sine argument; the prior work measured its downstream cost (state/FIR
still fail).  The missing narrowing is emitted after optimized GIMPLE and is
not selected by any of `-fexcess-precision`, `-ftrapping-math`,
`-fsignaling-nans`, `-frounding-math` or the optimization level.

**The measured domain, stated precisely:** under `-ffast-math`, GCC 14 emits
`fsin` with the object's single double pi load but omits the binary32 return
narrowing; every tested flag that restores the narrowing either replaces
`fsin` with a library call or rounds intermediates the object keeps extended.
The combination is unreachable by flags in this domain.  Downstream consumers
`t_resampler` (3174/23067) and `t_v92modstate` (1957/36848) remain red;
`t_lowpassfir` is link-excused by #82 and has no binary to run.

## 4. Next discriminating test

The register is reconciled and the flag domain is bounded.  The next test is
**not** another option matrix and **not** another caller/inlining or
standard-precision repeat (both were exhausted in #19).  It is to identify the
RTL/lowering pass that drops the float conversion, and to determine whether a
`tools/toolchain/` apparatus shim is justified — or whether the tier is
declared per-fixture as the x87 class.  Concretely:

1. Take the `sinc<float>` probe, compile it with
   `-ffast-math -fdump-rtl-expand -fdump-rtl-combine -fdump-rtl-<pass>` and
   find the pass in which the `(float)` conversion present in the optimized
   GIMPLE disappears.  The #19 ledger already showed the cast survives into
   optimized GIMPLE (`_2 = s_6 / y_5; _7 = (float) _2;`), so the search starts
   after `optimized` and ends at `final`.
2. If the pass is identified and no flag selects it, decide between (a) a
   `tools/toolchain/` shim that re-inserts the narrowing for the modern build
   only, and (b) declaring the sinc/FIR/V92 consumers in `tools/gccdiverge.json`
   the way `t_psd`/`t_v90equproc` are, each in its own binary per
   F2157/F3002/F6000-6002.  (b) is the lower-risk option and is already the
   pattern for the x87 class.
3. Separately, `t_v34hshak`'s SIGSEGV is not a check divergence and is not
   explained by x87; it needs its own issue.  `t_v27fax`'s 8/272 is FAX, which
   `AGENTS.md` puts last, and is likewise not #30's x87 class.

## Reproduction

    make -j1 -k test > /tmp/opencode/issue30-census.log 2>&1
    # per-binary clean failure list:
    for t in <29 fixtures>; do ./build/test/$t 2>&1 | grep '^FAIL '; done
    # sinc probe: see /tmp/opencode/sinc_probe.cpp and the table above
    make period J=1        # 376 passed, 0 failed
    python3 tools/refcheck.py

Artifacts: `/tmp/opencode/issue30-census.log`,
`/tmp/opencode/issue30-clean-fails.txt`, `/tmp/opencode/sinc_probe.cpp`,
`/tmp/opencode/sinc/`, `/tmp/opencode/psdvar/`.  Finding F11363.

(2026-09-19)

## 5. Resolution (2026-09-19): the modern tier is functional, not byte-exact

The project owner set the rule this section implements:

- **`make period` (GCC 3.4.2-r2 Gentoo) is the reconstruction authority** and
  stays byte/value-EXACT against the blob, **no allow-list**.
- **The modern tier (GCC 14) is a portability check.**  It must produce a
  FUNCTIONALLY CORRECT result, not the blob's exact code or its exact x87
  values; x32->x64 legitimately changes codegen and rounding.
- The tolerance is **modern-tier-only, documented, denominator-reporting, and
  never used to excuse a period failure**.

### What was measured, per fixture

Each of the 29 was run with `DSPLIB_MAX_REPORT=0` and every failing source line
classified by the harness comparison form it reaches.  The full table is in
F11364; the summary is:

- **Rounding-level and harness-reachable: one fixture.**  `t_v90cdesign` -- 71
  checks, all `diff_eq_float`, max **2 ULP**.  It is now green:
  `PASS ... 496 checks (71 within modern tolerance)`.
- **Rounding-level in value, but NOT harness-reachable** (raw object bytes, a
  boolean `memcmp`, or a raw word inequality): `t_floatarma` (1 ULP returns,
  struct bytes), `t_gtonedet` (one byte = 1 ULP), `t_resampler` (1..113 ULP
  coefficient bits), `t_v92modstate`, `t_v90demprog`, `t_v92dec`,
  `t_v90demctor`, `t_v90leaves`, `t_vpcmrunpcm`, `t_v34info1a`,
  `t_vpcmflomodem`, `t_v90adid`.
- **Transcripts, left to the register**: `t_v90cdadjust`, `t_v90cdnoise`,
  `t_v90dataph`, `t_v90demod`, `t_v90eqdata`, `t_v90specialcond`.  A transcript
  encodes decisions as well as formatted floats, so parse-and-compare was
  declined.
- **NOT rounding-level -- changed outcomes that stay hard failures**:
  `t_v90prefilter` (bank index -2 vs 3547), `t_v90trn2design` (ucodes, dMin),
  `t_v90p3ddec` (decision -1480 vs 0), `t_v90spectral` (verdict, counts),
  `t_v90modprog` (flag byte), `t_dspmath` (0.08 vs the blob's NaN at n==1),
  `t_v90rundemod`/`t_vpcmqcline` (test meta-assertions), `t_v34hshak`
  (SIGSEGV), `t_v27fax` (FAX).

### Why the tolerance cannot reach the rest

`diff_eq_int` on `0`/`1` is a decision; making it float-tolerant would excuse
every boolean in the suite.  `diff_eq_obj` has no field-type information at
runtime.  Extending the mechanism to those forms is larger and riskier than the
tier can justify, and the decision-level reds would remain red regardless.  So
the tolerance is implemented where it is honest and the rest is reported.

### The mechanism

`HARNESS_FLOAT_TOL`, a Makefile-provided `-DHARNESS_FLOAT_TOL=1e-6` on the
modern harness object only (never a `__GNUC__` test).  It is a RELATIVE
criterion `|a-b| <= eps*max(|a|,|b|)` inside `diff_eq_float_` alone, applied
only where the call site stated no budget; `diff_end` prints the count of
checks that passed only because of it; `test/safety/t_float_tol.c`
(`make safety`) is the adaptive negative control.  Full detail, including why
1e-6 (~8 ULP) and the four negative-control cases, is in
`docs/method/compilers.md` and F11364.

### Verdicts

- `make period J=1`: **376 passed, 0 failed** (the define is absent from the
  period compile; the tolerance code is not compiled there).
- Modern tier: **28 red**, down from 29 -- `t_v90cdesign` closed, the rest red
  for the reasons above.  The tier is NOT green and this pass does not claim it
  is.
- `python3 tools/refcheck.py`: clean.

(2026-09-19)
