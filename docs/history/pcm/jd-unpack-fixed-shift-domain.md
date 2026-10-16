# JD unpacker fixed member-shift transfer

Declared548b5edd before compilation. Existing V90 packData lever13 uses15
constant crc member shifts to enable loop promotion. Prior V92 pack helper
domain explicitly leaves both unpacker shifts untouched. New full original
unpacker graphs independently show the same promotion: V90 CRC16loads before
1edd0..1ee62 loop and16stores afterward; V92 data12b40..12bd0 and phase
12750..127e0 likewise preserve crc entries outside the32input iteration loop.
CRC initialization and final absolute-difference checks remain indexed loops.
There is no indexed15copy loop in these original recurrence bodies.

Test baseline/15literal shifts in V90 unPackData (two fullTUs), and independently
cross the same expansion in V92 unPackJdData/unPackJdPhaseData (four fullTUs).
Keep ascending assignment order so each next source is still the old element;
tap3/tap10 computed before shifts and final feedback updates unchanged. No
helper factoring, pointer/cache locals, widths/counters/states/return changes,
profile/slots/registers/declarations fitting. This is a transfer to untouched
consumer graphs, not reopening V92 pack helper spellings.

Require raw baseline, retained Gentoo profile/bug define last, live complete
body grades, binding/data/BSS/nontext relocation and every bystander review.
Recover operation alone is insufficient; only full strict EXACT gain with
complete audits can be adopted. Stop after6cells absent new original witness.
No fuzz/mutation, V34/PR263/issue22/shared header edits.
