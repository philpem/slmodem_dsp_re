# GenerateAnsTone: pointer clear and direct elapsed field

Baseline9a16b620,866/1852 exact functions and84,159 exact bytes. Reference
GenerateAnsTone199B uses a signed count guard, advancing short pointer and
machine countdown in its silence arm. Retained196B source has an ascending
indexed loop. Tone-call narrowing and phase/elapsed semantics are identical.

Four complete-TU controls cross a copied signed-positive countdown with an
output cursor. Baseline196B/SIZE3; countdown/both174B/SIZE25; cursor199B/BYTES4.
The cursor with original ascending int loop recovers the complete clear loop.
Gentoo .09.loop reports "Can reverse loop" and "Reversed loop": the machine
countdown does not imply an author-written countdown. Preserve original count
for subsequent elapsed arithmetic and call narrowing.

The four remaining bytes are cmp and signed-branch opcodes for the two elapsed
thresholds. A second two-cell domain raw-reproduces the cursor object and tests
direct elapsed-field updates in each active arm against the merged temporary.
`ans->elapsed += count` followed by field comparisons, without the obsolete
common temporary store, recovers all199 bytes and the canonical FPM_TONE_generate
call relocation. Original >= tone / > silence thresholds, counter resets,
phase values, done-arm no-op and return1 are preserved. GCC coalesces direct
field stores into the same reference tail; source writes need not imply early
machine writes. No comparator/register/store-order permutation was used.

All6 valid cells preserve the TU's one strong function; all2 complete unchanged
controls raw-reproduce, staged against the saved cursor object. Final TU0/1 ->1/1,
no loss. There are5 distinct source forms/5 distinct emissions. The explicit
countdown/both forms compile to distinct complete objects despite equal sizes;
this is not inferred from score. No flags or public signatures change.

[Loop domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945628420),
[elapsed-field discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945643553).
Replay tools/playbook_anstone_loop.py then tools/playbook_anstone_elapsed.py,
each with --domain URL. Artifacts in corresponding build/ directories retain
complete Gentoo GCC3.4.2-r2/selected assembler2.15.92.0.2 identities, saved
flags and mandatory DSPLIB_REPRODUCE_BUGS, source/header/object hashes,
inventories and all changed-body disassembly.

## Validation scope

The existing fixed t_v32anstone uses component fixtures, compares output and
complete cadence/tone state, tests each phase, exact tone/silence boundaries,
multiple whole cadences, generator progress, zero/negative counts and lengths,
and untouched done/invalid phases. Component probes are not modem-lifecycle
reachability claims. No fixture or tolerance change, fuzzing or mutation
execution. The24-anchor suite is retargeted where needed to preserve their faults, including
counter-before-add, overrun carry, zero-loop and done-counter writes; no snapshot
refresh or new dynamic mutation result is claimed.

## Retained result

Complete comparison build300/300, zero failures; only v32anstone.c.o changes,
and its full bytes raw-reproduce the tested winner. Whole-tree866/1852 ->867/1852,
exact bytes84,159 ->84,358, only GenerateAnsTone gained, zero losses. Fixed
Gentoo make phase385 passed/0 failed; t_v32anstone reports76+8+286+49 checks.
Structural references/anchors clean;285 suites/10,038 static anchors all unique.
No changed fixture, snapshot refresh or modern portability claim.

Complete same-order300-object partial links remain DIFFERENT(strict exit1).
Positioned equality68,326 ->68,317/943,398; allocated bytes914,142 unchanged.
Exact section records70/92, symbol records394/2,907 and relocation records
1,018/18,317 unchanged. The canonical function gain does not establish
partial-link identity or a monotonic positioned-byte gain.
