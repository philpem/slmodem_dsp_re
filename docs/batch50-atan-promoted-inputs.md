# FPM atan promoted signed input carriers

Declared complete-TU three cells: raw902 baseline; signed int working copies
of unchanged short x/y arguments; that input promotion plus independently
signed-short small-angle shortcut conversion. Blob axes use full-register
sign masks and absolute values cltd/xor/sub rather than source's short tests/
branches (historical F2903 records witness, never a compiled local-carrier
control). Blob ratio shortcut movswl ax,edx (not zero-extended SI copy) before
small/table join. Inputs/return-pointer ABI unchanged, table/ratio/wrap semantics
retained: shortcut ratio<=126 so signed conversion equal. No explicit bitmask
abs rewrite, declaration reordering or final-store padding. Full retained
profile/rawbaseline/bugdefine/selectedassembler, complete TU audit and RTL.
Stop three-cell family on miss. Parent period batch gate; no fuzz/mutation.

Result: three valid TUs remain0/1exact. Int carriers leaveSIZE11; additional short shortcut leavesSIZE6. No source adoption; no signature/ABI or table changes.
