# V92 mapping-array lifecycle recovery

F11615/F11616. Baseline ef019461,897/1852 exact/90513 exact bytes.
[Deletion cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951609883) and [paired allocation cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951650663).

F7818 declined array-delete because its then-current C reconstruction could
not express it, and language provenance was unproved. F11404 later recovered
V92MappingParamsInt.cpp's FILE/language and kept the two creators exact.
Current fields are six int* constellations and four float* coefficient banks,
not void*: F11404's historical sentence claiming void* is stale. Reopen only
the newly supported array-expression family; no file rename or language change
in this wave, and no flag/register/declaration/operator-position permutations.

Four complete deletion cells cross constellation/filter delete[] expressions,
with an ordinary TU-local inline unsized operator delete[] forwarding to
sysdep_free immediately after includes. Each independently closes its own
173B/106B body from BYTES3 to EXACT, combined two gains/no losses. The other
three bodies unchanged: creators121B/73B EXACT, parameter fill2679B SIZE16.
All five function symbols/type/binding/visibility/imports/exports and allocated
nontext sections/relocations agree; zero named data objects, no helper export
or extra allocation/cookie/destructor call.

A separate five-cell allocation cross retains the original raw baseline and
holds both deletions fixed while crossing typed new[] for the six int arrays
and four float arrays. One inline operator new[](size_t) forwards original
sizes to sysdep_malloc, immediately before the deletion adapter. Array counts
are existing byte size divided by sizeof(element); no value initialization,
extra checks or exception specifications. All four deletion-bearing cells
raw-merge with the first experiment's complete deletion-both object.
Thus adopt idiomatic paired new[]/delete[] without claiming unique original
allocation spelling. No body/data/binding regression or additional byte gain
is hidden behind this allocation choice. Nine valid compilations total.

Existing t_v92alloc live cases allocate all6/all4 actual host blocks, check
all frees/live0/bad0 and preserved dangling slots. Its three null-pattern
unknown-pointer cases are synthetic apparatus probes (swallowed by the
allocator), not lifecycle reachability evidence. Keep those separate from the
real live-allocation checks. Exact creator bodies preserve all original
allocation calls and unchecked return handling; no new failure-path claim from
the ordinary live fixture. Full period gate required before commit.

Retarget eleven static anchors, preserving half-allocation/missing allocation/
wrong-slot/size/extra-bank/dangling/null/duplicate-free defects. No fuzzing or
mutation execution. Replay tools/playbook_v92_array_delete.py and
tools/playbook_v92_array_allocation.py --domain <respective linked URL>.
Raw baseline/helper/header/source/compiler-command/object hashes, complete
body/data/export audits and disassemblies retained under
build/playbook-v92-array-delete and build/playbook-v92-array-allocation.
Actual saved complete flags include mandatory DSPLIB_REPRODUCE_BUGS,
Gentoo GCC3.4.2-r2 and executed assembler2.15.92.0.2.

All300 period objects rebuilt: exactly V92MappingParamsInt changes, raw-matching
the promoted complete object. Census897→899/1852,90513→90792 exact bytes,
two gains/no losses. Fixed make phase385 passed/0 failed, structural gates
clean; static anchors285 suites/10038, zero detached/nonunique.
Complete same-order partial links remain DIFFERENT with all aggregate metrics
unchanged: positioned68786/943398, allocated914510, exact sections70/92,
symbols394/2907, relocations1024/18317. Artifacts under
build/playbook-v92-array-adoption. No modern portability claim or
whole-object/original-profile identity claim.
