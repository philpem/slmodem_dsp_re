# HDLC unframer current parent across callbacks

Two fullfaxvmi_hdlc TUs, baseline/current-parent-reads e0052eec. This family
is distinct from closed HDLC frame reload domain. Original unframe parent
reloads after unknown FCS/debugcalls (960e0,96116,96154,etc); retained fr entry
snapshot survives every call. Candidate uses ordinary vmi->framer members
for accesses after loopentry, retaining all initial scalar snapshots and
sourcebitpacking/CFG/bug behavior. This consistent memberowner family
preserves independent aliases rather than caching across calls. Predeclared
2cells, not a4cell debug/FCS insertion scheme; direct parent member reads
avoid artificially wrapping function returns to insert local assignments.
Require rawbaseline, all functions/fullmetadata/data/BSS/relocation audit;
no exactlosses and no adoption absent complete match. No runtime/fuzz/mutation.
