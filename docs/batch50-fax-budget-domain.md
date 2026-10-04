# Ten transmitter countdown budget comparisons

902f47fa retained profile. Independent original operand witness in six V17
handlers SCR1/Bridge/EQCond/AB/TEP/Silence and four V29 SCR1/EQCond/AB/Quiet:
blob MOVSWL remaining and MOVZWL budget followed by 32-bit signed comparison.
Current source explicitly casts budget to short and compares signed words.
For high-bit budget, these differ: positive remaining is less than the
original large unsigned budget; reconstructed negative budget wins instead.
Preserve this discrepancy as original fidelity, not an invalid-input excuse.

Two independent source axes: unsigned-at-use budget min operand; unsigned
short countdown carrier with signed narrowing at its <=0 and min comparison
uses. Four cells per TU, all other statements/calls/store ordering/widths
fixed. Carrier-only retains baseline semantics; unsigned-budget restores
original observed comparison semantics over the whole word domain. Stop
at this bounded family, no local/cursor permutations. If a target closes,
replay individual and combined winners including previously recovered idle
state, full-TU exports/data/relocations/bystanders, before source adoption.
Major period fixture gate belongs to root endbatch; audit any failure's
reachable operand boundary and do not weaken its verdict.

First four cells per TU gave no exact gains/losses. New independent witness
at TxHdxTEP_V17: blob reloads countdown after choosing n before subtraction;
source subtracts from cached remaining. Cross exactly one compound member
decrement (`prm->countdown -= n`) with the four retained cells, extending
to eight cells per TU. No other expression/statement permutations.
