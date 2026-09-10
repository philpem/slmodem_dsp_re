---
name: reconstruction-experiment
description: Use when working on issue #22, partial-link fidelity, byte-exact recovery, compiler profiles, or source-and-flag experiments.
---

# Reconstruction Experiment

Treat the reference object as canonical. Current source and flags are
hypotheses, including currently exact functions. Never read, search, or
reference `re/`.

Read `AGENTS.md`, `docs/method/experiment-design.md`, and the relevant issue
before choosing a domain. Use the recovered Gentoo GCC 3.4.2-r2 toolchain and
`DSPLIB_REPRODUCE_BUGS` for every reconstruction compilation.

## Experiment Contract

1. Reproduce the unchanged full-TU baseline through the experimental path.
2. State the competing explanations, prediction, falsifier, bounded candidate
   domain, and cell count before compiling.
3. Keep source form, optimization level, individual passes, tuning, and
   definition/emission order as separate axes until evidence links them.
4. Record complete commands, compiler/assembler identity, revisions or hashes,
   compile/rejection counts, and shared-symbol denominator.
5. Score full TUs with the existing tools. Compare exact gain and loss sets,
   bodies, relocations, exports, call boundaries, and changed nonexact code.
6. A loss under an alternative profile may be an inaccurate source
   reconstruction that happens to compensate under the retained profile. It is
   evidence to investigate, not a veto of the profile.
7. Complete the declared domain. After two non-informative batches, reframe
   from a new discriminator rather than retrying nearby spellings.

## Acceptance

Do not select near matches, pad code, remove behaviour, weaken a comparator, or
infer a global profile from an aggregate score. A positive candidate requires
independent review before retention, then the deciding `make period` gate and
relevant full-TU and partial-link before/after measurements. Run `make phase`
before calling a branch finished, subject to the compiler-divergence rules in
`AGENTS.md`.

## Resource Discipline

Use one worktree per modifying investigation. Do not run concurrent full gates
on the shared machine. Prefer focused loops during exploration; the parent runs
final period, phase, and partial-link checks after merge. Pass paid models a
compact evidence packet, not raw logs or an unbounded transcript.
