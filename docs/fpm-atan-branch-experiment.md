# `FPM_atan` final abs/branch-orientation experiment

## Question and declared domain

The first bounded experiment showed that builtin integer absolute value supplies the reference's branch-free magnitude mechanism, while explicit axis sign masks plus builtin magnitudes produce a 421-byte SIZE 12 body under retained flags. Two source questions remained bounded by direct evidence:

- Does ordinary `abs`, declared by `<stdlib.h>`, emit differently from the diagnostic `__builtin_abs` spelling?
- Does orienting the magnitude comparison like the reference's layout matter? The reference's `ay >= ax` division is fallthrough and `ay < ax` is out of line, whereas the retained source spells `if (ay < ax)` with the `ax` division first.

The complete domain was declared before compilation and contains six retained-profile cells:

- builtin versus ordinary `abs`, each with `int a` and `short a`;
- ordinary `abs` at each width with the complete division branches swapped under `if (ay >= ax)`.

The reversed cells swap both complete branch bodies, so behavior is unchanged. Every cell retains the ratio-127 notch, `0x7fff` fold, and integer absolute-value handling of `-32768`. No new flags, attributes, volatile objects, padding, or synonymous-expression search were introduced. This is the second and final batch regardless of score.

## Reproducible setup

Artifacts are in `build/fpm-atan-branch-experiment/`. `results.json` records all source axes, complete commands, hashes, function verdicts, relocations, symbol records, and table checks; `toolchain.txt` records the compiler and selected assembler.

The run explicitly uses `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, `/usr/i386-pc-linux-gnu/gcc-bin/3.4`, and `native=True`. Retained flags match `build/tc_repro/.build-config`, and `tools/experiment_toolchain.py::compile_shell` appends `-DDSPLIB_REPRODUCE_BUGS` last. The unchanged compilation path was already validated byte-identical in the immediately preceding experiment; the builtin cells are freshly compiled here through the new artifact path.

## Results

All six cells compile successfully and all six have the same comparator result:

| Absolute form | `a` width | Division orientation | Result |
| --- | --- | --- | --- |
| builtin | int | retained `ay < ax` | SIZE 12, 421 vs 409 bytes |
| ordinary | int | retained `ay < ax` | SIZE 12, 421 vs 409 bytes |
| ordinary | int | reversed `ay >= ax` | SIZE 12, 421 vs 409 bytes |
| builtin | short | retained `ay < ax` | SIZE 12, 421 vs 409 bytes |
| ordinary | short | retained `ay < ax` | SIZE 12, 421 vs 409 bytes |
| ordinary | short | reversed `ay >= ax` | SIZE 12, 421 vs 409 bytes |

There are zero exact hits. All cells retain the same three relocation targets: two `R_386_PC32` references to `FPM_div` and one `R_386_32` reference to `FPM_atan_table`. Relocation offsets move relative to the retained baseline because the body changed, but targets and addends do not.

The 514-byte table remains byte-identical to the baseline in every cell. `FPM_atan_table` remains a 514-byte GLOBAL OBJECT and `FPM_atan` remains a GLOBAL FUNC. There are no binding, visibility, table-data, call-target, or data-target changes.

## Interpretation and closure

For this compiler and complete TU, ordinary declared `abs` and `__builtin_abs` are emission-equivalent. The builtin mechanism probe therefore does not require an intrinsic spelling, but neither spelling recovers the reference when combined with the tested sign-mask source.

Within this combined source, `int a` and `short a` are also emission-equivalent. This confirms the prior warning: the short declaration was only a plausible carrier in isolation and is not width proof.

Reversing the complete division branches is emission-equivalent too. The reference's fallthrough/out-of-line layout is not controlled by this source branch orientation under retained GCC 3.4.2 flags.

No candidate is isolated for adoption. The two bounded batches are closed: direct products of the tested magnitude form, axis-mask spelling, angle width, one if-conversion control, and division orientation do not reproduce `FPM_atan`. Further nearby variants would be synonym search without a new discriminator.

If the TU is revisited in a later scope, begin from an instruction-level analysis of where the 421-byte combined emission first diverges from the 409-byte reference and identify a different source property that predicts that boundary. Do not reopen ordinary-versus-builtin abs, `int`-versus-`short a`, division orientation, or global if-conversion controls absent new evidence.

No versioned source, headers, gates, or commits were changed.
