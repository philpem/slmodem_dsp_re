# `FPM_atan` bounded source/profile experiment

## Scope and evidence

The post-PR95 retained TU has one shared function, `FPM_atan`: BYTES 290 with equal 409-byte bodies. Its other allocated payload is the 514-byte GLOBAL `FPM_atan_table`.

The reference disassembly forces three useful questions. Its magnitudes use branch-free `cltd`/xor/sub sequences where the retained ternaries branch. Its zero-axis results use arithmetic sign masks (`sar $31` followed by `and $0x4000`) where the retained ternaries branch or shift an unsigned sign bit. Around the small-angle select and later folds, the reference performs short extensions where the retained `int a` keeps a full-width value.

The domain deliberately preserves the known behavior: the shortcut remains `ratio <= 0x7e`, retaining the ratio-127 notch; the fold remains `0x7fff - a`; and magnitudes are calculated in `int` before conversion to `unsigned short`, retaining the `-32768 -> 32768` result.

## Reproducible setup

Machine records are in `build/fpm-atan-experiment/results.json`; generated sources, objects, complete commands, stderr and hashes are retained beside it. `toolchain.txt` records the compiler and the assembler actually selected.

The run explicitly uses `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, compiler path `/usr/i386-pc-linux-gnu/gcc-bin/3.4`, and `native=True`. This avoids relying on the helper's local-alias image detection. The C flags exactly match `build/tc_repro/.build-config`; `tools/experiment_toolchain.py::compile_shell` appends `-DDSPLIB_REPRODUCE_BUGS` last.

The unchanged-path control reproduces `build/tc_repro/src_dsp_fpm_atan.c.o` byte-for-byte. It reports the expected 409-byte GLOBAL function and 514-byte GLOBAL table. The matrix is therefore valid.

## Finite crossed domain

Three binary source axes produce eight variants:

- magnitudes: retained ternaries or `__builtin_abs((int)x/y)` before unsigned-short conversion;
- zero axes: retained ternaries or explicit signed shift-and-mask expressions;
- angle intermediate: retained `int a` or `short a`.

Each is compiled with retained flags and with the single diagnostic `-fno-if-conversion` control: 16 cells plus the unchanged control. No attributes, volatile objects, padding, synonymous-expression search, or score-forcing constructs were used.

The builtin is a mechanism probe, not automatically retention-quality source. If its mechanism proves useful, ordinary `abs` with its standard declaration remains the source-level discriminator.

## Complete results

All 17 compilations succeed. There are zero exact hits.

| Magnitude | Axes | `a` | Retained | `-fno-if-conversion` |
| --- | --- | --- | --- | --- |
| ternary | ternary | int | BYTES 290, 409 bytes | SIZE 27, 436 bytes |
| ternary | ternary | short | SIZE 1, 410 bytes | SIZE 25, 434 bytes |
| ternary | mask | int | SIZE 38, 447 bytes | SIZE 9, 418 bytes |
| ternary | mask | short | SIZE 54, 463 bytes | SIZE 25, 434 bytes |
| builtin abs | ternary | int | SIZE 31, 378 bytes | SIZE 15, 394 bytes |
| builtin abs | ternary | short | SIZE 30, 379 bytes | SIZE 28, 437 bytes |
| builtin abs | mask | int | SIZE 12, 421 bytes | SIZE 15, 394 bytes |
| builtin abs | mask | short | SIZE 12, 421 bytes | SIZE 28, 437 bytes |

The 514-byte table is byte-identical to the baseline in every cell. Both symbols remain GLOBAL with their original type, visibility and table size. Every function retains exactly three relocation targets: two `R_386_PC32` references to `FPM_div` and one `R_386_32` reference to `FPM_atan_table`; offsets move with the bodies, but no call/data target changes.

The unchanged source variant under retained flags reproduces the baseline function and relocation offsets. Generated-source object files have different raw metadata because their source paths differ, so candidate assessment uses the function body/relocations and explicit symbol/table records rather than raw whole-file equality.

## Interpretation

Builtin integer absolute value confirms the magnitude mechanism: it removes substantial branch code and creates the expected branch-free family, but by itself produces a 378-byte body and does not recover the reference's complete scheduling and folds. It is diagnostic evidence for integer absolute-value semantics, not a retained-source decision.

The explicit axis-mask spellings do not reproduce the reference recognition in this context. Under retained flags they enlarge the body to 421-463 bytes depending on the other axes. Thus the reference instructions establish arithmetic sign-mask behavior, but this tested source spelling is excluded.

Changing `a` to `short` alone gives the numerically closest length, 410 versus 409, and supplies a narrowing boundary, but it remains a SIZE miss. A one-byte size gap is not one differing byte and does not establish that the declaration was short. The width remains a plausible carrier, not uniquely recovered source.

`-fno-if-conversion` is a negative mechanism control. It changes broad control flow, ranges from 394 to 437 bytes, and yields no hit. It does not isolate the reference's axis masks.

## Conclusion and next discriminator

No source or option cell is adopted. This complete 16-cell domain excludes the tested direct product and supplies three bounded facts: integer-absolute-value form controls the magnitude branches; the direct mask spelling is not sufficient; and a short intermediate changes the intended extension boundary but does not close the TU.

If reopened, the next small source discriminator is standard `abs` with the period compiler's normal declaration versus the builtin probe, crossed only with the two most informative retained-profile width cells. Prediction: if the author's ordinary integer absolute call was recognized as the same builtin, it should reproduce the builtin magnitude sequences without introducing a call; an emitted call or different body falsifies that source form. After that, inspect the exact reference/variant fold instruction boundary before defining one bounded sign-mask-temporary family. Do not expand into synonymous ternaries or further global pass toggles without a new instruction-level prediction.

Existing differential coverage already exercises axes exhaustively, the origin grid, all 16-bit values against fixed counterparts, diagonal neighborhoods, all table entries including ratio 127 through `-32768`, and a large deterministic sample. No new behavioral test was needed for this negative experiment. No source files, gates, or commits were changed.
