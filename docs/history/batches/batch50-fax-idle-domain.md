# Transmitter idle source boundaries

902f47fa fixed profile, completed-source refinement. V17/V29 blob first
handles nonempty FIFO with transition/zero return, then no-carrier case.
Ours reverses arms. Their FIFO child read occurs after status-byte store
in blob but before it in source. Cross only arm orientation and child
read placement; parent pointer remains before store as object shows.
V27 already agrees on both, but extends budget signed then reconverts;
blob retains unsigned local count. One unsigned-short taken control.
No shared headers, unrelated permutations or profile changes. Full-TU and
raw baseline proofs; preserve call-visible budget reload and subtraction.
