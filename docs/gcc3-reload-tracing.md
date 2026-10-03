# GCC3 local allocation and reload reproduction

This compiler-only investigation belongs to [issue #246](https://github.com/philpem/slmodem_dsp_re/issues/246),
separate from the source/profile experiments in PR #245. Baseline is merged
master `47174bf3`. No reconstruction source, compiler profile, fuzzing or
mutation harness was changed or run. There is no new byte-exact gain.

## Reproduce the measurement

Use an unchanged production `tc_out` directory containing `.build-config`
and `src_pump_v34_V34TX.c.o`, built from the same source/header revision:

```sh
python3 tools/gcc3_reload_trace.py --self-test
python3 tools/gcc3_reload_reproduce.py --baseline-dir build/tc_out
python3 tools/gcc3_reload_trace.py \
  build/gcc3-reload-reproduction/full-rtl/V34TX.c.24.lreg \
  build/gcc3-reload-reproduction/full-rtl/V34TX.c.25.greg \
  --function updateAlpha --json build/updateAlpha-reload.json
```

The reproduction driver needs Docker and the saved production image. It
prints the compiler and **executes** its selected assembler for identification,
reuses saved flags and appends `DSPLIB_REPRODUCE_BUGS` through the shared
experiment helper. It saves complete commands, revision, source/object hashes,
compiler logs, dumps and `results.json`. Run with stdout captured to retain
the toolchain identity. Repeated runs replace individual case directories;
do not use those directories for hand-written artifacts.

The measured compiler was Gentoo GCC 3.4.2-r2, with assembler 2.15.92.0.2.
Full-TU compilation must reproduce the saved production object **raw**;
otherwise the experiment fails. A baseline from a different checkout is not
an interchangeable control.

## Seven compilation attempts

| Case | Result | Compared instruction UIDs | Stack substitutions |
| --- | --- | ---: | ---: |
| Full V34TX, plain / `-da` | Both valid; raw objects identical | 44 | 4 |
| Exact updateAlpha extraction, plain / `-da` | Both valid; raw objects identical | 44 | 4 |
| Extraction with debug condition disabled, plain / `-da` | Both valid; raw objects identical | 34 | 4 |
| Full TU, `-fdump-tree-all` | Rejected by cc1 | N/A | N/A |

Total: **6 valid compilations, 1 capability refusal**, 3/3 diagnostic pairs
raw-identical, 3/3 known spill graphs and 3/3 resident-quotient negative controls
pass. Full compilation contains seven defined functions. Full and extracted
`updateAlpha` are each 204 bytes; debug-disabled extraction is 139 bytes.
The reference function is 169 bytes. A smaller diagnostic body is not a
recovered source candidate. Debug-disabled code is explicitly synthetic.

Gentoo cc1 reports `unrecognized command line option "-fdump-tree-all"`.
This probe is a refusal, not a successful build with missing tree dumps.
`-da` works and did not change any of the three compared objects.

The parser also passed **8/8 synthetic controls**: known spills, a resident
negative, metadata exclusion, and five refusals (truncated expression,
unmatched close, missing function, duplicate UID, zero comparable UIDs).
Before fresh compilation, both historical F11660 width cells reproduced
the known stack homes and the resident negative. Those width experiments
remain closed; this is a new diagnostic boundary, not another spelling search.

## What happens to the values

| Value | Local allocation | After reload, full/extracted case |
| --- | --- | --- |
| Shift count, pseudo 71 | ECX | Supplies variable shift |
| Numerator, pseudo 73 | EDX | SI stack home at SP+28 |
| Divisor, pseudo 74 | EAX | Reassigned to ECX |
| Raw quotient temporary, pseudo 77 | EDX | Stack home at SP+24; HI read for sign extension |
| Named quotient `r`, pseudo 67 | Global allocation candidate | EAX; no observed stack substitution |

Global allocation order is `59 66 67 83 58 64 78`. Neither 73 nor 77 is
in that list: they already had local hard-register assignments. In the
debug-disabled control, their stack homes become SP+4 and SP+0, but the same
local assignments and substitution graph survive. Neither the rest of the
TU nor the debug call is required for this behavior.

UID 51 becomes a store of 1 into the numerator's stack home. UID 52 shifts
that stack value in place. At divide UID 55 the dump explicitly requests
AREG input reload for 73 and AREG output reload for 77, both using EAX.
Inserted instructions load the numerator and store the quotient temporary.
UID 56 sign-extends the temporary's HI stack read into named `r` in EAX.
The dump also explicitly reports divisor 74 reassigned to hard register 2.

These observations establish the allocation-to-reload transition. They do
**not** establish the exact ranking decision that initially assigned EDX,
nor prove what source/profile gave the blob its different allocation.
Calling this a spill of named `r` would conflate two different pseudos.

## GCC implementation context and next discriminator

Upstream GCC 3.4.2 sources explain the machinery; they are not a substitute
for the measured Gentoo executable. Relevant files are
[local-alloc.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/local-alloc.c),
[reload1.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/reload1.c)
and [i386.md](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.md).

`*divmodsi4_cltd` requires EAX numerator/result and an early-clobbered EDX
remainder, with register-or-memory divisor. The variable shift requires ECX.
Local `block_alloc` first tries suggested hard registers, then sorts quantities
by a priority involving reference count, frequency, size and lifetime, with
quantity-number tie breaking. Quantities can combine pseudos, so printed
pseudo use counts alone cannot reconstruct this ordering. Its attempts may
also use extended lifetimes to avoid false dependencies before retrying the
actual interval. `finish_spills` clears assignments for spilled pseudos and
retries global allocation while excluding previously used or forbidden homes.
The `.greg` boundary includes reload: it is not solely a global-allocation dump.

The next discriminating experiment is to instrument a provenance-validated
Gentoo compiler at `block_alloc`/`find_free_reg` and reload spill selection:
record quantity membership, suggestions, birth/death, priority, attempted
classes/hard registers and rejection masks for 73/74/77. Require the
instrumented full and extracted objects to remain raw-identical to these
controls. That can distinguish suggested-register allocation, ordering and
conflict rejection without another source permutation matrix. Until then,
the divide constraint explains why reloads are needed, but the initial
allocation choice remains open.

## Tool limits

The reader compares balanced RTL **instruction patterns** by persistent UID
and structural operand path. It excludes REG_EQUAL and dependency metadata.
It recognizes direct pseudo/subreg-to-SP-memory substitutions; inserted
instructions are inventoried separately. Changed pattern structures are
reported as unclassified rather than forced into a guessed correspondence.
The full and extracted cases each have two inserted instructions and six
unclassified restructures.

This is bounded to GCC3 i386 dumps: SP hard register 7 and first pseudo 53.
It does not infer liveness, identify every possible spill representation,
reconstruct CFGs, compare arbitrary passes or prove absence of spills from a
zero substitution count. It reports instruction and substitution denominators.
No runtime equivalence or reachable modem-state claim follows from these
compiler-only reproductions.
