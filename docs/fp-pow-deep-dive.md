# FP_Pow: an EXACT candidate and its memory contract

The 2026-11-04 deep dive at `2bdd7930e5ec1cc77213e18bbad0762acba5f555`
finds an **EXACT scratch candidate** for `FP_Pow`: all 55 non-padding native
instructions, register relationships, branch targets and normalised relocations
match. Production source remains unchanged and NONMATCH. Integration needs a
maintained test-memory binding for the original seventh coefficient read.

## Native constraints and recovered source

`Fact_FP` is a twelve-byte symbol containing six signed shorts:
`16384, 8192, 2730, 683, 136, 23`. Native `FP_Pow` at `0x7e1b0` has two signed
16-bit indexed loads, at `0x7e1d0` and `0x7e208`. Each relocation resolves to
the table base, with effective index scale two. Both loops process indices
0 through 6 unless an earlier contribution terminates them. The latch compares
the old short index with 5, increments it, and sign-extends the new short.
The seventh read reaches the next symbol, `IIRFilterScales[0]`, whose value is 3.
This is the original out-of-bounds read documented before this inquiry.

The current reconstruction substitutes 3 through `fp_pow_coef(int i)` and uses
an int counter with a counted for-loop. Prior experiments independently changed
the counter, lookup and latch but retained the helper. They did not close the
function. The successful candidate combines the observed native constraints:

```c
short i = 0;
do {
    /* existing arithmetic and early termination, with direct Fact_FP[i] */
} while (i++ <= 5);
```

Both native arithmetic arms are preserved: shift the term before multiplication
for large inputs; multiply before shifting for small inputs. The direct lookup
is confined to `DSPLIB_REPRODUCE_BUGS`. The normal arm retains the guarded
six-entry lookup and fallback value 3. The scratch helper still contains an
unused reproduction arm; remove that redundant arm during integration and
recheck the unchanged full-TU result. Scratch comments also require updating:
they still describe the old substitution and overstate overflow avoidance.
There is no claim of uniquely recovering the author's original spelling.

## Why the helper mattered after inlining

The decisive comparison is between the prior `post-index` cell and the new
`direct-post-index` cell. In initial RTL, the helper's int formal parameter
creates an early copy of the promoted short index. The post-increment separately
creates a late old-value copy at the latch. In the actual Gentoo compiler's
`.06.cse` dump, CSE redirects the comparison to the early helper copy. Its live
range now spans the loop body, contributing to an extra saved register and the
wrong allocation. Removing the helper call at the two reproduction lookup sites
removes that early formal-parameter copy; the native allocation is recovered.

The local upstream GCC 3.4.2 `gcc/cse.c` explains the observed mechanism:
`make_regs_eqv` maintains equivalent-register chains and `canon_reg` chooses
an older equivalent representative. This source explains the actual dumps;
identity of the entire upstream and Gentoo-patched compiler source is not
claimed. The causal evidence is the controlled source change and Gentoo dumps,
not an assumption that an inlined helper must be code-generation neutral.
An independent review reproduced the live-range distinction, complete-TU
collateral result and all eight synthetic expected values.

| Full-TU cell | FP_Pow instructions | Longest native run | Grade |
|---|---:|---:|---|
| Current control | 57 | 11/55 | NONMATCH |
| Prior short counter | 59 | 11/55 | NONMATCH |
| Prior direct-read helper | 53 | 23/55 | NONMATCH |
| Prior post-index with helper | 58 | 11/55 | NONMATCH |
| Direct lookup plus post-index | 55 | 55/55 | EXACT |

The unchanged control reproduces the current faithful object byte for byte.
All cells use the Gentoo GCC 3.4.2-r2 reconstruction profile and BUG mode.
Only `FP_Pow` changes in the winning complete TU. `GetFP_Value` stays NONMATCH;
`Get_Detection_Threshold_Table` and `TONE_read` stay EXACT. Table bytes, symbol
bindings and other non-executable data are unchanged. No compiler flag or
fidelity-recogniser change is involved.

## Differential evidence and the missing memory environment

The candidate's own six-word table ends its object section. In the investigated
period executable, the following linked word is 18241, whereas the reference's
following word is 3. Matching instructions therefore cannot make the unbound
linked candidate behave like the reference. Enlarging the source table would
change its measured extent and hide the original operation.

A one-off scratch binder redirects only the two candidate table-read relocation
operands to the original reference table. It preserves the six-word source
definition, all old symbols and unrelated relocations. The existing D302 binder
is unchanged: its six-byte table, single read and negative addend are a separate
contract. The scratch tool is exploratory: it uses assertions and writes before
all postconditions have run. It is not ready for production or grading input.

The runtime path first relinks the unchanged period `t_fp_math`; that executable
is byte-identical to the existing baseline. The same ordered 305 supporting
objects and period link flags are then used for the candidate probes.

| Period probe | Result |
|---|---|
| Unchanged relink control | Pass |
| EXACT candidate, own linked table neighbour | 89,858 FP failures |
| EXACT candidate, original reference backing | All 109,293 FP comparisons pass |
| Complete existing t_fp_math with shared backing | All 155,755 checks pass |

The FP denominator comprises 32,767 small inputs, 76,460 large inputs and
66 boundary inputs. The unbound failures comprise 13,332 small, all 76,460 large
and all 66 boundary comparisons. The remaining 46,462 checks cover coefficient
self-checks and `GetFP_Value`. These are component-level machine-fidelity probes;
they do not establish defined ISO C behaviour for the original over-read.

A second fixture binds both native and candidate reads to an actual seven-word
allocation. It keeps the six original coefficients and varies word seven between
-7 and 19. Inputs are 16383, -16383, 16384 and -16384, covering both arithmetic
arms and signs. Eight paired calls pass all 24 checks, including independently
calculated expected results. The current literal-3 implementation, subjected to
the same binding, fails 16/24 checks; the eight reference checks pass. This
negative control establishes sensitivity to a real seventh-word read.

For seventh word `s`, the expected results are respectively
`44526 + floor(16377*s/16384)`, `6030 + floor(-16380*s/16384)`,
`44532+s` and `6032-s`. This synthetic context tests the read contract; only
the original-backed context represents the blob's historical neighbouring data.

Two positive scratch binding runs produce stable bytes. Five malformed inputs
are rejected: unsigned load, wrong coefficient, wrong table extent, nonzero
addend and missing read site. These controls support the investigation but do
not replace a maintained binder's complete rejection/atomic-publication suite.

## Recommended bounded integration

1. Add an explicit FP_Pow test-memory profile: twelve-byte six-coefficient table,
   exactly two signed 16-to-32-bit indexed reads, scale two and zero table-relative
   addend. Preserve D302's separate admission checks. Require named failures,
   atomic publication, unchanged unrelated symbols/data/relocations, and cache
   invalidation when context or binding code changes.
2. Retain the original-backed differential and the synthetic -7/19 fixture with
   its literal-3 negative control. Exercise malformed ELF/profile/site cases and
   the existing D302 controls. Keep binding outside production and fidelity
   objects. Do not change the source table to seven entries.
3. Integrate the short post-incremented counter and reproduction-only direct
   lookups. Retain the guarded normal-mode fallback. Validate the normal build
   separately; its runtime has not been exercised in this inquiry.
4. Recheck complete-TU fidelity/data, focused period FP and CallingTone checks,
   then run one integration checkpoint with phase, census, actual/faithful parity
   and applicable partial-link/CI checks. Expected promotion is one NONMATCH to
   EXACT, subject to these checks; no global promotion is claimed now.

The next useful work is this integration, rather than another source-spelling
search. Stop and investigate if the maintained binding cannot prove the two
exact read sites or if any bystander changes. Do not weaken the grader or use
source padding to force the seventh word's placement.

## Reproduction artifacts

Local evidence is under `scratch/fp-pow-deep/`: `compile.py`, `results.json`,
`control/`, `direct-post-index/` (full compiler dumps), `FP_Pow-native.asm`,
`table-extents.json`, `binding.json`, `runtime-probes.json`, `probe-controls.json`,
`seventh-probe.c`, `synthetic-probes.json`, drivers and individual runtime logs.
Earlier losing cells are under `scratch/readage-width-wave2/fp-pow/`.
These ignored scratch artifacts are local, not a maintained public tool API.
The synthetic driver's compile-command metadata was corrected after its loop
had overwritten that field with a link command; its runtime/link results were
unchanged. Source SHA-256 is
`db78ce71e225db0b0274007591595a5aa0f8a8adba169cdffce3ad6902906943`;
candidate object SHA-256 is
`8e2e7d1b809b9f64d3f60f45728f4a34cebc68d25afc6f4d97ffa11f3c78698c`.
