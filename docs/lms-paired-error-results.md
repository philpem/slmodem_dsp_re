# Call-boundary screen and paired LMS error recovery

Baseline: 4de483e2, PR #284, fixed Gentoo GCC3.4.2-r2 profile with
DSPLIB_REPRODUCE_BUGS. Master remains736cf61c; issue #22's latest update was
2026-10-04. This pass does not reopen its option/profile domains.

## Screening and refusal controls

`tools/narrow_call_boundary_screen.py` disassembles complete objects once and
compares eight-instruction windows before calls only in bodies with identical
ordered named-PC32-call sequences. It nominates extra register-register byte
or word extensions (including CWTL). It does not establish argument ownership,
block correspondence, formal widths, or the absence of extensions elsewhere.
Indirect/unrelocated calls are outside its matching sequence.

Measured denominator:300 objects/1934 emitted bodies;1886 have an original
name,1744 have equal named-call sequences,142 differ,48 lack an original.
The1744 contribute4671 matched calls. There are62 nominations in39 bodies.
These are per-object emitted bodies, including repeated weak/inline symbols,
not the1852-symbol strict census. Hashes, configuration, original and candidate
windows are in `build/narrow-call-boundary-screen.json`.

The historical AGC length-only experiment is the live positive: full chosen
length plus narrow RMS formal adds an extension. Identity is the negative.
The first attempted positive used the earlier production object, whose chosen
length was already narrow; it correctly did not fire and the run aborted
before writing a report. It supplies no screening conclusion. The positive
was corrected to the independently recorded length-only control, rather than
loosening the assertion.

Manual reviews distinguish four nominations:

| Body / nominated target | Original evidence | Conclusion |
| --- | --- | --- |
| FPM_MTD_detect / FPM_iir_filt | a923f already extends AX, just outside the window | Scheduling/window false positive |
| FPM_TONE_detect / FPM_iir_filt_II | Converted energy is not a filter argument | Unrelated value; no prototype evidence |
| settxlevel / bitreverse | Argument is shifted/masked to0..7; extension belongs to another value | Unrelated value; no prototype evidence |
| FPM_FSE_receive / second FPM_lmsupd | Both original errors extended before first call | Paired-value lifetime witness |

The other35 nominated bodies are not classified by this pass. Existing closed
count/prototype domains are not reopened merely because this screen fires.

## Paired errors: three cells,12 shared verdicts

Original FSE computes both scaled errors at a8444/a844a, narrows at
a845c/a8464, then calls at a8477 and a8497. Between calls it only transfers
the saved second error and prepares arguments. Retained source computes the
second error after the first call. Thus the original does not justify widening
the exact LMS helper's error argument; it supplies a different lifetime.

The predeclared domain tests unchanged source, two short locals computed before
both calls, and rescaling the two existing error carriers before both calls.
Both candidates produce raw-identical complete objects. Adopt the explicit
locals: they separate scaled update arguments from the residual error values.
No unique original variable names or declarations are claimed.

Initial RTL contains one multiply and one arithmetic shift between calls in
baseline, zero in each paired control. Native output likewise removes the
inter-call multiply/shift/narrowing, agreeing with original. The boundary is
already different before allocation, so this instance is not a register-only
residual. Other allocation, arithmetic and graph differences remain.

Complete-TU raw baseline reproduces. All3 cells pass symbol metadata, named
objects, allocated data, relocation and ordered call-target audits; only
FPM_FSE_receive changes. Its body shrinks2166→2157 bytes versus original2131 (SIZE35→26), with no exact gain or
loss among the4 shared bodies. The exact standalone LMS helper is untouched.

## Production validation and limits

The selected production object is raw-identical to its experiment cell. Only
FSE changes among300 canonical objects;299 raw objects remain identical, and
all300 faithful-link inputs raw-match canonical objects. Configuration is
unchanged. Static mutation locators for rounding and the Q-error source are
retargeted to the same operations; no mutation execution, fuzzing or snapshot
update is performed.

Partial-link `.text` falls702396→702380 and allocated PROGBITS
918404→918388. Positioned equal bytes fall69558→69540/943398; strict partial
comparison stays DIFFERENT/exit1. Local source/dataflow improvement does not
establish whole-object layout or byte-exact improvement.

Final `make phase J=8` exits0:388 period tests passed/0 failed and285
suites/10038 static anchors green. The existing FSE receive fixture includes
8824 LMS-gate checks. Census remains1082/1852 EXACT/119759 original exact
bytes,669 SIZE/65 BYTES/31 REGALLOC/5 UNRESOLVED; ratchet green. No new
fixture, tolerance or modern-portability claim. Finding F11976 records this
checkpoint.

## Replay

From a populated period build at the declared baseline:

```sh
python3 tools/narrow_call_boundary_screen.py
python3 tools/gcc3_lms_paired_error.py --domain docs/lms-paired-error-domain.md
python3 tools/gcc3_lms_paired_error_audit.py
make phase J=8
python3 tools/toolchain/byteident.py --ratchet --json-out build/lms-paired-error-census.json
```

The screen requires the historical positive object. Recreate it with
`tools/gcc3_agc_rms_boundary.py` and `docs/agc-predicate-domain.md`, following
the archived baseline instructions in `docs/agc-predicate-results.md`, or pass
that object's path with `--positive-object`. The experiment uses its declared
source revision and requires matching historical headers and archived objects
when replayed after later source/header changes. No baseline cache bypass.

## Ranked next steps

1. Inspect other matched repeated-helper pairs for values originally captured
   before the first opaque call. Require def/use evidence, not a missing
   extension in a fixed window. Preserve arithmetic/narrowing and helper ABI.
2. Review the remaining screen nominations against original caller arguments
   and callee consumption, deduplicating already-closed domains. Full-formal
   versus narrow-local distinctions remain useful only with an independent
   original boundary, as RMS demonstrated.
3. For AGC, seek a new original value lifetime or helper/caller witness before
   further source experiments. Its previous predicate/count/guard matrices
   remain closed. Do not chase its one-byte size gap or the IIR/LMS exact
   helper through spelling permutations.
