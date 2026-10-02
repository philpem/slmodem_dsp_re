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

## Allocator/reload trace resolves the ambiguity (F11547)

[Two unchanged complete-TU traces declared before execution](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944130707).
The installed Gentoo cc1 carries DWARF debug information. A host GDB/runtime
mounted read-only inside its container observes reg_renumber at global_alloc
entry, reload entry and reload return. Compiler and assembler remain Gentoo
image executables; shared helpers preserve the saved flags and append bug
reproduction. Both traced full-TU objects raw-reproduce the saved objects.

| Source | After local allocation | After global allocation / before reload | After reload |
| --- | --- | --- | --- |
| Computed shift | nbits/reg unassigned | nbits=EBP, reg=ECX, int counter=EBX | nbits=EBP, reg spilled, int counter=EBX |
| Short post-decrement | old shift temporary=ECX; nbits/reg unassigned | nbits spilled, reg=EBX, short counter=EBP | Same assignments |

Baseline synthetic counter 95 first appears in **.09.loop**: the dump reports
`giv at 58 reduced to (reg 95)`. It remains the direct shift operand through
.22.regmove/.24.lreg. The short form creates old-value temporary 81 in initial
RTL, narrows counter 76 before the shift, then shifts using temporary 81.
Local allocation gives temporary 81 ECX, producing the observed extra hard
conflict before global allocation. In the baseline, reload insn 59 instead
requires CREG for the low byte of counter 95 in EBX; its reload record names
CL. Reg66 was allocated ECX and is evicted during reload. This resolves the
previous checkpoint's allocation-versus-reload ambiguity for these controls.

Replay (saved objects/logs required):

```
python3 tools/detsequence_allocation_trace.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944130707 --analysis-only
```

Omit --analysis-only to rerun both unchanged complete-TU compiler traces.
Artifacts: build/detsequence-allocation-trace/{results.json,trace.gdb} and
V32prc/{baseline,shift-counter}/{candidate.o,trace.log}. Raw controls 2/2;
boundary snapshots 6/6. Exactness unchanged; mechanism evidence only.

## New, bounded source-lifetime control (F11548)

The trace supports a new factoring question, not a repeat of the earlier
counter spelling domain: does shifting first, then separately decrementing
short shift, remove the temporary that local allocation reserves in ECX?
[Two cells declared before compilation](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944170159):
previous found-once source as raw control, and the same source with shift--
in a statement immediately after updating reg and before testing the match.
The continuing state and extracted bit are unchanged.

The prediction holds. Initial RTL and the preallocation shift use short
counter 76 directly; there is no old-value copy at the shift. Its traced
allocation path matches the computed counter's: reg goes to ECX, nbits to
EBP, shift to EBX; reload evicts reg and supplies CL. The final short running
counter, bound and register spill locations now agree with the blob.
Scheduling still places decrement/narrowing between copying BL to CL and SHR,
so machine instruction order did not imply post-decrement in the expression.

Canonical result remains **non-exact**: control 290B/SIZE15, separate-decrement
275B/BYTES112 against blob275B. Both preserve 8 functions/global definitions
and 6/8 exact; only DetSequence changes, zero exact gains/losses. Remaining
observed differences include spill-slot offsets, scratch registers and
prologue/word-entry scheduling. No closest-score production adoption.
The two-cell source domain is closed; no nearby spelling matrix follows.
An initial generator assertion failed on the source's multiline formatting
before either compilation; /tmp/detsequence-shift-lifetime-invalid-generator.log
preserves that apparatus failure. The corrected generator ran the declared
pair without changing its domain.

Reproduction:

```
python3 tools/playbook_detsequence_shift_lifetime.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944170159
python3 tools/detsequence_allocation_trace.py --source-family playbook-detsequence-shift-lifetime --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5944170159 --analysis-only
```

Full inventories, body/relocation verdicts, source/header hashes and commands:
build/playbook-detsequence-shift-lifetime/results.json. The second trace's
raw controls 2/2 and snapshots 6/6 are in
build/detsequence-allocation-trace-shift-lifetime/results.json. Across the
two trace domains: 4/4 full-TU controls, 12/12 snapshots. No production/header/
fixture/anchor edits, fuzzing or mutation execution. Retained 860/1852 exact
is unchanged. Candidate differential/partial-link adoption checks NOT RUN:
no candidate is proposed for retention. A further experiment requires fresh
independent evidence about the remaining spill layout or scheduling.

## Final apparatus and validation

The final trace runner uses a file-based driver wrapper and waits for its
compiler.exit sentinel after GDB follows cc1. It checks driver/assembler exit
status and full-TU raw identity before accepting snapshots. Earlier attempts
at waiting via multiple GDB inferiors and an inline shell wrapper failed;
artifacts under build/detsequence-allocation-trace-invalid-multi-inferior,
build/detsequence-allocation-trace-invalid-inline-wrapper and the corresponding
shift-lifetime directories are preserved and excluded. Both final trace
pairs were rerun with the corrected runner: 4/4 raw controls, 12/12 snapshots.
These were apparatus repairs, not new source/profile domains.

Final fixed make phase: 385 passed, 0 failed; structural checks clean, including
14,223 references, 2,677 finding headings and 285 suites / 10,038 static anchors.
All 300 retained compiler objects were independently raw-rechecked against the
saved 860-exact baseline and remain identical. No modern portability claim;
upstream drift remains untested because the upstream checkout is absent
(the seven manifest files were checked against their manifest only).
