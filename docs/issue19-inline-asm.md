# Issue #19: inline-assembly removal

Work on [issue #19](https://github.com/philpem/slmodem_dsp_re/issues/19),
starting at `bcc86c61`. This is an implementation/evidence ledger, not a claim
that the complete reconstructed object is exact. Outstanding work stays in
the issue. No differential tolerances are changed.

## Commit disposition — 2026-09-10

The project owner requested committing and pushing the period-validated
issue #19 changes with modern portability tracked separately in
[issue #30](https://github.com/philpem/slmodem_dsp_re/issues/30).
The current source/header census contains no active asm statements or symbol
aliases. The deciding period suite passes **375 tests, 0 failures**; the
modern GCC 14 phase gate remains failing. The compiler-option controls below
are experiments, not retained fixes. Neither phase completion nor final
partial-link identity is claimed. Historical first-pass exactness counts
below do not establish the final candidate's code-generation census.

## Current bounded validation repair — 2026-09-10

### Modern return-boundary discriminator: four new negative controls

The next bounded pass inspected the actual prior nine-cell script and logs.
Those cells were signed zeros, trapping, both, explicit fast excess precision,
DspMath-only, SineWave-only, rounding, float-store and standard precision.
Standard precision was already applied narrowly to both trig TUs, with ordinary
fixtures, and compiled successfully: DspMath failed **725/670,171** (49 sinc,
598 cosine-window, 78 dispatch checks). It was not rerun. Its existing sinc
object still lacks float return narrowing and loads pi with `fldt` from
`.rodata.cst16`, rather than the restricted-unsafe object's `fldl` from
`.rodata.cst8`. Applying that same DspMath compilation alone cannot change its
own emitted return sequence.

Declared and exhausted four new modern-only controls, holding restricted
unsafe expansion in DspMath/SineWave and ordinary flags elsewhere:

1. Disable IPA in DspMath: cp, cp-clone, sra, pure-const, reference, modref,
   icf, vrp, bit-cp, ra and stack-alignment.
2. Disable inlining in DspMath: inline-functions, inline-small-functions,
   inline-functions-called-once, early-inlining and inline.
3. Both sets in DspMath.
4. Both sets in the complete LowPassFIR and V92Modulator caller TUs instead.

All four use explicit `-fno-finite-math-only` for DspMath, alongside
`-funsafe-math-optimizations -fno-associative-math -fno-reciprocal-math`.
Saved ordinary-cache source hashes were checked before reuse. Existing ordinary
fixture objects and targeted SineWave were reused; full link commands identify
every replacement. GCC/selected assembler identities, compiler commands,
source hashes and original/optimized tree dumps are retained. These are
portability controls, not period reconstruction or byte-identity claims.

**6/6 complete-TU compilations and 12/12 executions completed**, with identical
test verdicts in every cell:

| Fixture | Failures / checks, each cell |
|---|---:|
| `t_dspmath` | **0 / 670,171** |
| `t_lowpassfir` | **12,472 / 365,528** |
| `t_v92modstate` | **1,937 / 39,798** |

FIR failures remain 9,543 construction, 2,645 primitive paths and 284 redesign;
all 6,511 nonfinite-cutoff checks pass. V92 failures remain 1,821 resampling
and 116 progress; all 65 independent boundary checks pass.

**The cast is not removed from optimized GIMPLE.** In the no-IPA DspMath dump,
`_2 = s_6 / y_5; _7 = (float) _2;` feeds the float return PHI. Nevertheless,
the emitted sinc has fsin/division and no `fstps/flds` return rounding. This
locates the missing machine narrowing after optimized GIMPLE; it does not
identify a specific RTL pass or prove that the ABI requires this behavior.
Disabling caller IPA/inlining does not close the consumer failures. The
minimal witnessed incompatibility remains extended-argument fsin plus a
binary32-narrowed return: ordinary modern supplies the narrowing but calls
double-argument library sin; restricted unsafe supplies fsin but omits the
return narrowing; standard precision also omits it and changes the pi load.

Bounded collateral inventory versus the saved modern controls: each DspMath
object has **11 shared function symbols**, no additions/removals; the two
no-inline cells change designWindow's size. Caller controls have **6 shared
LowPassFIR symbols**, one added delete helper and two constructor size changes;
V92Modulator has **18 shared**, 16 added helper symbols, one removed local
resampling clone and seven shared size changes. These are symbol-size/binding
inventories, not full-body equality scores; raw objects differ and no candidate
was selected for retention.

No source overload/type candidate is justified by this result. The reference
double pi/product and final float spill do not independently establish a float
sin overload; changing the sine input type also changes the input boundary.
Any such proposal still needs reference full-TU evidence and a period cross,
not just a modern green leaf test. No source, build apparatus, fixture,
allowlist or tolerance change was retained, and no phase rerun was warranted
after all four focused controls failed. This domain is closed negatively.
Reopening requires a discriminator in lowering the surviving float conversion
to x87 code, rather than another caller/inlining or standard-precision repeat.

Artifacts: `/tmp/opencode/issue19-modern-boundary.py` and
`/tmp/opencode/issue19-modern-boundary/` (`summary.txt`, manifest, per-cell
commands, dumps, symbol inventory and complete test output). Execution was
foreground under `timeout --foreground --kill-after=10s 600s`; no agents or
commits were used.

### Targeted modern trig follow-up: rejected by downstream witnesses

The next requested configuration was measured: ordinary modern C++ flags
(including the existing `-fno-math-errno` and `-fno-builtin-memcpy`) everywhere,
with only the complete `DspMath.cpp` and `SineWave.cpp` TUs additionally receiving
`-funsafe-math-optimizations -fno-associative-math -fno-reciprocal-math`.
This is a **modern per-TU semantic-compatibility experiment for the original
x87 transcendental expansion**, not an assertion of period per-TU profiles.
Finite-only assumptions are disabled in the recorded optimizer dump. Period
source/profile, fixtures, comparators and divergence declarations were unchanged.

**It does not pass.** The two-TU Makefile candidate was used for phase and then
withdrawn, restoring the entry Makefile math configuration. No reconstruction
edits, mutation snapshot refreshes, divergence removals or commits were made
by this follow-up. Prior working-tree changes remain intact.

#### Cached focused control: 16 binaries × two profiles

The ordinary cache contains all 84 C++ TUs; every source SHA256 was checked
against its saved manifest before reuse. Both trig TUs and all 16 fixtures
were freshly compiled, with complete commands and the reproduce-bugs define.
The cache is the completed `issue19-modern-precision/strict` control, not a
new period/code-generation measurement. Compiler remains Debian GCC 14.2.0-19.

| Exact check or binary | Ordinary control | Targeted trig |
|---|---:|---:|
| DspMath, 670,171 checks | 49 failures (sinc) | 0 failures |
| SineWave, 9,738,336 checks | 28,789 failures | 0 failures |
| Uinfo, 2,004 checks (6,121 in binary) | 0 | 0 |
| V.92 mkResampledSignal, 35,625 checks | 0 | 1,821 failures |
| V.92 progress, 1,223 checks | 0 | 116 failures |
| V.92 independent 15-sample boundary, 65 checks | 0 | 0 |
| Demodulator/session termination, 222,518 checks | 0 | 0 |
| GenericToneDetector, 28,220 checks | 0 | 0 |
| V.92 modulator, 47,340 checks | 0 | 0 |
| V.92 converter/mapper, 19,986 checks | 0 | 0 |

The other eight binaries fail under both profiles. In the targeted run:
`Psd::process` **7,550/140,404**; equalizer beta setters **5,608/86,026**,
power-of-two shift **80/105**, enterPhase3 **2/2,396**, freeze **2/480**,
enterRRN/FPE **7/1,244**, enterPhase4 **1/1,092**, convert-to-MMX **618/4,778**;
equalizer process RESET **731/120,974** and phase-4 arms **1/18,023**;
spectral process **181/35,450**; AGC process **15,504/1,524,096**;
P4 unordered flag **2/9**, echo sentinel **492/2,703**, ADID unordered scan
**6/25**. In particular, the equalizer power-of-two and phase-4 process
failures are outside those binaries' listed divergent groups. Library
logarithms therefore cannot be assumed adequate from the green Uinfo result.
No declaration is stale under this configuration, and removing declarations
would not fix these failures. The earlier fast-math stale-AGC result is not
transferable to this profile.

#### Full phase and bounded witness follow-up

`make phase J=2` completed with **Error 2**. Its period tier reports
**375 passed / 0 failed**. The modern run stops at `t_lowpassfir`:
construction **9,543/322,440**, primitive window paths **2,645/33,775**,
redesign **284/2,802**; nonfinite cutoffs still pass **6,511 checks**.
The instrumented tier reports seven disagreeing binaries:
`t_lowpassfir t_resampler t_v90cdesign t_v90demctor t_v90demprog
t_v90modprog t_v92modstate`. This is not a completed phase verdict.
The candidate Makefile invalidates all modern mutation snapshot fingerprints:
phase reports **0 current / 262 stale / 262 registered**, none missing.
No results were re-recorded against this rejected configuration.

New witnesses licensed only a bounded follow-up in the same two TUs, testing
DspMath, SineWave, V.92 state and low-pass FIR (four binaries per cell):

- Restoring signed-zero semantics, trapping semantics, or both; explicitly
  selecting fast excess precision: all four cells pass both direct trig
  binaries and fail state/FIR.
- DspMath-only expansion: DspMath passes; SineWave, state and FIR fail.
  SineWave-only expansion: SineWave and state pass; DspMath and FIR fail.
  Thus DspMath's change suffices to reintroduce the state failure with every
  other TU held ordinary.
- A concrete assembly witness then licensed three return-boundary controls:
  `-frounding-math` still passes direct trig and fails state/FIR;
  `-ffloat-store` and `-fexcess-precision=standard` pass SineWave but fail
  DspMath, state and FIR. None is an accepted configuration.

The assembly witness is `sinc<float>`: the blob's linkonce section has
`fsin`, division, then **`fstps (%esp); flds (%esp)` at +0x20/+0x23** before
return. Targeted GCC 14 emits `fsin` and division but returns without that
float narrowing. Ordinary GCC 14 retains the return narrowing, but calls
library `sin` after narrowing its argument to double. Direct `t_dspmath`
rounds the returned value for comparison; its green result alone therefore
does not establish the precision received by callers. This is a measured
instruction difference, not proof that it explains every downstream failure.
The next discriminator must address this return boundary while retaining the
extended sine argument, and must check the caller as well as the leaf. No
further common-profile or arbitrary option matrix is licensed by these runs.

Artifacts and reproducible commands (all foreground, finite timeouts):

```sh
timeout --foreground --kill-after=10s 600s python3 /tmp/opencode/issue19-modern-targeted.py
nohup timeout --foreground --signal=TERM --kill-after=60s 1800s \
  make phase J=2 > /tmp/opencode/issue19-modern-targeted-phase.log 2>&1
timeout --foreground --kill-after=10s 600s python3 /tmp/opencode/issue19-modern-trig-witness.py
timeout --foreground --kill-after=10s 600s python3 /tmp/opencode/issue19-modern-trig-witness.py return
```

Focused output/commands: `/tmp/opencode/issue19-modern-targeted/` (32 runs),
`issue19-modern-trig-witness/` (24 runs), `issue19-modern-trig-return/`
(12 runs). The phase candidate was a recursive `CXXMATHFLAGS` selected by
`$(filter src/dsp/DspMath.cpp src/dsp/SineWave.cpp,$<)`, applied by the existing
modern source recipes, including 64-bit; all other expansions were empty.

### GCC 14 portability follow-up: bounded negative result

The later portability investigation found **no passing common modern flag
profile in the tested domain**. No reconstruction, fixture, Makefile, shim,
allowlist or tolerance change was retained by this follow-up. The existing
candidate still passes the period tier, **375 binaries passed / 0 failed**.
The latest phase attempt stops earlier in the modern run at the **stale
`t_agc` divergence entry**: all seven AGC groups pass, so the register's
nonzero-baseline requirement rejects it. This does not establish that later
modern tests pass; the isolated controls below reproduce their failures.

At investigation entry, `CXXFLAGS` already excluded fast math and
`CXXMATHFLAGS := -ffast-math` was applied only to source C++ recipes, including
the 64-bit source recipe. C++ fixtures use ordinary `CXXFLAGS`; C harness
objects use `CFLAGS`. Thus the global-fixture-fast-math concern in the brief
was not the current recipe. Freshly compiling every selected fixture without
unsafe/finite assumptions still reproduces the failures. All reconstruction
compiles append `DSPLIB_REPRODUCE_BUGS` using the shared experiment helper.
The existing `-fno-builtin-memcpy` protection remains present, and signalling
NaN tests were not removed or weakened.

Compiler: **Debian GCC 14.2.0-19**. Complete compiler commands, executed selected
assembler identity, optimizer-option dumps, fresh fixture commands, full link
lines, source hashes and executable output are in:

- `/tmp/opencode/issue19-modern-matrix/`: seven profiles × six complete TUs,
  four fixtures, **42 successful source compiles / 28 executions**;
- `/tmp/opencode/issue19-modern-fullcpp/`: two profiles × all **84 C++ TUs**,
  seven fixtures, **168 successful source compiles / 14 executions**;
- `/tmp/opencode/issue19-modern-precision/`: two profiles × all **84 C++ TUs**,
  seven fixtures, **168 successful source compiles / 14 executions**.

The first domain is fast math; fast math with finite-only, associative, or
reciprocal math individually disabled; fast math with all three disabled;
`-funsafe-math-optimizations -fno-associative-math -fno-reciprocal-math`;
and ordinary flags. The second domain crosses the restricted unsafe profile
with `-fno-builtin-log10l`. The final control compares explicit
`-fexcess-precision=standard` on restricted unsafe math against ordinary flags
for the entire C++ inventory. Optimizer dumps confirm the individual
finite/association/reciprocal switches actually change; fast math explicitly
sets excess precision to `fast`, whereas the unsafe-only dump says `default`.
That observation licensed the final precision control, not a claim about what
the default must mean. Other source and C harness objects retain their existing
Makefile build; these are focused modern portability controls, not complete
period-object or byte-identity measurements.

| Profile / scope | DspMath failures / 670,171 | Uinfo failures / 2,004 | V.92 state failures / 39,798 | Demodulator failures / 222,518 |
|---|---:|---:|---:|---:|
| Fast, six TUs | 4 | 27 | 1,957 | 116 |
| Fast minus finite-only, six TUs | 0 | 27 | 1,957 | 0 |
| Restricted unsafe, all C++ | 0 | 27 | 1,957 | 0 |
| Restricted unsafe minus builtin log10l, all C++ | 0 | 27 | 1,957 | 0 |
| Restricted unsafe + standard excess precision, all C++ | 725 | 27 | 1,957 | 131 |
| Ordinary flags, all C++ | 49 | 0 | 0 | 0 |

Uinfo's full binary contains 6,121 checks. The all-C++ ordinary control also
passes tone detector **28,220**, V.92 converter/mapper **19,986**, and V.92
modulator **47,340** checks. Restricted unsafe math fails **1,703 / 28,220**
tone-detector checks; standard excess precision fails **3,580 / 28,220**.
Both V.92 converter/mapper and modulator binaries pass all four whole-C++
profiles. No tested profile passes all seven binaries.

Precise witnesses retained for the next discriminator:

- Fast window failures: `t_dspmath.cpp:165,173`, input 1000, and `:195`,
  inputs 201000/301000; the reference bits are -4194304 (negative NaN).
  Disabling only finite-only assumptions closes all four.
- Ordinary DspMath: **49 / 4,019 sinc checks** fail; input 7 returns bits
  607372669 against reference 607372280. Ordinary flags therefore cannot
  replace the source math profile globally.
- Restricted unsafe Uinfo: input 20, object byte **+32444**, `86` versus `85`;
  input 21 also differs at **+32388/+32392**. Disabling builtin `log10l`
  does not close these, so attributing this solely to logarithm expansion is
  unsupported. Arithmetic narrowing/optimization remains a discriminator.
- V.92 state: **1,841 / 35,625** `mkResampledSignal` and **116 / 1,223**
  progress failures under restricted unsafe math, starting at trial 0's
  resampled buffer (`t_v92modstate.cpp:1140`). All **65** independent
  15-sample boundary checks still pass. Ordinary flags must reach its callees,
  not just `V92Modulator.cpp`, to close the state binary in these controls.
- Fast demodulator: reset **96 / 14,979**, session termination
  **16 / 192,008**, wrapper **4 / 12,005**; finite-only withdrawal closes
  them in the focused control.

Commands, run sequentially with finite timeouts:

```sh
timeout --kill-after=10s 600s python3 /tmp/opencode/issue19-modern-matrix.py
timeout --kill-after=10s 900s python3 /tmp/opencode/issue19-modern-matrix.py full
timeout --kill-after=10s 900s python3 /tmp/opencode/issue19-modern-matrix.py precision
nohup timeout --signal=TERM --kill-after=60s 1200s make phase J=2 \
  > /tmp/opencode/issue19-modern-portability-phase.log 2>&1
python3 /tmp/opencode/issue19-modern-summary.py
git diff --check
```

Two setup attempts stopped before source compilation at nonexistent guessed
test target names (`t_vpcmxfterm`, `t_v92mapper`); the completed runs use
`t_v90demod` and `t_v92convmapper`. Those setup attempts supply no verdict.
The phase log records period **375/0** at line 4485 and modern stale-AGC
rejection at 4938, ending in Make **Error 2** at 4947. It has no completed
phase denominator/OK verdict. `git diff --check` passes. Further work should
isolate the unsafe arithmetic/narrowing effects independently of natural
transcendental expansion; no instruction-emulating or macro-forcing shim is
licensed by this negative result. No commit was made.

**The five stale anchors are repaired; phase now reaches a genuine modern
differential failure.** The sequential `make phase J=2` run again passes the
period tier, **375 passed / 0 failed**, but stops at modern `t_dspmath`.
The reproduction build and byteident/partial-link comparison remain pending
the requested phase gate. No current exactness counts are inferred from the
historical measurements below.

### Mutation changes and denominator

Inspected the actual JSON and current source expressions. The global anchor
check, repeated inside phase, reports **262 suites / 10,042 mutations**, zero
nonunique anchors, zero identical replacements, zero wrong-arm anchors, and
zero skipped suites. Counts remain **24 in v90equ / 49 in vpcmflomodem**.

- Equalizer reciprocal-multiply versus direct division is reanchored at the
  current linear MMX conversion expression, retaining that arithmetic fault.
- Wrong-base logarithm is anchored at the linear beta setter's numerator.
  Its denominator remains `log10(2)`: changing both logarithms would cancel
  the intended fault. The removed shared helper no longer supplies the anchor.
- The old equalizer asm-to-double-libm mutation is **retired**, since double
  `log10` is already the source. Its replacement omits the numerator logarithm.
- The old long-double-helper-local to double-local mutation is **retired**;
  its replacement narrows the current logarithm result to float. This is a
  new precision boundary, not a claim that the vanished local was tested.
- The formerly equivalent Uinfo asm-to-libm mutation is **retired**, with its
  historical equivalence argument no longer presented as a live asm retention
  rationale. Its replacement changes `log10l(x)` to `logl(x)`, a wrong-base
  math fault. It is no longer marked equivalent. No no-op or duplicate fault
  is inserted to preserve a count, and no catch is attributed to the retired
  implementation change.

The stale `period.mk` explanatory comment now describes the actual fast-math
C++ source profile and the separate fixture profile. Flags and reconstruction
source were not changed by this repair.

### Bounded mutation measurements and snapshot handling

`t_v90equ` is registered in `tools/gccdiverge.json`, so its modern nonzero
baseline cannot judge mutants. A scratch runner compiles each complete mutated
TU with the current **Gentoo GCC 3.4.2-r2** production flags, parsed directly
from `period_inner.sh`, using `experiment_toolchain.compile_shell` to append
`DSPLIB_REPRODUCE_BUGS`. It links the existing full period source/harness
inventory and the unchanged, non-fast-math period fixture. A freshly compiled
unmutated control passes first; fixed link-input SHA256s and production source
are checked unchanged at the end. Compiler, executed selected assembler and
linker identity, complete commands, per-cell output, source hashes and results
are saved with each run. No allowances or differential tolerances are applied.

| Period mutation suite | Complete result |
|---|---|
| `v90equ` | **24 mutants: 16 differential failures, 4 crashes, 1 timeout, 3 survivors** |
| `vpcmflomodem` | **49 mutants: 48 differential failures, 1 crash, 0 survivors** |

**All five repaired/replacement faults produce differential failures**, rather
than relying on crashes or timeouts. The equalizer survivors are the unmasked
shift count (an existing test gap), allocator skew and mode-store/log-call
order (the latter two already declared equivalent). They are not counted as
catches. The coefficient-sum widening fault is caught by the period fixture;
its modern-only historical survivor rationale is not transferred to this run.

Artifacts:

- `/tmp/opencode/issue19-repair-period-mutants-v2/`: complete equalizer run;
- `/tmp/opencode/issue19-repair-period-mutants-vpcmflomodem/`: complete Uinfo suite;
- `/tmp/opencode/issue19-repair-period-mutants.py`: final parameterized runner.

Each compiler/link command has a 180-second limit, each mutant executable a
30-second limit, and each suite an outer 1,200-second limit. An initial runner
attempt stopped at an incorrect expected-count assertion before compiling;
the next stopped at mutant 7's timeout. Those attempts are not complete suite
results. The `v2` run explicitly records timeouts and crashes separately and
completes all 24 cells. Non-foreground timeout invocations that returned no
verdict are likewise not evidence; subsequent commands use `--foreground`.

The ordinary snapshot tool was run only for the five previously edited modern
suites (`vpcmflomodem v92mapper v92mod v92modstate vpcmxfterm`, `--jobs 1`,
1,800-second outer limit). It refreshed only results it could actually measure:

- `v92mapper`: **12/12 caught by test**;
- `v92mod`: **40 mutations: 37 caught by test, 1 unusable, 2 equivalent**.
  A focused rerun confirms the unusable mutation, “the phase 4 modulator is
  handed the mapping parameters as its CP”, fails compilation; this is not
  credited as a catch or mislabeled an anchor failure.
- `vpcmflomodem`, `v92modstate`, `vpcmxfterm`: refused at non-green modern
  baselines and **left stale**, not re-recorded. Direct bounded reruns expose
  their failure summaries below. The snapshot runner omits stderr from its
  short refusal report, so that short report alone was insufficient diagnosis.

The modern snapshot is **2 current / 260 stale / 262 registered**, with no
missing, orphaned or inconsistent entry reported by phase. The custom period
results are deliberately recorded here and in their artifact manifests, not
inserted under modern snapshot keys. In particular, the equalizer and Uinfo
historical snapshot entries remain stale and cannot be quoted as current.

### Sequential phase and precise next blocker

After all mutation commands completed:

```sh
nohup timeout --foreground --signal=TERM --kill-after=60s 7200s \
  make phase J=2 > /tmp/opencode/issue19-repair-phase.log 2>&1
```

The log records period **375/0** at line 566. `make -s print-CC` selects `gcc`;
executing it identifies **Debian GCC 14.2.0-19**. The modern tier then reports:

- `dspmath: the cosine windows`: **2 / 462,336 checks failed** (line 1244);
- `dspmath: designWindow selects the right one`: **2 / 167,936 checks failed**
  (line 1245).

`run-t_dspmath` fails at Makefile:468 and phase terminates with `Error 2` at
Makefile:750. This is the first full-phase blocker after successful structural
checks, not a completed modern/coverage/portability verdict. `t_dspmath` has no
entry in the inspected divergence register. The next bounded investigation is
the current modern cosine-window arithmetic/profile versus the already-green
period run, without modifying reconstruction source just to satisfy modern GCC.

Focused mutation preflights also expose later modern blockers:

- Uinfo: **27 / 2,004** in `getUinfoValue`; four other groups pass.
- V92 state: **1,841 / 35,625** in `mkResampledSignal` and **116 / 1,223** in
  `progress`; the independent 15-sample boundary still passes **65 checks**.
- Session-termination fixture: **96 / 14,979** in demodulator `reset`,
  **16 / 192,008** in `sessionTermination`, and **4 / 12,005** in the wrapper.

These are pre-mutation failures, not mutation catches. They prevent a claim
that fixing only `t_dspmath` will finish phase. No tc reproduction rebuild,
full byteident or partial-link measurement was launched after this failed
gate; comparison to `/tmp/opencode/issue19-r2-baseline-tc` and the saved partial
object remains pending. `git diff --check` passes. No commit or push was made.

## Prior recovered candidate validation — 2026-09-10

**The sections below this update describe the earlier first pass, not the
current source.** Their stopping point and exact-symbol counts are historical.
The current candidate removes all reconstruction asm and enables source-only
C++ fast math. Validation is **blocked by mutation anchors**, not by a period
differential failure. No current full-object exactness result is claimed.

### Source and compiler identity

Validated in `/home/philpem/slmodem_dsp_re`, branch `issue19/remove-inline-asm`,
HEAD `bcc86c61ba14b10410741c71197b9e379b18e554` plus the existing working patch.
The tracked binary diff SHA256 is
`4f21f908018ee1a23ddd8440a823996a50cd4b303a11cef65af2b25e0cf97d08`.
It is preserved as `/tmp/opencode/issue19-recovered-source.diff`; this ledger
is untracked and therefore not included in that hash. The inventory JSON beside
it records individual source/header hashes. A comment/string-aware lexical
census of **495 C/C++ source/header files found zero asm tokens**, and no
`.S`/`.s` files were found under `src/` or `include/`. This is a source census,
not an inference from the earlier ledger or a claim about system-header code.

Executed compiler identity in `dsplibs-tc342-gentoo`:

```
gcc/g++ (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
GNU assembler 2.15.92.0.2 20040927 (i386-pc-linux-gnu)
GNU ld version 2.15.92.0.2 20040927
```

The assembler was executed with `"$(gcc -print-prog-name=as)" --version`,
not identified merely by its path. Current period C++ source flags printed by
both gate runs are:

```
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -Itest/harness -DDSPLIB_REPRODUCE_BUGS
-D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-fno-exceptions -fno-rtti -fno-math-errno -ffast-math
```

`period.mk` has the same production C++ optimization, architecture and compat
flags when invoked for reproduction (`TC_EXTRA=-DDSPLIB_REPRODUCE_BUGS`);
it omits the harness include path and builds no fixtures. `period_inner.sh`
deliberately omits **only `-ffast-math`** from its C++ fixture profile. Its C
profile adds `-std=gnu99`; both paths retain the same local DCR override
`-O2 -fno-rerun-cse-after-loop`. The modern Makefile likewise limits
`CXXMATHFLAGS` to source objects. The comment in `period.mk` saying the C++
flags are independent of unsafe/finite math is now stale: the actual flag
string includes fast math. This validation records that inconsistency without
editing the candidate build/source.

### Recovery after the first-pass stopping point

The current diff replaces the remaining sine/cosine and logarithm asm with
ordinary math calls, including `sin(y)`, `sinl`, `cosl`, and `log10l`, under the
recovered math-header/source profile. The broad first-pass fast-math failure
below is historical evidence about that earlier source, not a verdict on this
recovered candidate.

The next bounded V.92 pass is recorded in
`/tmp/opencode/issue19-v92-boundary-named-v2/REPORT.md`. It independently
identified binary32 `0x3f555555` in both constructor and progress constants,
named it `V92MOD_RATE_RATIO`, and added the 15-sample boundary group with a
literal, production-independent oracle of 12 symbols. The packet reports
**65 boundary checks**, increasing the state fixture to **39,798 checks**.
Its old-quotient/fast negative control fails 14 of those 65 boundary checks;
the named candidate passes both production profiles. It also records
whole-object identity between the literal and named forms for each profile,
over all 24 function symbols. These are prior focused results, not freshly
rerun controls. Their fixtures used fast flags, unlike the intentional fixture
profile in the full gates below.

### Sequential full gates and exact blocker

Commands executed sequentially, each with its own captured exit status:

```sh
nohup timeout --signal=TERM --kill-after=60s 7200s make period J=2 \
  > /tmp/opencode/issue19-recovered-period.log 2>&1
rc=$?; printf '%s\n' "$rc" > /tmp/opencode/issue19-recovered-period.status
# Only after period completed successfully:
nohup timeout --signal=TERM --kill-after=60s 7200s make phase J=2 \
  > /tmp/opencode/issue19-recovered-phase.log 2>&1
rc=$?; printf '%s\n' "$rc" > /tmp/opencode/issue19-recovered-phase.status
```

| Gate | Current result |
|---|---|
| Standalone period | **375 passed, 0 failed**, exit 0 |
| Phase | **FAIL**, exit 2, at `refs` mutation-anchor validation |
| Period within phase | **375 passed, 0 failed** |

The first failure is phase log line 16:

```
NOT UNIQUE  v90equ: log10 via libm rather than the coprocessor     matches 0 time(s)
```

The same check also reports three further zero-match anchors in `v90equ`
(`log2 instead of log10`, `the intermediate is double, not long double`,
`the reference level is divided rather than mul...`) and one in
`vpcmflomodem` (`Uinfo: log10 comes from libm rather than the c...`).
**262 suites / 10,042 mutations checked; five anchors fail uniqueness**,
zero identical replacements and zero wrong-arm anchors. `refs` exits 1;
phase exits 2 after its already-running period job finishes. This is the
structural anchor check, not five executed mutants or a modern numerical
failure. No completed portability/coverage phase verdict is available.

Per the stop-on-gate-failure instruction, no anchors, tests or allowlists were
changed, no mutation campaign was launched, and the tc reproduction rebuild,
full current byteident/full-TU/export audit and partial-link census were **not
run**. In particular, comparison against
`/tmp/opencode/issue19-r2-baseline-tc` and prior reference artifacts is pending;
the historical counts below must not stand in for that comparison.
`git diff --check` passes. No commit or push was performed.

### Narrow conclusion and next validation

The asm-free recovered candidate passes all 375 period differential binaries,
with fixtures intentionally outside fast math. It is not yet phase-clean.
Restore the five mutation anchors on their original fault intent, validate
their effectiveness with only the relevant suites, then rerun the sequential
gates before the mandatory reproduction build and full census. Current exact
gains/losses, nonexact-body collateral, export/binding changes and positioned
partial-link differences remain unmeasured for this final candidate. Broad
fast-math behavior outside tested inputs and unresolved body differences are
not settled by a green differential tier. Neither these gates nor the shared
constant control uniquely recover the original source spelling or compiler
profile.

## Historical first-pass ledger

## Direct member calls

`VPcmXfTerm.cpp` can include `V90Demodulator.h` now that `V90Parameters` has
one definition. Its former free-function alias becomes the ordinary call
`self->modem.demodulator->sessionTermination()`.

`v34pcmmain.cpp`'s five weak aliases and callee-address guards are obsolete:
all five members have strong definitions. `tools/dis.py` identifies direct
reference calls at:

| Reference `.text` offset | Callee |
|---|---|
| `0xb84f` | `GenericToneDetector::process(float *, unsigned)` |
| `0xba41` | `VPcmFloModem::qcLineVerification` |
| `0xbc2b` | `VPcmFloModem::runPcmModem` |
| `0xc7f7` | `VPcmFloModem::v90RunDemodulator` |
| `0xcfef` | `VPcmFloModem::vPcmResetPhase3Modem` |

None tests the callee address. Removing those guards preserves the dispatch
and arguments while eliminating missing-callee fallback paths. The old
recorder API remains for test compatibility; no member path records a failure.
The empty weak-declaration macros in the two class headers are removed too.

### Period full-TU controls

The shared experiment helper compiled both unchanged TUs byte-identically to
the faithful baseline before editing. Compiler: Gentoo GCC 3.4.2-r2;
selected assembler: GNU 2.15.92.0.2. Complete C++ flags:

```
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-fno-exceptions -fno-rtti -DDSPLIB_REPRODUCE_BUGS
```

The final reproduction define is appended by `experiment_toolchain.py`;
the baseline configuration already contains it as well.

Across the two TUs: **19 shared function symbols, no added/removed function
definitions**. Sixteen bodies are EXACT versus baseline. The two size changes
are `VPcmV34Progress` (6755 to 6462 bytes) and the apparatus recorder reset
(19 to 8 bytes). `v90Phase34` has an UNRESOLVED relocation in `byteident.py`;
its complete objdump instruction/relocation listing is unchanged. This is not
silently promoted to EXACT. All three reference-EXACT functions stay exact.
`VPcmXfTerm.cpp` is also raw-object identical to its baseline: 19/19 allocated
bytes, 1/1 relocation, 2/2 defined-symbol records.

The progress function remains nonexact. Its blob body is 7278 bytes, so
removing invented guards **increases** its existing size gap (523 to 816).
That is not a claim to have recovered the rest of its body. Five undefined
bindings change weak to global; exported definitions are not weakened.
The obsolete soft-mode data word and `abort` reference disappear.

The session-termination mutation anchors retain all five original fault
scenarios. `python3 tools/mutate.py --suite vpcmxfterm --jobs 2` catches
**5/5**, all by differential test, zero unusable/equivalent/uncaught.

## Byte copies on modern GCC

The empty integer-register asm in `x87copy.h` was a modern-compiler workaround.
GCC 3.4 still receives the unchanged `*dst = *src` branch. Modern builds now
call ordinary `memcpy` through an unsigned temporary and use
`-fno-builtin-memcpy` in `CXXFLAGS`. The temporary preserves overlapping-copy
behavior and avoids interpreting the representation as floating point.

This is a portability flag, not a recovered period-compiler option. It
withdraws the builtin optimization that quietens signalling NaNs during a
nominal byte copy. No libc-private macros, assembly, volatile barriers, or
new floating-point tolerance is introduced.

### Crossed modern controls

GCC 14.2.0, full `SineWave.cpp`, `Queue.cpp`, and `Agc.cpp` TUs; the existing
three differential fixtures were relinked against the complete Makefile
`OBJ_REPRO` inventory with exactly those TUs replaced. Each compilation
enabled `DSPLIB_REPRODUCE_BUGS`.

The first 12-cell screen crossed retained/plain-assignment/builtin-memcpy
source with baseline, `-fsignaling-nans`, `-fno-tree-sra`, and
`-fno-tree-forwprop`. All eight unbarriered-source cells failed the SineWave
and Queue signalling-NaN checks. All four retained-source cells passed those
fixtures. Thus neither a clean detector nor an empty denominator is being
mistaken for success.

The next four-cell cross used retained versus ordinary library `memcpy`, with
and without `-fno-builtin-memcpy`. The library source passes SineWave/Queue
only with the flag; retained source passes both profiles. Agc has its existing
declared modern comparison divergence in all these passing-copy cells, with
identical complete test output. Finally, the retained two-copy unsigned-
temporary implementation was compiled/tested with and without the flag: it
passes the two fixtures only with the flag, and adds no Agc differences.

The initial attempt to link a glob of build objects failed because stale
case-variant object names were present. It is INVALID and excluded. The
valid runs use `make -s print-OBJ_REPRO` rather than filesystem discovery.

## Artifact ledger and gates

Local experiment artifacts are under `/tmp/opencode/`:

- `issue19-baseline-tc/`, `issue19-baseline-partial.o`: faithful baseline;
- `issue19-alias-build.py`, `issue19-alias-score.py`, alias build logs and JSON:
  full-TU reproduction, source/object hashes, body and binding comparisons;
- `issue19-copy/`: candidate headers, commands and complete fixture outputs;
- `issue19-*-period*.log` and `.status`: gate logs and captured exit status;
- `issue19-*-partial*.log`: partial-link census.

Baseline `make period J=4`: **375 passed, 0 failed**. The alias-only candidate
also passes **375/375**. Final combined gates are recorded below.

Baseline partial link: 273/273 TUs, DIFFERENT; 54,108/943,398 positioned
reference bytes, 905/18,317 exact relocation records, 222/2,907 exact symbol
records. Alias candidate: 54,083/943,398 bytes, 906/18,317 relocations,
222/2,907 symbols, still DIFFERENT. The positioned-byte loss is reported,
not hidden by the relocation gain. Strict object completion remains open.

## Six square roots

All six `fsqrt` asm helpers now use `__builtin_sqrt`. C++ compilation uses
`-fno-math-errno` in the modern Makefile and both period build paths. This is
independent of unsafe/finite-math optimization: the reference uses bare fsqrt
without an errno-setting fallback, even where negative values are reachable.

The first delegate's rejection of double sqrt as necessarily narrowing was
too broad. Parent review required a full-TU experiment rather than reasoning
from the prototype. Ordinary `sqrt(x)` and `__builtin_sqrt(x)`, crossed with
baseline/no-errno flags, emit the instruction, but converting a long-double
Agc expression to their double argument inserts a store/reload and fails one
of the 1,524,096 checks in the pole/reference/block/length group. It is not
retained, even though most checks pass.

The successful source uses **double expressions throughout that boundary**:
Agc's `lvl`, `t` and `1.0` constants; the equalizer's mean-square quotient and
`rms`; double arguments/results in the six helpers. The period compiler keeps
these double expressions at x87 excess precision without the narrowing
conversion. In particular, the complete `V90Equalizer.cpp` object is
byte-identical to baseline despite replacing its asm. Agc's crossed retained
versus double-expression controls pass its complete fixture under both base
and no-errno profiles; no-errno is retained to eliminate the extra fallback.
Artifacts: `issue19-agc-double.py`, `issue19-agc-double-results.log` and
`issue19-agc-double/`; the delegate's earlier narrowed controls and snapshots
are under `issue19-math-artifacts/`.

The full faithful object build is **273/273 TUs**. Compared with the initial
baseline, 265 objects are raw-identical and eight change, including the alias
TU. All 103 shared function definitions in those eight were inventoried;
there are no added/removed function definitions or changed exported bindings.
Full instruction/operand diffs are retained in `issue19-fulltu-audit.json` and
`issue19-body-diffs.log` and were reviewed, including nonexact bodies:

- Agc retains reciprocal/multiply/sqrt/smoothing arithmetic; x87 stack
  scheduling changes and its body grows 8 bytes.
- `Std<float>` loses its redundant variance recomputation and sqrtf errno
  fallback, becoming reference-EXACT (86 to 29 bytes).
- `V90Resampler::getTimingHistoryStd` loses its sqrtf fallback (102 to 50
  bytes) but remains nonexact. Its other reported UNRESOLVED body has identical
  normalized instructions.
- The constellation and phase-4 diagnostics reschedule control-word saves and
  stack operations around the same roots and integer conversions.
- TRN2 changes frame/temporary/register allocation and control-word-save
  placement, including non-sqrt regions; its body grows 16 bytes. No new call
  or data target is introduced. V92Mapper adds a stack exchange (2 bytes).

Tree-wide `byteident.py` compares **1852 symbols**: grade-0 EXACT rises from
813 to **814**, with the single gain `_Z3StdIfET_PS0_j` and **no losses**.
The four UNRESOLVED results are still unresolved, not promoted to exactness.
This is not a claim of complete body recovery for the other changed functions.

The V92Mapper mutation anchor was updated without changing its fault (omit
the root). Serial `--suite v92mapper --jobs 1` catches **12/12**, all by test.
An earlier two-shard copy failed and is explicitly not a result. The changed
source suites contain 543 literal mutation anchors, with no newly broken
anchors after updating the two affected mutation files.

## Final retained-change gates

| Command | Result |
|---|---|
| `make period J=3` | **375 passed, 0 failed** |
| `make phase J=3` | **PASS**, including its separate period 375/375; modern tier accepts only the existing declared divergences |
| `python3 tools/mutate.py --suite vpcmxfterm --jobs 2` | **5/5 caught** |
| `python3 tools/mutate.py --suite v92mapper --jobs 1` | **12/12 caught** |
| `make partial-compare J=2` | 273/273 TUs; **DIFFERENT** |
| `TC_OUT=build/tc_repro python3 tools/toolchain/byteident.py --list-exact` | **814/1852 EXACT**, no lost exact symbols |
| `python3 tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o build/partial/dsplibs.o --require-exact` | **FAIL / DIFFERENT**, as before |

The phase closing denominators are **49,022/51,388 source lines**, 233 files,
1425 debug sites and 35 anchored deviation sites. Its separate translated-
symbol census read 307 cached objects for 273 source files; the regenerated
`docs/coverage.md` also included unrelated voice/helper changes and is not
retained as evidence of an issue-19 coverage change.

Final strict partial-link census: **53,957/943,398 positioned reference
bytes**, 905/18,317 exact relocations, 223/2,907 exact symbol records;
67/92 shared section records exact. Thus the function-exact gain does not
hide the positioned-byte decrease. Strict final-object completion stays open.

Logs and captured exit statuses use the `issue19-sqrt-period`,
`issue19-final-phase`, `issue19-final-byteident-complete` prefixes. The final
partial JSON is `issue19-final-partial.json`. Build caches subsequently became
unavailable during additional profile investigation; this ledger records the
completed runs, not a claim that their executables remain in `build/`.

## Remaining arithmetic and stopping point

**Issue #19 is not complete.** Thirteen arithmetic asm statements remain:
six sine/cosine sites in `DspMath.cpp` and `SineWave.cpp`, and seven logarithm
sites in `psd.cpp`, `V90ConstellationDesigner.cpp`, `V90TRN2Designer.cpp`,
`V90Equalizer.cpp`, `V90Phase4Demodulator.cpp`, `V90Demodulator.cpp` and
`VpcmFloModem.cpp`. No symbol aliases, square-root asm, or empty copy barrier
remain in reconstruction code.

Host-GCC arithmetic scratch models are invalid reconstruction evidence.
Libc-private `__FAST_MATH__`/`__LIBC_INTERNAL_MATH_INLINES` toggles are
mechanism controls, not retained application source. `sqrtl` also remains a
call under no-errno on this GCC; that negative result did not justify ruling
out ordinary double sqrt.

The final period model crosses ordinary double log/sin/cos builtins with
baseline, no-errno, unsafe-math, and unsafe-math plus no-errno: the first two
profiles call all three; unsafe-math emits fsin/fcos but still calls log10.
The recovered glibc header exposes the logarithm instruction expansion under
its fast-math macro. This is evidence for a source/profile inquiry, not license
to invent private macro toggles.

Before contemplating that broad flag, retained full DspMath/Psd TUs were
crossed with `-ffast-math`; both fixtures pass both profiles (4/4). A broader
mechanism control then compiled all **84 C++ TUs** with that flag and the
mandatory bug-reproduction define, retaining the other production objects
and baseline-compiled test apparatus. It **fails `t_v90equ`**: 72/86,026
set-beta checks fail, and conversion checks fail too. Therefore the local
positive controls do not justify globally enabling fast math.

That broad control is **incomplete**, not a whole-suite score. Its first run
exhausted tmpfs while linking `t_v90leaves`; after releasing reproducible
test executables and resuming, the main build inputs became unavailable at
`t_v90modchain`. The observed equalizer failure is retained; no 375-test
denominator is claimed for this diagnostic. Artifacts/scripts are the
`issue19-fast-control*`, `issue19-transcendental-model*`, and
`issue19-fast-profile*` families. The retained source does not enable fast math.

Next discriminating work belongs on the issue: cross ordinary transcendental
calls with an independently justified math-header/compiler profile, and trace
the fast-profile equalizer set-beta/conversion failures against the reference
before considering profile adoption. Do not repeat the rejected long-double
sqrt or private-header substitutions, or accept a change just because its
tested inputs miss an added narrowing/errno path.
