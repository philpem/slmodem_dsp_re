# FDSP initialization capture and float-clear boundaries

Base240481e6, four complete retained-profile TUs. Original aeb33 loads
k->chan_a a second time before its delay/coef clears; first owner remains
used for short_1690. The reconstruction captures once. Original final four
float clears use direct unsigned bound CMP500/CMPf0 with JB whereas the
reconstruction's fixed index loops compare bound-1/JBE. Existing exact
zFLTUTL_FloatMemSet is a same-TU plausible helper originally inlined for these
clears. Cross helper calls and recapture with raw repeated baseline.

No alias/capture behavior equivalence claim for overlapping kernel/child
objects; initializer allocation establishes separate objects. Complete-TU
bystanders and all allocated data are audited. Nonexact cells remain findings
only; nearest size is not adopted.
