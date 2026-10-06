# Scope review: diagnostic cap graph and literal exit

After two negative domains, further result/guard/arm-order controls stop.
One final extension has two independent remaining instruction discriminators:
original case0CMP EDX,ECX is followed by MOV EDX,EBX before the conditional
branch, unlike the same-owner if assignment; original overflow arm separately
XORs EAX despite the entry zero, whereas the shared result control preserves
entry zero and lacks this instruction. The229-byte positive-arm control has
70instructions against72original; it is not merely a register renaming.

Declare five full TUs on6b4509bd: raw baseline, repeated positive-arm/control,
then conditional-value case0 cap and direct literal return0 in overflow,
separately and together. Keep case1 cap unchanged, shared default result and
its positive guard, all negative count/copy/clear/store behaviors and scalar
widths/interfaces. Conditional-value source predicts separate selected-value
construction in initial RTL; literal exit predicts independently materialized
overflow zero. Failure means closure, not extra synonyms/declarations/slots.
Full Gentoo retained flags and bugdefine last; allbody/data/metadata/relocation
audit, no source adoption unless wholefunction exact plus final period gate.
No profile, alias tricks, fuzz/mutation or source permutation sweep.
