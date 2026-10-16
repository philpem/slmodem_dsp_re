# FixedRC generic polyphase convolution

Baseline `a5f306a3`, retaining the previous iteration's pair and circular-wrap
owner recovery. This is the generic runtime-tap Q14 convolution in kind zero,
not a per-mode specialization and not a circular kind-one loop.

Original `0xb140d..0xb1432` starts with an unsigned entry guard against the
sign-extended short tap count, then advances coefficient/history pointers by
two and decrements the remaining count to zero. Retained native code at
`0x683..0x6c5` instead indexes both arrays, increments an index and uses signed
JG. The helper's source declares a signed index. The original addressing,
loop direction and unsigned guard provide distinct witnesses. Valid
constructor-created tap counts are positive; no negative-count reachability
or fixture is inferred from that unsigned instruction.

Declare four complete-TU cells crossing unsigned countdown with advancing
coefficient/history pointers. Countdown-only computes an ascending index
from the saved count; pointer-only retains the existing signed loop bound.
The combined cell has one unsigned remaining counter and two forward
cursors. Preserve tap/phase types, coefficient order, accumulator width,
Q14 narrowing, all other helpers and every conversion mode.

Prediction: unsigned countdown restores the original unsigned entry/zero
backedge; cursors restore the two unit-step address producers. Reproduce the
unchanged full TU raw, then inspect each graph independently of size. No
new flags, register/declaration choices or nearby loop spelling search.
Full-TU data, binding, relocation and exact-name audits precede adoption;
run the deciding period fixture before a source commit.

## First result and final counter/induction control

All four cells compile; the unchanged control raw-reproduces its baseline.
There are no exact gains/losses.
Pointer-only keeps the same SIZE263 grade but changes the body; unsigned countdown-only
is SIZE235, combined SIZE280. The combined form has the intended two forward
address producers but its postfix condition decrements before the body and
exits on -1. Original decrements after accumulation and exits on zero.
That counter-value epoch is not recovered, so none of these cells is adopted.

Declare one final four-cell source cross. The first axis uses unsigned
remaining, tests nonzero before the iteration and decrements at the end,
with the witnessed forward pointers. It must restore the original zero
guard/backedge independently of the byte count.

The second axis tests a different producer for the circular wrap: a signed
integer history index that descends position..0, then carries its terminal
-1 into `index += count` before the second range. The earlier indexed source
reset that index to `count - 1`, and the retained pointer source carries a
one-element-biased predecrement pointer. This control preserves the terminal
owner using legal integer arithmetic; no before-array C pointer is formed.
On valid positions 0..count-1 it reads exactly the original two ranges.
Prediction: induction lowering may produce the original post-load decrement
and terminal-pointer wrap from this carried index. Original machine pointer
bias alone does not prove this was the author's source.

Both are operation/producer hypotheses, not counter-name, declaration-order
or register choices. Keep the aggregate carrier and every other operation
fixed. The unchanged control remains `a5f306a3`. After these four cells,
close the source family if the predicted native relationships remain absent;
do not add a spelling palette.

The second four cells also miss: SIZE263/369/280/385 for unchanged,
carried circular index, corrected generic countdown, and both. The generic
counter now exits at zero, but its entry is TEST/JE rather than the original
unsigned CMP/JAE. Carried integer induction fails its native-address
prediction and is not adopted.

## Hypothesis review: width versus direction

The opening domain coupled unsigned width with an explicit source countdown.
That conflates a proved operand property with a machine loop direction that
the compiler may itself introduce. The original entry CMP/JAE is also the
natural condition of an unsigned increasing index against the signed-short
tap count. Current source's signed index instead produces JG. No prior cell
isolated this smallest width change.

Declare four final cells crossing **only** signed versus unsigned index with
indexed versus advancing-pointer reads in the original increasing `for`
loop. This isolates frontend width from later induction lowering under the
same profile. Save the initial RTL and loop dumps; predict the unsigned
entry graph independently of whether the compiler introduces a countdown.
No other width, bound, direction or circular-loop change is included. This
closes the type/induction hypothesis after its complete crossed control.

## Closed result

The final four cells have no exact gains/losses: SIZE263/263/263/264 for
baseline, pointer-only, unsigned-only and both. The combined cell restores
the unsigned entry predicate (CMP0/JBE instead of signed JG), two unit-step
loads and DEC/JNZ. The original uses an equivalent accumulator-zero CMP/JAE
guard with different registers. The source's increasing unsigned index is
lowered to a countdown by the compiler; the earlier explicit countdown
controls had obscured that distinction.

Retain the unsigned increasing index and forward cursors for their measured
operand/address graph, not the size change. All other new controls are
declined. The circular carried-index alternative does not restore the
original native cursor graph and is not retained; the valid predecrement
implementation and terminal-owner recovery remain unchanged.

The three new domains compile 12 full TUs and audit 60 shared-body verdicts,
with three raw unchanged controls. Together with the preceding pair/owner
domains, `recovery_fixedrc_pair_audit.py --polyphase` checks 24 cells/120
verdicts, every data/symbol/nontext-relocation record and the packed-carrier
positive/negative controls. Only Resample changes and neither prior exact
name is lost. The selected independent object is
`0d4c2090a33bb832b6b610b796ef17bd6fb7896f57d159d367acf11a6bd3a800`.

Replay the three runners with `--domain
docs/recovery-fixedrc-polyphase-loop.md --baseline-dir ARCHIVED_A5F306A3_OBJECTS`.
The complete commands, profiles, dumps, object hashes and body verdicts are
under `build/recovery-fixedrc-{polyphase-loop,counter-epoch,index-width}/`.
The verified `a5f306a3` replay inputs are also frozen at
`build/recovery-packers-next/baseline-a5f306a3/`.
Close these width, direction and carried-index domains; remaining original
private homes and scheduling need a new source/caller/compiler witness.

The final selected canonical object raw-matches that independent candidate.
Deciding `make period T=t_rcresample J=3` passes 1/0 after the final source
change; the complete integrated gate/census is recorded in the campaign report.
