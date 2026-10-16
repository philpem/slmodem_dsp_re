# Source-recovery implementation, 16 October 2026

Branch: `investigate/source-recovery-next`, based on upstream `4169cb3b`.
The [implementation plan](source-recovery-next-plan.md) and its
[opening audit](source-recovery-audit-2026-10-16.json) remain the historical
starting point. This report records implementation results separately.

## Validated result

The integrated source at `98c4caec` passes **388 period differential binaries, zero
failures**, and the default structural/provenance checks. Its fresh census
is **1,091 / 1,852 exact function names**, covering **122,715 / 720,125
original bytes**. Against the clean rebuilt baseline of 1,086 names and
120,322 bytes, that is five additional exact functions and 2,393 bytes.
Every baseline exact name remains exact.
The durable ratchet now protects all 1,091 names. Its earlier twelve-name
baseline catch-up is not counted as a campaign gain.

| Newly exact function | Original bytes |
| --- | ---: |
| `DemodDataB103` | 568 |
| `DemodDataV17` | 415 |
| `DemodDataV27` | 331 |
| `DemodDataV29` | 398 |
| `_tx_scrambled_ones_state` | 681 |

The decisive changes cross original operand and lifetime witnesses:
an int-formal/use-site-narrowing SRE representation reproduces the original
caller promotions, alongside unsigned low-word producer results,
scratch-pointer acquisition after the entry guard, child owner reloads,
and ownership of the published FIFO count. The later B.103 cross is detailed
below. The original SRE formal type
remains unproved. A single local
cast often made no progress because another boundary still forced the wrong
extension or lifetime. See [SRE boundaries](recovery-sre-boundaries.md) and
[FIFO results](recovery-fifo-results.md) for complete-TU controls and evidence.

## Structural recovery and measurement

* [FixedRC](recovery-fixedrc-results.md): implemented omitted kind-1 mode-0/1
  cascades, with 36 differential groups and 242,701 checks. The function
  remains nonexact. The original kind-0 arm uses runtime-tap convolution;
  the earlier specialization claim was corrected rather than implemented.
* [DIL packers](recovery-dil-results.md): restored fixed field regions, CRC
  stage movement, count read ages, padding cursors and narrow floating inputs.
  Both complete packers remain nonexact. Mutation locators retain their
  original alternatives; no mutation snapshot or tolerance was changed.
* [Diagnostic scratch extent](recovery-encode-extent-results.md): restored
  the original 270-byte bug-compatible array. Both function bodies retain
  their prior grades; this is data fidelity, not a function-count gain.
* [Completion gates](recovery-completion-gates.md): the comparator now covers
  previously omitted ELF metadata, undefined symbols, relocation target
  identity and group policy. `make partial-compare-exact` requires both
  semantic and literal raw identity and propagates failure. A reference
  self-comparison passes both contracts.
* [Residual ledger](recovery-residual-ledger.md): inventories all 300 TUs,
  1,852 shared names and 1,886 emitted shared copies, including SIZE bodies.
  It resolves 50 direct local-call sites without inventing targets for 95
  indirect calls. It records refusals and denominators explicitly.

## What kept the search in a local minimum

The opening audit's concerns were borne out in concrete cases. Symbol
presence hid omitted FixedRC arms. Small near-match filters excluded much
of the remaining original code. Single-axis local edits could not remove
constraints imposed by another call boundary. Extracted common tails and
generic loops obscured original fixed regions. Compiler tracing explained
the retained source but could not supply evidence of the original source.

The implemented alternative is a bounded source cross tied to original
operands, control flow or value ownership. It reproduces unchanged complete
objects first, audits every emitted body and storage record, and protects
the entire prior exact set. A smaller size gap is diagnostic evidence only.
Negative domains stay closed until a distinct witness supplies a new test.

The [local-call investigation](recovery-local-boundaries.md) demonstrates
that stopping rule: a private float capture restored GetGain's witnessed
read age but remained nonexact, so production source was restored. Repeated
receiver calls could not be safely paired by physical block order and were
refused. The period-image libm `e_pow.o` also differs from the original;
it was inspected as an input-provenance lead and was not substituted.
The extracted object is retained at `build/recovery-libm-input/e_pow.o`:
`readelf -sW` reports a 613-byte `__ieee754_pow`, constants in `.rodata`
and an additional `p63`. The reference instead has an unsized `pow`
and its eight named constants in `.text`. Recover the control with
`ar p /usr/lib/libm.a e_pow.o` inside the pinned period image. This rules
out that archive member, not every possible historical library input.
`src/core/pow.S` deliberately emits only FILE records; its body is supplied
by the reference during differential links. The 1,852-name sized-function
census does not include or certify this input. Literal whole-object
completion must resolve it separately.

## Reproduction and completion limits

The deciding image is
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, repository digest
`sha256:14efe17550e390798c81d78a612b538af6c8bd70b8b6fc75ee19115147ef5d96`.
Every reconstruction experiment uses the retained complete profile and
`DSPLIB_REPRODUCE_BUGS`. Baseline artifacts live in
`build/recovery-baseline/`; the first integrated checkpoint lives in
`build/recovery-integrated/`, including object hashes, census and gate logs.
Final evidence lives in `build/recovery-final/`: `gates.log`,
`byteident.json`, `object-audit.json`, `partial.json` and `checkpoint.json`.
The configuration is unchanged; 289 of 300 objects remain raw-identical
to the baseline in the final integrated branch. The ratchet update consumes
that fresh census after rechecking all 300 hashes, configuration and the
successful gate result; its three positive/negative self-controls pass.

```sh
make phase partial-link J=4 TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
TC_OUT=build/tc_repro python3 tools/toolchain/byteident.py --ratchet
make partial-compare-selftest
make partial-compare-exact
```

The last command still fails as intended. Function identity canonicalises
relocations; complete ELF identity additionally requires original positions,
layout, symbols, metadata and raw contents. At the final checkpoint the
hardened comparator reports 70,072 / 943,398 equal positioned allocated
bytes, 421 / 2,931 exact symbol records and 988 / 18,317 exact target-aware
relocation records. Both semantic and raw verdicts remain DIFFERENT.
The comparator schema changed during this campaign, so its symbol and
relocation figures must not be directly compared with the opening schema.

Local master was fast-forwarded to `origin/master` at `4169cb3b`, with the
owner's existing edits preserved. Cleanup removed stale registrations for
already absent worktrees; useful experiment artifacts and active worktrees
were retained. No modern portability claim, mutation-score claim or public
modem reachability claim is implied by the period component tests.

## Expanded return-mode screen

The [return-mode screen](recovery-return-mode-screen.md) covers all 1,852
reference names without the old size or source-family exclusions. It proves
both extension modes for 147 of 4,703 aligned calls and explicitly refuses
the remaining 4,556. Seven disagreements include three positive controls
from the SRE recovery and four fresh nominations. This catches a gap in the
older count-only heuristic: one signed extension and one unsigned extension
have equal counts but different operand semantics.

The resulting B.103 cross adds exact `DemodDataB103`, 568 original bytes.
It jointly restores unsigned low-word FSD result capture into an int slot,
short loop induction with unsigned count comparison, child-owner reloads,
the original literal AGC argument, a full-width return representation and
the success-fallthrough branch graph. The AGC argument is unused; changing
it is argument fidelity, not a claimed output fix. Original source types
are not uniquely established by the representation. The other 16 TU bodies
and all four other header-consuming objects remain raw-identical.
The test now compares the complete return carrier. Six focused period
binaries pass; the other three nominees close without source adoption.

## CP/MP structural family

The [CP/MP report](recovery-bitpacker-results.md) records 32 valid full-TU
cells and 504 body verdicts. Fixed CP field regions, MP fixed-stage CRC
movement, per-Type encoder CRC bodies, full-width selector representation,
integer absolute-value checks, and original capture/publication boundaries
are retained. There are no whole-function exact gains or losses. Remaining
size differences are CP 69 bytes, MP encoder 15 and MP decoder 49; these
are not measures of instruction similarity or evidence of unique source.

The MP encoder change also changes registers in the already nonexact
`evaluateCRC` body, despite unchanged source there. Its 18 operand-only
substitutions are preserved in the audit; no stronger register-mapping
equivalence claim is made. Existing
focused and full period differentials validate it. Mutation alternatives
retain both encoder arms after the former shared tail is restored per arm.
The next discriminator is the original rate-mask TEST/SETNE versus retained
AND/MOV through saved RTL, together with the Type lifetime/stack carrier.
The recorded spelling, expansion and boundary domains are closed.

## Next implementation queue

Continue from the final reports and their explicit stopping conditions,
rather than repeating the opening matrices. The remaining work includes:

1. Recover structural SIZE bodies alongside small residuals. For FixedRC,
   separate the restored cascade semantics from its remaining code-generation
   difference. For DIL, test a new original CFG/induction witness before
   reopening a closed ceiling or declaration domain.
2. Extend the call-boundary method only where correspondence and first use
   are proved. Repeated calls in reordered blocks, indirect targets, aliases
   and incompatible call topologies remain refusals until a stronger CFG
   correspondence resolves them. Extension counts alone are not evidence.
3. Keep ABI/storage and compiler-profile questions crossed with source
   structure when the original supports both explanations. Require unchanged
   TU reproduction and complete collateral audits; do not select a profile
   by its net exact count.
4. Recover excluded historical link inputs, original data extents, TU/export
   boundaries and ordering separately from function recovery. Use the strict
   semantic/raw gate for the final claim. A 1,852/1,852 function census would
   still leave obligations outside that denominator.

For each new domain, write the original witness, finite axes, predicted graph
change, falsifier and next discriminator before compiling. Run focused period
differentials, then `make phase`, and ratchet every newly validated exact name.
Preserve raw unchanged controls and rejected candidates as replay artifacts.
