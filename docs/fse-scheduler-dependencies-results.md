# FSE scheduler dependencies: a measured memory-type mechanism

Base6b4509bd. Two verbose diagnostic full-TU compilations reproduce both
earlier closed source cells raw, not merely their grades: retained structure
copy and builtin memcpy after the four enable writes. The added
-fsched-verbose=5 supplies dependency/priority/ready/issue diagnostics without
changing any emitted byte. Four bodies per TU, eight live grades; no gains,
losses, binding/import/export/data/BSS/nontext relocation changes.

F11832 located sched2 as the first order-changing pass. This trace explains
the legal scheduling graph under the retained compiler:

| Copy source form | Copy UID/alias | Copy priority | Edges to freq29/phase45 | Issued order |
| --- | --- | ---: | --- | --- |
| cfg structure assignment |19/type set2|5|absent/absent|29,45,19|
| builtin memcpy after enables |27/untyped set0|8|present/present|27,29,45|

Both SI stores have priority6. Both forms have an edge from copy to the
short mu_sel store31. The retained cfg aggregate contains short and pointer
members, while the two SI stores have type alias set9. Initial and bbro RTL
retain these memory types. Typed-copy graph19 has no edges to either SI
store, permitting both to issue before copy; their priority exceeds copy's.
Untyped-copy graph27 includes both edges and increases their incoming counts
6→7, so copy must issue first. These are actual Gentoo scheduler diagnostics,
not an inference based only on final register colours.

Official GCC3.4.2 source explains the mechanism consistently: sched-ebb.c
calls sched_analyze then compute_forward_dependences before priorities;
sched-deps.c checks pending reads/writes with anti_dependence and
output_dependence; alias.c write_dependence_p rejects DIFFERENT_ALIAS_SETS_P
before other overlap tests. alias_sets_conflict_p treats set0 as possibly
aliasing anything and record_component_aliases records aggregate component
types. Five official source files are hash-pinned by
tools/fse_scheduler_source_fetch.py. They are upstream explanatory source,
not a verified reconstruction of Gentoo's complete patch stack. The actual
instrumented graph and raw repeats remain the deciding measurements.

This identifies a dependency mechanism, not a source preimage. The original
binary does not reveal its memory alias tags; original cfg typing, copy
spelling and compiler alias options are not uniquely recovered. Both measured
cells remain nonexact458B, BYTES56 and BYTES49. Do not retype an unused field,
insert union/void-pointer/volatile casts or change flags just to force edges.
Before another source candidate, find independent original field/copy/API
evidence that predicts the new alias graph. Issue22 and PR263 remain untouched.

Replay tools/fse_scheduler_dependencies_reproduce.py using its declared domain
and baseline directory; audit tools/fse_scheduler_dependencies_audit.py.
The known detector fires: two missing SI edges become present while the HI
edge remains. Raw-object expected hashes, source hashes, full commands,
profile/bug define, complete bodies and all scheduler dumps are retained.
