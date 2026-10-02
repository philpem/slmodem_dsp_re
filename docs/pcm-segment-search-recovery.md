# PCM encoders: table cursor, late bound and input magnitude

Baseline 7ddff66c. The full pcm.c TU defines six public functions and eight
strong globals, including both conversion tables. Four functions are exact;
linear2alaw is SIZE18 and linear2ulaw SIZE2. Three finite domains isolate
observable source properties without changing flags or public signatures.

## Cursor and helper bound

Both blob encoders load from a table pointer and advance it by two bytes:
linear2alaw at 0xb07c0/3, linear2ulaw at 0xb0880/3. Retained search_segment
indexes seg_end by its segment counter. A two-cell whole-TU control replaces
that access with a const-short cursor. It recovers pointer progression but
leaves linear2alaw SIZE2 and linear2ulaw BYTES2. No adoption at that stage.

The latter differs only in `cmp 7; jle` versus blob `cmp 8; jl`. Initial RTL
already compares the fixed-bound loop with seven. A second domain uses the
cursor control as its raw baseline and tests a helper size argument passed
as eight, then table and size arguments together. Both parameterized cells
make linear2ulaw EXACT100B, leaving linear2alaw SIZE2; their complete objects
are byte-identical to each other. Initial RTL holds the inlined size pseudo
initialized to eight; late substitution preserves the strict comparison.
By first CSE the loop comparison uses literal eight, while initial RTL had
the size pseudo as its operand. The fixed-bound form already held seven in
initial RTL. This is a measured pass boundary, not a guessed original flag.

| Helper cell | linear2alaw | linear2ulaw | Exact TU |
| --- | --- | --- | --- |
| Indexed fixed table/bound | SIZE18 | SIZE2 | 4/6 |
| Cursor, fixed bound | SIZE2 | BYTES2 | 4/6 |
| Cursor, size argument | SIZE2 | EXACT | 5/6 |
| Cursor, table and size arguments | SIZE2 | EXACT | 5/6 |

This supports a late-inlined bound; it does not uniquely identify the
original helper signature. Retain the minimal size-argument spelling and
fixed table because an additional table parameter supplies no further
observable constraint. The helper remains private and both calls pass eight.

## Magnitude carrier

The blob's A-law entry loads the input directly into its magnitude register,
and the negative arm copies it before negating. The size-argument source
control instead loads the input separately and copies to a local mag. A
two-cell domain holds the helper fixed and reuses pcm_val for magnitude:
negative arm `pcm_val = -pcm_val - 8`, positive arm no copy. It recovers
linear2alaw EXACT110B, with only that body changed, TU exact5/6 ->6/6.

All seven valid cells preserve six functions/eight globals and their binding;
all three staged raw controls reproduce. The four previously exact bodies
remain untouched throughout. Canonical relocation targets include the same
segment table. Tables, masks, quantization, saturation and arithmetic types
are unchanged. Every tested domain is closed; no source/flag permutation.

## Replay and controls

The domains were declared before compilation:
[cursor](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945174855),
[bound](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945189426),
[magnitude](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945203488).
Run tools/playbook_pcm_cursor.py, tools/playbook_pcm_bound.py and
tools/playbook_pcm_magnitude.py respectively with `--domain URL`.
Their build/playbook-pcm-{cursor,bound,magnitude} directories retain sources,
header hashes, full commands, dumps, inventories/bindings, canonical verdicts
and all changed relocation-bearing disassemblies. Shared Gentoo GCC3.4.2-r2,
selected assembler2.15.92.0.2, saved complete flags and mandatory
DSPLIB_REPRODUCE_BUGS are recorded.

The first magnitude run had an incorrect domain URL. Preserve it under
build/playbook-pcm-magnitude-invalid-domain-url and exclude it. Both cells
were rerun with the actual predeclared link; adopted measurements use that
corrected replay. The earlier source and compiler controls remain valid.

The unchanged fixed t_pcm exhaustively compares both encoders across all
65,536 signed-16-bit values, plus positive and negative saturation values
32,768..40,000. It also covers all256 inputs for each code conversion. No
fuzzing, mutation execution, tolerance changes or fixture changes.

## Adoption validation

Complete retained build300/300, zero failures. Only src_service_pcm.c.o changes;
its full bytes raw-reproduce the corrected input-magnitude winner. Whole-tree
863/1852 ->865/1852, exact bytes83,848 ->84,058, only linear2alaw/linear2ulaw
gained and zero losses. All300 baseline objects and before/after exact-name
censuses are saved in `build/playbook-pcm-adoption/`.

Same-order complete300-object partial links remain DIFFERENT(strict exit1).
Positioned equality68,340 ->68,308 /943,398, reflecting later code shifts;
candidate allocated bytes914,174 ->914,158. Exact section records70/92,
symbol records394/2,907 and relocation records1,022/18,317 remain unchanged.
Canonical function gains do not establish whole-object identity.

Cumulative branch audit against the saved pre-GenSequence300-object baseline
finds only four changed TUs (V32prc, fpm_rms, DualTone_Detector and pcm). Its
exact-name delta contains exactly five gains—GenSequence, FPM_rms,
Dual_TONE_create, linear2alaw and linear2ulaw—with zero losses.

Fixed Gentoo make phase passes385/0, structural checks clean. Reference check
14,224 refs/2,687 finding headings; static anchors285 suites/10,038, all unique.
Three new replay tools compile and whitespace checks pass. No anchor retargeting,
snapshot refresh, fixture changes or modern portability claim.

## Rebase onto the finding-renumber fix

Rebased onto master41a7a017, which fixes refcheck's silent no-op when renumbering
F-prefixed headings. No finding collision required renumbering here. All7
historical commit pairs retain identical source/header/fixture/toolchain trees;
all300 retained Gentoo objects raw-reproduce after rebuilding on the new base.
Replay scripts now reference the corresponding reachable rebased baselines:

| Historical baseline | Rebased baseline |
| --- | --- |
| 3cbe7d52 | f206063c |
| 74cda31e | 62e9866f |
| 2be7a9e8 | a22e7229 |
| 7ddff66c | 1b81dd2b |

Original PCM experiment artifacts are preserved in their `-pre-rebase`
directories; fresh replays use the updated baseline IDs. Earlier historical
hashes in this ledger identify the actual original measurements, not new
source differences introduced by the rebase.

Fresh replay identity:7/7 source/object/inventory/binding/verdict records
match their pre-rebase controls, including3/3 raw staged baselines. The new
whole-tree census keeps exactly the same865 exact names and84,058 exact bytes.
Post-rebase fixed Gentoo make phase passes385/0, structural checks clean
(14,224 refs/2,687 finding headings;285 suites/10,038 static anchors).
