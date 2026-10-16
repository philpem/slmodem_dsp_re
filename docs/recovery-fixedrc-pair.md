# FixedRC interpolation-pair representation

Baseline `d9995470`, following the closed traversal, compaction, helper-origin
and dispatch domains in `recovery-fixedrc-results.md`. This domain changes
only the two adjacent interpolated values in mode zero. It does not reopen
cursor spellings, declaration order, reset/create or compiler flags.

Original `RcFixed_Resample` stores the first Q15-times-two result with a
low-half mask/OR at `0xb1769..0xb1787`, then merges the second result into
the high half at `0xb17fd..0xb181c`. The combined carrier is subsequently
read as low word at `0xb183e` and high word through SAR16 at `0xb198c`.
The retained source uses unrelated scalar `short low, high` locals.
This witnesses two short components in one full-width carrier, without
establishing the author's aggregate spelling.

Declare three complete-TU cells: unchanged, a two-short array, and a struct
containing those same two shorts. Preserve declaration scope, evaluation
order, Q15 narrowing, times-two truncation, output order and all consumers.
Prediction: aggregate representation may recover the original partial-word
merge and high-half extraction. Falsifier: the aggregate remains scalarised
or produces a different carrier graph. A size improvement alone is no basis
for retention. Do not try packing shifts, unions or register constraints
after a miss without a new original witness.

Use the complete baseline configuration, shared experiment-toolchain helper
and mandatory `DSPLIB_REPRODUCE_BUGS`. Require raw unchanged-TU reproduction,
all-body and nontext/binding/relocation audits, protection of every baseline
exact name, and period differential validation before a source commit.

## First result and terminal-pointer boundary

All three cells compile and reproduce the unchanged full TU raw. Array and
struct produce the same complete object, 2,456 Resample bytes versus 2,640
original (SIZE184), with no complete exact gain or loss. Both recover the
original low/high mask-and-OR carrier and SAR16 extraction; initial RTL gives
the pair one SI aggregate carrier. The production source is still untouched.

A distinct pointer-owner witness remains: each original second circular
range derives its cursor from the terminal first-range cursor, for example
LEA `0xae(%ebx),%ecx` at `0xb1746`. Retained source discards that cursor and
restarts from `history + count`. Its valid predecrement first loop ends at
`history`, so `cursor += count` gives the same valid one-past second-range
start without forming a before-array pointer. This changes the carried
producer, rather than testing another increment/decrement spelling.

Declare four complete-TU cells crossing the observed pair aggregate with
this terminal-pointer-derived wrap start, including an unchanged control.
Prediction: the second range carries the first range's pointer instead of
reconstructing its base from history. Inspect original and candidate address
graphs and complete TU collateral before judging sizes. A miss closes the
owner domain; no nearby cursor alternatives follow automatically.

The four cells have no exact gain or loss. The carried wrap start restores
the original terminal-owner dependency, independently of the aggregate carrier;
the combined source remains SIZE263. These address and carrier observations
change the assumptions of the prior helper-origin control: that control
recovered private argument homes and the original 0x9c frame while lacking
both newly witnessed mechanisms. Declare one final four-cell cross of the
same two ordinary static-inline implementation helpers on the combined
source, plus an unchanged-TU control (five compilations). This tests their
interaction with the aggregate and carried pointer; it does not search
helper order, attributes or arbitrary argument/declaration permutations.
Prediction: private argument copies and frame pressure can be reviewed
together with the original packed carrier. Audit every emitted function;
do not adopt helper provenance merely for a matching frame or smaller gap.

## Closed result and validation

The final five cells have no complete exact hit. Resample sizes are 2,377,
2,379, 2,387 and 2,375 bytes with neither helper, cascade only, polyphase
only and both helpers respectively. No helper-origin alternative is adopted.
This closes the interaction domain; matching private homes alone does not
establish the original factoring.

Across all three domains, 12 complete-TU compilations provide 60 shared-body
verdicts and three raw-reproduced unchanged controls. The scalar control
lacks the original low/high mask merges; every aggregate control recovers
them and high-half extraction. Array and struct emit identical objects.
`tools/recovery_fixedrc_pair_audit.py` checks those positive/negative
controls and every cell's symbol metadata, named data, allocated nontext
contents and relocation records. All agree. Only Resample changes; no exact
name is gained or lost. The prior two exact functions remain exact.

Retain the array carrier and terminal-pointer wrap owner, both supported by
the measured original graph. The complete body remains SIZE263. Its canonical
object raw-matches the independently compiled combined cell, SHA-256
`f90eb83eea93a48d6ec3191629fdfdb3fbc12527caf6fdcaef7c1ed5e093cea3`.
Focused deciding `make period T=t_rcresample J=3` passes 1/0; its existing
36 groups and 242,701 differential checks include constructed mode-0/1
lifecycles. The final integrated gate is reported separately.

Artifacts live in `build/recovery-fixedrc-pair/`,
`build/recovery-fixedrc-wrap-owner/`, `build/recovery-fixedrc-pair-origins/`
and `build/recovery-packers-next/fixedrc-audit.json`. Each retains actual
commands, source/header/object hashes, full dumps and complete body grades.
Replay the three runners with `--domain docs/recovery-fixedrc-pair.md
--baseline-dir ARCHIVED_D9995470_OBJECTS`; then run the audit.

Next evidence would need to distinguish the remaining original cursor
schedule and private argument origins beyond these closed families.
Declaration/register/slot permutations and another local cursor spelling
are not justified by the surviving size gap.

The selected predecrement loop terminates at `history`, whereas the original
machine post-load cursor terminates one element before it. The following
wrap cursor has that same one-element bias until the candidate predecrements
for its load. Both read exactly the same in-array elements for valid positions;
the owner relationship is restored, not the exact pointer value/schedule.
A machine decrement after the final load does not prove the author formed
an invalid C pointer. The previous domain's before-array prohibition bounded
that experiment; it does not rule out an original postdecrement spelling.
