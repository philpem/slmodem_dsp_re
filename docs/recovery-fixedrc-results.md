# FixedRC mode-0/1 reconstruction

Baseline `4169cb3b`, Gentoo GCC 3.4.2-r2, the unchanged complete reproduction
flags and mandatory `DSPLIB_REPRODUCE_BUGS`. The unchanged experimental
translation unit reproduces the saved production object raw:
`1b73cfce04b794f0ae34475d49e157a5ad9b1e42915bb276a45523d58d01f73b`.

## Recovered component boundary

The original `RcFixed_Resample` dispatches kind 1 at `0xb14b5`; its two
mode arms begin at `0xb15f5` (interpolation) and `0xb1513` (decimation).
The existing constructor already creates both states. The new fixture first
fails against the omitted implementation, then passes with the recovered
conversion: 2,560 calls, 40,432 reference outputs, eight constructed pairs.
Each pair crosses constructor, processing, reset, reused histories and delete.
These are component lifecycle witnesses; modem callers select other modes.

Mode 0 applies the three-tap droop filter, inserts a zero between two
87-tap stages, then inserts zeros between four 15-tap stages. Each FIR
narrows its Q15 result. The 87-tap outputs double with 16-bit narrowing;
the final 15-tap results multiply by 31957 and shift by 13. Mode 1 halves
each input before the 15-tap filter, runs the 87-tap filter every two
inputs and the droop filter every four, then multiplies by 32440 and shifts
by 13. The two decimation phases are local to each call, starting at zero.

Limits are checked after producing output. Mode 0 emits four-sample groups
and uses an unsigned greater-or-equal test; a zero limit still emits the
first group. Mode 1 checks after each fourth input produces one output;
zero similarly stops after its first output. The kind-0 polyphase arm has
its distinct equality test, where zero means unlimited. Zero input counts
produce zero outputs in all three arms.

The constructor/reset clears four bytes per history. The initial convolution
reads other history bytes provided by the harness allocation fill; the
fixture labels those separately from constructor writes. It compares all
three allocation extents and payloads, all three coefficient-table payloads,
scalar state and record padding after normalising only the six pointer
addresses. Separate guarded output arrays cover impulse, constant, full-scale
alternation and deterministic mixed inputs. No history state is planted.

## Correction to the historical size explanation

F610/F611 identify specialization as the reason for the large size deficit.
The complete original body now separates the regions: kind 0 contains one
generic runtime-tap Q14 inner product at `0xb13df..0xb1434`. The numerous
literal-length two-loop convolutions are in the omitted kind-1 arm itself.
There are seven interpolation and three decimation FIR instances. Therefore
the old prose does not establish a separate missing per-mode polyphase
specialization. Preserve that history, but do not schedule a new matrix on
that inference.

## Declared first code-generation domain

The indexed recovery produces a 2,303-byte body against 2,640 original bytes.
Only Resample changes among the eight emitted functions; the same two of five
blob-common functions remain exact. Two independent observed boundaries
justify a four-cell full-TU source cross under unchanged flags:

| Axis | Indexed recovery | Alternative |
| --- | --- | --- |
| Circular traversal | Indexed backward history reads | Valid predecrement cursors for the two backward ranges |
| Polyphase compaction | `memmove` | Ascending short-pointer copy loop |

The original uses descending unit-step history pointers in each circular
range; it copies the polyphase tail forward without a library call at
`0xb13a0..0xb13ad`. Predict those exact addressing/call boundaries before
reading sizes. The cursor alternative must never form a before-array pointer.
The copy preserves the valid allocated compaction extent and direction.
The repeated indexed baseline must raw-reproduce its saved object. Complete
bodies, data, relocations and globals remain the comparison unit. A recovered
local boundary does not establish complete byte identity; if either axis
fails its prediction, close it rather than expanding cursor synonyms.

Artifacts, complete commands, compiler dumps and hashes live under
`build/recovery-fixedrc/`. Results of the declared cross and final gates
will be recorded below.

The original also promotes the signed-short mode before its two dispatch
tests (`0xb14fb..0xb150d`: MOVSWL, TEST32, DEC32). The recovered if/else
instead tests a word and places the interpolation arm first. This is a new
instruction-width/control-flow witness, independent of FIR traversal. Declare
two unchanged-profile full-TU cells: repeated recovered if/else and an ordinary
switch over the same mode. Predict the promoted mode dispatch and original
arm layout; retain default-arm behaviour and all widths. Do not retype mode
or expand to adjacent predicate spellings if that prediction fails.

## Code-generation results and stopping boundary

| Cell | Resample bytes | Strict verdict |
| --- | ---: | --- |
| Indexed recovery, `memmove` | 2,303 | SIZE337 |
| Circular cursor only | 2,500 | SIZE140 |
| Forward compaction only | 2,256 | SIZE384 |
| Circular cursor and forward compaction | 2,426 | SIZE214 |
| Above, kind-0 implementation helper | 2,435 | SIZE205 |
| Above, kind-1 implementation helper | 2,423 | SIZE217 |
| Above, both implementation helpers | 2,469 | SIZE171 |
| Cursor/copy source with witnessed mode switch, retained | 2,437 | SIZE203 |
| Original | 2,640 | reference |

The four-cell traversal/copy domain has four different emissions. Both
predictions fire: circular histories use descending unit-step cursors, and
the extra `memmove` import/call disappears. The valid predecrement cursor
keeps all source pointers inside their declared arrays. It does not reproduce
the original post-load cursor schedule. The larger SIZE gap in the copy-only
control does not negate recovery of the observed call boundary.

A second four-cell diagnostic crossed ordinary static-inline kind-0/kind-1
implementation helpers. The original captures input/output arguments into
separate private homes for its two branches (`+0x58/+0x50`, `+0x88/+0x80`).
Helper-origin controls recover private argument copies and the original
`0x9c` frame in the combined cell, but their home ordering and complete bodies
still differ. Source helper provenance remains unproved; no implementation
helper was retained merely because its size or frame was closer.

The final two-cell dispatch control restores the original MOVSWL/TEST32/DEC32
mode promotion and places the decimator before the interpolator, without
changing the stored signed-short field. This supported boundary is retained.
Ten valid domain cells produce nine distinct source byte strings, eight
distinct complete objects and fifty blob-common body verdicts. Deliberately
repeated controls reproduce raw. All cells retain eight emitted functions,
the same five shared definitions and the same two exact functions. Only
Resample changes. Zero exact gains and zero exact losses are claimed.

One dump-isolation setup initially wrote GCC's dump stream in the working
directory, where later cells overwrote it. Those unattributed dumps are
preserved under `unattributed-dumps-invalid` and excluded from stage evidence.
All eight traversal/helper-origin cells were rerun with per-cell working
directories: every corrected object raw-reproduces its earlier object, and
248 separately attributed dump files survive. A separate zipped-symbol-list
audit incorrectly paired records after removing the `memmove` import; it is
preserved as `whole-object-audit-invalid-zipped-inventory.json`. The corrected
name-keyed audit below explicitly reports removal and does not invent changes
to subsequent data symbols.

The missing conversion is closed. Complete byte recovery remains open:
Resample is SIZE203, Create is SIZE14 and Reset is BYTES41. Existing
factory/reset/deletion domains remain closed. The measured cursor schedule,
private-home ordering and the packed interpolation-pair carrier remain
distinct residuals; neither forced registers, declaration permutations nor
another nearby cursor spelling follows from this finite result.

## Final verification

`make phase J=6` exits zero: **388 period differential binaries pass, zero
failures**, with structural checks green. The type census is 335 types in
168 files with zero known duplicates; the state has one shared definition.
Anchor validation checks 285 suites and 10,040 static mutation rows with zero
detached, repeated, ineffective or wrong-arm anchors. No mutations or fuzzing
are executed. The fixture's eight kind-1 groups each pass 9,008 checks,
including table target identity as well as payload equality. Its original
18-mode and reset/limit groups continue to pass. Negative control logs retain
the known omitted-path failures before reconstruction.

The complete 300-object rebuild changes only `src_core_FixedRC.c.o`.
Its SHA-256 changes from
`1b73cfce04b794f0ae34475d49e157a5ad9b1e42915bb276a45523d58d01f73b` to
`e8c916139f41c752416f889f5a2298764d6586ebd6c56b6ba05f1cbf22add869`.
The latter equals the selected experimental object raw. All allocated
nontext bytes, NOBITS size and nontext relocation records in this TU agree.
Defined symbol names, types, bindings, visibility and sizes are unchanged
except for the reconstructed Resample body size; the undefined `memmove`
import disappears as predicted. There are no added definitions.

The complete strict function census remains **1,086/1,852 exact**, covering
120,322 of 720,125 original compared bytes. The full exact-name set has zero
gains and losses; 667 SIZE, 65 BYTES, 29 REGALLOC and five unresolved remain.
The existing exact-set ratchet passes. The final fixed fixture has 36 passing
groups and 242,701 checks in total; 72,064 checks belong to the eight added
kind-1 lifecycle groups.

Both complete 300-object partial links remain DIFFERENT, strict exit one:

| Measurement | Before | After |
| --- | ---: | ---: |
| Positioned equal allocated bytes / 943,398 | 69,291 | 69,396 |
| Allocated candidate bytes | 919,060 | 921,012 |
| Exact relocation records / 18,317 | 1,031 | 1,031 |
| Candidate relocation records | 18,308 | 18,307 |
| Exact defined-symbol records / 2,907 | 397 | 397 |
| Exact section contents / 92 | 67 | 67 |
| Candidate NOBITS / 2,836 | 2,812 | 2,812 |

Raw partial-link file identity also remains false. A 1,944-byte body increase
uses 1,952 allocated bytes after alignment. Functional reconstruction and
supported local compiler boundaries are the retained result; neither the
remaining 203-byte length gap nor the positioned-byte gain is byte identity.

Replay the production checks with `make period T=t_rcresample`, `make phase`,
`make partial-link` and the standard `byteident.py`/`partialcmp.py` commands.
The saved isolated experiment driver, nine input sources, complete commands,
header/config hashes, corrected dumps, raw objects and disassemblies remain
under `build/recovery-fixedrc/`. `declared-cells.json`, `whole-object-audit.json`,
`final-phase.log`, `functional-fixture.log` and `partial-{before,after}.json`
carry their separate denominators. Compiler portability was not run.
