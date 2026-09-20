# P4D period fixture validity and bounded silence lifecycle

## Disposition and identities

Reconstruction first: original source, binary and functional fidelity under
Gentoo GCC 3.4.2-r2. This audit does not authorize a modern tolerance or
exemption. No reconstruction source is changed.

The read-only audit examined `51584000ca3cd2c8c6a2d2d4bd4216a1870c15c0`.
The fixture was developed on `investigate/p4d-period-fixture-validity`,
based on merged master `6d4dd254266e6c98d54def129ea07352644ea489` (PR181
containment). Reference object SHA-256:

```
1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379
```

`test/unit/t_v90p4dperiod.cpp` is the bounded primary-period measurement test.
`t_v90p4dnan` remains an exploratory synthetic fidelity probe. Its period
agreement is useful, but neither negative energy nor its count/period pair is
evidence of a normal measurement history.

**PR184 #183 continuation:** the short-CP section advances the V.92 boundary to
sample-driven, CRC-validated short CP/CPnot followed by Ed. The earlier
three-flag fixture, counts and review below are retained as historical evidence.
The mapping/calibration and evaluator outcome remain explicit component inputs;
neither version establishes a public-modem connection or observes raw dB.
The independent-review follow-up matrix at the end of this document further
shows that **either short message alone suffices** at this boundary. The earlier
2/2-message requirement describes that fixture's chosen stimulus, not a protocol
necessity. Earlier executable addresses/hashes remain historical build identities.

## Original-object evidence

Independent review reproduced the existing period binary's 347,300 checks and
the eight subprocess control cases. It approved the bounded component claim,
not universal reachability or a raw runtime NaN witness. Allocation contents
are checked at measurement endpoints; this does not prove allocation lifetime
identity or absence of transient writes between observations.

The review also found that Python `assert` made the observer runner unsound
under `python -O`. Acceptance now uses explicit `require` checks. Both ordinary
and optimized Python pass all eight cases. A negative control under optimized
Python substituted exit 0 and empty output for the unknown-probe response;
the runner rejected it with `RuntimeError` instead of claiming success.

All addresses below are `.text` offsets in the reference object, read with
`tools/dis.py`. No decompiler output supplies this evidence.

The decision symbols are
`_ZN20V90Phase4Demodulator14getV90DecisionEs` (0x25ea0, 0xc17 bytes) and
`_ZN20V90Phase4Demodulator14getV92DecisionEs` (0x26ac0, 0xcb4 bytes).
`Es` is the short sample argument. The energy fields are binary32 at +0x3508
(before) and +0x350c (after); state/count/progress are +0x20/+0x24/+0x28,
and the keep-rate flag is +0x3510.

| Action | V.90 instructions | V.92 instructions | Meaning |
|---|---|---|---|
| Ed entry initializes before | 26a55–26a6c: count=0, register=0, `mov` to +3508, state=0xa | 276fd–2770e: count=0, register=0, `mov` to +3508 | Initialization is an entry action, not a constructor default |
| Silence -> before measurement | 262a7–262e3: unsigned wait comparison, divisibility by six, state=0xb/count=0 | 26f53–26f8f | Before energy is not cleared again here |
| Before accumulation | 261d7–261ef: `imul`, `fildl`, `fadds +3508`; `fstps +3508` at 26210 | 26ef9–26f11; store 26f32 | Signed short squared into signed 32-bit integer, then added |
| Before normalization | 26987–26996: zero high word, `fildll`, divide, `fstps +3508` | 275d1–275e0 | Divisor is unsigned count |
| Wait -> after measurement | 26273–26286: state=0xd/count=0/after=0 | 26ecb–26ede | Real after-energy initialization |
| After accumulation | 2617d–2619b: `imul`, `fildl`, `fadds +350c`; store 261bc | 26e29–26e47; store 26e68 | Same square update |
| After normalization | 267ed–26804: unsigned count conversion/divide/store | 27437–2744e | Same normalization |
| Before diagnostic | 269ac load, 269bb `fsqrt`, 26a15 field compare | 275f6, 27605, 2765f | Read only |
| After diagnostic | 2681a load, 26829 `fsqrt`, 26889 field compare | 27464, 27473, 274d3 | Read only |
| Ratio/log | 268aa–268c4: load 1, divide by after, multiply by before, `fldlg2`, `fyl2x`, multiply by 10 | 274f4–2750e | No zero or nonfinite guard |
| Decision | 26956–2697c: load dB, load parameter +404, `fcompp`, `fnstsw`, `sahf`, `setb` | 275a0–275c6 | An unordered comparison can also produce flag 1 |

The census inspected **17 P4D member symbols**, finding **28 explicit energy
displacement instructions**, all in the two decision functions. A separate
`.text` byte-pattern census for the two displacements found the same 28
candidates, accounted for by the disassembly. This is not an alias analysis
or proof excluding arbitrary memory corruption.

### Construction, reset, callers and configuration

* P4D C1 at 0x25a20 and C2 at 0x25930 construct the embedded modulator and
  detectors and install dependencies. Neither directly initializes the energies.
* `_ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj`, 0x277c0:
  27801 stores its supplied state, count is cleared, and 27947–2796d optionally
  pumps zero samples through the selected decision function. It does not
  initialize either energy. Calling reset with a measurement state is therefore
  not a substitute for the real entry path.
* `V90Demodulator::exitPhase3` at 1bd78–1bda2 calls that reset with state=0
  and zero pumped samples, not a measurement state.
* `_ZN20V90Phase4Demodulator13resetBeforRRNEv`, 0x25d30, sets +38/+3c and
  clears +44/+48/+30; no energy or measurement-state store. Production
  `V90Demodulator::progress` calls it at 1cbdb, then copies the evaluator's
  silence request to P4D +40 at 1cc01.
* V.90's Ed path checks evaluator +90 and P4D +3c/+38 at 2673a–26756 before
  entering silence. V.92 checks +30/+3c/+44/+48 at 272c4–272dd. After clearing
  before energy, V.92 selects silence if +4c=0, otherwise WaitForRt, at
  2775a–2776c.
* `getDecision` sign-extends its short at 27787. The three calls from
  `V90Equalizer::process` are 39b9f, 39d24 and 39fe4. Upstream floats, including
  nonfinite ones, cannot be passed directly as floating sample values here.
* Default parameter stores at 2a229–2a296 set +3f8=2500 (SCR length),
  +3fc=132 (wait), +400=240 (measurement), +404=2.0f (threshold).
  `evaluateConnection` writes finite threshold constants at 3ead8/3eecc:
  approximately 0.65f, 1.8f and 2.0f.
* `loadParams` passes the timing/threshold fields to parser functions, but this
  blob's `Vparser_read_int` at b0990 and `Vparser_read_float` at b09a0 are
  `xor eax,eax; ret` stubs. Arbitrary configuration-file values are not thereby
  demonstrated public inputs.
* The measurement's demapper call only advances its separate +18 frame cursor:
  `incrementRBSFramePosition`, 30b50–30b70, computes unsigned modulo six.

## Invariant and exceptional values: scope matters

After the real energy initialization transitions, with fixed default timing,
correctly separated objects and ordinary x87 execution, each sample square is
in [0, 2^30]. Signed 32-bit multiplication cannot overflow for a signed short.
The 240-sample windows are far below binary32 overflow; their positive unsigned
divisors cannot create negative energies. Thus the measured energies are finite
and nonnegative on this path. Construction alone, arbitrary reset arguments,
stale/uninitialized storage, alias corruption and caller-written energies are
outside that invariant.

Unsigned timing comparisons mean a negative configured integer is interpreted
as a large threshold, not a negative divisor. Huge thresholds can prevent a
transition. Counter wrap, changing a parameter during a window, nonstandard
floating environments and all arbitrary configurations have not been validated
by this fixture. No global impossibility claim follows from the direct-store
census.

| Completed before/after energies | Arithmetic inference with masked exceptions |
|---|---|
| positive/positive | finite dB for these bounded windows |
| zero/positive | ratio zero, log gives negative infinity |
| positive/zero | reciprocal and dB give positive infinity |
| zero/zero | `(1/0)*0` produces NaN before `fyl2x` |

**No live raw dB/NaN witness is claimed.** The fixture observes raw energy words,
the actual flag and diagnostics. Diagnostics format the floating quantities
through integers and are not lossless floating witnesses. Flag 1 is also
produced by finite values above the threshold. The original audit could not run
gdb because it is absent; no instructions or source were added to force an x87
spill or expose the intermediate.

## Historical PR182 input boundary and coverage

Both sides independently construct parameters (with separate host parameter
blocks, min/max rate 28000/56000), evaluator, ADI, CP, MP, descrambler, demapper
and P4D. P4D reset starts at WaitForRi with zero pumped samples. Whole-object
P4D storage is initially poisoned, so an omitted energy initialization cannot
be hidden by zero-filled storage. That storage poison is not a valid energy
input and is never treated as one.

The fixture supplies a component environment:

* separate mapping blocks with 24 bits/frame, eight descending levels per
  phase, spectral rate/id zero;
* calibrated ADI levels 300, 280, ..., 160 for codes 0..7, alternate-RBS flags
  zero and PCM type 0; no claim that modem training produced this calibration;
* evaluator `silenceRrnRequest=1`, an assumed evaluator outcome;
* real `resetBeforRRN`, demapper/descrambler and message-decoder reset methods,
  with group size 24; real `enterWaitForEd` (V.90) or `enterWaitForCP` (V.92),
  explicitly bypassing earlier Ri/TRN negotiation.

**V.90: component-method reachability.** Twelve samples of -300 pass through
real `hardDecision`, demapper `process`, descrambler and MP `bitsToInfo`, giving
Ed and the real silence entry. No message return is stubbed and no decoder
counter is planted.

**V.92: CP-boundary-assisted transition coverage.** In addition, each cycle
assumes P4D +30=1, +44=1, +4c=0: preceding CP/CPnot accepted, handshake permits
silence, no peer WaitForRt bypass. These three flags are explicitly planted;
the preceding CP exchange is **not** exercised. The following twelve -300
samples still pass through the real CP decoder and Ed transition. This is
narrower evidence than the V.90 path, not public modem end-to-end reachability.

Neither path assigns an energy, count, or measurement state. After each actual
Ed entry, both sides run the default sequence:

```
132 silence + 240 before + 1782 wait + 240 after = 2394 calls
1782 = ceil((2500 - 3*240)/6)*6
```

Six cycles reuse the same constructed pair without re-running P4D reset:
`2/1`, `0/0`, `0/1`, `1/0`, `1/1`, `2/1`, where each value is the constant
short sample in the respective measurement window. The first finite cycle
ensures later initialization is tested over nonzero old energies. The zero/zero
case and repeated final finite case check reinitialization, not just first-use
zeroing. There are five distinct inputs, six cycles per mode, twelve total
cycles and 28,728 measurement calls per side.

### Comparison surface

* Every measurement call checks both return values against each other and the
  input; state/count/progress/cursor/energies/flag against independent expected
  steps. All samples and expected energy arithmetic here are small exact
  integers; there is no float tolerance API.
* Full P4D and demapper bytes, including pointers and guards, are checked
  against each side's own preimage with only the independently predicted step
  updates applied. This avoids masking any pointer-shaped bytes. Both energy
  words also compare directly between sides, including zero signs.
* Parameters, host parameters, mapping blocks, evaluator, ADI, CP, MP,
  descrambler and **all 36 live allocations across the pair** are frozen after
  Ed and checked unchanged after measurement, using requested allocation sizes.
  This is a measurement immutability check, not a claim that constructor/Ed
  heap graphs were fully compared between sides. The constructor and reset
  suites retain that separate responsibility.
* Guards are checked against their original canaries. Allocation retention,
  final frees and bad-free counts are checked. P4D dependency identities are
  checked independently after reset.
* Diagnostics use exact captured bytes, callback counts and completeness via
  `transcript_exact`. Exactly four measurement calls per cycle must emit
  diagnostics; silent calls are also compared. Ed transcripts must be nonempty.
* Opt-in observer probes corrupt return, energy, state, guard, peer or transcript
  on the same zero/zero final call. They are apparatus controls on a green
  period baseline, not a mutation sweep or an exemption protocol.

## Correction to the historical synthetic sentinel

`t_v90p4dnan` plants before=-4, after=1, sample=300, threshold=-80, state=0xd
and count=41 with period=36. The normal initialized arithmetic does not produce
that negative before energy. With a fixed period of 36, the uninterrupted
measurement would already exit at count 36; the count problem applies to its
finite controls too. No evidenced mid-window configuration update supplies the
missing history. Keep these inputs as labelled exploratory fidelity probes.
The historical comparison/order findings retain their synthetic-input evidence;
they do not acquire a modem reachability claim from the new fixture.

## Reproduction and verification record

Working directory: `/home/philpem/slmodem/claude_re_worktrees/mutate-workdir`.
One job at a time, `J=1 -j1`; no modern campaign, tolerance migration or
reconstruction source change.

```sh
PERIOD_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  make -j1 J=1 period T=t_v90p4dperiod
build/period/t_v90p4dperiod
DSPLIB_P4D_PERIOD_FAULT=energy build/period/t_v90p4dperiod
python3 test/safety/test_v90p4dperiod_fixture.py build/period/t_v90p4dperiod \
  --artifacts /tmp/opencode/p4d-period-fixture-observers
make -j1 J=1 phase
python3 tools/refcheck.py
git diff --check
```

Disassembly reproduction:

```sh
python3 tools/dis.py ref/slmodemd/dsplibs.o _ZN20V90Phase4Demodulator14getV90DecisionEs
python3 tools/dis.py ref/slmodemd/dsplibs.o _ZN20V90Phase4Demodulator14getV92DecisionEs
python3 tools/dis.py ref/slmodemd/dsplibs.o _ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj
python3 tools/dis.py ref/slmodemd/dsplibs.o _ZN20V90Phase4Demodulator13resetBeforRRNEv
python3 tools/dis.py ref/slmodemd/dsplibs.o _ZN13V90Parameters12setToDefaultEv
python3 tools/dis.py ref/slmodemd/dsplibs.o 0xb0990 0xb09a3
```

Initial development control: a call to `setToDefault` on a parameter slot with
no host-parameter pointer failed (0 passed, 1 failed, exit 139). It was a fixture
construction error: that method reads host min/max rates. The fixture now uses
the actual parameter constructor with separate host blocks. The failed run is
retained at `/tmp/opencode/p4d-period-fixture-targeted.log`, not counted as a
gate pass. The first complete lifecycle run (`targeted-2.log`) passed 347272
checks before dependency/ownership assertions and observer probes were added.

Final measurements:

| Measurement | Actual result |
|---|---|
| Focused Gentoo period gate | 1 passed, 0 failed |
| New fixture, raw period execution | **347300/347300 checks**, exit 0 |
| Input denominator | 5 distinct sample pairs, 6 cycles per mode, 12 total cycles; every Ed entry in 12/192 bounded calls |
| Measurement denominator | 2394/2394 calls and 4/4 diagnostic transitions per cycle, 28,728 calls per side |
| Observer controls | **8/8 passed**: baseline, 6 named faults, unknown-probe rejection |
| Full `make -j1 J=1 phase` | **385 passed, 0 failed**, exit 0; previous 384 + this one unit binary |
| Structural phase checks | 2202 offset annotations; 2679 live period assertions; 336 types/171 files/1 known duplicate; 600/600 banners; partialcmp 8/8; TU attribution/order 11/11 |
| Phase reference check | 14050 references, 0 unresolved/pending/stale |
| Standalone `tools/refcheck.py` | 14050 references, 2571 headings, 0 unresolved/pending/stale; exit 0 |
| `git diff --check` | exit 0 |

Observer failures, all with ordinary exit 1 and the same 347300-assertion
inventory: return 2, energy 2155, state 1, guard 11, peer 9, transcript 1.
These are failure counts, not independent defects: persistent corruption can
also be detected on subsequent checks/cycles. Unknown probes fail separately.
The unmodified period baseline is green; no red-baseline mutation claim is made.

Toolchain executed for identity: GCC and G++
`3.4.2 (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)`;
the compiler-selected assembler and ld both report `2.15.92.0.2 20040927`.
Image ID and published digest:

```
sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16
ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker@sha256:14efe17550e390798c81d78a612b538af6c8bd70b8b6fc75ee19115147ef5d96
```

The gate logs print the complete base, source-C++ and fixture-C++ flags.
All include `DSPLIB_REPRODUCE_BUGS`; source C++ retains the existing
`-ffast-math`, while fixture C++ does not receive it. No profile change was
made. Audit SHA-256 identifiers (not a trusted build authorization protocol):

```
f66be41c01679c790cfb8beef43f477c1c32821ea76c08e611f15d8c185038d0  build/period/t_v90p4dperiod
810e2074eae7f6b8039d4710d76a79b19089e669cbf9bf45dd1b5b3e52d1b295  test/unit/t_v90p4dperiod.cpp
c001a5d3669aae3af97f61b4a6c582379a9f2f36254d07902c6c4e5a7f0b2b73  test/safety/test_v90p4dperiod_fixture.py
```

Artifacts under `/tmp/opencode/`:

* `p4d-period-fixture-targeted-final.log`: focused compile/link/run gate and flags.
* `p4d-period-fixture-phase-final.log` and `p4d-period-fixture-phase-status.json`:
  completed full gate, with the direct subprocess return code 0.
* `p4d-period-fixture-controls-final.log`: all eight observer-control verdicts.
* `p4d-period-fixture-observers/`: complete baseline and six probe logs, unknown
  probe log, and `status.json` with raw subprocess exits and executable identity.
* `p4d-period-fixture-toolchain.log`: executed compiler, selected assembler and
  linker versions.
* `p4d-period-fixture-refcheck.log`: standalone reference-check result.
* `p4d_period_phase_driver.py`: detached serial gate driver preserving the
  actual exit status.

The earlier `p4d-period-fixture-phase.log` is **not** a completed gate: the
foreground tool timed out and make reported `No child processes`. Its orphaned
period container was allowed to finish (exit 0) before a new gate began; that
container result is not substituted for a phase verdict. The detached driver
then ran the full gate to the positive final boundary. Development core dumps
were removed; the failed-run logs remain. The vendored-header gate checked
7/7 manifest entries but reported the external upstream checkout absent; no
upstream-drift check is claimed.

## PR182 remaining limits and next discriminating work

The audit remains open for preceding V.92 CP negotiation, evaluator production
of the silence request, ADI calibration generation, and public modem input
reachability. The next entry test should generate the preceding CP exchange
through real methods and remove the three explicitly assumed flags, while
retaining the same Ed/measurement assertions. Do not silently relabel the
current assisted path as an end-to-end connection.

For a future noninvasive raw witness, observe the original's binary32 dB word
at stack +0x10 immediately before the final comparison: V.90 symbol+0xab6,
V.92 symbol+0xae0. Locate the reconstruction's corresponding operand in the
actual period executable independently. Record both energies and classify the
consumed operand by integer exponent/fraction bits. A recomputed logarithm,
flag value or integer diagnostic is insufficient. Until then, NaN is an
instruction-level inference backed by zero/zero energy observations.

## Issue #183: sample-driven short CP entry (2026-09-20)

Investigation base `edce26e3`, branch `investigate/issue183-p4d-entry`, in the
same worktree recorded above. Source/compiler profile and the synthetic probe
are unchanged. The bounded discriminator was: if CP validation supplies the
three assumed results, complete CRC-bearing messages must set them through
`getDecision`; corrupt or absent messages must not enable Ed. The input domain
is one short-form pair (acknowledgement 0 then 1, peer SUV bit 0), plus two
negative streams (both CRCs corrupted; both messages absent), with 192 sample
calls available per stream. This is a component message boundary, not a
standards oracle or a complete exchange between two negotiating modems.

### Blob trace and boundary actually advanced

All instruction evidence here is from `tools/dis.py` on the reference object:

* `V90CP::infoToBits`, 52240–522bd: 17 one bits, zero at 17, short-form
  type at 18, reserved zeros at 19..31, peer bit at 32, acknowledgement at
  33, CRC framing zero at 34. Sixteen CRC bits follow, then zero padding.
  The fixture implements the CCITT recurrence as an integer polynomial
  (`0x1021`, initial `0xffff`), rather than copying receiver state or its
  register-array implementation. Each message occupies 72 bits at group size 24.
* `bitsToInfo`, 52d20–53677: real run detection and state machine; 52f03–52f32
  collects the CRC frame, 5335d onwards evaluates it. The accepted tail at
  52e97–52efe reports the short forms as 3/4; the bad-CRC branch resets the
  detector and emits its existing diagnostic. The independent zero-run test
  at 52dd3–52dff reports Ed (5) only with cursor 18 and `2*groupSize` zeros.
* `getV92Decision`, 2723d–272b3: **every** supplied sample goes through real
  `hardDecision`, `process`, descrambler and `bitsToInfo`. There is no prefeed
  into a decoder's middle, stubbed return, counter write or one-bit shortcut.
* Result 3: 2735e writes P4D +30; 27373–2737b copies decoded CP +ca0 to +4c;
  27393 writes +44 only after the +40/+4c and +48 tests. Result 4 has the
  corresponding stores at 273cd, 273df and 273f7. The fixture observes the
  ordered progress results 0x31 then 0x33 and the three fields, rather than
  assigning them. The Ed branch at 272c4 and 276e3–2776c remains unchanged.
* **Newly explicit remaining prerequisite:** +40 is the caller's local silence
  request, not a CP-decoder result. `V90Demodulator::progress`, 1cbc0–1cc01,
  tests a nonzero session and request code 0x22/0x23, calls `resetBeforRRN`,
  and copies evaluator +90 to P4D +40. The fixture performs that copy at this
  component boundary. It does not execute `progress` or prove that its enclosing
  data-phase/request conditions arose. This replaces neither the evaluator
  history nor the larger caller. With peer bit zero, +40 must be nonzero for
  these CP replies to set +44; the old assisted fixture bypassed that condition.

The stimulus generator reads no receiver state. It scrambles the message bits
with zero history and taps 18/23, serially differential-encodes the six sign
bits of each frame, and maps the remaining 18 bits to six radix-eight indices.
`ModulusDecoder::progress` at 320f0–32305 reconstructs those digits and emits
LSB-first bits; `V90Demapper::process` at 31720–318b8 places six decoded signs
before them. The resulting sample magnitudes are the supplied eight exact
levels. Every completed frame on **both sides** must reproduce all 24 scrambled
wire bits independently, before CP acceptance is credited.

Measured positive boundary: six V.92 cycles, each **2/2 decoded short messages**,
Ed in **48/192 sample calls**, followed by all **2394/2394** measurement calls
and **4/4** diagnostic transitions. V.90 still enters in **12/192** calls.
The six energy pairs, raw energy comparisons, complete per-step P4D/demapper
checks, all 36 measurement heap snapshots and ownership checks remain active.

Two rejection streams run before the V.92 cycles on the same constructed pair:

* Flip the first CRC bit of **each** message, before scrambling. Both sides
  emit the bad-CRC diagnostic and accept **0/2** messages in **192/192** calls.
* Replace the complete message stream with descrambled zeros. Both sides accept
  **0/2** messages in **192/192** calls and emit no bad-CRC diagnostic.

Both controls require WaitForCP, acceptance/handshake still zero, and both
energy words unchanged from their own preimages. An additional opt-in `cp-crc`
observer fault applies the corrupt stream to the **positive** case: the
two-message and Ed-entry requirements must fail even though both implementations
agree. Thus agreement on rejection cannot masquerade as successful entry.

### Assumption ledger and interface domains

"Component-admissible" below means the inspected accesses/arithmetic support
these particular inputs. It does **not** mean every API-callable value, or the
joint supplied environment, is reachable from modem startup.

| Assumption | Disposition and exact boundary | Missing preceding operation |
|---|---|---|
| P4D +30=1, +44=1, +4c=0 | **Removed as assignments.** Produced by CRC-validated short messages through the full sample decoder path; corrupt/missing controls reject. | Remote state machine and timing of its acknowledgement remain outside the message boundary. |
| PCM law and no alternate RBS | **Derived at reset boundary.** Paired real ADI `reset(0, PCM_TYPE_MU_LAW, 0)` and `resetLinearMapping()` replace reliance on zero storage for these initial values. | Law selection from preceding negotiation and absence of alternate RBS on an actual channel are not demonstrated. |
| ADI levels 300,280,...,160 at codes 0..7 in all six phases | **Open, supplied calibration.** Positive signed-short magnitudes, strictly descending in demapper order, distinct by 20, all indices inside 0..127. Alternate table supplied identically but not selected. | DIL code/sample association and measured calibration; these are not claimed physical mu-law levels or training output. |
| Mapping: 24 bits/frame, six eight-entry constellations, codes 0..7, shaper rate/id 0 | **Bounded component input.** Six sign bits + 18 modulus bits = 24; `8^6=2^18` makes every generated frame representable. No spectral division is needed at rate 0. `codecConstellation` is supplied identically. | `exitDIL`, `V90TRN2Design`, earlier Jd/power/codec negotiation and later constellation design; no synthesized short CP carries this mapping. |
| Evaluator `silenceRrnRequest=1` | **Open, assumed outcome**, retained visibly. 1 is an actual possible store value of the original evaluator; it is not constructor output. No arbitrary threshold, error average or evaluator count is planted to manufacture it. | Equalizer error history, designer-provided thresholds/distance, duration/override conditions and an actual `evaluateConnection` verdict. |
| P4D +40 receives that request | **Bounded caller-interface copy**, now explicit. Original load/store and guards traced at 1cbc0–1cc01. | Actual `V90Demodulator::progress` invocation from a qualifying data/request state. |
| Repeated RRN entry | Real reset/entry methods called between cycles, without P4D re-construction or full reset. | Rt/RtNot, data recovery, Ri/TRN and an entire repeated live connection are not exercised. |

Origin trace behind the open rows:

* ADI C1 only installs its parameter pointer. Its real `reset` at 40300
  stores the PCM law at 4031a, clears per-phase alternate flags at 403f7,
  and initializes accumulators. `resetLinearMapping`, 40500–4056e, clears both
  tables and seeds only the reference-code slot. Neither method produces the
  eight supplied levels. `updateLinMappMeanAndVar`, 40fa0–4101f, requires a
  nonzero cell count, computes reciprocal-count times magnitude sum, then
  stores a rounded short at 41016. Supplying arbitrary code-labelled samples
  to that method would move, rather than establish, the missing training history.
* `V90Demodulator::exitPhase3`, 1bc4d, calls `exitDIL`; 1bc7b calls
  `setNofUcodesInTrn2`; 1bcb1–1bd3d supplies learned tables, usable/alternate
  masks, law, phase, maximum code, Jd lookahead and transmit-power bound to
  `V90TRN2Design`. That call writes the primary mapping passed to P4D reset at
  1bda2. The fixture replaces this entire producer boundary, not just a
  constructor default. Demapper reset at 3087f–308ad derives bit counts;
  309ba–309d7 indexes the ADI table through the supplied mapping codes.
* Evaluator `updateCurrentConstellationData` at 3e600–3e639 stores the signed
  distance and three float thresholds and **clears** the request. Its
  `updateAvePdsnr` at 3e580–3e5f0 consumes float error and unsigned symbol
  count. `evaluateConnection` rejects an empty count at 3e6f0; the ordinary
  rate-down path at 3ebc0–3ec15 checks enable, error threshold and both duration
  conditions before copying parameter +3f0 to request +90. Later verdict/override
  paths can clear that result. Calling the setter with invented thresholds or
  only assigning a positive symbol count would not establish the joint
  designer/equalizer history. The current test therefore claims the request
  only as an interface assumption. P4D consumes it as a boolean; no unrestricted
  domain for the evaluator's other inputs is claimed.

### Raw dB witness: still inferred, capability precisely missing

Host PATH checks using `shutil.which` found none of `gdb`, `gdb-multiarch`,
`lldb`, `rr`. Executed `command -v` checks inside the existing Gentoo image
also found none of those four. This is a scoped availability check, not a
claim about every executable anywhere on the machine. No external debugger
location was supplied, and no software was installed.

What is missing is an existing debugger capable of launching this i386 ELF,
stopping at both actual consuming comparisons and reading the unmodified
binary32 stack operand (or its lossless x87 representation) with the matching
energy fields. The reference sites listed in PR182 remain the targets; locate
the reconstructed sites independently in the built executable before observing.
The integer-formatted diagnostics do not expose that operand losslessly.
Neither a duplicate logarithm nor flag 1 qualifies. **Raw NaN was not observed
in either implementation**; zero/zero energies plus the original instructions
continue to support the arithmetic inference only.

Read-only disassembly of the actual paired period executable independently
locates the consuming operands on both sides (addresses are executable VAs,
not reference-object offsets):

| Side/mode | Safe observation point before consumption | Raw dB word; object base |
|---|---|---|
| Original V.90 | 0x0811f006, `flds 0x10(%esp)`; final `fcompp` at 0x0811f021 | `[esp+0x10]`; `esi` |
| Original V.92 | 0x0811fc50, `flds 0x10(%esp)`; final `fcompp` at 0x0811fc6b | `[esp+0x10]`; `esi` |
| Reconstructed V.90 | 0x080da0c7, `fcomps 0x1c(%esp)` | `[esp+0x1c]`; `esi` |
| Reconstructed V.92 | 0x080d945a, `fcomps 0x1c(%esp)` | `[esp+0x1c]`; `esi` |

All four use energies at `[esi+0x3508]` and `[esi+0x350c]`. The reconstructed
words are existing `fstps` spills at 0x080da03a/0x080d93cd, not added
instrumentation. The reference spills at 0x0811efb6/0x0811fc00 feed its reloads.
These locations belong to executable SHA-256
`45ccae455c396e606ada947c2366259578c46d13dbd0d1dad483b51d4986e76b`;
re-resolve after rebuilding. Artifacts `issue183-final-period-{v90,v92,ref-v90,ref-v92}.dis`
retain the complete symbols. Static operand location is progress towards a
future witness, **not** a runtime observation.

### Issue #183 verification and artifacts

Unchanged-base focused Gentoo gate: **1 passed, 0 failed**, direct exit 0,
`/tmp/opencode/issue183-baseline.log`. First sample-driven implementation:
**350962 checks**, focused gate exit 0 (`issue183-targeted-1.log`); the final
ADI reset/interface assertions added 28 checks (`issue183-targeted-final.log`,
350990 checks). A first full phase passed **385/0**, exit 0
(`issue183-phase.log`, `issue183-gate-status.json`). Review then fixed the
failed-entry call report (193 for a 192-call bound), made rejection reports
print actual observations, added two explicit rejection-bound checks, and kept
the stimulus's sign arithmetic signed. The final focused gate
`issue183-final-targeted.log`: **1 passed, 0 failed**, direct exit 0; actual
binary **350992/350992 exact checks**. No new unit binary was added; the full
period denominator remains 385, not 386.

Ordinary and optimized Python observer runners retain explicit `require`, never
Python `assert`. Optimized run: **9/9 subprocess controls**, exit 0; baseline
includes the two message-rejection cases. The six postimage fault counts are
unchanged (2,2155,1,11,9,1), all out of 350992 checks, all direct exit 1. The
new `cp-crc` run exits 1 with **11/177114** failed checks on positive entry;
its assertion inventory is shorter because it never starts V.92 measurement.
Unknown probes exit 1.
Per-run output, binary SHA-256 and direct return codes are retained in
`issue183-final-observers-optimized/` and `issue183-final-observers/` under
`/tmp/opencode`; earlier `issue183-observers*` directories retain the 350990-check
version. Final targeted/control exits are in `issue183-final-gate-status.json`.

One **invalid apparatus run** is retained: the temporary optimized observer
negative-control script imported an unrelated `/tmp/opencode/dis.py` through
`unittest.mock` and exited 1 before executing its control. See
`issue183-final-observer-negative.log`; that driver correctly stopped and did
not claim a phase pass. The script now removes its temporary artifact directory
from Python's import path before loading standard-library modules. The corrected
control substitutes exit 0 and empty output for the `cp-crc` subprocess only;
it must raise `RuntimeError`, using explicit checks even under `python -O`.

The whole-symbol disassembly artifacts are `issue183-v92.dis`,
`issue183-cp.dis`, `issue183-evaluator.dis`, `issue183-exitPhase3.dis`,
`issue183-demod-progress.dis`, `issue183-demapper-reset.dis` in that directory.
Exploratory numeric disassembly starts at 27200 and 3f490 landed inside
instructions and are **invalid for their initial partial instructions**; those
fragments are excluded. Whole-symbol reads supersede them; no finding is based
on the spurious `(bad)` instructions. No invalid differential run occurred in
this continuation.

Final results, with direct subprocess exits (not pipeline/wrapper last-command
status):

| Gate/control | Denominator and verdict | Exit |
|---|---|---|
| Focused Gentoo `make -j1 J=1 period T=t_v90p4dperiod` | 1 passed, 0 failed; fixture 350992/350992 exact checks | 0 |
| Ordinary / optimized observer runners | 9/9 each; 12/12 measurement cycles, 2/2 CP rejection cases | 0 each |
| Optimized false-clean CP runner control | 1/1 substituted exit-0/empty-output result rejected | 0 (control passed) |
| Final `make -j1 J=1 phase` | **385 passed, 0 failed**; structural boundary OK | 0 |
| Final standalone refcheck | 14051 references, 2571 headings; 0 unresolved/pending/stale | 0 |
| `git diff --check` | No whitespace errors | 0 |

Final phase retains 2202 offset annotations, 2679 live period assertions,
336 types/171 files/1 known duplicate, 600/600 banners, partialcmp self-test
8/8 and TU attribution/order self-test 11/11. The vendored-header manifest
passes 7/7; its absent external upstream checkout remains an explicitly skipped
drift check. No modern tier or mutation-execution sweep was run.

`issue183-verified-gate-status.json` records the corrected apparatus control,
final phase, refcheck and diff exits. Corresponding logs use the
`issue183-verified-` prefix. `issue183-final-gate-status.json` retains both
successful final fixture/control runs and the invalid temporary-runner exit.
The executed compiler/selected assembler/linker identities are in
`issue183-toolchain.log`: Gentoo GCC/G++ 3.4.2-r2, assembler and ld
2.15.92.0.2 20040927, with the same image ID/digest recorded in PR182.
Complete source/fixture flags remain printed in the focused and full gate logs,
including `DSPLIB_REPRODUCE_BUGS` and the existing source-only C++ `-ffast-math`.

Reproduce the fixture and controls using the PR182 commands above plus:

```sh
python3 -O test/safety/test_v90p4dperiod_fixture.py build/period/t_v90p4dperiod
DSPLIB_P4D_PERIOD_FAULT=cp-crc build/period/t_v90p4dperiod  # expected exit 1
python3 -O /tmp/opencode/issue183_observer_negative.py
```

Disposition: ready for independent review of this **bounded** continuation.
The next reachability discriminator is an actual DIL/TRN2 design plus
designer/equalizer/evaluator history reaching the qualifying caller request;
the next raw-witness prerequisite is an approved available i386 debugger.
Neither open item is replaced by an API-callability claim, a tolerance or an
exemption. No publication or reconstruction-source change is part of this pass.

## Independent-review follow-up: request/message matrix

The preceding bounded implementation was independently approved. This follow-up
adds only a bounded matrix and its reporting to the same dirty branch; it does
not broaden the supplied calibration/evaluator boundary. The original six
measurement cycles per mode remain intact. No raw NaN observation is added.

### Domain and independent stimulus

**40 named cells:** ten message sequences crossed with local evaluator request
`L in {0,1}` and transmitted peer SUV `S in {0,1}`. Each cell freshly constructs
the paired objects, resets through the real methods, copies the supplied local
request at the already-licensed caller interface, and sends at most 192 samples.
It stops immediately when either implementation leaves WaitForCP. No measurement
state, count, energy or CP-result flag is assigned. The peer bit is placed in the
message **before** computing its CRC, and individual corruption flips the first
CRC bit **after** computation, before scrambling. There is no read of receiver
state in stimulus construction.

Each slot is 72 descrambled bits. An omitted slot is 72 zeros, not a compressed
timeline; all remaining input is zeros. CP-only and CPnot-only place their sole
message in slot 0. Thus CP-only and omit-CPnot are deliberately identical
stimuli, whereas CPnot-only and omit-CP differ by one slot of initial zeros.
For the neither case, the unused peer-bit selector changes no stimulus. There
are consequently **34 distinct component inputs including L**, not 40 unique
streams (17 distinct sample streams). The 40-cell denominator preserves the
requested named comparisons and explicitly discloses these duplicates.

The generator remains based on the original `infoToBits` layout and the integer
CRC recurrence already described above. No decompiler output, external modem
implementation or standards text supplies the new fixture. This is an
original-object differential investigation, not independent standards validation.

### Observed matrix from the original object

The following timings are **sample-call observations**, not individual internal
bit-call timestamps. Full 24-bit frames are delivered every six sample calls;
acceptance is observable at calls 18/36, although the decoder's padding boundary
occurs inside that delivered frame. All four `(L,S)` combinations give the same
acceptance/exit timing for each sequence below. Destination is listed separately.

| Sequence | Slot 0 / slot 1 | Accepted acknowledgement bits, at sample calls | First Ed-driven exit, or bound |
|---|---|---|---|
| pair | CP / CPnot | 0 at 18; 1 at 36 | 48 |
| CP-only | CP / absent | 0 at 18 | 30 |
| CPnot-only | CPnot / absent | 1 at 18 | 30 |
| reverse | CPnot / CP | 1 at 18; 0 at 36 | 48 |
| bad-CP | corrupt CP / CPnot | 1 at 36 | 48 |
| bad-CPnot | CP / corrupt CPnot | 0 at 18 | 48 |
| omit-CP | absent / CPnot | 1 at 36 | 48 |
| omit-CPnot | CP / absent | 0 at 18 | 30 |
| both-bad | corrupt CP / corrupt CPnot | none | No exit in 192 |
| neither | absent / absent | none | No exit in 192 |

After **at least one accepted message**, with this reset history:

| L | Decoded S | Acceptance latch at acceptance | Handshake +44 | Ed destination | Before energy |
|---|---|---|---|---|---|
| 0 | 0 | 1 | 0 | B1d (7), progress 0x1c | unchanged |
| 0 | 1 | 1 | 1 | WaitForRt (14), progress 0x35 | initialized to +0 |
| 1 | 0 | 1 | 1 | Silence (10), progress 0x35 | initialized to +0 |
| 1 | 1 | 1 | 1 | WaitForRt (14), progress 0x35 | initialized to +0 |

With no accepted message, all combinations remain WaitForCP (4), progress 0,
count 192, +30/+44 remain zero, and both energies and P4D +4c remain untouched.
For every entry case, count is zero at exit. After energy remains untouched in
all 40 cases. The matrix therefore covers 8 B1d, 8 Silence, 16 WaitForRt and
8 blocked cells, with **2856 sample calls per side** and **40 accepted-message
events per side**, 20 carrying nonzero SUV. Every one of the 476 complete frames
per side is compared against its independently generated 24 wire bits.

### What licenses the conclusions, and what they disprove

* `resetBeforRRN`, 25d34–25d50, establishes +3c=1, +44=0, +48=0, +30=0.
  The local request copy at 1cc01 is still a supplied component interface.
* Short-message replies 3/4 each set +30 and copy the decoded peer bit to +4c:
  2735e/2737b and 273cd/273df. Each independently sets +44 when `L || S`,
  provided +48 is zero (27379–2739a and 273dd–273fe). Without either request,
  acceptance still occurs but progress is 0x2f/0x30 (276bf/276b3), not
  0x31/0x33. The observer counts both kinds of acceptance separately from
  handshake enable and checks the ordered acknowledgements and their timings.
* Ed at 272c4 first requires +30. With +3c and +44 nonzero and +48 zero,
  276e3–2776c initializes before energy, sets +48, clears +30, and selects
  WaitForRt for nonzero +4c or Silence for zero +4c. Otherwise 272e3–2732a
  enters B1d without clearing before energy. These are three distinct outcomes,
  not simply accepted/rejected.
* **Two-message necessity and CP-before-CPnot necessity are disproved at this
  boundary.** Either valid short form alone enables the same gate; reversed
  order works; corrupting or omitting either message does not block entry when
  the other is accepted. Requiring 2/2 in the existing six-cycle fixture remains
  useful for testing its selected pair, but is not a reachability prerequisite.
* Corruption and omission are not always timing-equivalent: corrupting the
  second message delays exit to call 48, whereas omitting it permits call 30.
  CRC rejection resets the decoder, but does **not** revoke the P4D acceptance
  latch set by the first valid message. The ordinary Ed zero-run detector later
  acts on that retained latch. No-acceptance controls remain blocked despite
  zero-run Ed replies.
* **The peer-store observer is now nonvacuous for both values.** C1, full reset
  (277d5–27820 state stores) and RRN reset do not initialize +4c. Each fresh
  object's existing whole-storage poison survives there as `0xa5a5a5a5`, which
  is logged and checked before decoding. At first acceptance in each of the
  32 entry cells, the actual 0 or 1 differs from that preimage. The 20 nonzero-SUV
  observations per side comprise 16 first acceptances and four repeated
  same-value observations; the latter do not independently prove another store
  occurred. The store instructions above license that control-flow claim.
  No direct peer-field seed was introduced. Without acceptance, the old raw word must
  remain identical; it is not treated as a valid negotiated bit.

These conditions are bounded by +3c=1/+48=0 from the actual reset and a fixed
peer bit within each cell. They do not establish behavior for alternating peer
bits, later RRN exchanges, or a complete remote modem. The experiment does not
require preceding DIL/TRN2; the existing assumption ledger remains open there.

### Matrix development error and verification record

The first matrix run failed **96/363210** checks, and the focused gate correctly
reported **0 passed, 1 failed**, child exit 1. The new oracle had incorrectly
assumed full reset zeroed +4c: 80 preimage expectations and 16 no-acceptance
postimage expectations were wrong. Both implementations instead retained
`0xa5a5a5a5`; all observed message/destination timings agreed. Inspection of
the original reset stores corrected the **fixture oracle**, not production
code. The failed gate and full output are retained as
`/tmp/opencode/issue183-matrix-targeted-1.log` and
`issue183-matrix-observed-1.log`. This is a failed development run, not a pass
or an exception. No period source defect was established.

The corrected matrix also asserts observed exit/bound timings, per-message
bad-CRC diagnostic counts, the actual peer preimage/postimage difference,
state/count/progress/handshake, energy initialization versus preservation,
exact paired transcripts, frame bits, guards and allocation release. The
ordinary and optimized Python runner requires all 40 named cells and the
2856-call/20-nonzero-copy summary using explicit `require` checks.

Final serial results on the matrix version:

| Gate | Result | Direct exit |
|---|---|---|
| Focused Gentoo period | 1 passed, 0 failed | 0 |
| Raw fixture | **363571/363571 exact checks**; prior 350992 + 12579 matrix checks | 0 |
| Matrix | **40/40 named cells**, 2856 samples/side, 476 frames/side, 40 acceptance events/side (20 with SUV 1) | included above |
| Existing lifecycle | Six cycles/mode, 12 total; 2394 calls and 4 diagnostic transitions each | included above |
| Observer controls, ordinary Python | **9/9** | 0 |
| Observer controls, optimized Python | **9/9** | 0 |
| `make -j1 J=1 phase` | **385 passed, 0 failed**, structural boundary OK | 0 |
| Refcheck | 14051 references, 2571 headings; 0 unresolved/pending/stale | 0 |
| `git diff --check` | no whitespace errors | 0 |

The six postimage faults still fail 2, 2155, 1, 11, 9 and 1 checks respectively,
now out of 363571, with ordinary exit 1. `cp-crc` still fails positive entry
before the matrix runs, and unknown probes still fail. No new unit binary,
modern run, mutation-execution sweep, tolerance or source/include change was
introduced. Compiler flags and Gentoo image selection are unchanged; complete
flags are printed in the focused and phase logs. Structural denominators remain
2202 offset annotations, 2679 live period assertions, 336 types/171 files/1
known duplicate, 600/600 banners, partialcmp self-test 8/8 and TU tests 11/11.
The vendored-header upstream-drift check remains explicitly skipped because its
external checkout is absent; 7/7 manifest entries pass.

All continuation artifacts are under `/tmp/opencode/`:

* `issue183-matrix-gate-status.json`: actual subprocess commands and exits for
  focused period, raw execution, both observer runners, phase, refcheck and diff.
* `issue183-matrix-observed-final.log`: all 40 original-side observations and
  the exact-check verdict; `issue183-matrix-targeted-final.log` and
  `issue183-matrix-phase.log`: deciding compiler gates.
* `issue183-matrix-observers/` and `issue183-matrix-observers-optimized/`:
  complete baseline/fault outputs and direct exits, executable identity.
* `issue183-matrix-reset.dis`: original reset evidence for the corrected
  preimage expectation. The original decision/CRC disassemblies from the
  preceding section continue to license the branches; production code did not
  change.

Matrix executable SHA-256:
`656f5e898046f40e148f27557bd7999b8056cee7f69c4d62fd569f1edfb16cfe`.
The older executable-VA observation table belongs to its recorded earlier hash,
not this rebuild. Raw NaN remains **not observed**. Reproduction uses the same
focused/observer/phase commands above; the matrix is now part of
`t_v90p4dperiod`. The dirty branch is left for review without publication.

## Issue #183 producer-history continuation (review pending)

Base `a4077068`, branch `investigate/issue183-producer-history`, same worktree.
This advances **one bounded producer chain**, not full training or a public
modem connection. All original twelve measurement cycles and forty CP matrix
cells remain; no case was removed. Their eight-level synthetic environment
remains labelled as supplied. Two additional V.90 environments replace it with
actual built-in DIL generation, ADI accumulation/finalization and TRN2 design.
The positive evaluator request and the V.92 caller copy remain supplied.

### Inventory of supplied state, including unchanged assumptions

Offsets are i386 object-relative; types are the fixture's types corroborated by
the original loads/stores. Array formulas include every supplied element, not
only the first phase. Original addresses below were read using `tools/dis.py`.
Constructor/reset outputs are distinguished from direct fixture assignments.

| Supplied state or boundary | Offset/type/value in the original fixture | Original writer / preceding operation and precondition | This continuation |
|---|---|---|---|
| Host parameter block | All bytes initially zero; `minRate` +38 `uint=28000`, `maxRate` +3c `uint=56000` | External host configuration, consumed by real parameter C1/defaulting; not an internal training result | Unchanged separate host blocks and real C1; no parser-stub or arbitrary config reachability claim |
| Storage preimages | All object storage zero except whole P4D `0xa5`; 32-byte trailing guards `0x69` | Apparatus, not a production writer or valid negotiated state | Unchanged. Unwritten padding/tails remain preimages, never evidence of initialization |
| ADI law/reference/RBS hint | `pcmType` +a95c enum 0, `ucode` +a96b byte 0, `ucodeLevel` +a96c short 0, `altRbsExpected` +a96e short 0, `altRbsFlag` +2800+2p short 0 | `reset` 40300, law store 4031a and phase flag clear 403f7; `resetLinearMapping` seeds the reference slot. Negotiated law/reference would precede this | Same real resets. Mu-law is a selected component law. Code zero is a neutral reset seed, not a claim that a silent TRN1 reference trained a connection; DIL generation does not use that reference level |
| ADI calibration | +0+256p+2c and +600+256p+2c, `short`, p=0..5/c=0..7, `300-20*c`; remaining cells inherited from reset | `calculateLinearMeanAndVar` 42090 produces +1000 magnitude sums/+1c00 counts/+9118 square sums; `updateLinMappMeanAndVar` 40fa0 needs count>0, writes variance +9d48 and rounded short at 41016. Alternate table needs alternate-RBS study | **Replaced in new cases**, not merely moved into a helper. Each side derives its own primary table from its own DIL symbols; inactive alternate table stays reset output. Original CP cases retain the supplied tables |
| Mapping frame count and sizes | +0 `uint=24`; +604+4p `uint=8` | `V90TRN2Design` 3cbab–3cbb5 copies default parameter +78; 3cc30–3cd1e computes frame bits | **Replaced in new cases**: default sizes 8, frame bits **23**, shaper rate **1** |
| Mapping code lists | +4+128p+c and +304+128p+c, `uchar=c`, p=0..5/c=0..7; unused tails zero | Designer primary list at 3d8b7 (no-alt branch); codec list 3d2d0 onward/3d6c7 onward depends on learned levels and both laws | **Replaced in new cases** by full paired TRN2 calls; primary and codec lists compared exactly |
| Mapping shaper and auxiliary fields | +61c `uint=0`; +620 `int=0`, +624 `uint=0`, +628/+62c/+630/+634 floats 0; +638+4p `int=0` | Designer writes +61c=1 at 3cb93, shaper from defaults at 3cbd0–3cc2a with id bounded by lookahead, and six distinct indices zero at 3cd20–3cd34 | **Replaced in new cases**. No parameter override to recover the old 24-bit stimulus |
| Secondary mapping `mb` | Whole `V90MappingParams` byte-copy of own `ma`, including zero tails | A separate production mapping is later made by `V90ConstellationDesigner::process`, e.g. caller 1d850; TRN2 alone is not that history | Still an explicit initial alias-in-value assumption, separately owned; new cases copy their own produced TRN2 mapping. No full data-phase design claim |
| Evaluator request | +90 `uint=1`; matrix overrides it to 0/1 | `evaluateConnection` 3e6d0; ordinary rate-down store 3ec0f copies parameter +3f0 after enable, error and duration tests. Other arms can override/clear it | **Open positive history**. Added actual empty-history evaluator calls reject production (return 0, request 0); only then the old visible request assumption is assigned |
| P4D local request | +40 `int`, copy of own evaluator +90 | `V90Demodulator::progress` 1cbc0–1cc01 after nonzero session, request +3c in {22,23}, real RRN reset | Remains explicit in V.92 original cases, not invoked as a caller witness; new V.90 cases consume evaluator +90 directly |
| Decoder group sizes | MP +114 / CP +3ba8, `uint=ma.word_0` | P4D writes these in its preceding decision states (260f2/263a9 and 26d1d/26d97/2705c), from the mapping bit count | Still an entry-interface copy: 24 in original cases, produced 23 in new MP cases |
| P4D dependency/session and reset arguments | C1 installs 32-bit pointers: +4 own params, +c/+10 own `ma`/`mb`, +14/+18 own CP/MP, +1c NULL phase3, +3054 own demapper, +3058 own descrambler, +34f8 own evaluator, +3514 own ADI; +0 session `uint=0/1`; reset +8 ucode byte 0, +20 state enum WaitForRi, pumped samples 0, +34 quick-connect `uint=0` | Real P4D C1/reset; production `exitPhase3` calls reset at 1bda2. A real modem supplies a phase3 object and prior phase2 information | Unchanged real lifecycle and bounded methods which do not dereference the null phase3 dependency; not a valid whole-modem object graph |
| Demapper and descrambler arguments | Demapper capacity 72; descrambler taps 18/23, capacity 99, history reset 0 | Real constructors/reset; sample decoder consumes six-symbol frames | Unchanged, independently allocated. Capacity covers the tested frames |
| Entry timing and external samples | Explicit `resetBeforRRN` and WaitForEd/WaitForCP entry; V.90 -300, V.92 polynomial-generated CP/Ed stream; measurement pairs listed above | Earlier Ri/TRN/data/RRN progression normally selects these entry methods | New V.90 Ed samples use each side's produced first constellation level at each phase, negative sign. Both recover 23 zero wire bits/frame. Measurement inputs unchanged; repeated RRN negotiation remains unexecuted |
| Debug and fault inputs | Both debug levels 3; named observer faults opt-in | Harness controls, not modem inputs | Added missing-calibration fault; synthetic status explicit |

Zero storage in fields not written/read on this boundary is not silently promoted
to a production value. Dependencies continue to be checked by identity and own
preimages rather than by erasing pointer-shaped bytes for comparison.

### Original writers and tested producer domain

The fixture uses original symbol identities via paired ABI entry declarations:
unprefixed reconstruction and `ref_` original. No reconstruction producer is
shared by the reference side. `setDilDescriptor` at **31c60** reads its own
built-in `.data` tables and writes the descriptor on each side independently.
Only `DIL_TYPE_ADI=0` is selected (the production type domain is 0/1, selected
by quick-connect). It is not a hand-filled code/sample list.

Each side constructs and resets its own `V90Phase3Modulator`, mu-law, DIL state,
zero warm-up, null unused Jd pointers, own descriptor. `resetDILGenerator`
**2aed0–2b069** decodes the descriptor's segment/DIL levels and zeros cursors.
`generateDIL` **2b070–2b21a** emits one whole descriptor cycle, observed as
**32280/40000** bounded calls and a return to index/segment position zero.
The generated sample arrays are compared exactly, as are the descriptor and
generator's DIL state. No generator cursor or expected result is planted.

The channel boundary is aligned signed-short symbols with deterministic gain
**1 or 1/2**, no noise, no phase shift and no alternate RBS. Every generated
mu-law level is representable, and division by two is integral for these
levels. The received magnitude is therefore tied to a generated symbol, not
assigned to an invented PCM code. `calculateLinearMeanAndVar` determines that
code from the **symbol**, using real companding; using `dilPcmCode` for every
sample would be wrong because segment-reference samples have a different code
(2b092 versus 2b180). The fixture schedules phase as sample index modulo six.

This deliberately stops short of P3D's timed study machine. Original P3D
`getV90Decision(float)` calls the same accumulator at **23ed7** and updater
at **24a5f** in the second-study path (other study arms have other schedules).
The fixture finalizes all cells after the complete generated cycle, not at
every original P3D update/unite boundary. Unmeasured cells retain reset values.
`porcessSecondStudy` **41cb0** selects reference phase zero from reset's clear
suspected flags, and `determineMaxUcode` **441f0** produces the usable mask and
six maxima. Its argument 116 is independently checked against the highest
code of the real descriptor. No learned mask, variance, count or maximum is
hand-populated. The full resulting ADI bytes compare around the one independently
validated parameter pointer, **before demapper reset clears accumulators**.
All 48 selected constellation entries are explicitly required to have measured
counts and positive, strictly descending levels in each phase.

`V90TRN2Design` **3cb60** consumes those tables/masks/maxima. Remaining explicit
designer arguments are both laws mu, lookahead 0 (within the Jd two-bit domain),
normal spectral condition 0, and transmit index 1 (the caller's phase2 power
byte 0 plus 1, **1bcd5–1bcea**; safely inside the power ladder). These are bounded
interface choices, **not demonstrated negotiation outcomes**. Codec detection,
pad-gain inference and phase2/Jd decoding have not been exercised. All real
default parameters, including eight TRN2 codes, shaper rate 1 and silence
timings, remain unchanged; a complete own-side parameter/guard comparison
enforces this. TRN2 output mapping, power-helper scalars/arrays and own-table
pointer identity are compared, as are complete producer diagnostics.

`V90Demapper::reset` **30870**, particularly **309ba–309d7**, turns that mapping
and calibration into its levels. Each of the 48 level lookups is checked;
`hardDecision(1000)` is a common finite sample discriminator. Real Ed decoding
then consumes each side's produced constellation, and requires both 23 zero
bits per complete frame and silence entry. Both finite cases reach Ed at call
12 and each executes all six existing measurement cycles, using the unchanged
2394-call step/energy/diagnostic/peer/heap oracle. Generator, descriptor,
designer and power storage join the frozen peer inventory. The temporary
generator allocations are destroyed before measurement, leaving **36 live
allocations** across the pair as before; guards and final frees are checked.

Existing fixtures were inspected for reusable ABI/layout and production calls:
`t_v90p4ddec`'s `exitPhase3` cases and `t_v90trn2designrecip` plant their input
tables/configuration and therefore cannot supply the missing history here.
Their synthetic expected data was not reused. This continuation reuses the
production routines themselves and the existing period lifecycle oracle.

### Controls and an original-object counterexample

| Case | DIL calls/side | Designer return | Frame bits | Usable measured descending entries | `hardDecision(1000)` | Measurement |
|---|---:|---:|---:|---:|---:|---|
| Unity gain | 32280 | 1 | 23 | 48/48 | 988 | 6 cycles |
| Half gain | 32280 | 1 | 23 | 48/48 | 622 | 6 cycles |
| Missing calibration, synthetic fault | 32280 generated, **0 accumulated** | **1** | 23 | **0/48** | **0** | Not admitted |

The half-gain case must change the learned table, resulting mapping **and**
downstream decision; comparisons are against the saved unity run, not just
between implementations. The missing-calibration control still generates the
whole DIL and calls the finalizers/designer. In the no-alt arm, zero learned
levels make the distance and threshold zero; the scan can fill repeated code
2 without advancing and return success (3d7a6–3d8b7). Thus **return 1 is not a
calibration-validity test**, and side agreement alone would miss dead producer
calls. The fault is not a legal completed training environment and is never
fed into the positive measurement claim.

`DSPLIB_P4D_PERIOD_FAULT=calibration-missing` separately suppresses accumulation
on **both sides of the unity positive case** while retaining its positive
requirements. It fails **6/538058** checks, direct exit 1: usable levels,
measured entries and downstream nonzero decision on each side. The shorter
denominator reflects the six intentionally unentered measurement cycles.
The ordinary baseline contains the explicit negative case and is green.
This is an observer/producer relevance control, not a mutation campaign.

### Evaluator/caller boundary: traced, positive history still open

TRN2 design is **not** the producer of the evaluator's distance and thresholds.
`V90Demodulator::progress` calls **`V90ConstellationDesigner::process` at 1d850**,
then loads designer short +a and float +18/+1c/+20 at **1d87e–1d89d** and calls
`updateCurrentConstellationData` at **1d8a4** (another path at 1e3aa).
That setter clears request +90. An equalizer-derived error average/symbol count
and appropriate data durations must subsequently reach `evaluateConnection`.
The caller invokes it at **1cf73**, dispatches its verdict at **1cf81**, and
only qualifying V.92 request/session states reach the **1cc01** copy.
Calling just that copy with newly planted demodulator state would not recover
this history and was not done.

All three new constructed pairs actually call `evaluateConnection` before
assigning the existing request assumption. Each returns 0 with request 0 and
symbol count 0, and only latches `initDmin` from -1 to 0; the complete own-side
evaluator/guard postimage is checked. This agrees with the original **3e6dd,
3e6f0, 3e8e3–3e8f1**. Constructor plus evaluator invocation is therefore a
measured **insufficient prerequisite**, not a new request witness. The next
positive experiment needs data-constellation designer outputs plus equalizer
error history, not invented thresholds passed through a setter. Ordinary
rate-down checks at **3ebc0–3ec15** require enable, average above threshold,
rate-down duration and minimum data duration; later retrain/override paths
can clear the request. This pass establishes no period reconstruction defect.

### Validation and artifacts

All runs use Gentoo GCC 3.4.2-r2 with `DSPLIB_REPRODUCE_BUGS`, unchanged period
flags, serial `-j1 J=1`. No new unit binary: full denominator stays **385**.
Focused baseline, three development builds and the final focused build all exit 0;
the first added-boundary version had 711612 checks, the final fixture has
**711782/711782 exact checks** (348211 more than base). No failed differential
development run occurred. Ordinary and `python3 -O` observer runners each pass
**10/10** controls, with 24/24 measurement cycles on baseline, unchanged
40-cell/34-input CP matrix, three producer cases and six empty evaluator calls.
Postimage-fault failures are 6, 6465, 3, 33, 11 and 3 out of 711782 respectively;
the additional V.90 cycles also exercise those opt-in injections.

| Final gate | Measured result | Direct exit |
|---|---|---:|
| Focused Gentoo period | 1 passed, 0 failed; 711782 exact checks | 0 |
| Ordinary / optimized controls | 10/10 each | 0 each |
| `make -j1 J=1 phase` | **385 passed, 0 failed**, structural boundary OK | 0 |
| Standalone refcheck | 14051 references / 2571 headings, no unresolved/pending/stale entries | 0 |
| `git diff --check` | No whitespace errors | 0 |

Final structural counts: 2202 offset annotations, 2679 live period assertions,
336 types/171 files/1 known duplicate, 600/600 banners, partialcmp self-test
8/8, TU attribution/order 11/11. Vendored headers pass 7/7 manifest entries;
the absent external upstream checkout still means no upstream-drift check.
No modern tier, mutation-execution or mutsnap sweep was run.

Artifacts use `/tmp/opencode/issue183-producer-`:

* `baseline`, `targeted-1`, `targeted-2`, `targeted-3`, `targeted-final`: `.log` and `.json`
  commands/direct exits; complete period flags printed in each build log.
* `controls-final`, `controls-optimized-final`: runner logs and direct-exit JSON;
  `observers-final/` and `observers-optimized-final/`: per-probe full output and status,
  including executable hash and raw child exits.
* Named whole-symbol `.dis` files: descriptor, DIL reset/generation, P3M reset,
  ADI accumulate/mean/reset/second-study/max, TRN2, demapper, P3D, exitPhase3,
  evaluator, caller, Ed, V.90/V.92 decisions and parameter defaults.
  Final script resolves all 19 symbols successfully. Early misspelled caller
  symbol lookups returned 1 and supplied no evidence; corrected whole-symbol
  reads supersede them.
* `phase.log`/`phase.json` and `phase-final.log`/`phase-final.json`: detached full serial gates with direct make exits;
  standalone `refcheck` and `diff-check` logs/status are recorded separately.

The final executable measured by both observer runners is SHA-256
`776a667ec198376d832f6dc6118e28450f9f29e333c32c98952304477b75be21`.
The earlier `controls`/`observers` artifacts retain the pre-cleanup executable
`3c93ef8ce5fe417f56d52467752039e4481573e65886d49fe76651a86fee1540`:
final cleanup aligned the raw ADI snapshot explicitly and fixed indentation;
the assertion count and all reported observations are unchanged.
Executed compiler and compiler-selected assembler identity is retained in
`toolchain.log`: GCC/G++ 3.4.2 (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1,
pie-8.7.6.5), assembler/ld 2.15.92.0.2 20040927. `identity-final.log` records
the reference, fixture source, runner source and executable hashes together.
Reference identity remains the SHA-256 recorded at the top of this audit.
Reproduction uses the same targeted/phase commands above and the updated
observer runner; the producer cases are part of `t_v90p4dperiod`.

**Independent review pending; unpublished.** Remaining assumptions are timed
P3D/TRN1/DIL study and synchronization, codec/pad/channel negotiation, later
data-constellation/equalizer/evaluator history, actual qualifying caller request,
and producer-backed V.92 CP encoding with the default shaped mapping. The old
negative-energy fixture stays exploratory. **Raw NaN remains unobserved** with
the debugger prerequisite deferred; no source probe or ptrace machinery was
added. Neither finite-channel success nor the synthetic counterexample licenses
a public-modem reachability claim.

## Issue #183: timed P3D study (base 6750e022, review pending)

This continuation is on `investigate/issue183-p3-study` in
`/home/philpem/slmodem/claude_re_worktrees/mutate-workdir`. PR185 was reviewed
and merged as `6750e022`; its earlier pending-review wording above is historical.
The changes here are fixture/observer/documentation only. No original-source
defect was established. All previous cases remain, including the manual ADI
boundary as a comparison control. No publication is authorized by these results.

### Discriminator and actual entry boundaries

The competing possibilities were that replaying DIL alone would enter study,
or that the missing preceding receive history would prevent it; and that a
real timed study could differ materially from the manual 32280-call schedule.
The tested domain is three constructor-boundary replays and three TRN1-boundary
studies, each with separate original and reconstructed producers/receivers:
unity gain, half gain, and missing received DIL. These are six bounded component
environments, not six public modem connections.

* **Constructor replay:** real parameter, ADI and P3D construction; P3D's own
  constructor resets to WaitForSd, reference code 64. A separately constructed
  generator emits the previous ADI DIL stream (mu-law, reference code 0).
  Unity/half/zero received streams all time out at **12000/32280** calls:
  state **0 -> 20**, count 0, event 21. Complete ADI bytes stay identical to
  each side's post-constructor preimage. The remaining DIL cycle is not replayed
  after the terminal event. This is an honest insufficient-history result.
* **TRN1 boundary:** after real construction, the explicit supported
  `reset(mu,64,TRN1dKnownData,0,ownRxJd,NULL,ownDescriptor,0,1,0.0f,0)`
  boundary assumes earlier Sd/SdNot synchronization. The separate transmitter
  resets to TRN1d with code 64, zero warm-up, its own constructor-produced Jd,
  and its own built-in ADI descriptor. `generateSymbol` runs its real TRN1/Jd
  machine. Once the receiver reports CRC-decoded Jd (event 6), the fixture calls
  that transmitter's **real `exitJd`**. This immediate feedback is an explicit
  component scheduling choice, not a demonstrated bidirectional modem handshake.
  The generator itself supplies JdNot and enters DIL; P3D independently detects
  JdNot and resets its own internal generator. There is no direct study-state,
  study-counter, learned-table or accumulator assignment.

The half gain applies throughout the TRN1/Jd/DIL stream. The missing-DIL control
keeps the preceding unity training/Jd samples and substitutes zero only for
received DIL samples, while both actual generators continue. It is a controlled
channel interruption, not a completed usable calibration. No receiver field is
consulted to choose a DIL code label or to supply an accumulator argument.

### Original instructions and state path

Addresses are reference-object `.text` offsets from `tools/dis.py`, not Ghidra.
P3D C1 **212c0–2142c** calls reset at **21421**, passing state 0 and reference
code 64. Reset **20f90–212b0** stores the supplied state/count at **21017/2101a**;
its explicit state-3 arm **21210–21253** resets ADI mapping and starts the
embedded TRN1 generator. Using that documented method boundary does not prove
that the larger `V90Demodulator::enterPhase3` caller reached it with these inputs.

The V.90 decision function **23830–258ea** increments count at **23899**.
The constructor replay's Sd detector call is **241ed**; the timeout arm is
reached without any DIL calibration calls. For the timed studies:

| Call from TRN1 reset | Transition | Event | DIL calls so far |
|---:|---|---:|---:|
| 2040 | 3 -> 4 (known -> data-directed TRN1) | 0 | 0 |
| 14040 | 4 -> 5 (reference study) | 4 | 0 |
| 21240 | 5 -> 6 (WaitForJd) | 5 | 0 |
| 21324 | 6 -> 9 (Jd received) | 6 | 0 |
| 21336 | 9 -> 10 (first DIL study) | 8 | 0 |
| 25176 | 10 -> 11 (second study) | 0 | 3840 |
| 49896 | 11 -> 12 (third study) | 16 | 28560 |
| 50496 | 12 -> 16 (study finished) | 17 | 29160 |

All three inputs follow these eight transitions on both sides. The fixture
checks state and elapsed count against this independent observation table on
every call, as well as exact paired decisions/events/transcripts. Each study
stops at **50496/70000**, with count zero. The stopping point is **study complete,
not Phase3 Terminated or data phase**; it does not execute `exitPhase3`.

Instruction anchors: known-data count **24042**, reference-study reset
**240a3** and live handler **244ce**, Jd unpack **24688**; JdNot requires run
length >11 and position 12 modulo 72 at **23fb4–23fd9**, with the internal
generator reset at **256e8**. The transmitter's `exitJd` at **2ad80–2add7**
requires state 3 and nonzero duration and finishes at its own 72-symbol boundary.
Timed ADI calls/branches include **24254**, **24a5f**, first-study finalization
**2512a–25158**, and second-study finalization **2545c–25468**. The default
study durations are 3840 + 24720 + 600 = **29160**, selected by production
parameter comparisons, not by the fixture calling ADI finalizers.

### Calibration, mapping and downstream comparisons

The fixture observes cell counts after each real decision call, retaining an
ever-seen bitmap. This measures **end-of-call nonzero-count observations**, not
an instruction trace of every store. It is necessary: `uniteLinMappInfoOfUnsuspectedPhases`
**41688–416a5** clears magnitude sums, counts and square sums after combining
them. A final nonzero-count census alone would wrongly call the finite studies
mostly unmeasured. All 690 observed cells are compared through the paired ADI
postimages; each selected constellation code must have been observed.

After state 16, `determineMaxUcode(116)` remains the explicit component max-code
interface used by the manual case. Its argument matches the built-in descriptor's
maximum. The enclosing caller's `dilMaxUcode` writer, codec/pad-gain pipeline,
phase2 power negotiation and `setDigitalImairmentsInfo` are still outside the
fixture. No learned maximum or usable mask is assigned. Real paired TRN2
designers consume the resulting ADI, with the same explicitly bounded laws,
lookahead, spectrum and power arguments as PR185.

| Timed input | Seen cells / 768 | Live counts at stop / 768 | Positive descending selected levels / 48 | Max code, all 6 phases | `hardDecision(1000)` |
|---|---:|---:|---:|---:|---:|
| Unity | 690 | 12 | 48 | 116 | 988 |
| Half | 690 | 12 | 48 | 116 | 622 |
| Missing DIL | 690 | 690 | **0** | 80 | **0** |

All three return TRN2 SUCCESS=1, frame bits 23, shaping rate 1, masks 702/768,
and no alternate RBS. All 48 selected entries have observed counts even in the
missing-input case. **Neither observed counts, the completion event, the mask
count, nor designer SUCCESS establishes usable calibration.**

Against each side's saved manual case, both finite studies have **0/768 primary
level differences, 6/768 alternate differences, 0/768 mask differences**, and
byte-identical complete TRN2 mappings. The six alternate differences reflect
the different reference-code reset boundary (64 here, zero in the manual case);
the alternate table is not selected. The missing-input study retains the trained
reference, hence 6/768 primary and 6/768 alternate differences from the completely
unaccumulated manual control, but the same unusable TRN2 mapping. Accumulator
history is not claimed identical: live counts alone already distinguish it.

Both finite studies feed their own actual mapping/calibration into a freshly
constructed P4D component chain. Each reaches Ed in 12 calls, recovers 23 zero
wire bits/frame, and executes the same six 2394-call measurement cycles. The
positive evaluator request, MP group-size copy and explicit RRN/WaitForEd entry
remain supplied exactly as before. The prior 24 cycles plus these 12 give
**36 cycles / 86184 measurement calls per side**. No old case was replaced.

Comparison surface: generated symbols, P3 state/count/event/frame and complete
ADI bytes (around its independently checked own parameter pointer) every call;
the internal/external DIL generator region +54..+397 on every DIL call; exact
per-call diagnostics including silent calls; complete decoded Jd, learned ADI
snapshot, mapping and finalization/design diagnostics. Parameters, descriptor
and transmitter Jd are checked against own preimages. Guards and allocation
release/bad-free counts are checked. This is not a claim of whole-P3 heap-graph
identity at every instruction. The P4D phase retains the existing complete
step/energy/guard and all-live-allocation immutability oracle, now also freezing
the retired P3/Jd storage; 36 live allocations are observed during measurement.

`DSPLIB_P4D_PERIOD_FAULT=study-missing` substitutes zero DIL on **both sides of
the unity positive case**, retaining its positive requirements. Both still
complete the study and return SUCCESS; both produce 0/48 usable levels and
decision zero. The positive-output checks reject this with exit 1,
**22/2871341 failed checks**. The shorter denominator excludes the six unadmitted
measurement cycles. The baseline's explicit missing-DIL case checks this
counterexample without crediting it as a positive connection.

### Evaluator discriminator: inspected, still stopped at real error history

The next producer is the **data** constellation designer, not this TRN2 designer.
The newly inspected complete `V90ConstellationDesigner::process` symbol at
**4cbd0** passes its float noise argument to `setConstellationToNoise` at
**4cd2b** (or the forced-rate method at **4cf3d**). In the ordinary helper,
**48ba8–48bde** derives the initial signed-short distance from that float;
**497a2–497dc**, **49880–498b9**, and **498e0–49915** store the three
float thresholds from the noise input and branch-specific factors. Rate-action
history can retain/adjust distance, and `process` can rewrite parameter enables
at **4d04d–4d095**. These are real input-dependent outputs, not TRN2 fields that
can be copied into an evaluator as substitutes.

The caller's **1d850 -> 1d87e–1d8a4** supplies those actual outputs to
`updateCurrentConstellationData`; the setter clears the request. Equalizer
`calcMeanErrorStatistics` **388c9–388f7** requires recorded errors (count or full
buffer), otherwise its empty arm does not establish a trained noise estimate.
Equalizer `process` **399f9–39a26** takes the block RMS, updates its smoothed
error at +80 and passes that value plus the actual block sample count to
`updateAvePdsnr`. The evaluator's **3e6f0** rejects an empty symbol count;
its plain rate-down tests **3ebc0–3ec15** require error above the designed
threshold and both elapsed-duration conditions before requesting silence.
Later retrain/override arms may replace that request. The qualifying caller
copy at **1cc01** remains unexecuted.

**Stopping bound:** the fixture does not run the equalizer that precedes these
P3 decisions or its later phase4/data error history. Consequently it has no
jointly produced noise/duration input with which to claim a positive evaluator
request. No invented noise, duration, counter, designer threshold or caller
state was installed to close that gap. No dynamic data-designer/evaluator
production result is claimed here. The next discriminating experiment is to
compose the actual equalizer with this timed producer history, retain the
recorded phase4 error statistics, pass those to each side's actual data designer,
then drive a bounded finite channel degradation through real error updates and
duration gates. Withheld/incorrect error history must reject the positive request
on both sides. Earlier synchronization and remote Jd scheduling remain separate
reachability questions. Raw consumed-dB/NaN observation remains deferred.

### Serial verification and retained artifacts

Artifact prefix: **`/tmp/opencode/issue183-p3-`**. The driver
`/tmp/opencode/issue183_p3_run.py` records each complete command, working directory
and direct return code in a sibling `.json`, with full combined output in `.log`.
All reconstruction runs use the unchanged Gentoo GCC 3.4.2-r2 period profile,
`DSPLIB_REPRODUCE_BUGS`, `-j1 J=1`; the focused logs print all C/source-C++/fixture-C++
flags. `toolchain.log` executes GCC, G++, the compiler-selected assembler and ld;
`image.log` records the image ID/digest. No modern or mutation-execution/mutsnap
sweep was run.

* `baseline`: unchanged 711782-check fixture, focused period 1/0, exit 0.
* `targeted-1` through `targeted-5`: all focused period gates exit 0. `observed-1`,
  `observed-2`, `observed-4` preserve incremental observation/check inventories;
  they are development artifacts, not the final fixture identity.
* Final focused `targeted-5`: **1 passed / 0 failed**, exit 0. Final fixture:
  **3045107/3045107 exact checks**. No new unit binary; phase denominator stays 385.
* `controls` and `controls-optimized-complete`: **11/11** each, exit 0;
  `observers/` and `observers-optimized-complete/` retain each raw child exit,
  full baseline/fault log and executable hash. The six postimage faults fail
  10, 10775, 5, 55, 13 and 5 checks respectively out of 3045107.
* `observer-negative`: optimized Python rejects an injected exit-0/empty-output
  `study-missing` response, **1/1**, exit 0. Earlier responses are replayed from
  the recorded control logs; this validates runner rejection, not another binary run.
* `controls-optimized` is **incomplete**: the outer tool timed out after 120 s
  spanning two sequential control runners. The ordinary runner had completed;
  no optimized final status was written and no exit is inferred. Process inspection
  confirmed no surviving runner/fixture before the separately named complete rerun.
* Whole-symbol `*-dis.log` files retain the P3 constructors/reset/decision,
  transmitter/Jd, caller, ADI reference study/unite, data designer/noise helpers,
  equalizer and evaluator evidence. `demod-dis` and `equalizer-dis` are failed
  symbol lookups (exit 1), superseded by `caller-dis` and `equalizer-correct-dis`;
  they contain no usable instruction evidence.

Reproduce with the existing focused/observer commands, adding the new fault:

```sh
make -j1 J=1 period T=t_v90p4dperiod
python3 test/safety/test_v90p4dperiod_fixture.py build/period/t_v90p4dperiod
python3 -O test/safety/test_v90p4dperiod_fixture.py build/period/t_v90p4dperiod
DSPLIB_P4D_PERIOD_FAULT=study-missing build/period/t_v90p4dperiod # expected exit 1
make -j1 J=1 phase
python3 tools/refcheck.py
git diff --check
```

Final serial gate record:

| Gate | Result | Direct exit |
|---|---|---:|
| Focused Gentoo period (`targeted-5`) | 1 passed, 0 failed; 3045107 exact checks | 0 |
| Ordinary / optimized observer controls | 11/11 each | 0 each |
| Optimized false-clean response control | 1/1 rejected | 0 |
| Full `make -j1 J=1 phase` (`phase.log`/`phase.json`) | **385 passed, 0 failed**; structural boundary OK | 0 |
| Standalone refcheck | 14051 references, 2571 headings; 0 unresolved/pending/stale | 0 |
| `git diff --check` | no whitespace errors | 0 |

Phase was run under `nohup` with one job. It reports 2202 offset annotations,
2679 live period assertions, 336 types/171 files/1 known duplicate, 600/600
banners, partialcmp self-test 8/8 and TU attribution/order 11/11. The 7/7
vendored manifest check passes; absent upstream checkout means upstream drift
is still **not checked**. The anchor check validates 272 suites/10040 anchors;
it is structural validation, not mutation execution.

`identity.log` records these final SHA-256 identifiers; the executable agrees
with both completed observer runners' `status.json`:

```
2d14e4ff057ae55a7d1d68984c0b85d667dbe8eab6370a7e4c0a6f4d440e23b6  test/unit/t_v90p4dperiod.cpp
cee71ddf92a2489570e05c4fd103d93c0c626b1aef5142c46000c4809ce7057b  test/safety/test_v90p4dperiod_fixture.py
81ff7de58f1f12e391d67b1040835828566eefd3b0638001d9010fa26d7ca676  build/period/t_v90p4dperiod
```

Independent review remains pending. The branch remains uncommitted and
unpublished at base 6750e022; these results do not close #183.

### Independent-review follow-up: malformed mapping containment

Independent review approved the bounded study evidence and requested apparatus
hardening before publication. The follow-up remains uncommitted/unpublished and
changes only this audit, `t_v90p4dperiod.cpp` and its Python observer runner.

The study observer previously used a produced constellation byte to index
`seen[128]` and `linMapp[128]`, including a second lookup of the previous code,
without checking the code's range. That could make a malformed-output probe
exercise undefined observer behavior instead of reporting a controlled failure.

`mapping_in_range` now checks all six active constellation sizes equal eight,
and all 48 primary **and** 48 codec codes are below 128 before code-indexed
observations or decoder use. Its own scan is fixed at eight entries, independent
of the output size. The study's level/seen loop runs only after this preflight;
the previous level is cached from the validated lookup rather than indexed
again. Invalid mappings cannot reach `dm_reset`, `hardDecision` or downstream
P4D. The earlier manual/matrix/Ed boundaries also preflight before decoder use,
and the producer Ed level accessor checks its code immediately before indexing.
Valid mappings retain the same production calls and exact expected values.
This validates the fixture's eight-level shape and code domain, not every
possible malformed field of an arbitrary mapping object.

Two bounded probes, `mapping-128` and `mapping-255`, corrupt the unity-study
designer's output at **constellation[5][0] on both sides**, after design and
before any observer indexing or demapper reset. That slot is also the previous
entry for index 1 in the old loop. Each probe reports
`mapping code below 128 before use` and both sides' exact rejected coordinates;
both report `dm_reset=0 hardDecision=0 P4D=0`. Each exits **1**, with
**10/2897477 failed checks**, rather than crashing. The six blocked measurement
cycles reduce the probe's denominator to **30 cycles**, while the valid baseline
still executes all **36**. The normal and optimized Python runner uses explicit
`require` checks for the ordinary exit, named failure, both-side rejection and
blocked downstream reports, and the remaining cycle denominator.

The optional coordinate evidence was strengthened without expanding the test
domain: all six alternate-table differences are at **[phase 0..5][code 64]**,
**timed 1980 versus manual 0**, in each of the three study cases on each side.
The report contains 36 coordinate/value observations; the existing independent
six-difference count and whole-ADI comparisons remain active. These inactive
reset-seed entries stay 1980 even in the half-gain study; they are not newly
claimed alternate-RBS training results.

Artifacts use `/tmp/opencode/issue183-p3-hardening-` with the same direct-exit
JSON/full-log driver. `focused` passes **1/0**, exit 0. The new valid fixture
passes **3072611/3072611 exact checks** (27504 additional checks), with all prior
state paths, mappings, decisions and cycle expectations retained. `controls`
and `controls-optimized` each pass **13/13**, exit 0; `observers/` and
`observers-optimized/` retain the complete baseline/probe output, raw exits and
binary hash. The six older postimage-fault failure counts remain
10/10775/5/55/13/5, now against denominator 3072611. No tolerance, production
source change, modern sweep or mutation-execution sweep was introduced.

The serial full-phase/refcheck completion record follows below.
