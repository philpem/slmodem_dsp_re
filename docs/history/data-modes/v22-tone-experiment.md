# V.22 tone-table order experiment

## Question and reference evidence

The reference partially linked object places the two local 36-byte tone
configurations in this order:

```
000084e0 00000024 r TONEv22_CFG
00008520 00000024 r TONEv22INIT_CFG
```

The retained `src/pump/v22/V22.c` names and uses agree with the reference, but
Gentoo GCC 3.4.2-r2 emits `TONEv22INIT_CFG` at TU `.rodata+0` and
`TONEv22_CFG` at `.rodata+0x40`.  Their contents are identical.  This experiment
therefore tested placement mechanisms only; it did not swap names or calls.

## Validity and provenance

Artifacts are under `build/v22-tone-experiment/`.  The unchanged experimental
object SHA-256 is
`c4165faf4eae12f53da02a1ac0fdf9acd9c45423721ffcb17f9f695efebea5d9` and is
byte-for-byte identical to fresh
`build/tc_repro/src_pump_v22_V22.c.o`.  Its complete flags match the fresh
`build/tc_repro/.build-config`:

```
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4
-include tools/toolchain/period_compat.h
-DDSPLIB_REPRODUCE_BUGS
```

`DSPLIB_REPRODUCE_BUGS` was appended after every cell's configurable flags.
The image was
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, with the compiler path
explicitly set to `/usr/i386-pc-linux-gnu/gcc-bin/3.4`.  Recorded identity:

```
gcc (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
i386-pc-linux-gnu
GNU assembler 2.15.92.0.2 20040927
```

Nine of nine cells compiled.  They produced four distinct object emissions.
The complete commands, hashes, generated sources, compiler output and strict
partial-link reports are retained with the artifacts.

## Declared matrix and result

The retained and explicit-`.rodata` source forms were crossed with retained
flags, `-fno-unit-at-a-time`, `-fno-merge-constants`, and both flags.  A
declaration-order swap was retained as the known negative control.

| Source form | Flags | Object relative to corresponding retained-source flag cell | Tone order |
| --- | --- | --- | --- |
| retained | retained | baseline | INIT `0x00`, CFG `0x40` |
| retained | no unit-at-a-time | distinct symbol table | INIT `0x00`, CFG `0x40` |
| retained | no merge-constants | distinct section layout | INIT `0x00`, CFG `0x40` |
| retained | both | both effects | INIT `0x00`, CFG `0x40` |
| explicit `.rodata` | each of four profiles | byte-identical in every pair | INIT `0x00`, CFG `0x40` |
| swapped declarations | retained | byte-identical | INIT `0x00`, CFG `0x40` |

The detector fired on known controls: `-fno-unit-at-a-time` moved the local
`V22DiconnectThreshTable` symbol-table entry ahead of the tone entries, and
`-fno-merge-constants` removed `.rodata.str1.1`, enlarged `.rodata` from
`0x64` to `0x94`, and redirected the three debug-string relocations in
`V22FP_create`.  Thus the unchanged tone result is not an empty or broken
generator.

The TU has two shared exported functions.  Against retained, the unit-at-a-time
control kept both `V22FP_create` (2409 bytes) and `V22FP_delete` (296 bytes)
exact.  The merge control kept `V22FP_delete` exact and left `V22FP_create`
instruction-identical but relocation-section-unresolved because its strings
moved.  All cells retained GLOBAL binding for both functions and LOCAL OBJECT
binding and 36-byte size for both tone configurations.  No symbols were added,
removed, strengthened or weakened.

Strict 275-object partial links gave these metrics against the reference:

| Cell | Positioned bytes | Exact symbols | Exact relocations | Candidate delta |
| --- | ---: | ---: | ---: | ---: |
| retained | 68,456 / 943,398 | 299 / 2,907 | 974 / 18,317 | -45,764 |
| no unit-at-a-time | 68,456 / 943,398 | 299 / 2,907 | 974 / 18,317 | -45,764 |
| no merge-constants | 69,071 / 943,398 | 300 / 2,907 | 974 / 18,317 | -45,780 |
| both | 69,071 / 943,398 | 300 / 2,907 | 974 / 18,317 | -45,780 |

All four remain `DIFFERENT`.  The merge control's apparent gain of 615
positioned bytes and one exact symbol is unrelated section-packing collateral:
it deletes the reference-like mergeable-string section and changes three call
operands.  It is a mechanism control, not a source/profile candidate.

## Conclusion and next discriminator

This bounded domain is negative.  Declaration order, an explicit ordinary
`.rodata` attribute, unit-at-a-time ordering, constant merging, and their tested
interaction do not invert the tone tables.  No source or flag change should be
adopted from it.

The next discriminating test is definition timing rather than another nearby
declaration spelling: compile a crossed full-TU control with file-scope
declarations visible before `V22FP_create` but initialized definitions placed
after it, in both declaration orders, under retained flags.  Prediction: if
GCC's varpool insertion point is fixed by first declaration, the emitted order
will follow the forward declarations; if it remains INIT-first, this source
family is excluded and the inquiry should move to recovered TU/file-boundary
or historical compiler-patch differences.  Any promising cell must retain the
reference names, call identities, bindings, full-TU bodies/relocations and
strict partial-link inventory before it is considered a source candidate.
