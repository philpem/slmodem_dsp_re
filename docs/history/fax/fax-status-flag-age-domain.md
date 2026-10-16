# Status flag clear and source-byte age

Base9025b8d8. Two status leaves, six complete-TU cells each. Original V17
flags+14 uses register read/AND/store, then input+10 read, flags+15 clear,
then final flags+14 overwrite. Source currently uses direct memory AND for
the first clear and reads input after flags+15 write. V29 shares both gaps.
Original V27 status already captures its final byte before flags+15 and is
EXACT; leave it alone.

Cross witnessed final-byte age with three conventional representations of
the initial low-two-bit clear: direct compound mask; explicit unsigned-byte
local/read/write; two independent bit clears. No bitfield/type/header overlay,
volatile or alias coercion. Last form tests whether two source bit writes
combine into original register operation, without claiming unique spelling.
Both initial writes and the defective final overwrite remain reproduced.

All twelve cells are diagnostic unless complete strict EXACT; full-TU
bindings/data/nontext/bystanders and period differential required to adopt.
Original alias-visible input read must precede the adjacent-byte clear.
No register/slot permutations, mutation or fuzzing. Close after twelve.
