# Callprog diagnostic read boundaries declared before compile

Root allocates Callprog.c exclusive work after the small DSP controls.
Baseline93d7eee1 full retained profile/raw production TU, unchanged headers.
Never inspect excluded work. Four cells: baseline, independent no-answer
getter-before-timeout-writes diagnostic, independent blind-pause re-read at
diagnostic, both. No register/declaration/order permutations outside these
object-visible call/read boundaries. No fuzz/mutation or repeated phase runs.

CALLPROG_Dial blob has7 debug relocation sites vs ours6, but the source has
all7 strings: compiler merges refusal and final exit into one tail JMP with
selected format pointer. This is not a missing print. Blob actually gets
GetNoAnswerTimeOut again for the diagnostic BEFORE its5 timeout writes;
retained source prints the fifth cached result AFTER writes. Blob also gets
GetBlindDialPause again at its print; retained source prints the cached field.
These getter calls are externally observable at the component API, especially
when getter results change or update state; do not claim the correction is
behaviorally inert. Object strings and relocation call sequence establish
both concrete preimages. Inspect all public/static TU bodies, nontext/data,
canonical relocations and exports. Correct call boundary alone is not byte
identity; record size/complete verdict and remaining inline/allocation shapes.

## Measured result

All4 complete TU controls reproduce raw baseline and compile under the actual
retained profile. CALLPROG_Dial: baseline955B/blob1094B (SIZE139), blind re-read
978B/SIZE116, no-answer re-read1001B/SIZE93, both1021B/SIZE73. No exact gain or
loss:3/6 TU exact unchanged; only CALLPROG_Dial changes. All named objects,
metadata/bindings and data/nontext relocation targets preserved. String sections
retain the same length and every NUL-delimited string with its multiplicity;
no-answer relocation changes move only string emission order. The named function has7 diagnostic source statements, all
present; machine6 sites results from one shared tail jump, not omitted text.

Initial RTL getter sites9/10/10/11 match crossed predictions. Blob debug paths
at0x7a92b (GetNoAnswerTimeOut=30 before5 timeout assignments) and0x7a975
(GetBlindDialPause=29 at print) establish actual extra input reads. Mainline
blind timeout assignment already performs its own parameter29 call. No-answer
print changes location as well as the printed read value. An unchanged constant
getter can hide these source differences; fixed dynamic getter/call-order
coverage would discriminate them without fuzzing. Candidate is a correction,
not a complete byte preimage. Batch owner authorized adopting the two independently observed read corrections.
The initial two-read source was a hypothesis, superseded by the runtime
controls in [the callback-order follow-up](batch20-callprog-callback-order-controls.md).
Its valid Create → Dial fixture failed 10/62 strict trace checks: the static
address of the blind diagnostic cold block had been mistaken for its execution
order, and the larger-validation branch had an additional parameter-34 read.
The final source restores both original boundaries. The original baseline
fails 12/70 checks; the restored final source passes 70/70 in the deciding
period fixture. See the follow-up for the five-cell full-TU audit and preserved
negative controls. No byte-exact gain is claimed for Callprog.
