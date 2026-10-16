# Recovering a GCC build environment

Reconstruct the compiler environment alongside the source. Use this procedure
for GCC-built objects, especially GCC 3 on x86 with x87 floating point. It
complements [the source recovery workflow](recovering-source.md); the existing
[compiler record](compilers.md) documents this project's individual variances.

## Identify the compiler and its supporting tools

Read `.comment`, file symbols, section conventions, mangled names and runtime
imports. Preserve the complete compiler banner, including distribution suffixes
and dates. A banner identifies a candidate producer; it does not disclose a
complete command line or prove every input used identical options.

Here the reference identifies GCC 3.4.2. The reconstruction ultimately adopted
Gentoo's patched GCC 3.4.2-r2 in a reproducible container. Earlier measurements
used Debian's GCC 3.4.4 and stock GCC 3.4.2; they were useful controls but
did not establish an identical compiler. The provenance and measurements are
recorded in findings F2200, F2201 and F2500.

Capture the compiler, assembler, linker, target, headers, runtime libraries and
container image identity for each experiment. Execute the assembler selected
by the compiler and record its version; printing its path alone is insufficient.
Keep stock and distribution-patched compilers as distinct experiment arms.
Where the original toolchain cannot be recovered, state that limitation rather
than attributing every mismatch to source.

## Infer options through controlled comparisons

Collect evidence for language dialect, ABI, instruction set, floating-point
model, exception/RTTI support, position independence and optimisation. Treat
absence as a constraint whose strength depends on the inspected code: no
particular instruction may reflect source choices as well as target options.
Likewise, absent exception-runtime imports alone do not prove an option's
spelling.

The retained common comparison profile in this tree is:

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
```

This is a tested reconstruction baseline, not a uniquely recovered original
command line. Architecture and scheduling options answer different questions:
`-march` bounds available instructions; `-mtune` affects choices for a processor
within that bound. Local source changes and inlining can also compensate for
an incorrect optimisation hypothesis.

Use [period.mk](../../tools/toolchain/period.mk) and its actual build commands
for reproductions, rather than copying the common profile as a complete recipe.
It adds include paths, apparatus support and `DSPLIB_REPRODUCE_BUGS`; C++ has
additional options, and DCR has a provisional per-file candidate. The file
records those qualifications. A comparison with the compatibility define
missing can change data and behaviour before source reconstruction begins.

To test an option hypothesis:

1. Rebuild an unchanged control with the retained compiler and complete flags.
2. Name the observed mismatch and the instruction, relocation or compiler
   stage that the proposed option should affect.
3. Cross the option change with the plausible source change. Include source
   only, option only, both and unchanged controls where applicable.
4. Compare complete translation units, including non-exact bodies, data,
   binding and exports. Record exact gains and losses by symbol name.
5. Decide whether the result supports the hypothesis, requires another
   discriminating test or exhausts the declared domain.

Keep invalid runs labelled and excluded from conclusions. A net gain in exact
functions can hide regressions or compensating assumptions. The detailed
[experiment procedure](experiment-design.md) gives acceptance and stopping
criteria.

## Locate the first compiler stage that diverges

When ordinary source remains non-exact, compare compiler intermediate forms
as well as assembly. GCC's register transfer language (RTL) represents
operations, values and machine constraints through successive passes. GCC 3
dump switches and filenames differ from newer GCC releases; use the selected
compiler's help and a small control to establish which dumps it provides.

Follow the first appearance of the difference. This diagnostic map describes
the questions used in this project; pass ordering and names depend on the
compiler build:

| Stage or mechanism | Question to investigate |
|---|---|
| Expansion and early simplification | Were widths, constants, operations and address expressions lowered as expected? |
| Common-subexpression elimination and loop transforms | Were loads reused, stores moved, loops duplicated or values promoted? |
| If-conversion | Did a branch become a conditional value computation? |
| Register allocation and reload | Did live ranges, pressure or constraints cause a spill or reload? |
| Scheduling and x87 stack conversion | Were operations reordered or assigned different x87 stack positions? |
| Assembly and partial linking | Did alignment, relocations, section order or symbol records change? |

Use a small probe to isolate a mechanism, then transfer the hypothesis back
to the complete original translation unit. A standalone helper omits inlining,
aliasing and compiler state from neighbouring definitions. GCC 3 can make a
later function's allocation depend on earlier compilation activity; moving
definitions or inline helpers can therefore affect bystanders. The
[refinement guide](refinement.md) records measured examples and counterexamples.

Diagnostic pass disabling helps identify a cause. It does not establish that
the original author used that option. Require independent source or build
evidence before adopting it. Preserve unresolved scratch-register and padding
differences rather than inventing source operations solely to force them.

## Separate compiler fidelity from portability

Run differential tests with the period compiler as soon as the harness works
there. On x87, a change in spilling can alter narrowing, comparisons and
outputs. A modern-only green test can encourage casts or `volatile` stores
that the original compiler did not need. First test the ordinary source under
the recovered compiler, then investigate modern portability separately.

In this project `make period` has no tolerance or allow-list. Modern
floating-point tolerances are separately configured and counted; they do not
apply to the period oracle or excuse changed integer outcomes, object bytes
or transcripts. See [compiler policy](compilers.md) for the precise boundary.

Keep verification support outside reconstructed production code. A period
compiler may need compatibility support for assertions or the harness. A
modern compiler may demand semantics or runtime support absent from the
original build. Prefer an appropriate build option, then an apparatus shim
where needed, and verify the effect under both compilers. Moving even an
inline helper can affect GCC 3 output, so measure rather than assuming that
an organisational change is inert.

## Measure identity at the scope you claim

Use progressively stronger comparisons and preserve what each excludes:

| Comparison | Useful result | Remaining limitation |
|---|---|---|
| Size or instruction count | Fast triage | Equal length says little about operands or behaviour |
| Mnemonic sequence | Finds broad structural similarity | Ignores significant operand differences |
| Function bytes with relocation-aware comparison | Establishes the comparator's defined function-body identity | Does not settle whole-object layout or all anonymous data |
| Register-renaming equivalence | Classifies some non-identical bodies | Requires validated rules and remains weaker than byte identity |
| Strict final partial-link comparison | Checks sections, data, relocations and symbol records together | Does not identify unique original source text |

In this repository, build fresh period objects before reading identity reports:

```sh
make tc
python3 tools/toolchain/byteident.py --why FPM_div
python3 tools/toolchain/byteident.py --list-exact
make partial-link
python3 tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
    build/partial/dsplibs.o --require-exact
```

The final command is a completion gate and may fail during reconstruction;
`make partial-compare` produces a diagnostic census. Capture a baseline exact
set before editing and compare it with the rebuilt candidate set. Check
relocation types, target identity and addends: masking encoded addresses
without validating their relocation meaning can produce a false match.

Maintain negative controls for comparators, including changed constants,
member offsets and relocation targets. Report skipped or unresolved cases
and the number examined. A clean report from an empty or stale input set is
not evidence of identity. See [gate design](gates.md) and
[equivalence evidence](evidence.md) before strengthening an identity claim.
