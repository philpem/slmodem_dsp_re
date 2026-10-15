# Transmitter budget corrections and partial loop recovery

Baseline 1b0a3097; [declared domain](transmitter-budget-fidelity-domain.md).
Fourteen full-TU cells supply110 shared-body verdicts. All raw baselines
reproduce, including the Quiet follow-up's corrected-comparison baseline.
Commands, compiler/selected-assembler identities, headers, source/object hashes
and RTL dumps are saved under build/transmitter-budget-fidelity,
build/transmitter-v27-budget, build/partial-loop-snapshot and
build/transmitter-v27-carrier. The retained profile and bug define are unchanged.

## Fourteen signed-countdown/unsigned-budget corrections

The original compares a sign-extended positive countdown against a
zero-extended budget word. Retained source sign-extended the budget too.
For budget0x8000 and countdown48, the source selected32768 symbols while the
original selected48. Correct the comparison use, preserving the countdown,
selected-count carrier, subtraction, helper interfaces and call ordering.

The earlier ten-handler investigation was a valid negative exactness result;
its no-adoption conclusion left this behavioural discrepancy in place.
Partial fidelity is now the objective. Six V17 and four V29 handlers receive
the measured correction. A source-wide comparison scan found four analogous
V27 handlers; native MOVSWL/MOVZWL operands independently confirm the same
correction there. V21's FIFO handlers already consume unsigned budget words;
this family supplies no additional V21 edit.

| Family | Bodies | SIZE gaps before → after |
| --- | ---: | --- |
| V17 SCR1/Bridge/EQCond/AB |4|11→12 each|
| V17 TEP / Silence |2|5→5 /15→20|
| V29 SCR1/EQCond/AB/Quiet |4|11→12 /23→24 /11→12 /20→21|
| V27 SCR1/EQCond/Alt/Quiet |4|12→12 /40→38 /12→9 /21→1|

These are length gaps, not differing-byte counts. No exact gain or loss.
All other emitted bodies, complete symbol metadata, named/allocated data, BSS,
nontext relocations and direct-call inventories remain unchanged in each TU.

## Fixed behavioural evidence

Each fixture constructs separate transmitters, executes the genuine START
transition, checks the installed Quiet/Silence handler and seeded countdown,
then calls that handler with budget words0x7fff,0x8000,0xffff. No internal state
is planted. V17/V29 seed48 and use the default10/3 resampler. V27 explicitly
selects4800 bps, seeds32 and uses5/1. Separate262144-short V17/V29 outputs and
524288-short V27 outputs bound even the old incorrect65535-symbol selection;
input is unused on these no-carrier arms. Compare all output including the
unchanged tail, return, budget, countdown and the existing complete tree checks.

| Fixed component fixture | Pre-fix | Corrected |
| --- | --- | --- |
| V17 |16/138 checks fail|138/138 pass|
| V29 |14/159 checks fail|159/159 pass|
| V27 |16/174 checks fail|174/174 pass|

The pre-fix logs are retained. All other existing groups pass in those negative
runs. This establishes the component boundary, not high-bit budgets from the
public fixed-small-budget modem wrapper. No tolerance or assertion is weakened.

## Request-byte snapshots and FSK history

V29RX_control now captures ctl1 before its first child publication and ctl0
before its first conditional child clear. Original reads each request byte
once; the baseline reads each twice, including after those stores. The selected
control also reads each once, changing BYTES74→24. The entire status bystander
and metadata/data/calls remain unchanged. No arbitrary overlapping-request
lifecycle or original local-name claim is made.

V34 fskdetect now combines the third interpolation phase with its history
transfer. Capture both sample and coefficient before the history store, retain
unsigned wrap arithmetic, use the original signed-word loop counters and capture
the delayed-tap pointer before shifting the delay line. Original coefficient
read-before-transfer and corrected controls fire in the existing audit; earlier
wrong store-before-coefficient controls remain rejected. Gap231→135; the complete
fskdemodulate bystander and metadata/data/calls remain unchanged. Existing fixed
t_v29fax/t_v34fsk/t_v34hsfsk period fixtures pass3/0. No new writable-coefficient
alias history is invented. The helper remains inline as before.

## Quiet follow-up and stopping boundary

After the comparison fix, Quiet's selected-count extension and cached
subtraction still differ from the original. Four full-TU controls cross short
versus unsigned-short chosen count with cached versus compound member subtraction:
SIZE1, SIZE1, SIZE20, BYTES76. All four distinct source/object emissions preserve
nine bystanders and both exact bodies. No adoption follows. The joint control
removes selected-count extensions but still reuses an earlier member value and
moves sign extension before the initial gate; it does not recover the original
reload/read sequence. Equal size supplies no missing source statement.

CrossDataLinks remains withheld: its scalar parameter owners are stable and the
old cursor/count product does not explain the remaining first-loop/constant
initialisation boundary. The dftenergy exit domain supplies alternate CFG
spellings without an independently recovered original source statement. Existing
source/flag, AGC smoother and register-carrier domains remain closed. These
finite reviews are exhausted; reopening requires another original operand/read
boundary or a compiler-stage discriminator, not a nearby size score.

## Reproduction

```
python3 tools/transmitter_budget_fidelity_reproduce.py --domain docs/transmitter-budget-fidelity-domain.md --baseline-dir build/tx-fidelity-baseline
python3 tools/partial_loop_snapshot_reproduce.py --domain docs/transmitter-budget-fidelity-domain.md --baseline-dir build/tx-fidelity-baseline
python3 tools/transmitter_v27_budget_reproduce.py --domain docs/transmitter-budget-fidelity-domain.md --baseline-dir build/tx-fidelity-baseline
python3 tools/transmitter_v27_carrier_reproduce.py --domain docs/transmitter-budget-fidelity-domain.md
python3 tools/transmitter_budget_fidelity_audit.py
```

The carrier cross starts from the corrected comparison; its raw retained object
must therefore be the corrected V27 object. Other baselines pin1b0a3097. Historical
header drift requires explicit --historical-headers and archived --baseline-dir.
No compiler-profile or register/layout forcing, fuzzing or mutation execution.

## Final validation and limits

Final permitted period:387 requested/0 unknown,387 passed/0 failed. All three
new fixed groups pass471/471 checks; no fuzzing or mutation execution. The earlier
387/0 run before the V27 extension remains a separate checkpoint. The final run
includes all five source TUs and three updated fixtures. No portability/full-phase
claim or mutation snapshot update.

Census remains1086/1852 EXACT and120322 original exact bytes, zero exact losses;
667 SIZE/65 BYTES/29 REGALLOC/5 UNRESOLVED, ratchet green. Exactly5/300 canonical
objects change,295 raw-identical. All five selected controls raw-match production,
and all300 faithful objects raw-match canonical with the profile unchanged.
Partial text702476→702620, allocated PROGBITS918484→918628, positioned equal
69472→69559/943398. Both strict comparisons remain DIFFERENT/exit1. These
behaviour and operand recoveries add no whole-function byte identity.

The build/tx-fidelity-baseline directory archives the five original objects and
configuration for the first three replay commands. A fresh checkout needs an
independent1b0a3097 build to populate that archive; default current objects no
longer reproduce those earlier baselines. The Quiet follow-up instead uses the
corrected V27 object, as specified above. Generated objects/logs are not committed.

Structural reconstruction targets pass, including285 suites/10038 static anchors
with zero detached/nonunique/vacuous locators and no locator changes,2265 offset
annotations and14458 references/3095 finding headings. Against1b0a3097, zero
references repoint. New Python tools compile and whitespace checks pass.
