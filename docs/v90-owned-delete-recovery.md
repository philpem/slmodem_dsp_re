# V90 parent modulator owned-member deletion

F11618. Baseline9daec211,901/1852 exact/90980 exact bytes.
[Predeclared nine-cell complete-TU cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951914061).

Independently of the phase-four child gain, the parent blob keeps one evaluated
pointer through each phase3Modulator(+0x38), phase4Modulator(+0x3c) and
bitsToSymbol(+0x40) destructor/free pair. All three class pointer types and
destructor targets are established by shared headers and actual call
relocations. Manual source pairs reloaded each member after destruction.
F10160 recovered explicit destructor calls from assembly labels, but did not
test ordinary member delete.

Nine complete cells: retained source; adapter-only TU-local inline unsized
operator delete forwarding sysdep_free after the final include; seven nonzero
three-owner crosses. Adapter-only wholeobject raw-merges. Each nonzero cell
changes only D1/D2, all199B versus original200B; only all-three recovers the
complete199B clones. Partial001/010/011/100/101/110 remain BYTES38/38/22/36/22/20.
The result is not a size fit: three independently observed pointer lifetimes
are all needed. Exact19→21/25 functions, two gains/no losses. All23 bystanders
canonically identical, zero named data objects. Symbols/type/binding/visibility/
imports/exports and allocated nontext contents/flags/alignment/relocation
records agree. No operator import, helper symbol or array-cookie side effect.
Preserve the two primitive buffer frees, call/release order, dangling members
and automatic embedded Scrambler destructor. No broad cleanup rewrite or
unique original syntax/profile claim; no inferred ordinary lifecycle bug.

Fixed t_v90modchain drives C1/C2 constructors, D1/D2 releases through actual
owned child allocation chains, including real all-live lifecycle and supplied
converter ownership. Its five pre-release-and-null component subsets are
synthetic modified histories; retain their coverage without calling them
reachability evidence. Parent integration fixtures are additional coverage.
Four static anchors preserve missing null guard, missing child destruction,
missing free and inverted converter guard defects. No fuzzing/mutation
execution or invented alias tests.

Replay tools/playbook_v90_owned_delete.py --domain <linked URL>. Artifacts
build/playbook-v90-owned-delete retain complete actual saved flags/mandatory
DSPLIB_REPRODUCE_BUGS/Gentoo GCC3.4.2-r2/executed assembler2.15.92.0.2,
source/header/command/object hashes, initial RTL, complete audit and all
changed-body disassemblies. All300 period objects rebuilt: exactly one changes and raw-matches the
promoted complete cell. Census901→903/1852,90980→91378 exact bytes,
two gains/no losses. Fixed make phase385 passed/0 failed and all structural
gates clean; static anchors285 suites/10038 zero detached/nonunique.
Complete same-order partial links remain DIFFERENT; positioned68778→68770
/943398 (eight fewer), allocated914510 unchanged; exact sections70/92,
symbols394/2907 and relocations1024/18317 unchanged. Retain this mixed
layout measurement without claiming whole-object/profile recovery. Artifacts
under build/playbook-v90-owned-delete-adoption. No modern portability claim.
