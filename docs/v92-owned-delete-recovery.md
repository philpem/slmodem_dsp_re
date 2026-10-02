# V92 parent modulator owned-member deletion

F11620. Baseline7b201c02,905/1852 exact/91768 exact bytes.
[Predeclared33-cell complete-TU cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952276249).

Five independently observed nonvirtual class owners retain the captured blob
pointer across actual destructor/free pairs. D1 sites: phase3Modulator+0x44
0x142f3/0x142fb; phase4Modulator+0x48 0x14313/0x1431b; bitsToSymbol+0x4c
0x14333/0x1433b; Queue<float>+0x74 0x14373/0x1437b; FloatFIR+0x78
0x14393/0x1439b. Shared types and direct relocation targets support ordinary
scalar delete. Manual source reloaded each member between calls. The old
restriction was an untested library dependency inference; the local inline
unsized host adapter removes that dependency without adding exported symbols.

Complete33 cells: unchanged baseline; adapter-only after last include; all31
nonzero subsets of five owner-delete axes, bits0..4 as listed above. Baseline
reproduces retained full object, adapter-only raw-merges. All31 nonzero cells
produce503B D1/D2 versus retained504B. All30 partial crosses still miss the
complete bodies (BYTES22..86); only11111 recovers both503B clones. Size alone
would therefore have accepted every failing partial cell. All31 also recover
unchanged126B enterPhase3; this bystander differs only by ECX/EDX renaming,
verified over complete instructions/operands and call relocations. It is a
measured TU register-allocation consequence, not a recovered statement there.
Exact20→23/30 functions, three gains/no losses. All27 other bodies canonically
unchanged, one named data object. Full symbol types/binding/visibility/imports/
exports and allocated nontext contents/flags/alignment/relocations agree.
The33 distinct sources produce32 distinct full objects; no invalid compiles.

Preserve six-class release order, existing virtual resampler deletion, five
primitive guarded frees, dangling members and generated embedded Scrambler
destruction. No broad cleanup, nulling, layout forcing or uniquely recovered
original profile claim. Fixed t_v92mod C1/C2 and D1/D2 constructs actual owned
children. Subset0 first destruction is the genuine all-live lifecycle; other
2047 subsets temporarily mask selected real owner pointers, then restore and
release remaining allocations. Those are synthetic guard histories, not
reachability evidence or pre-released children. Their second destruction is
also a synthetic cleanup probe. Fixture checks free/null/bad-free counts,
unchanged owner bytes and final allocator live0. Five static anchors preserve
missing guard/destruction/added clear defects; do not replace guard removal
with `delete NULL`, which retains its language guard. No fuzzing or mutation
execution.

Replay tools/playbook_v92_owned_delete.py --domain <linked URL>. Artifacts
build/playbook-v92-owned-delete retain actual full saved flags/mandatory
DSPLIB_REPRODUCE_BUGS/Gentoo GCC3.4.2-r2/executed assembler2.15.92.0.2,
source/header/command/object hashes, RTL, disassemblies, full object and
bystander audits. All300 production objects checked: exactly one changes,
raw-identical to the promoted full cell. Adoption artifacts under
build/playbook-v92-owned-delete-adoption. No modern portability claim.

Whole-tree census905→908/1852,91768→92900 exact bytes, three gains/no losses.
Complete same-order300-object partial links remain DIFFERENT (both strict
exit1); positioned68347→68344 /943398 (-3), allocated914670 unchanged.
Exact sections70/92, symbols394/2907 and relocations1021/18317 unchanged.
Retain this mixed measurement without claiming whole-object/profile recovery.

Fixed make phase385 passed/0 failed and all structural gates clean;
285 suites/10038 static anchors zero detached/nonunique. Updated ledger
refcheck14258 references/2749 headings, zero unresolved/stale entries.
