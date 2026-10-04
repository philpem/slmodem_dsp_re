# Batch 5: calling-tone loop and load boundaries

Baseline80c5dea3, unchanged complete CallingTone.c through Gentoo saved profile,
reproduction bugs and selected assembler. Full phase deferred to final batch;
no fuzzing/mutation. No closed local CallingTone source domain found in scoped
Playbook/findings search.

Blob215B versus216. Blob clamps a copy of remaining, has on/nonzero arm first,
and reads amplitude before TONE_read. Current ternary clamp initializes from
count, zero arm first, amplitude read after TONE_read. Those are three visible
boundaries, not a guessed allocator spelling. Fully cross eight full-TU cells:
retained ternary vs end=remaining then if(end>count)end=count; retained zero-first
vs on-first source arms; retained amplitude field expression after call vs one
int amplitude read before TONE_read within the sample loop. Keep tone period
accounting bugs, rounding shifts, wrapping stores, loop counts and transition
tail intact. Prediction restores clamp/source branch geometry and the amplitude
lifetime crossing the call, allowing blob's count argument to remain on stack.
Falsifier: unchanged asserted lifetime/CFG boundaries, baseline raw mismatch,
reset bystander or metadata/data/reloc drift, or no exact candidate. Close the
family if it misses, then use first differing RTL stage for a new mechanism.


Measured8-cell cross: no hit. Combined source recovers215B and the full entry,
clamp and sample-loop geometry, remaining BYTES64. Two independent boundaries
are still visible: blob consumes/scales the saved amplitude in its register,
ours multiplies into TONE_read's return register; blob's remaining-zero branch
transitions inline with the nonzero save out of line, ours has nonzero save
inline. Before compilation declare five new complete-TU cells: raw baseline,
combined negative control, combined with in-place amplitude *=v then >>=13,
combined with explicit remaining==0 transition else save (instead of
nonzero-save/continue), and both. Keep the complete original transition stores
and loop behavior. Prediction first boundary changes product lifetime at
combine; second changes source CFG branch geometry before block placement.
Falsifier: raw controls fail or both independent boundaries change without an
exact candidate; close this family and do not vary declaration/register names.


Outcome:13 valid complete TUs,11 distinct sources/11 raw objects. Only the
combined period-if-else/in-place-amplitude source is EXACT215/215, with
ResetCallingTone still exact and raw-unchanged. Two functions, bindings/data/
allocated nontext/canonical relocation checks pass in every cell. Sixteen
selected stage records and four firing combine product-carrier checks pass;
output stores use the updated amplitude pseudo only in the in-place controls.
The production object raw-matches the measured candidate. The source restores
the observed boundaries but does not claim a unique original spelling.

Replay (preserve the historical merged production archive/config):

    python3 tools/gcc3_batch5_calling_tone_reproduce.py --domain docs/batch5-calling-tone.md --baseline-dir build/production-before
    python3 tools/gcc3_batch5_calling_tone_reproduce.py --tail --domain docs/batch5-calling-tone.md --baseline-dir build/production-before
    python3 tools/gcc3_batch5_nlencoder_reproduce.py --domain docs/batch5-v34-nlencoder.md --baseline-dir build/production-before
    python3 tools/gcc3_batch5_root_audit.py

The unchanged complete historical source must reproduce its archived object;
all tools append the reproduction-bug define after configurable flags and save
actual commands/compiler/assembler identity, source/object hashes and -da dumps.
No per-cell behavioral, mutation or fuzz harness execution. The final batch
period and structural gate is run once after integrating all five exact gains.


Integrated production batch: fresh300-object raw baseline; only CallingTone,
V8Detector and GenericToneDetector objects change and each raw-matches its
declared candidate. Exact set935→940/1852, +1,093 exact bytes, zero losses.
Same300-input recovered partial-link order: positioned matching bytes
68,679→68,664/943,398; relocation records1,026→1,029/18,317; symbol records
394/2,907 unchanged. Both whole-object comparisons remain DIFFERENT.
Function-normalized gains are not a claim of whole-object positional identity.


Final repaired batch gate: make phase J=4 passes388 period differential tests,
0 failed, and all structural/provenance checks. Static anchor check:285 suites,
10,038 anchors, no detached/non-unique/wrong-owner anchors. Reference check:
14,286 references, no unresolved/stale/live-mutant reports. Mutation metadata
was only statically retargeted; no fuzzing or mutation harness was executed.
