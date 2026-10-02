# Psd primitive-array allocation recovery

Complete src/dsp/psd.cpp at41b7a6fe, Gentoo GCC3.4.2-r2 and selected
assembler2.15.92.0.2. Full retained .build-config flags include C++
-fno-exceptions,-fno-rtti,-fno-math-errno,-ffast-math and mandatory
DSPLIB_REPRODUCE_BUGS. Shared replay engine now invokes g++ and appends the
saved CXX flags for .cpp cells; C commands are unchanged. Complete unchanged
C++ TU raw-reproduces production, demonstrating the apparatus on known input.

Both blob constructor clones82B versus85 production. At0x46a6c–0x46a72 the
reference reloads m_length, increments element count then shifts to bytes;
raw allocator arithmetic folds into lea4(,length,4). Surrounding field reloads,
three calls and overlap store are preserved. Destructor already uses delete[].
F10214's primitive-array-new precedent motivates this bounded transfer; F875
and F7786 establish owner/destructor behavior, not this allocation-spelling
family. No definition-order or register permutation is performed.

[Predeclared five-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947049706):

| Full-TU cell | C1 / C2 |
| --- | --- |
| unchanged baseline |85B/SIZE3 each|
| fixed allocator scaffold, raw arrays |85B/SIZE3 each|
| first array new[] |85B/SIZE3 each|
| second array new[] |82B/EXACT each|
| both arrays new[] |82B/EXACT each|

Scaffold adds stddef, allocator declaration and one inline replacement new[]
definition immediately after constructor. Unsized delete[] stays in place.
Scaffold and first-only objects raw-agree with baseline. Two hits raw-agree
with each other. Five distinct sources/two complete emissions, thirteen
functions/thirteen global bindings and every data section preserved. Only
constructor clones' canonical bodies change. Six/thirteen ->eight/thirteen
exact, no losses. No allocator symbol/cookie or output initialization/null
check is emitted; sysdep_malloc remains the allocator. This isolates second
allocation expansion, not a uniquely established spelling for the first.
Retained both-new form is idiomatic and consistent with destructor ownership.

Existing t_psd fixture alternates constructor clones over window/length/overlap
shapes, seeds allocator buffers, compares window/state/unwritten FFT bytes,
counts two allocations per side with exact byte sizes, and covers destruction,
getters and processing. Huge-count/failure behavior is not claimed as forced
fixture evidence. One static FFT-overallocation anchor is retargeted from raw
byte allocation to one extra array element, preserving its fault meaning.
No fuzzing or mutation execution.

Replay tools/playbook_psd_array_new.py --domain URL above. Artifacts in
build/playbook-psd-array-new retain actual commands, source/header/object hashes,
full inventories, changed bodies/relocations and RTL. Adoption records:
build/playbook-psd-array-new-adoption. F11578.

Retained validation: all300 period objects build; only src_dsp_psd.cpp.o
changes and it raw-matches the winning full-TU cell. Whole-tree872/1852
->874/1852 exact,85,127 ->85,291 exact bytes; only C1/C2 gain, no losses.
Fixed Gentoo phase385 passed/0 failed, including existing allocation-accounting
and constructor/destructor fixtures. Complete300-object partial links in the
same recovered order remain DIFFERENT (strict exit1): positioned68,419
->68,418/943,398 bytes; allocated914,094 bytes unchanged; section70/92,
symbol394/2907 and relocation1,018/18,317 exact records unchanged. Per-function
gains do not establish complete-object identity or the original inline profile.
C++ compiler and selected executed assembler identities are retained separately
in cxx-identity.log alongside the exact invocation.
