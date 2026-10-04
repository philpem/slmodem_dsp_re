# Fixed-point atan intrinsic and signed word-use boundaries

Precompile eight complete fpm_atan.c source cells at856c1ecb: standard abs(int)
for two magnitudes × arithmetic sign masks for axis returns × signed-short
small-angle shortcut value. F2903 independently identified branchless original
CLTD/XOR/SUB and sign-mask axis calculations, explicitly left unattempted;
current short-condition magnitude and axis ternaries branch. Original small
ratio path MOVSWL AX→EDX versus retained copy EAX→EDX, even though <=126
makes behavior equal. No public formal/header change. Standard abs expresses
32-bit magnitude before existing unsigned-short truncation, valid for all input
shorts. Arithmetic sign masks use ((int)value >>31)&0x4000, matching original
SAR31/AND4000; axis-zero semantics unchanged. Retain folding/control layout and
all calls, flags and data. Audit full TU/imports/relocations/metadata. Stop on
miss; no statement-order/register/padding fits or synonym sweep.

Initial eight miss; combined421B versus409B. Independent terminal witness:
original all octant folds narrow then MOVSWL into EDX before jumping to shared
pointer store/epilogue, also used by x==0. Current separate pointer assignments
emit additional terminal blocks without MOVSWL. Predeclare four follow-up cells:
retained baseline, abs+axis+signed predecessor, common result/store boundary on
retained and on predecessor. Result is int receiving existing explicit short
fold conversions, final short pointer store; existing early axis returns route
through shared source label after setting same result. No fold predicate/order
change, no short-versus-int result variants or label position sweep.

Common-result follow-up retained460B/integer449B versus409B, no gain. Both
word sign-extension and branch merging require more than just shared final
assignment; no label/scope/return carrier permutations are justified. Close
these tested intrinsic/axis/table-use/result families. Twelve complete-TU
controls preserve all metadata, sole function's FPM_div/table relocations and
nontext data; unchanged raw baseline matches saved period object. No source
adoption or differential/runtime execution.
