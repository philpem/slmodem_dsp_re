# GCC scratch-model screen of non-exact translation units

The model transferred to three additional translation units and correctly
predicted a byte-exact `FloatFIR::reset` in three source-order controls. None
of those controls preserved the original object's emitted function order.
The body gains and layout differences are separate results; no reconstruction
source change was retained in this initial study. The experiment establishes
useful compiler predictions and leaves their adoption for source and layout
review.

This study follows the toolkit committed as `b93beb9e8`. Inputs are the
unchanged complete translation units at that revision, compiled with the
recovered Gentoo GCC 3.4.2-r2 and the production `period.mk` recipe and flags.
Each experimental baseline matched its independently built production object
literally. Capture also required ordinary, repeated and observed compiler
outputs to agree.

## Baseline coverage

This table records the initial SI-only model. The later
[HI extension](gcc3-model-search.md#add-another-mechanism) also calibrates the
complete toneiir stream; it does not change the baseline objects or grades.

| Translation unit | EXACT / shared sized functions | Scratch events | Whole-stream model |
| --- | --- | --- | --- |
| `V90Phase3Modulator.cpp` | 19/28 | 68 SI | Calibrated |
| `V92Modulator.cpp` | 25/30 | 53 SI | Calibrated |
| `FloatFIR.cpp` | 4/8 | 7 SI | Calibrated |
| `toneiir.c` | 3/8 | 10 SI, 6 HI | Unsupported HI events prevent composition |

The denominator is 74 shared sized functions across four complete TUs, of
which 23 are non-exact. The three supported streams contain 128 events.
The six HI events are retained as unsupported evidence; removing them would
lose compiler state needed to predict later choices. `_iir_filter_create`
has no observed scratch search in the captured toneiir compilation.

Among the 18 non-exact functions in the three supported TUs, 15 have observed
scratch events and 14 have more than one choice signature across hypothetical
incoming cursors. This prioritises inspection; it does not establish that
scratch choice explains their byte differences. The V.92 example below is a
counterexample to that inference.

## FloatFIR: a confirmed prediction with a layout constraint

The baseline `FloatFIR::reset` differs by four bytes. Its scratch temporary
uses ECX where the reference uses EDX. The capture and RTL associate this
choice with the taps store: input UID 58 becomes instructions 118/119, and
the register difference survives the later renaming pass.

A declared six-order family permuted three existing complete definitions:
the block process, reset and scalar process. Predictions were written before
forward compilation. Three of six model orders predicted EDX. All three were
compiled and verified across all seven scratch events, and all three made
the 61-byte reset function EXACT. None lost an existing exact function, and
all preserved the function inventory, exported bindings and allocated data.
One additional, previously predicted negative order was compiled: its seven
events also matched the model, and reset retained ECX and BYTES(4).

Thus the search examined 6/6 model configurations and the forward checks
covered four generated source variants: all three predicted positives and
one negative, with 28/28 events verified. This is not a claim that all six
source variants were compiled.

Independent ordering evidence identifies a layout tradeoff. The baseline emitted order
matches the reference's eight sized symbol groups, including constructor and
destructor variants. All three positive permutations change that order.
Cells 2 and 3 change only the reset body or its relocations; cell 5 also
changes the already non-exact scalar process. These are measured function-body
gains with a separate placement mismatch, not evidence that the unknown
original source used a particular definition order. Adoption requires weighing
both results and period differential validation; emitted order alone is not
an automatic veto.

Keeping the original order, hypothetical TU-entry cursors 2 through 14 would
also select EDX (13/53 states). The actual compiler begins at zero and emits
ECX. These states are diagnostic constraints, not an available compiler knob.
The next source-level question is the preceding block process and its scratch
history while preserving original emission order.

## V.90 phase 3: a precise target and bounded negative searches

The emitted weak `Scrambler<unsigned char, int>::reset(unsigned char)` differs
at one byte: reference `pop %ebx` versus baseline `pop %ecx` at function offset
`+0x28`. Input RTL UID 101 is a stack adjustment that peephole2 turns into an
unused pop. Its captured scratch search accounts for this difference.

Local block-entry cursors 3 through 12 select EBX. With the complete captured
TU order fixed, only hypothetical TU-entry cursor 50 selects it; the observed
entry is zero. Two declared six-order families each predicted zero target
hits. Their twelve cells are family evaluations, not twelve distinct source
programs or an exhaustive exclusion of other source histories.

Five source variants in the family that moved definitions across a local
helper failed to compile because they removed the helper's prior declaration.
They remain invalid source cells. No declarations were added to force them
through. One valid changed-order control from the other family verified all
68 event predictions and produced the predicted `pop %eax`. It gained and
lost no exact functions among 28 compared, and preserved exports and the
function inventory. Its 68 changed raw jump-table addresses resolve to the
same function-relative targets. Its emitted order also differs from the
reference, so it remains a diagnostic control.

## V.92: a mismatch before scratch selection

`Queue<float>::write(float)` has two supported scratch events, but its
SIZE(14) residual begins in frame/save construction: the reference uses stores
within a 12-byte frame where the reconstruction uses pushes and a four-byte
frame. This difference is already present in the pre-peephole RTL. Predicting
the scratch choices does not explain or repair that earlier decision.

## Follow-up: source families and earlier compiler decisions

A further FloatFIR experiment compiled 29 cells preserving the original
emitted order: 27 distinct source programs and 19 distinct raw objects.
The families tested captured history pointers and index decrements, owner-load
order, local convolution expansion, and accumulator/operand orientation.
Across 232 function reviews there were no EXACT gains or losses and no
REGALLOC gains. Some accumulator-width alternatives lack differential
validation and remain compile-only controls. A fresh observation of one
negative repeated all seven baseline scratch decisions.

The reset-first positive was also rebuilt and literally reproduced the earlier
object. Only reset's canonical body changed, from REGALLOC (raw BYTES4) to
EXACT; seven other bodies, exports and allocated data were unchanged. Baseline
and positive each passed `t_floatfir` (47,226 checks) and `t_v90prefilter`
(125,693 checks), relinked with existing period fixtures and other-TU objects.
This adds bounded behavioural evidence to the body gain while retaining the
separate emitted-order mismatch. The reset-first representative was subsequently
adopted: it recovers the reference's EDX scratch operand without altering any
algorithm or another function body. This is partial function-body recovery;
the original source definition order is not established. Final-object layout
still needs recovery, and later work on block process must recheck both its
scratch history and reset. Reports are in `build/gcc3-fir-recovery/summary.json` and
`parent-review.json` in that directory.

V.92 controls using `isFull()` or a local space value reproduce an exact
85-byte `Queue<float>::write(float)`, but change the previously exact
`V92Modulator` C2 constructor to BYTES7 and also change `progress`. Three
full-TU cells, including the unchanged production baseline, provide 90
function verdicts; exports and allocated data remain unchanged. Subsequent
review traced the collateral and retained the `isFull()` helper-use candidate;
the space-local form remains a diagnostic control.

A read-only compiler observation locates the queue frame decision before
reload. With unchanged active-instruction counts, the baseline's success-path
frequency is 9833 and its weighted instruction sum is 201683, above the
200000 threshold. Both controls lower that frequency to 9600 and the sum to
199984. GCC then selects the reference's move-save prologue instead of push
saves. The branch-prediction dump attributes the difference to combining the
opcode-nonequal and negative-return heuristics in the baseline; the controls
retain only the latter. This establishes a source-to-prologue mechanism worth
modelling separately from peephole scratch selection. Evidence is under
`build/gcc3-v92-recovery/`.

The later constructor and `progress` patterns agree through `27.flow2` and
first diverge in peephole2. Removing the queue's dead-pop search changes the
TU's event count from 53 to 52 and the constructor's incoming cursor from 0
to 2. Its seven changed bytes carry the same constants to the same argument
slots. This explains the lost match without establishing a constructor-source
defect. The [Playbook's worked example](refinement.md#queue-recovery-changes-a-later-constructors-scratch-registers)
records why the queue recovery is retained and the follow-up plan. All four
focused period binaries pass on both forms (804321 checks in 27 groups), with
the candidate's linked scalar write independently confirmed EXACT in each.

## Reproduction and artifacts

Use [the model-search guide](gcc3-model-search.md) for capture and screen
commands. The generated research artifacts are retained under:

- `build/gcc3-broaden-leaves/`: `summary.json`, frozen family predictions,
  full-TU results, emission-order evidence and the real negative control.
- `build/gcc3-broaden-cpp/`: `summary.json`, baseline results, frozen model
  families, compiler rejection logs, original/current disassembly and RTL.

These directories contain input sources, commands, hashes, capture logs and
objects. Source-order alternatives remain generated artifacts. The reusable
screen reports current evidence afresh and refuses stale captures; historical
counts in this note must not be reused as a live census after source changes.

Parent review independently repeated the 74-function baseline screen,
reverified the 28 FloatFIR and 68 phase-3 forward events, and rescored their
32 and 28 function verdicts respectively. It also checked original FloatFIR
symbol order directly from ELF symbol values. The screen's ten tests pass
normally and under `python3 -O`, including invalid captures, unsupported-event
shape checks and corrupted supported decisions within mixed-mode streams.
The full reconstruction gate, `make phase BUILD=build/gcc3-search-phase J=3`,
passed with 393 period differential tests, zero failures and successful
structural checks. The original model's 22 tests also remained green.
