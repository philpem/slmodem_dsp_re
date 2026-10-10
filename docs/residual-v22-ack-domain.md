# V.22 ACK detector square and verdict lifetimes

Pinned baseline df4b23b9, retained Gentoo profile/current headers. Original
Detect_Rmloop2_ACK209B computes ideal*ideal before the first sample loop, saves
that product, and initializes the unsigned-short result after both loops. The
retained source repeats ideal*ideal in the second loop (compiler hoists only
there) and declares the initialized result before both loops.

Four complete-TU cells cross an explicit int idealEnergy=ideal*ideal before the
loops with moving unsigned short ms=0 after both loops. Existing short sums,
signed-short index/truncation, unsigned count, thresholds, comparison and final
conversion unchanged. Product is int as under existing integer promotion, no
width/layout/ABI, helper, flag, register or declaration-order sweep.

Require raw baseline reproduction, inspect actual operations and complete-TU
bodies/data/metadata, and preserve all measured bystanders. Exact candidate
requires supported dataflow and period gate before adoption. Miss closes this
finite domain rather than authorizing nearby register-color fitting. No fuzzing
or mutation execution.
