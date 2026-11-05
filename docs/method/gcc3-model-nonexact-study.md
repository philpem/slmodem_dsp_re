# GCC scratch-model screen of non-exact translation units

The model transferred to three additional translation units and correctly
predicted a byte-exact `FloatFIR::reset` in three source-order controls. None
of those controls preserved the original object's function order, so no
reconstruction source change was retained. The experiment establishes useful
compiler predictions and identifies the boundary of their recovery value.

This study follows the toolkit committed as `b93beb9e8`. Inputs are the
unchanged complete translation units at that revision, compiled with the
recovered Gentoo GCC 3.4.2-r2 and the production `period.mk` recipe and flags.
Each experimental baseline matched its independently built production object
literally. Capture also required ordinary, repeated and observed compiler
outputs to agree.

## Baseline coverage

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

Independent ordering evidence rejects adoption. The baseline emitted order
matches the reference's eight sized symbol groups, including constructor and
destructor variants. All three positive permutations change that order.
Cells 2 and 3 change only the reset body or its relocations; cell 5 also
changes the already non-exact scalar process. A local EXACT gain does not
justify moving the full object away from its recorded layout.

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
