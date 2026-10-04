# Fixed-point AGC word counted cursors

Precompile eight full-TU fpm_agc.c cells at856c1ecb: unsigned-short decrement
sample counter/pointer traversal in both clearing and applying loops × short
outer block countdown × explicit signed-word gain/level gate comparisons.
Original sample loops use len-1, unsigned MOVZWL counter and INC AX/JNE check;
retained ascending int indexed loops. Original outer loop uses MOVSWL after
DEC, TEST SI sign; retained int decrements without narrowing after initial
cast. Original level/gain gating CMP CX / TEST BP vs retained full-width tests.
Use word-at-comparison casts rather than narrow level_est owner because its
first smoothing product intentionally remains full-width until sum. No
cfg/state declaration/store order changes, no flags/loop-unrolling controls.
Counter domains len0..65535 and outer initial signed-short range preserve
retained loop termination; unsigned postdecrement wraps only on exit unused.
All sample reads/writes preserve pointer progression and same end pointer.
Audit every bystander, data, metadata and relocations, raw retained control.
Stop this domain on miss; no predicate algebra/scope/permutation follow-ups.

Eight controls miss:556B baseline,561B outer-short only,639B combined versus566B.
Word casts cause branch expansion in each gate conjunct, whereas original
independently forms both boolean operands SETL/SETE then TEST. Independent
connector witness permits a separately declared three-cell discriminator:
retained baseline, gate-word predecessor, predecessor with '&' (not '&&') in
both inner pure boolean conjuncts. Top logical OR remains, matching original
first-conjunct taken branch. No loads/calls added to expressions, so eager
operand evaluation has identical observable behavior. Original if-conversion
remains competing preimage; no unique bitwise-source claim or adoption bysize.

Eager-inner control556B vs566B, no gain. Current logical condition compiler
if-conversion is a competing explanation of original eager boolean sequence;
source bitwise connective cannot be uniquely established. Eleven full TUs
preserve three exact bystanders, all data/nontext/bindings/relocations; raw
baselines match saved retained period objects. Known-source trace fires on
five controls × four RTL stages (20 stage cases), showing expansion's added
sign-extension for short outer decrement and AND operations for explicit
eager gate. No source adoption or runtime/mutation/harness execution.
