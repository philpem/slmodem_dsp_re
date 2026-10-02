# Floating RMS and silence constructor: closed negative controls

Baseline d9a21365,866/1852 exact functions and84,159 exact bytes. The refreshed
small-function screen covers300 objects,493 shared symbols after its existing
path exclusions, and154 eligible non-exact functions. Eligibility bounds size;
it does not establish recoverability. This pass examines two independent
source questions. No production source, fixtures or compiler profile change.

## Floating RMS reciprocal type and evaluation boundary

Reference fComputeRMSValueFloatBuf is82 bytes. It divides the sum by unsigned
count after the first indexed loop, retains fld1 across the variance loop,
converts count again and performs the final reciprocal/product in registers.
Retained86-byte source uses a memory reciprocal constant in the tail.

The first two-cell domain changes only reciprocal1.0f to1.0, retaining its
(float)n denominator and the arithmetic tree. Baseline raw-reproduces. The
92-byte candidate produces fld1/register division in the tail, but adds a
float store/reload at return: the expression's product is now double-typed.
It does not reproduce the reference, and no source change is retained.

This new type-boundary observation motivates a second domain: baseline and
two named float reciprocal locals assigned immediately after mean=sum/n,
with numerator1.0f or1.0. Both named forms yield the same complete68-byte object
body for the target, recover fld1 before the loop and avoid final narrowing.
They compute both divisions before the variance loop and convert count only
once. The reference computes its final division afterward and converts count
twice. Shorter code and the right fld1 are insufficient evidence of recovery.

Five valid compile cells over two staged domains include two raw controls:
4 distinct source forms,3 distinct emissions. Every cell preserves23 function
symbols/22 globals and8/23 exact bodies; no exact gain or loss. Only the RMS
function changes instructions. CrossDataLinks, FDSP_DP_Run and bSearchEnergy
also change canonical bodies through shifted .rodata.cst4 addends: removing
the float1.0 pool word moves their constants back4 bytes. Full disassembly
and pool bytes confirm the referenced values/instructions stay the same.
These collateral changes are reported rather than hidden by unchanged scores.

[Literal domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945565054),
[typed-local discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945577945).
Replay tools/playbook_float_rms_literal.py and tools/playbook_float_rms_scale.py
with --domain URL; artifacts use matching names under build/.

Both finite domains are closed. No loop-cursor/countdown change is justified:
the reference loops are indexed. Do not permute initialization/declaration
order to try to hold fld1. Reopening requires a source/pass discriminator for
the retained constant and two separated count conversions. Candidate runtime
validation and partial links were NOT RUN: no candidate was adopted.

## Silence constructor common pointer return

Reference silence_create82B and retained80B both check allocation failure
and initialize the same fields. The proven Dual_TONE_create common-return
source family supplies an independent two-cell control, avoiding guesses
about field-store or register order: baseline and allocation followed by
successful initialization guarded by s!=0, then unconditional return s.

Both cells preserve5 functions/globals,3/5 exact; only silence_create changes.
Candidate76B/SIZE6 is non-exact, no gain or loss. The raw complete baseline
reproduces. The family is closed for this constructor; no source change.
Do not infer that the successful Dual_TONE pattern recovers every constructor.
[Domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945597387).
Replay tools/playbook_silence_create.py --domain URL; artifacts under
build/playbook-silence-create. Runtime/partial-link candidate gates NOT RUN.

## Shared controls

All7 valid complete-TU cells execute Gentoo GCC3.4.2-r2 and its selected
assembler2.15.92.0.2, retain full saved .build-config flags and mandatory
DSPLIB_REPRODUCE_BUGS via shared experiment helpers. All3 raw unchanged
controls reproduce. Records retain source/header/command/object hashes,
inventories, canonical scores and changed-body disassembly. The production
exact-name set remains unchanged; no fuzzing or mutation execution.
