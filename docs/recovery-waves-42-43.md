# Waves 42–43: compiler causality and generated INFO0 history

Base: `9d7799b8e7337a1ca33049286a1d8176a40be64d` (PR #417). Issue
[#409](https://github.com/philpem/slmodem_dsp_re/issues/409) records the screen,
GCC-first constraint, single unsigned-expression control and fixture promotion.
No source-order or compiler-option search was performed. Astra was not used.

## Wave 42: explain the candidate before changing its source

Twelve short native/current pairs were freshly screened; four were selected:
`V8GetMessage`, `V8SetMessage`, `check_for_valid` and `Queue<float>::read`.
The remaining eight are not claimed explained. Nine unchanged full-TU replays
across three TUs reproduce production raw objects, covering 60 sized-function
records. The diagnostic packets preserve actual Gentoo GCC 3.4.2-r2 commands,
selected assembler/linker identity, preprocessor inputs, source patches and hashes.
One invalid driver-path invocation is preserved and excluded.

The causal findings concern the candidate compiler, not unavailable original RTL:

| Target | Measured cause or stopping point |
|---|---|
| `V8GetMessage` | Error-result hoisting occurs in `ce2`; the separate result spill appears in `greg`. Inspected return boundaries agree. |
| `V8SetMessage` | Helper-result common return already appears during tree inlining. Before reload, pointer motion in `ce2` is blocked by flags-register interference. Reload splits arithmetic into LEA, removes that interference, and permits `ce3` motion. Switch expansion separately depends on `TREE_UNSIGNED` and type bounds. |
| `check_for_valid` | `ce1` materialises the Boolean using the traced zero/one definitions and store-flag expression. |
| `Queue<float>::read` | Availability is already evaluated before the guard in initial expansion; `ce2` hoists the error result and `bbro` duplicates a shared-zero tail. Native post-guard placement still lacks an original-source explanation. |

Fifteen logged legality decisions and 128 Boolean-model routes, with positive
and corrupted-branch controls, support the bounded mechanisms. These do not
recover original factoring or justify moving statements until bytes improve.

### One source control licensed by switch expansion

Native `V8SetMessage` uses `cmp 1 / je / jae` and does not retest zero in its
bounded lower selector arm. GCC `stmt.c` uses expression unsignedness and type
bounds to choose that partition and omit the zero test. This supplies a specific
prediction for one control: cast only the `selected_sequence` switch expression
to `unsigned int`, retaining both public/helper `int` signatures and all cases.

Exactly **two cells / 14 function verdicts** compile. The unchanged object is
raw-identical to production. First RTL changes to unsigned `LTU` and omits the
bounded zero comparison; final assembly has the predicted native-style prefix.
Both predictions pass. The cast is retained as partial dispatch recovery, not
proof of the original public argument type, enum/cast spelling or helper boundary.

`V8SetMessage` changes **412→403 bytes**, versus native **392**, still non-exact.
The helper-null pointer join remains unresolved. Other **six functions are
raw-unchanged**. All 27 symbol identities/bindings and 133 relocation targets/
addends remain; 14 text-relocation positions move within the target. Data payloads
and TU exact/grade-0-or-1 membership remain unchanged at two of seven functions.
Parent production compilation reproduces all seven reviewed canonical bodies.

Whole exact and grade-0-or-1 gain/loss sets are empty; membership remains
**1197/1852**. Partial-link comparison stays **DIFFERENT**, raw-different:
positioned matches move **76779→76774 / 943398**, with candidate text 711415 bytes.
Relocation counts stay 18301 candidate and 1160/18317 exact-position matches;
those counts do not assert record identity.

## Wave 43: generated INFO0 acceptance and CRC rejection

`t_v34diag::run_info0_history` creates real caller and answerer graphs through
registered V.34 operations, then exchanges their generated samples through a
fixed delay channel. Current output/input buffers, return values, a selected
pointer-free state projection and named events are compared. No bits, decoder
state, counters or callbacks are planted. The projection is not the whole heap.

The single supported recipe is id 34, codec 4, rates 2400–33600, delay 216, sample
rate 9600, fragment 48 and caller selectors 1/0. Each process call handles four
samples through a 288-sample peer delay. The scope denominator is **one recipe ×
two external-input cases × two implementations × two endpoints**.

### Passing maintained cases

The positive case natively generates 17 payload bits (`0x1ff08`) plus 16 CRC
bits. An independent MSB-first CRC calculation, polynomial `0x1021` and initial
`0xffff`, yields **`0x8fd9`**, agreeing with producer and receiver. Both sides
complete `TX_DPSK→TONE_AB`; at process step 445 the caller accepts INFO0 and moves
`DET_INFO→RX_PHASE1_CALL`. The answerer enters its corresponding phase-1 transmit
state. This is generated component/channel evidence, not telephone interoperation.

The rejection case erases **84 external caller-input samples** at steps 390–410,
after payload completion. Native CRC rejection restarts detection at step 442;
this expected rejection is a passing normal-suite case. After reset the final
received CRC bit is unobserved. The preceding 15 known bits constrain it to
`0x8ffc` or `0x8ffd`, both distinct from `0x8fd9`; the fixture does not invent
the missing bit.

The maintained fixture adds **7808 assertions**, preserving the previous 47561,
for **55369 total**. It executes **eight graphs/eight runtimes, 3556 process calls,
14224 output samples and 34 registration/lifecycle calls** (3590 explicit calls).
Those are maintained counts; earlier exploratory assertion and registration
counts are superseded. Both cases stop at the INFO0 prefix: no INFO1, progress 1/2,
live status 2/3 or selector-8 coverage is claimed.

An observer-only control flips one bit of the copied accepted CRC on the
reconstructed side. It fails **4/7808 checks**: three named CRC assertions and
an event-record comparison. It changes no live state or channel input. Restoring
the fixture byte-for-byte returns the focused period test to green. The external
erasure separately demonstrates actual CRC rejection, not merely observer
sensitivity. All older fixture groups remain unchanged.

## Validation and next steps

Independent review checked GCC/native witnesses, the two-cell control and the
constructed peer history. Parent separately inspected the fixture and verified
final production bodies and whole-census membership. Focused `t_v8util` and
`t_v34diag` each pass **1/0**; final `make phase J=8` passes **394/0**, with all
structural checks passing. No new binary is registered. Period flags include
`DSPLIB_REPRODUCE_BUGS`; no global flag, tolerance or comparator change is made.

1. Continue this exact INFO0 history only to the first caller
   `RX_PHASE1_CALL→TX_PHASE1_CALL` edge. Observe the native counter>95 and received
   halfword==1 predicates and peer tone progression before proposing another
   fixture extension. Do not jump directly to status 2/3 or plant progression.
2. For V8's remaining helper-null join, trace the candidate expansion/threading
   limitation against native selector rejection before another source control.
   No code-structure, case-order or helper-removal palette is justified.
3. Preserve the other three causal results as explanations with explicit unknowns.
   If another original-source cause remains unknown, investigate GCC rather than
   trying nearby source forms. A new control needs a measured rule and prediction.

Evidence under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/` includes
`w42-residues/REPORT.md`, per-target ledgers, `unsigned-dispatch/`, parent
verification/partial/census logs, `w43-info-history/maintained/notes.md`, control
and restored runs, native transcripts, and `w42-43-phase.log`. Initial apparatus
and incorrect-oracle attempts remain labelled and excluded. The handover records
delivery and the limits of each conclusion.
