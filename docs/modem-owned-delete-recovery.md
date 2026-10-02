# V90 and V92 modem owned-member deletion

F11621. Baselinea3da40f7,908/1852 exact/92900 exact bytes.
[Predeclared42-cell complete-TU cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952375748).

V90 D1 retains EBX through actual destructor/free pairs for modulator+0
0x193c8/0x193d0, demodulator+4 0x193b3/0x193bb, jd+0xc 0x19387/0x1938f,
jd92+0x10 0x19375/0x1937d, params+0x49b4 0x19345/0x1934d. V92 D1 does
likewise for modulator+0 0x13b38/0x13b40, cp+0xaa4 0x13b26/0x13b2e and
parameters+4 0x13b14/0x13b1c. Shared headers and typed direct call relocations
establish nonvirtual class owners; retained manual source reloaded each after
its destructor. Test ordinary scalar delete, independently of primitive frees.

V90 family33 cells: retained baseline; TU-local inline unsized host delete
adapter after last include only;31 nonzero subsets, bits0..4 in the order
above. V92 family9 cells: baseline; adapter-only;7 nonzero subsets, bits0..2
as above. Both baselines reproduce retained full objects; both adapter-only
raw-merge. Only all-five V90 and all-three V92 recover their complete D1/D2.
V90 clones337→321B, exact2→5/8 functions; V92 clones261→229B,
exact1→4/7 functions. All30 V90 partials miss (SIZE5/7 or BYTES22..88);
all6 V92 partials miss (SIZE13/15/28). No compilation failures,42 distinct
sources produce40 raw emissions (32V90,8V92).

Each nonzero cross additionally recovers one unchanged bystander: V90
printTitle210B has only live-range EAX/ECX/EDX renaming; V92 progress121B
moves one diagnostic-string operand EDX→ECX. Complete instruction/operand/
relocation alpha controls agree. These are TU register-allocation consequences,
not recovered source statements at those methods. All other five V90 and four
V92 bodies unchanged. Zero named data objects in either TU; all symbol
names/type/binding/visibility/imports/exports and allocated nontext contents/
flags/alignment/relocations agree. Six exact gains/no losses in total.

Preserve debug and unsigned illegal-side tests, release order, dangling typed
members, bare phase2Info free (V92 conditional clear), V92 unguarded mapping
array helpers and guarded mapping block free, generated V90 CP/MP destruction.
No phase2 destructor call is supported by the blob; do not invent one based
on the typed-member result. Ordinary delete is a supported clear spelling,
not proof of unique original source or the original global inline profile.

Fixed t_v90modemctor run_dtor_live releases genuine constructed digital and
analog child graphs; illegal-side storage sanitation remains synthetic.
Its64 pre-release-and-null subsets destroy borrowed-parameter consumers before
the parameter owner; these are synthetic guard probes. V92 t_v92modem subset0
first destruction releases actual all-live analog constructor children.
Other15 subsets temporarily mask selected pointers; complement cleanup
restores surviving children and supplies an empty mapping block. Those are
synthetic component histories, not construction reachability or pre-release
paths. Bare phase2 unknown-pointer probes are distinct apparatus boundaries.
Both clone variants are covered. Eight static anchors retain wrong-child
free, missing destruction/free and added-pointer-clear defects; original
fixture metadata preserved, including existing duplicate why keys. No fuzzing
or mutation execution.

Replay tools/playbook_modem_owned_delete.py --domain <linked URL>. Artifacts
build/playbook-modem-owned-delete retain actual full saved configuration,
DSPLIB_REPRODUCE_BUGS/Gentoo GCC3.4.2-r2/executed assembler2.15.92.0.2,
source/header/command/object hashes, RTL, disassemblies and complete object/
bystander audits. All300 production objects checked: exactly two change,
both raw-identical to their promoted cells. Adoption artifacts under
build/playbook-modem-owned-delete-adoption. No modern portability claim.

Complete same-order300-object partial links remain DIFFERENT, both strict
exit1. Positioned68344→68568 /943398 (+224), allocated914670→914574 (-96),
exact sections70/92 and symbols394/2907 unchanged; relocations1021→1030
/18317 (+9). Retain the partial-link limits despite this wave's improvement;
function gains do not establish whole-object/profile identity.

Whole-tree census908→914/1852,92900→94331 exact bytes (+1431), six gains,
no losses. Fixed make phase385 passed/0 failed and all structural gates
clean;285 suites/10038 static anchors zero detached/nonunique. Ledger refs
resolve without stale entries. No source/fixture gate was weakened.
