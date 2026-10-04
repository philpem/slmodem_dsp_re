# SDM original postincrement mask boundary

Raw902 plus compound-XOR × postincrement-mask four cells (baseline included).
The prior compound control makes its first store disappear after GCC forwards
the mask. Blob instead copies the current pointer to EBX, advances EDI, then
loads/masks/stores through EBX. This witnesses the standard compound idiom
`*data++ &= mask` rather than separate mask and increment statements. Cross
with the original compound-XOR hypothesis, preserving unsigned-short saved
input and every declaration. No arbitrary order/type/flag expansion. Complete
TU rawbaseline plus metadata/nontext/reloc/bystander audit.

Measured result: 4cells best compound/postincrement168B/SIZE1; no gain. All thirteen SDM TUs pass complete data/nontext/relocation/metadata/bystander audit; only descrambler changes, zero exact losses. No adoption.
