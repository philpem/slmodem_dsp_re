# `VPcmV34Main.cpp` — the plan for the last big span

290,315 bytes over 739 symbols: V.90, V.92, K56Flex, and the construction path
that builds all of them *and* V.34. The largest single thing left, and the only
route to 56k.

**The decision is taken.** This is being reconstructed, in parallel batches, by
agents in worktrees. What follows is the split, the order, and the things that
have already cost this project time.

## Why this is also the V.34 construction path, which was not obvious

There is no V.34 datapump. There is a **V.PCM** datapump, registered by
`dp_vpcm_init`, and it builds V.34, V.90, V.92 and K56Flex as one object which
V.8 later steers. The entry chain is

    dp_vpcm_init -> vpcm_create -> VPCMXF_Create -> VPcmV34Create

and the closure of those four is 588 symbols / 349,182 bytes, of which
**487 symbols / 249,590 bytes are unwritten**. 256 of the symbols in it are
`V90*` or `K56Flex*`.

### CORRECTION — a V.34 connection does NOT need this span written

**The first version of this document said "a properly constructed V.34 modem
and a V.90 modem are the same work". That is wrong, and findings 800-806
disproved it within the hour.** It is left recorded rather than quietly edited
because the reasoning was plausible and someone will re-derive it.

The blob's own constructors are aliasable, and a blob-constructed V.34 object
turns out to be a VALID DIFFERENTIAL FIXTURE rather than a hybrid:

    127 allocations, 265,520 bytes live across 125 regions
    BLOB-CODE pointers in the whole graph:  2
    blob DATA pointers (coefficient tables): 21
    vtables in the root arena:               0   (2 of 4 in heap sub-objects)

Both code pointers are `struct v34_object` function pointers —
`ref_descrambleGPA` / `ref_scrambleGPC`, mirrored by `caller` — and
`src/pump/v34/v34digital.c:85` installs that same pairing on that same
condition. We have all four functions and four suites test them, so **two
stores replace both pointers** and nothing routes our code into the blob's.
Driving one block of our `datapumpv34` on a blob-constructed object leaves it
byte-identical over all 53,848 bytes to what `ref_datapumpv34` leaves.

So the constructor can be BORROWED. A genuine V.34 originate/answer connection
is reachable now, and only V.90/V.92 actually need the 250 KB written. That
does not change what this document plans — every byte below is still required
for 56k — but it removes V.34 from the justification and it means **this span
is no longer on the critical path to a working V.34 modem.**

What the fixture cannot do is test the constructor itself: it uses the blob's
construction and configuration. So wave 1 gains a reference object to diff
against, not a free pass.

`vpcm_create`'s prologue, read at 0x3a00, states the contract:

    0x3a11  test %edx,%edx / sete    side flag from `caller` -- originate/answer
    0x3a1c  cmp $0x2580,%esi         srate MUST be 9600
    0x3a37  cmpl $0x30,...  / jg     max_frag must be <= 48
    0x3a42  allocates 0xd258         53,848 bytes

## The split

Sizes are UNWRITTEN bytes from
`tools/closure.py dp_vpcm_init vpcm_create VPCMXF_Create VPcmV34Create --missing`,
grouped by class. **Recompute before quoting** — finding 330 invalidated every
closure number written before it, in both directions.

### Wave 0 — SERIAL, before anything fans out

| group | bytes | sym |
|---|--:|--:|
| `V90Parameters` | 12,004 | 7 |
| `V92Parameters` | 1,964 | 5 |
| `Resampler`, `V90Resampler`, `ResamplerTiming`, `ResamplerTimingOffset` | 4,981 | 32 |
| `Scrambler`, `Descrambler` | 2,242 | 34 |
| `FloatARMA`, `Psd` | 1,983 | 7 |
| **total** | **23,174** | **85** |

**These are shared types and base classes, and they are nobody's.** Half the
constructors in the span take a `V90Parameters *`, and the four `Resampler`
classes carry the vtables. `docs/v90rest.md` records that the V.90 line split
by CLOSURE and put two batches on the same header; the rule that came out of it
is one class, one owner, and the corollary is that a type everyone includes has
to land before anyone forks.

### Wave 1 — the construction path, and the oracle it may unlock

| group | bytes | sym |
|---|--:|--:|
| free functions (`dp_vpcm_init`, `vpcm_create`, `VPCMXF_Create`, `VPcmV34Create`, …) | 31,127 | 69 |
| `VPcmFloModem` | 7,020 | 6 |
| `V90Modem`, `V92Modem`, `K56FlexFloModem` | 2,598 | 13 |
| **total** | **40,745** | **88** |

Big enough to split in two if it resists. It goes early for a reason beyond
dependency: **the blob's own constructor is aliasable** — `ref_dp_vpcm_init`,
`ref_vpcm_create`, `ref_VPCMXF_Create`, `ref_VPcmV34Create` all exist, because
the Makefile globalizes file-locals before renaming. If a blob-constructed
object can be used as a reference, every later batch can diff its constructor
output field-by-field against it instead of being untestable until the whole
span works. Whether that holds is being measured; see the caveat below.

### Wave 2 — the receive chain, fan out freely

| batch | groups | bytes |
|---|---|--:|
| A | `V90Equalizer` | 20,612 |
| B | `V90ConstellationDesigner`, `V90ConstellationPower`, `V90TRN2Designer` | 25,754 |
| C | `V90Phase3Demodulator`, `V90Phase3Modulator` | 18,192 |
| D | `V90AutoDigitalImpDetector` | 16,728 |
| E | `V90Demodulator`, `V90Demapper`, `V90PreFilter` | 17,238 |
| F | `V90Phase4Demodulator`, `V90Phase4Modulator` | 15,129 |
| G | `V90CP`, `V90MP`, `V90Jd`, `V90RDetector`, `V90SdDetector` | 14,925 |
| H | `V90ConnectionEvaluator`, `V90SpectralShaper`, `V90SpectralVerifier`, `V90SpectralShapingFilter` | 12,149 |
| I | `GenericToneDetector`, `ANSamToneDetector` + strays | ~1,300 |

### Wave 3 — V.92

`V92Phase4Modulator` 7,129, `V92ModulusEncoder` 6,910, `V92Modulator` 4,174,
`V92Transmitter` 2,869, `V92EchoCanceller` 2,845, `V92ConvolutionEncoder`
2,592, `V92Jd` 2,409, `V92Phase3Modulator` 2,124, `V92CP` 2,026,
`V92BitsToSymbol` 1,472, `V92Precoder` 1,323, `V92PreFilter` 550, `V92Mapper`
229, `V92Phase2Info` 91 — **36,743 bytes**, two batches.

### Wave 4 — LAST, on purpose

`V90Modulator`, `V90BitsToSymbol`, `V90Mapper`, `V90SignBitsExtractor`,
`ModulusEncoder`, `ModulusDecoder`, `V90Phase2Info` — **6,817 bytes**.

This is the **digital-side sender**, and the blob never enters it: `vpcm_create`
passes a literal 0 to `VPCMXF_Create`, so the side is always 1, the analogue
client (findings 701, 702). A path the blob never enters cannot be driven
differentially, so only the codegen tier applies and the first evidence it
*works* is interop against live hardware. Doing it last keeps the differential
rule intact for everything above it.

## Rules for the batches, each of which has already cost time

- **RE-RECORD THE MUTATION SNAPSHOT ONCE, AFTER ALL MERGES.** Never per agent.
  The key in `tools/mutsnap.py` is deliberately coarse over `Makefile`, `src/`,
  `include/` and `test/harness/`, because every test binary links every object
  and a precise key would be "a precise lie". So ANY batch merging ANY `src/`
  change invalidates all 54 entries. Ten batches re-recording individually is
  ten wasted 20-minute sweeps. This was learned the expensive way.
- **Assign each batch a block of 20 finding numbers in its brief.**
  `docs/findings.md` conflicts on every merge and numbering has collided nine
  times. Taken so far: 1-756 and 780-788.
- **Never `git add -A`.** A mutation run patches `src/` in place while it runs,
  and agent worktrees live under `.claude/`. Name the paths. Finding 705.
- **`make phase` is the gate, not `make test`.** `export BLOB` (merged
  2026-08-09) is what lets it run in a worktree at all; without it `strings`
  fails on the blob path.
- **One class, one owner.** Never split a class across batches, and never let
  two batches own the same header.
- Merge one batch at a time and grep for a distinctive string from each side
  afterwards — `git checkout --ours` takes the whole file (finding 700).

## The golden-object oracle — MEASURED, and it holds

The caveat this section used to carry has been resolved. Findings 800-806: two
blob-code pointers in 265,520 bytes, no vtable in the root arena, no
function-pointer table, and our `datapumpv34` leaves a blob-constructed object
byte-identical to what the blob's leaves. **Every later batch can diff its
constructor's output field-by-field against a blob-constructed reference**
instead of being untestable until the span is complete.

Two things the next user of it must handle, both recorded in 806:

- `VPcmV34Create` leaves `+0x2218` at **0** (the data branch); something must
  write 2.
- Two constructions must be made CONGRUENT, or pointer fields must be excluded
  from the comparison — 125 heap regions come back at different addresses.

And two configuration parameters are ADDRESSES, not numbers —
`MDMPRM_DPRUNTIME` is dereferenced and `MDMPRM_DSPINFO` segfaults in
`vpcm_delete` if the harness default is used. Five overrides were needed to
construct at all (DPRUNTIME, DSPINFO, MIN_RATE 2400, MAX_RATE 33600, IODELAY
40), which bounds the result: **the configuration is plausible, not
recovered.** Deriving it properly is wave 1's job.
