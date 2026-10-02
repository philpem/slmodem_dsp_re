# DetSequence allocation evidence checkpoint (2026-10-02)

PR #243 landed as f6ae3f07. This follow-up reads the six saved, hash-verified
Gentoo controls; **no new compilation or production change**. Allocation is
still an open explanation, not a license to repeat the closed spelling domains.

## Priority is insufficient

Upstream GCC 3.4.2 `global.c:623` sorts allocation candidates using
`floor_log2(n_refs) * weighted_frequency / live_length`, scaled by size,
with allocation-number tie breaking. The memory-cost figures from `.24.lreg`
are not this sorting formula. `find_reg` also considers register classes,
conflicts, preferences and locally allocated registers; reload follows.

Sources inspected:
[global.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/global.c),
[local-alloc.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/local-alloc.c).
These are upstream explanatory sources, not a claim to have reconstructed the
complete Gentoo patch stack. The executed Gentoo compiler and its dumps decide.
Saved source hashes and six object hashes are in
`build/detsequence-allocation-evidence/analysis.json`.

Across **6/6 objects**, pseudo 66 (`reg`) ranks first and pseudo 64 (`nbits`)
ranks fifth in the printed allocation order. Within each object, their
conflict sets are identical after removing the pair themselves. Yet their
final assignments differ:

| Controls | Final nbits assignment | Final reg assignment | Hard conflicts of both |
| --- | --- | --- | --- |
| Baseline, countdown | EBP | Spilled | EAX, EDX, ESP |
| Shift-counter, both, lifetime baseline, found-once | Spilled | EBX | EAX, EDX, ECX, ESP |

Thus the priority order alone cannot explain the final spill selection.
The computed-shift controls already recover the blob's nbits/reg spill choice,
although their shift graph differs. They are a positive comparison control,
not an adopted exact reconstruction.

## Earlier shift graph is different

In the baseline `.24.lreg`, pseudo 95 is a compiler-created running **int**
shift count: initialized to nbits-1, decremented without short narrowing,
preferred CREG, 4 references across 23 instructions. Its final assignment
is EBX. The compiler has already strength-reduced nbits-1-bit into a counter.
In the explicit-short control, pseudo 76 is a source variable, has signed
short narrowing on initialization and decrement, prefers CREG, and has
3 references across 28 instructions. Its final assignment is EBP.

This rules out treating the source's computed expression as necessarily a
recomputed subtraction in generated code. It also separates source-variable
lifetime/narrowing from the mere existence of a running counter.
The blob has a signed-short running shift count in EBX and nbits in EBP.
Neither tested graph reproduces that combination exactly.

The `.25.greg` file spans both allocation and reload. The final mapping and
conflict dump do not establish whether the decisive eviction happened in
initial global allocation or reload, nor why ECX became an additional conflict.
Do not infer that ordering, spelling, or memory cost caused the difference.

## Next evidence, not another spelling matrix

Trace pseudo 95's first creation and later live range through the saved loop,
life, regmove and allocation dumps; compare the explicit short pseudo 76 at
the same boundaries. Then trace the ECX constraint and reload requirements
back to the relevant variable-shift instructions. If existing dumps cannot
separate initial allocation from reload eviction, state the missing evidence
and design an instrumented allocator diagnostic before compiling candidates.

No new source/profile domain is declared here. No fuzzing or mutation execution.
The retained baseline remains 860/1852 exact; this checkpoint claims no gain.
