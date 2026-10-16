# Tone IIR interval storage and arithmetic recovery

**Later partial adoption:** the expanded-stage/short-counter candidate was
revalidated with this interval correction and the separately witnessed shift
capture, then retained under explicit partial-reconstruction authorisation.
It remains non-exact; see [the current controls](recovery-call-partials.md).

The declared eight-cell domain crosses current versus expanded-four-section/
short-tap-counter source, short versus int interval storage, and signed division
versus arithmetic shift. The expansion is exactly the previously closed seed;
no new section, counter or shift-capture spellings were searched.

Original `cadence_create` loads the complete buffer length at 0x7d767 and
stores 32 bits into configuration +0x10 at 0x7d77a. `toneiir_progress`
compares the complete word at 0x7c7f1. The constructor's short read at
0x7c3cc is the argument boundary to `GetFP_Value(short, short)`, not the
storage boundary. Original 0x7c820 uses SAR 2 for the envelope comparison.

Retain int interval, the full-width cadence producer, and the arithmetic
shift. The constructor still implicitly narrows its short callee argument.
Remove the former +0x12 pseudo-padding and its zero initializer in both
configuration objects. All offsets, extents and configuration data stay equal.
The SAR is original representation evidence; no negative-envelope public
reachability claim is made.

## Experiment and collateral

Run `tools/recovery_toneiir_interval.py --domain <parent-plan>
--baseline-dir <frozen-baseline> --historical-headers` at baseline `426814ea`. The parent frozen
baseline is `source-recovery-next/build/recovery-unblock/baseline`.
Artifacts live in this worktree's `build/recovery-toneiir-interval`:
actual commands, source/header hashes, compiler/selected assembler identity,
RTL, raw unchanged control, complete bodies and relocation-aware verdicts.
The shared experiment helper appends `DSPLIB_REPRODUCE_BUGS`. The retained
Gentoo GCC 3.4.2-r2 full O3/register-renaming profile is unchanged.

All 8/8 cells compile; the unchanged full TU is raw-byte identical. Every
cell retains 3/8 exact functions with zero gains or losses. Tone progress
sizes, in `(seed,width,SAR)` order, are:

| Cell | Bytes (reference 1141) |
| --- | ---: |
| 000 | 692 |
| 001 | 674 |
| 010 | 691 |
| 011 | 673 |
| 100 | 1126 |
| 101 | 1096 |
| 110 | 1123 |
| 111 | 1101 |

Two additional Cadence cells reproduce the unchanged full TU byte for byte
and compare the full-width producer; both retain 2/4 exact functions, with
zero gains or losses. Only `cadence_create` changes, and configuration data
and bindings agree.

The expanded-short seed is not retained. Its helper remains 782 versus 802
bytes regardless of the new axes. Constructor and six bystanders are
unchanged in the matrix. This finite interaction domain is closed without
whole-function recovery; proximity alone does not justify another spelling
search.

A fresh complete 300-object Gentoo build audits every transitive header
consumer. 298 objects are raw identical. Only `cadence_create` and
`toneiir_progress` bodies change. Cadence remains SIZE(255); tone progress
moves SIZE(449) to SIZE(468). Existing exact membership is preserved with
zero gains/losses. Symbol inventory, binding, visibility and type are unchanged;
only tone progress's function size changes. Every non-text section is identical,
including both 44-byte configurations and their data relocations. Full body
relocation comparison finds no other changed body. Records:
`build/tone-object-audit.json`, `build/tone-full.diff`,
`build/tone-cadence-full.diff`, and `build/tc_candidate`. Both retained
objects independently reproduce their tested matrix cell bodies and canonical
relocations. Cadence retains all 113 canonical relocation targets (including
call/data targets); tone progress has no relocations. Changes in instruction
addresses, branch displacements and register scheduling remain visible in
the complete disassembly diffs.

## Differential boundary and negative control

The new Tone IIR fixture creates separate original/reconstructed objects from
valid component configurations with interval 65536+500. Its low short word
is nonzero 500, so constructor conversion remains defined and derives 35
required intervals. Ordinary zero samples then produce exactly one verdict
at sample 66036. This establishes the component configuration boundary, not
public modem-call reachability. No internal negative envelope is planted.
The lifecycle passes 66,172 checks. A cadence constructor sweep additionally
passes the full buffer length through the real producer: four groups with
218, 221, 220 and 220 checks.

Linking the unchanged archived Tone IIR object against the new fixture is the
negative control. It exits 1; the first discrepancy is sample index 499,
old source verdict 1 versus reference 0. The complete 66,172-check group
reports 134 failing checks in `build/tone-negative.log`; command is preserved in
`build/tone-negative-command.txt`.

Focused deciding period selection passes 5/5 binaries, zero failures:
`t_toneiir`, `t_cadence`, `t_detector`, `t_voicesvc`, `t_callprog`.
There is no registered Tone IIR mutation suite; existing cadence mutation
locators contain neither removed padding nor the changed producer expression.
No mutation suite or portability execution is claimed. The parent owns the
integrated strict whole-object measurement and final phase gate.

The isolated branch's full `make phase J=4` subsequently passed: period
388 binaries passed, zero failed; structural/provenance checks passed,
including 285 mutation suites and 10,040 static anchors, zero broken.
This is a reconstruction gate, not a portability or final-object completion
claim. The integrated branch still needs its own gate.
