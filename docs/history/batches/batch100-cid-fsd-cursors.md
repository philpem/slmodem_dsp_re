# CID FSD paired correlation cursor controls

Precompile domain856c1ecb, eight complete-TU cells: descending shared-wrap
history/consuming coefficient cursor for autocorrelation × same for lowpass ×
guarded capture of initial threshold. Original first correlation uses MOVSWL
through a descending pointer and ascending coefficient pointer; after first loop
it forms pointer+10 before second loop. Lowpass repeats with pointer+34. Retained
uses independent indexed loops. Original reads threshold after zero-count guard;
retained reads it before guard. Keep all arithmetic/count types and slicing CFG.
No arbitrary statement/register/flag permutation. Pointer forms are equivalent
for valid ring indices; malformed negative indices retain a distinct traversal
and must not be introduced just for byte fit. Guarded threshold skips a read on
zero count, as original does. Review original/source RTL access families and all
TU tables/bodies/bindings/relocations. No runtime/mutation/fuzzing execution.

Results: reference1049B; baseline795B; guard-only813B. Autocorrelation or
lowpass cursor alone885B (903B with guard); both869B (887B with guard).
None exact. Individual pointer controls recover the original descending/ascending
operand family, but full bodies remain different and the crossed pair does
not add sizes independently. The original guard-before-threshold read is an
observable access boundary, not license to adopt a nonexact candidate alone.

Eight full-TU audits pass: sole common function changes, all coefficient data
objects, allocated nontext/relocations and metadata unchanged, no exact losses.
All sources/commands/objects/disassembly/-da traces retained. Close these
cursor/threshold control families without extending to declaration permutations.
Malformed-ring-pointer behavior is not claimed equivalent; no source adopted.
