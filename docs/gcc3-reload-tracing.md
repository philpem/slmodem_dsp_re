# GCC3 local allocation and reload reproduction

This compiler-only investigation belongs to [issue #246](https://github.com/philpem/slmodem_dsp_re/issues/246),
separate from the source/profile experiments in PR #245. Baseline is merged
master `47174bf3`. The initial tool checkpoint changed no reconstruction
source and had no byte-exact gain; the follow-up below recovers the 169-byte
updateAlpha body. No compiler profile, fuzzing or mutation harness was changed
or run.

## Reproduce the measurement

Use an archived production `tc_out` directory containing `.build-config`
and `src_pump_v34_V34TX.c.o`, built from pre-recovery revision `47174bf3`.
The driver reads that historical source with `git show`; current recovered
source is deliberately not the original spill control:

```sh
python3 tools/gcc3_reload_trace.py --self-test
python3 tools/gcc3_reload_reproduce.py --baseline-dir build/gcc3-alpha-production-before
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

These observations establish the transition across the combined global/reload
dump boundary. The initial tool checkpoint could not distinguish which pass
evicted the values. The GDB follow-up below resolves that distinction.
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

The initial next discriminating experiment was to instrument a provenance-validated
Gentoo compiler at `block_alloc`/`find_free_reg` and reload spill selection:
record quantity membership, suggestions, birth/death, priority, attempted
classes/hard registers and rejection masks for 73/74/77. Require the
instrumented full and extracted objects to remain raw-identical to these
controls. That can distinguish suggested-register allocation, ordering and
conflict rejection without another source permutation matrix. Until then,
the divide constraint explains why reloads are needed, but the initial
allocation choice remained open at that checkpoint.

## Unchanged-compiler GDB observation closes the ranking question

The installed cc1 has full DWARF. `tools/gcc3_alloc_observe.py` copies that
unchanged executable, hashes it, prepares the same three historical inputs
through the period driver, and invokes host GDB with read-only callbacks in
`tools/gcc3_alloc_observe_gdb.py`. No compiler variables or instructions are
modified. The host libc differs from the image's; this is explicitly a
diagnostic execution environment, subject to unchanged-output controls.

```sh
python3 tools/gcc3_alloc_observe.py > build/gcc3-allocation-observation.log 2>&1
```

All three preparations raw-match their prior objects. Each container/plain
host/GDB assembly triple is raw-identical; assembling observed output with
the period driver's selected assembler reproduces each reference object raw.
Known coalescing and global-eviction controls pass in all three cells.
Observed event counts are 13, 13 and 12 for full/extracted/quiet respectively.
Commands, executable/source/object hashes, GDB version, host dependencies,
compiler logs and JSON events are retained under
`build/gcc3-allocation-observation`. Missing/optimized-out variables are
reported as unavailable. The tool is bounded to this compiler's DWARF types,
source locations, hard-register numbering and the historical pseudos.

Local allocation combines **73 and 77 into one quantity**, with seven
references, frequency1197 and birth/death6..16. Divisor74 has five references,
frequency855 and birth/death10..14. Neither has hard-register suggestions.
The priority formula favors the divisor: ignoring the common multiplier,
`floor_log2(5)*855/4 = 427.5`, versus `floor_log2(7)*1197/10 = 239.4`.
Divisor74 takes EAX first; the combined quantity fails AREG, then GENERAL
allocation chooses EDX. This is quantity ordering, not two independent
EDX assignment decisions.

Hardware watchpoints show **global.c find_reg** changing both73/77 homes to
-1 before reload. It allocates pseudo78, the divide remainder, in EDX.
Its eviction comparison uses local frequency1197/live-length14 against
remainder frequency171/live-length1:85.5 <171. Reload subsequently gives
those unallocated values stack homes. Divisor74's later reassignment to ECX
does occur in reload's finish_spills/retry_global_alloc path. Thus attributing
all three changes to reload would be wrong. The precise local/global
mechanism reproduces without the debug path or enclosing TU.

## Source cross recovers the complete function

The blob ADDs0x8000 into the normalized energy register, then shifts that
same value; our original expression produced a separate local divisor.
This supports testing an in-place update of the by-value energy parameter.
Unlike a register-fitting rewrite, it changes a source boundary directly
visible in the blob. Cross it with the independently measured HI quotient
boundary from F11660:

| Full-TU cell | updateAlpha | adaptecho | Exact functions |
| --- | ---: | ---: | ---: |
| Baseline | 204B | 795B | 1/7 |
| Short quotient only | 205B | 795B | 1/7 |
| In-place energy only | 170B | 779B | 1/7 |
| In-place energy + short quotient | **169B EXACT** | 779B | **2/7** |

Replay with `tools/gcc3_alpha_energy_reproduce.py
--baseline-dir build/gcc3-alpha-production-before --domain
https://github.com/philpem/slmodem_dsp_re/issues/246#issuecomment-5973663607`.
Use the pre-recovery baseline archive when replaying. Four valid full-TU cells produce four
distinct objects; raw baseline reproduces. Energy-update cells have zero
observed stack substitutions, with numerator/result locally assigned EAX.
The denominator remains the cross-block energy pseudo rather than a new
local allocation candidate. The combined cell matches every byte and all
three canonical relocation targets in the reference updateAlpha.
Run `python3 tools/gcc3_alpha_energy_audit.py` for the full-TU nontext,
metadata, exact-body and register-only bystander checks.

All seven functions and zero named data objects were reviewed across the
four cells: symbol metadata, allocated nontext extents, raw nontext and
canonical nontext relocations agree. Production changes updateAlpha and its
inlined adaptecho copy; txinit changes register colours only (alpha-equivalent
to baseline); four other bodies remain unchanged. There is one exact gain
and no loss in this TU. This is an evidence-supported source family, not
proof of unique original spelling or complete recovery of adaptecho.

Production validation reviews all **300** objects: only V34TX changes, and
that object raw-reproduces the combined candidate. Whole-tree exact census
is **926/1852 →927/1852**, exact bytes **95,435→95,604**, sole gain updateAlpha,
zero losses. Fixed period gate reports **388 passed, 0 failed**, with
structural checks green. Existing updateAlpha coverage passes588 numeric
checks and85 debug-transcript checks; txinit's44,041 checks pass too. These
are component fixtures, with the original excluded divide-trap/nonterminating
inputs unchanged; no new end-to-end or mutation coverage claim. No anchor
changes were needed;285 suite declarations/10,038 anchors remain attached.
The same-tree period partial link shrinks text by48B including alignment;
positioned reference bytes increase68,571→68,904, equal relocations
1018→1028/18,317, exact sections remain61. Both whole-object comparisons
remain DIFFERENT. Function identity is the completed gain, not full-object
or profile completion.

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
