# Calibrated GCC scratch-register search

The GCC search experiment predicts scratch-register choices from captured
Gentoo GCC 3.4.2-r2 state, enumerates a declared family of function orders, and
checks the predictions against fresh full-translation-unit compilations.
Generated source and compiler artifacts stay outside `src/`.

This is the first bounded implementation of the Norcroft experiment's method
for this compiler. Its supported mechanism is `peep2_find_free_register` for
`SImode` with constraint `r`. The model holds the captured eligibility and
event sequence fixed while changing the incoming search cursor. It does not
predict general allocation, spills, scheduling, inlining or x87 stack choices.

## Run the experiment

You need Python 3, Docker access to the period image, GDB with Python support,
and a host capable of executing the image's 32-bit compiler programs. Install
the repository's Python analysis dependencies as described in the root README.
Use a fresh output directory for each run.

```sh
python3 tools/gcc3_search_model_test.py
python3 tools/gcc3_search_pilot.py --output build/gcc3-search-pilot
```

The pilot builds an isolated production control, captures its scratch events,
and calibrates a model. It writes predictions before compiling the held-out
source orders. The source alternatives permute complete existing definitions;
they are calibration controls, not proposed reconstruction changes.

Read the generated `results.json` and the individual capture reports. A model
hit means the requested scratch choices were predicted. A verified prediction
also requires unchanged mechanism inputs and agreement with actual compiler
choices. Neither result alone establishes a function or object match.

The isolated production build explicitly selects one C++ translation unit.
It supplies those source lists to `period.mk` and suppresses its three recursive
source-list queries, avoiding a scan of unrelated dependency artifacts. It
retains that makefile's compilation recipe and flags. The literal baseline
object comparison checks the experimental compilation against this control.

## Measured pilot

The 5 November 2026 run is retained in
`build/gcc3-search-pilot-20261105-v3/results.json`. It used the complete
`V90Phase4Modulator.cpp` translation unit and permuted `resetBeforRRN`,
`resetRRNSecondSection` and `recivedPartTwoSilenceRrnSUV`. The target was to
reproduce their baseline scratch choices; it was a forward-prediction test,
not a new source-recovery claim.

| Measurement | Result |
| --- | --- |
| Source orders compiled and verified | 6/6 |
| Scratch events verified per translation unit | 97/97 |
| Predicted / actual target hits | 2/6 / 2/6 |
| Distinct target-choice patterns | 4 |
| Function verdicts against the blob | 318 (53 per order) |
| Deliberate corrupt-outcome / changed-input controls refused | 2/2 |
| Model tests, also run under `python3 -O` | 22 passed |

An independent parent review replayed all 582 event predictions and rescored
all 318 function verdicts with `byteident.py`. The unchanged experimental
object matched the isolated production object literally. All six captures
preserved their ordinary compiler outputs under observation, and all source
orders preserved the function inventory and exported bindings.

The baseline had 39 EXACT functions among 53 compared. Permutations lost
between zero and two existing exact functions and gained none. Some also
reordered string data. The reports retain this collateral; the generated
sources were not adopted. Raw body differences include relocated string
offsets, so their count must not be confused with canonical function changes.

Calibration took approximately 0.020 s and the six-case search, including
model validation, 0.021 s. The six ordinary initial compilations took 3.396 s
inside the container, or 4.655 s including their Docker launches. The full
capture commands, including repeated compilation, GDB observation and
reassembly, took 29.166 s. These are separate costs from one small run, not a
general speed-up estimate. A larger search must amortise capture and validation
costs before it offers a practical saving.

The reconstruction gate also passed:
`make phase BUILD=build/gcc3-search-phase J=3` reported 393 period differential
tests passed, zero failed, and successful structural checks. The isolated
`BUILD` directory avoids scanning unrelated dependency artifacts. The new
model controls are run separately by the first command above.

## Capture and search another declared family

`gcc3_search_capture.py` compiles one complete source file using a saved period
build configuration. It records tool and input identities, observes the actual
compiler, and requires the observed assembly and object to match the ordinary
compilation. Its observer reads compiler state without changing that state.

```sh
python3 tools/gcc3_search_capture.py \
    --source src/pump/v90/V90Phase4Modulator.cpp \
    --output build/gcc3-search-capture
python3 tools/gcc3_search_model.py calibrate \
    build/gcc3-search-capture/scratch-observe.json \
    --output build/gcc3-search-capture/model.json
python3 tools/gcc3_search_model.py search \
    build/gcc3-search-capture/model.json family.json --output search.json
```

Create `family.json` using block IDs from the model's `baseline_order`. For
example, this shape permutes two blocks and requests hard register 0 at the
first event of a third block; replace the illustrative IDs with captured IDs:

```json
{
  "groups": [["first_symbol#1", "second_symbol#1"]],
  "targets": [{"block": "target_symbol#1", "event": 0, "selected": 0}],
  "limit": 100000
}
```

Register numbers are GCC hard-register numbers, not source variable names.
Selecting the original object's required register for an event needs a
separate instruction-level correspondence. The pilot deliberately uses
known compiler outcomes to test the apparatus first.

`gcc3_search_model.py` supplies calibration, search and verification commands.
Use `--help` on each subcommand for its JSON arguments. A family specifies
groups of captured function blocks whose positions may be permuted, plus the
desired register choice at explicitly selected events. Unlisted blocks keep
their positions. The model enumerates the Cartesian product of the groups and
reports the total domain, examined configurations, matches and completion
status. A limited search cannot establish that the whole family has no match.
The default limit is 100,000 configurations. Reports retain at most 10,000
matching orders, count all matches examined, and explicitly flag truncation.

The source-to-emission-order relationship must be checked by recompilation.
Changing declaration order can affect inlining or other earlier passes; a
model cannot establish that a source permutation merely reorders its captured
blocks. Verification rejects changed event counts, eligibility or observed
block order. Such a result invalidates that prediction's assumptions; it is
not evidence that the source alternative cannot match the reference object.

## Evidence and acceptance

The calibration checks every captured event, including cursor continuity and
the selected register. Unsupported events prevent compositional prediction;
they cannot be skipped while carrying state through the translation unit.
The tests include deliberately corrupted outcomes and changed mechanism
inputs, so a clean result has a known negative control.

The pilot reports complete-TU function comparisons through `byteident.py`,
including gains, losses and changed bodies, alongside symbol-binding checks.
Compiler capture and model search have separate timing records. Model timing
does not include the cost of collecting a trace, and timing a traced compiler
does not measure ordinary compilation throughput.

Any proposed reconstruction edit still needs independent source evidence,
complete-TU review and `make phase`. Function identity uses `byteident.py`;
project completion requires `make partial-compare-exact`. A predicted register
assignment never replaces either gate. See [experiment design](experiment-design.md)
for controls and stopping rules.

## Extending the model

Add another compiler mechanism only after its unchanged decisions can be
replayed and checked on held-out source variants. Integer allocation will
need its own captured constraints, preferences, ordering and spill boundary;
the Norcroft allocation algorithm does not describe GCC's implementation.
Keep unsupported inputs explicit, pin the compiler identity, and preserve
forward compilation as the authority for every shortlisted candidate.
