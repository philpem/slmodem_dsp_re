# Maintainer MG-03: bounded typed-access recovery

At baseline `ac7f28e6`, issue #260 has already modelled the V.34 receiver
and its timing/equalizer subobjects. The guide's references to `pad_000`,
`pad_2a8` and a live `hs_put` are stale. The remaining `hs_get` has exactly
three callers, all in `datapumpv34`, reading root `+0xaa98` before storing
the receiver's `baud_copy`.

## Retained change

Replace those three reads with `obj->ratecfg.rxbits`, whose declared type
is the same signed `short` returned by `hs_get`. Remove the now-unused
`DP_FAA98` offset macro and raw accessor. No field is renamed or retyped,
no comparison is changed and the receiver store path is unchanged.

The original disassembly at `0x71960..0x71d64` contains the three `+0xaa98`
loads at `0x71b82`, `0x71c77` and `0x71cf8`. Their upper halves are discarded
by the destination stores; this cleanup makes no new signedness claim from
the zero-extending instruction. The already recovered field is the authority
for the direct expression.

A complete-TU audit using `tools/toolchain/byteident.py`'s `body` and
`verdict` routines compares all 54 emitted functions. The minimal candidate
leaves the complete ELF object byte-identical to the baseline, including
all bodies, relocations, data and symbol records. This is a representation
improvement, not a newly byte-exact function.

## Bounded negative control and remaining work

Two cells were measured at the same source/profile baseline:

| Cell | Scope | `datapumpv34` size | Whole-TU effects |
|---|---|---:|---|
| Baseline | Existing offset helpers | 988 bytes | 54 emitted functions |
| A, retained | Three direct `ratecfg.rxbits` reads | 988 bytes | Complete object unchanged |
| B, withheld | Direct receiver fields, typed counter-pointer helper, same signed promotions and wrap | 919 bytes | Only `datapumpv34` changes; no added/removed functions or exact losses |

The original body is 1028 bytes. Cell B changes the non-exact body's size gap
from 40 to 109 bytes. That number alone does not establish a semantic defect,
but no independent evidence recovers its changed helper/counter lowering.
It is not adopted as a source reconstruction merely to remove more offsets.
The earlier F8044/F8045 investigation similarly distinguishes the original's
16-bit comparisons from superficially closer but wrong rewrites. Neither
cell changes those comparison types or seeks a new compiler profile.

The remaining `dp_rxget`, `dp_rxput` and `dp_run` group is explicitly open in
[issue #286](https://github.com/philpem/slmodem_dsp_re/issues/286).
Revisit it when original control-flow/operand or compiler-stage evidence
distinguishes a typed formulation while preserving the measured behaviour.
No further spelling matrix is justified by this audit. Unmapped storage
without supported readers/writers remains neutrally represented; it is not
safe to manufacture fields or semantic names for it.

## Toolchain and reproduction

Both cells use the recovered
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest` image, GCC 3.4.2-r2 and
the assembler actually selected by that compiler (GNU assembler 2.15.92.0.2).
The complete C option vector from `build/tc_repro/.build-config` is:

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-DDSPLIB_REPRODUCE_BUGS -DDSPLIB_REPRODUCE_BUGS
```

The experiment uses `tools/experiment_toolchain.py`'s `compile_shell` helper,
which appends a final identical reproduction define. The extra repeated
define changes no macro value. The compiler command and executed compiler/
assembler identities are retained with the local cell artifacts under
`build/mg03/`; complete baseline objects are in `build/maintainer-baseline/`.
The C++ and DCR-specific flags are unmodified; neither cell compiles them.

For the retained tree, rebuild with `make tc-repro`, compare the complete
object against the saved baseline, and run `make phase`. The artifacts are
diagnostics, not replacements for the deciding period gate.

## Retained validation

The integrated fix tree preserves all 300 complete period source objects
byte for byte against `ac7f28e6`. `make phase` passes 388 differential
binaries with zero failures and all structural checks. The anchor gate checks
10,038 mutations in 285 suites with zero detached anchors; two existing
rate-copy anchors were updated to the new field expression without changing
their labels or replacement semantics.

The focused modern `t_v34datapump` also passes 16,850 checks. A complete rerun
of its 78 active mutations reports 75 caught, one uncaught, two equivalent,
zero unusable and zero miscounted. This matches the recorded verdict pattern;
the uncaught claim is still “the receiver loop runs before the modulator loop”.
It remains a coverage limitation, not an equivalent mutation or a passing
mutation gate. No snapshot is relabelled current and no tolerance is changed.
