# Paired LMS argument lifetimes

At 4de483e2, the eight-instruction call-window screen nominates the second
FPM_lmsupd call in FPM_FSE_receive. This is not a formal-width witness:
the original narrows both errors at a845c/a8464, before its first call a8477.
Our second multiply, rounding, shift and narrowing follow the first call.
The original preserves the second narrowed error across that opaque call.

Prediction: capturing both scaled errors before the first call will restore
that ownership boundary in initial RTL and native output. Falsifier: either
the unchanged complete-TU baseline does not reproduce, or the compiler still
forms the second scaled error after the first call. Exact size alone is not
acceptance evidence.

Three complete-TU cells, fixed Gentoo profile and bug define: unchanged,
two short argument locals before both calls, and reassignment of the existing
error carriers before both calls. The latter are dead after this update and
are assigned anew on the next iteration. Do not widen the exact LMS helper
prototype: the original already contains both short conversions. Review every
body, calls, metadata, named and allocated data; no unrelated source changes.
No declaration/operand-order permutations or new option matrix.

Screen nominations are not matched dataflow. The MTD nomination is disproved
by a923f: the original conversion lies just outside its eight-row window.
TONE's conversion is not an IIR argument, and settxlevel's conversion is not
the bitreverse argument. Preserve those negative interpretations.
