# INFO1a capture recovery and ring-capacity correction

This batch restores two V34 value boundaries and fixes a constructor mismatch
against the blob. Neither complete function becomes byte-exact. The starting
revision is `1ec83de9`, on PR #284; upstream master remains `736cf61c`.

## V34 INFO1a: cache the result and both destinations

Original `V34GiveINFO1aBits` sign-extends the `bitreverse` result before its
debug branch, then loads both session pointers before either byte store.
Retained source delayed the word conversion and loaded the second pointer
after the first byte store. Capability is stored before upstream on both sides.

The six-cell full-TU cross separates the cached carrier from destination
capture. Both `short uinfo` and an `int` with explicit assignment narrowing
produce identical objects, with and without pointer capture. The selected
short cache and two pointer locals restore both independently witnessed
boundaries in native output. Initial RTL contains the explicit sign extension
into `uinfo` and named destination captures before the stores. The unchanged
cache has its result hidden in a call placeholder at that stage; the audit
does not invent an absent assignment node.

Only this body changes among the TU's seven functions; metadata, data, exports,
call targets and all bystanders agree. The SIZE gap stays 16 bytes. These
changes do not identify unique original declarations, and no reachable modem
history that aliases a destination with the session pointer fields is claimed.

## Constructor comparison must precede narrowing

At `FAXVMI_create+0x56`, the original computes `max_frame + 3` as a promoted
integer and compares that full sum with `fifo_size`. Its fallback value is
narrowed only after selection. Retained source instead assigned the sum to
an unsigned word before comparison.

For `max_frame = 65534` and requested FIFO capacity 8, the blob selects one
word and the old source selects eight. A fixed nine-case constructor regression
measures this difference without modem processing or planted internal state.
It uses documented null slot 0, ordinary/equality controls and three wrap
boundaries; selected ring allocations contain at most eight words. The test
compares initialized contents through the smaller extent so the pre-fix
refusal cannot itself overrun either allocation. It releases both constructors'
allocated objects after each row.

The old source fails **6 of 172 checks**: the expected capacity and normalized
framer state disagree for each of the three wrap rows with FIFO request 8.
The blob agrees with every explicit expected capacity. After the correction,
all 172 checks pass. The other five fixture groups also pass unchanged.
These are constructor-component results; they do not establish that a complete
fax call supplies these configurations.

Four full-TU controls cross the requirement width and selected ring width.
Adopt an `int need`, with explicit unsigned-word narrowing in the fallback
arm, and retain the existing word ring carrier. Ring width alone cannot fix
the arithmetic. Initial RTL changes from a narrowed unsigned-word comparison
to a full signed integer comparison; all possible promoted values here are
nonnegative and at most 65538. The SIZE gap remains 103 bytes.

Static arithmetic enumeration separately checks all 196608 pairs of the top
three `max_frame` values with all 65536 FIFO capacities. The old formula
disagrees with the instruction-derived formula in 196602 pairs; the repaired
formula agrees throughout. For lower `max_frame` values the sum fits a word,
so the old and new expressions agree algebraically. This enumeration executes
no blob or modem code and is separate from the nine runtime constructor cases.

All four controls preserve symbol metadata, named/allocated data, calls and
the five other TU bodies. Together with INFO1a: **10 complete-TU cells and
66 shared function verdicts**, raw baselines reproduced, no exact gains or
losses. The initial audit had a syntax error and then incorrectly expected an
explicit baseline assignment inside the RTL call placeholder; both failures
were apparatus failures. The corrected audit checks the actual serialized
nodes and known positive/negative native boundaries.

## Aligner repair and declined leads

`bbalign.py` failed on the Decision caller when jump-table inspection imported
`elftools`, then `inspect`, which resolved `dis` to this repository's CLI.
Preloading standard-library `dis` with the tools directory temporarily absent
from the import path repairs the CLI. The large Decision comparison now runs;
the different `FPM_SDM_init` control fires on four alignment groups, and the
exact `RcFixed_Check_Combination` control stays clean on 43 instructions.
No byte comparison or instruction normalization is changed.

The corrected Decision comparison has 759 original versus 752 retained
nonpadding instructions and 76 mnemonic-alignment groups. These groups are
not a recovered source CFG. Its frame and saved-register differences alone
provide no new type or statement-order witness, so no source variant was
compiled for this function. Existing definition-order controls remain closed.

`reset_cid` already has the original CWTL/word test in its byte-clear loop;
the debug-window nomination was a scheduling false positive. The FDSP creator
nomination concerns a success predicate, not the allocation-size argument;
the existing staged-success domain remains closed. Four previously untouched
nominated bodies were reviewed here; 28 remain untouched. Other windows in
reviewed functions are not thereby certified.

## Validation and replay

The two targeted non-fuzz period binaries pass, followed by the complete
permitted set: **387 passed, zero failed**, with `t_fuzz` explicitly excluded.
The constructor regression adds one group of 172 checks to its existing binary;
the binary denominator remains unchanged. Fuzzing and mutation execution remain
excluded; no full `make phase` or modern-portability claim.

Strict census remains **1085/1852 EXACT and 120105 original exact bytes**, with
no gains or losses; the ratchet passes. Only two of 300 canonical objects change,
298 remain raw-identical, both selected cells match production raw, and all
300 faithful-link inputs match canonical objects. The complete compiler profile
is unchanged. Partial `.text` remains 702364 bytes and allocated PROGBITS
918366 bytes. Positioned equal bytes increase from 69392 to 69398 of 943398;
strict partial comparison remains DIFFERENT (exit 1). These observations do
not establish whole-object equivalence. All structural targets pass, including
285 suites/10038 static anchors and 14445 resolved references. No mutation
locator or snapshot changes were needed. F11984 records the checkpoint.

```sh
python3 tools/gcc3_v34_info1a_capture.py --domain docs/history/v34/v34-info1a-capture-domain.md
python3 tools/gcc3_vmi_ring_capacity.py --domain docs/history/project/vmi-ring-capacity-domain.md
python3 tools/gcc3_v34_vmi_boundary_audit.py
make period T='t_v34info t_faxvmicp' J=8
```

Replay requires baseline objects and headers matching the declared revision;
use the shared driver's explicit historical options after later changes.
Experiment commands, hashes, objects and RTL live under the two corresponding
`build/gcc3-*` folders. `build/vmi-ring-capacity-before-run.log` preserves the
failing runtime result. The final audit is
`build/v34-vmi-boundary-audit.json`. The permitted test manifest and recorded
result are `build/v34-vmi-boundary-permitted-tests.txt` and
`build/v34-vmi-boundary-period-result.json`.

Next priority: continue the untouched nominations with independently witnessed
capture/return boundaries. For the Decision caller, first isolate an original
value lifetime or a specific RTL transformation; do not search local types,
definition order or frame-size spellings without that evidence.
