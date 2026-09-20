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

## Implemented input boundary and coverage

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

## Remaining limits and next discriminating work

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
