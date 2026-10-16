# FSE_getdiag: bounded result/control domains remain negative

Base6b4509bd, full fpm_fse.c/Gentoo retained flags and headers. F11593's
positive case1 loop guard is a repeated control, not reopened alone.

The independent entry XOR EAX before dispatch motivates a common zero-default
result. Four result×guard cells emit213/217/226/231B against229original.
They recover the entry carrier but neither axis nor their cross is exact.

Two further original facts motivate a final selection/arm cross: original
uncapped diagnostic count is copied to a selected-count carrier before the
max branch; ordinary count passes through on the overflow-test fallthrough.
Five cells include raw baseline, repeated result/guard, early selected result,
positive ordinary-count arm, and both. Verdicts SIZE16/2/43,BYTES159,SIZE57.
The229-byte positive-arm body has70nonpadding instructions against72original;
equal size is not just a register-renaming result.

After the mandatory two-negative scope review, a final independently grounded
cap/exit domain separates conditional-value case0 capping and original
overflow's extra literal-zero exit. Case1 capping stays fixed. Five cells
including raw baseline yield SIZE16,BYTES159,SIZE6,BYTES151,BYTES163. The final
229B conditional+literal-exit body has71instructions, not72. No adjacent
source/declaration/slot/type/permutation family is proposed. These domains
close, not every possible original source expression.

Fourteen valid full TUs/56live function grades, three raw baseline repeats,
two cross-domain raw repeats and eleven known changed-body controls. Only
FSE_getdiag changes; exact1/4 unchanged, zero gains/losses. Metadata/bindings/
imports/exports, allocated nontext, BSS and symbolic nontext relocations remain
identical. Signed negative selected counts continue to be returned, not silently
zeroed; copy/clear asymmetries stay as in the original. No source adoption,
runtime/fuzz/mutation or modern portability claim. An initial generator assertion
failed before compilation; it contributed no cell and was fixed before running
the declared controls.

Replay tools/fse_getdiag_{result,selection,cap_exit}_reproduce.py with the
corresponding domain documents; audit tools/fse_getdiag_domains_audit.py.
All complete commands/selected compiler and assembler/hashes/fullRTL and
known detector outputs are preserved under build/fse-getdiag-*.
