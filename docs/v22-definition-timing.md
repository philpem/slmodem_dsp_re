# V.22 tone definition-timing experiment

## Question

The prior bounded experiment showed that declaration order, an explicit
ordinary `.rodata` section attribute, unit-at-a-time ordering and constant
merging do not change the two V.22 tone configurations' inverted placement.
This follow-up tested its one declared next discriminator: whether Gentoo GCC
3.4.2-r2 fixes varpool order at a file-scope declaration or at the later
initialized definition.

The source alternatives retain both reference names, initializers, LOCAL
linkage and call identities.  They replace the original adjacent initialized
definitions with file-scope tentative declarations at that point, then place
the initialized definitions immediately after `V22FP_create`.  Both
declaration orders were crossed with both definition orders.  The finite
domain was four alternatives plus the unchanged baseline.

## Validity and provenance

Artifacts are under `build/v22-definition-timing/`.  All five cells compiled;
none was invalid.  Every compile used the published authority image
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, explicit compiler path
`/usr/i386-pc-linux-gnu/gcc-bin/3.4`, and the complete retained flags:

```
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4
-include tools/toolchain/period_compat.h
-DDSPLIB_REPRODUCE_BUGS
```

`DSPLIB_REPRODUCE_BUGS` is last.  Actual commands and source/object SHA-256
values are recorded in `runs.json`.  The selected tools identify as:

```
gcc (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
i386-pc-linux-gnu
GNU assembler 2.15.92.0.2 20040927
```

The unchanged control is byte-for-byte identical to the fresh
`build/tc_repro/src_pump_v22_V22.c.o`.  All five cells have the same object
SHA-256:

```
c4165faf4eae12f53da02a1ac0fdf9acd9c45423721ffcb17f9f695efebea5d9
```

Thus the generator exercised four distinct source files but produced one
distinct emission; this is a measured negative rather than a stale build.

## Result

| Declaration order | Definition order | Compile | Emission | Tone order |
| --- | --- | --- | --- | --- |
| unchanged baseline | unchanged baseline | pass | retained | INIT `+0x00`, CFG `+0x40` |
| INIT, CFG | INIT, CFG | pass | byte-identical | INIT `+0x00`, CFG `+0x40` |
| INIT, CFG | CFG, INIT | pass | byte-identical | INIT `+0x00`, CFG `+0x40` |
| CFG, INIT | INIT, CFG | pass | byte-identical | INIT `+0x00`, CFG `+0x40` |
| CFG, INIT | CFG, INIT | pass | byte-identical | INIT `+0x00`, CFG `+0x40` |

The reference order remains CFG at `.rodata:0x84e0`, then INIT at `0x8520`.
Every cell instead retains INIT then CFG.  Both candidate tables remain
36-byte LOCAL OBJECT symbols.  `V22FP_create` and `V22FP_delete` remain GLOBAL
with candidate sizes 2409 and 296 bytes.  Their reference sizes are 2449 and
332 bytes respectively, so both remain `SIZE` against the original.

Because every experimental TU object equals the unchanged control byte for
byte, there are zero exact gains, zero exact losses, zero changed nonexact
bodies, zero changed relocations, no missing/new symbols, no binding changes,
and no data or call-target changes within the complete TU.

A representative 275-object partial link gives the unchanged retained result:

```
sections       67 / 92 exact records; 60 exact contents
contents       68,456 / 943,398 positioned reference bytes
relocations    974 / 18,317 exact; 17,144 candidate
symbols        299 / 2,907 exact; 2,959 candidate
candidate delta -45,764 bytes
verdict        DIFFERENT
```

The other three alternatives have identical partial-link input at the only
substituted object, so their complete link output and metrics are necessarily
identical.

## Conclusion and stopping point

The predicted declaration-versus-definition timing discriminator is negative:
neither first declaration nor completed-definition order controls these two
objects' emitted order in this compiler/profile.  No source change is a
candidate and nothing should be adopted.

Together with the preceding matrix, this exhausts the bounded local placement
families tested: direct declaration order, definition timing, explicit normal
section placement, unit-at-a-time and constant-merging controls.  Do not repeat
nearby declaration spellings.  Evidence that could reopen the question would
need to come from outside this exhausted local family, such as a recovered
translation-unit/file-boundary difference or a measured historical compiler
patch-stack difference that changes varpool emission while preserving names,
calls, bindings, complete bodies, relocations and partial-link layout.
