# Maintainer MG-03: typed supervisor recovery

At baseline `ac7f28e6`, issue #260 has already modelled the V.34 receiver
and its timing/equalizer subobjects. The guide's references to `pad_000`,
`pad_2a8` and a live `hs_put` are stale. The remaining `hs_get` has exactly
three callers, all in `datapumpv34`, reading root `+0xaa98` before storing
the receiver's `baud_copy`.

## Initial byte-preserving change

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

## Issue #286: receiver ownership and counter control flow

The follow-up removes all ten `DP_RX_*` field-offset macros and the
`dp_rxget`, `dp_rxput` and `dp_run` helpers. `datapumpv34` holds typed
receiver and transmit-queue pointers and accesses the already recovered
members directly. Each counter update now has an explicit conditional
increment or zero store. No field, type, compiler option or tolerance changes.

This formulation follows independently audited original operands and branches,
rather than the size of cell B:

| Original evidence | Preserved source contract |
|---|---|
| `lea +0x264` and `lea +0x221c` | Receiver and transmit-queue owners |
| `0x71a31`: `66 3d 2f 75`, then signed `jg` | Signed **16-bit** block-cap comparison |
| Error comparisons at receiver `+0x252`, `+0x254`, `+0x256` | Cached signed-short error and signed-short thresholds |
| Increment/word-store and separate zero-store arms | Consecutive counts wrap to 16 bits or clear |
| Unsigned elapsed-time tests | Delayed counters remain untouched until their gates open |
| Four fall-through supervisor blocks | Four independent `if`s, with reads retained after init/indication calls |
| Sign-extended counters and 32-bit rate products | Down/up thresholds do not narrow to short |

**Correction to F8044/F8045:** a zero-extending load does not establish an
unsigned comparison. F8044 incorrectly described the block-cap comparison as
32-bit and inferred a forced unsigned field; F8045 repeated that conclusion.
The instruction's `0x66` prefix and signed branch establish the opposite.
Keep the declared signed `short`. The new negative-block-count fixture is
explicitly a synthetic component fidelity probe, not evidence that this state
is reachable through a public modem call. The existing chunked V.34 connecting
fixture supplies separate lifecycle coverage.

The unchanged-TU control reproduces the baseline ELF byte for byte. The
retained explicit-branch formulation emits 1035 bytes for `datapumpv34`,
against 988 before and 1028 in the original. All 53 other emitted function
bodies and canonical relocations remain unchanged; no function is added or
removed. The function remains non-exact: register allocation, cached-error
extension and scheduling still differ. This is supported structural recovery,
not a claim to have recovered the exact original source spelling or profile.
The initial helper-pointer cell B remains a withheld historical experiment.

The whole-TU audit also checks all 85 defined symbol records: names, bindings,
visibility and sizes are unchanged except for the supervisor's size; later
text symbols move by 48 bytes. All non-text data are unchanged apart from
27 relocated `.rodata` jump-table words, each increased by 48 to follow its
text target. There are still 2357 TU relocations and 300 source objects;
299 complete ELF source objects are unchanged.

The 300-input partial link grows by 48 text bytes, with section order,
NOBITS sizes, symbol/relocation counts and bindings preserved. Against the
original, its positioned-byte census changes from 69,422 to 69,291 of
943,398 bytes, and exact relocation records from 1032 to 1031 of 18,317.
These are positional consequences, not an improvement claim: downstream
text addresses, relocations and jump-table addends move. The replacement
object remains different from the original before and after this change.


## Toolchain and reproduction

The initial cells and the issue #286 control/candidate use the recovered
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
Issue #286 records its unchanged control, typed candidate, actual command,
and full-TU/partial-link reports under `build/issue286/`. The unchanged
control reproduces the saved object exactly. An initial invalid candidate
compile accidentally included edits outside the target function; it failed
and was corrected before any results were interpreted. The C++ and
DCR-specific flags are unmodified; the experiment does not compile them.

For the retained tree, rebuild with `make tc-repro`, compare the complete
object against the saved baseline, and run `make phase`. The artifacts are
diagnostics, not replacements for the deciding period gate.

## Validation

At `ff5399cc`, the focused modern `t_v34datapump` passed 17,949 checks. Three probes
cover negative acquisition count, cached negative error and signed 16-bit
run-counter wrap; each is labelled synthetic and has a reference/reference
control and an absolute oracle. Existing lifecycle coverage remains separate.

At `ff5399cc`, the complete owning run reported 80 mutations: 77 caught,
one uncaught, two equivalent, zero unusable and zero miscounted. That
historical run caught both new signedness controls but retained the
receiver-before-modulator gap. The coverage follow-up below closes that gap
without changing the mutation or production source.

### Loop-order coverage

The old F10247 read/write-set audit missed the indirect path
`receiver -> decoderv34 -> demapFrame`. At a frame boundary, `demapFrame`
sets the receive shell's `latched` member (root `+0xe4c`, original store at
`0x596e3`). A fresh transmit vector reads that latch at `0x5a910`, then can
advance `train_symcount` at `0x5a937`. Thus reversing the loops can make
transmit processing consume a latch established in the same block.
Echo cancellation is not the shared dependency at this boundary.

The new `loop_order_cases` fixture in `t_v34datapump.c` initialises both
shells with `preinitdigital` and `initV34`, drives 166 or 167 real decoder
calls, then eight real mapper calls. It does not plant a latch or training
count. One pending receive symbol then crosses the warm-up/frame boundary
in the 167-call case; the neighbouring 166-call case remains unlatched.
The correct ordering leaves `train_symcount` at zero in both cases. The
reversed ordering advances it to one only in the crossing case.

Both cases compare complete object/arena state before and after the step,
with reconstruction/blob and blob/blob controls and absolute latch,
subframe and training-counter oracles. The final receiver flags,
acquisition count and queued samples are explicitly synthetic. The history
establishes the decoder component boundary; the combined fixture does not
establish public-call reachability. The separately negotiated `t_vpcmrun`
lifecycle coverage retains its own scope.

Both the period and focused modern binaries pass **18,749 checks** (800
added). Applying the existing
loop-swap mutation produces **seven failures in 18,750 checks**; the extra
check comes from the mismatch reporting path. The single-mutation run is a
positive control, not a complete-suite result.

The complete fresh run reports **80 mutations: 78 caught by tests, zero
uncaught, zero unusable, two equivalent and zero miscounted**. The two
equivalent cases retain their existing explanations: retrain reporting before
initialisation, and mode 3 versus mode 2 in the shared initialisation body.
Every previously caught verdict is preserved. `tools/mutsnap.py --update`
records the measured verdicts and current dependency key for `v34datapump`;
other suites retain their historical records. The production source and all
80 mutation descriptors are unchanged by this coverage follow-up.

Static validation checks 10,040 anchors in 285 suites with zero detached
anchors. The byte-identity census remains 1086/1852 exact, with neither
lost nor gained exact members against the saved baseline; the instruction
ratchet also passes. Strict MkDocs builds successfully. `make phase J=12` passes all 388
period differential binaries with zero failures, plus the structural and
provenance checks. The datapump binary runs the new probes within that
unchanged binary denominator. No full portability run is claimed.
