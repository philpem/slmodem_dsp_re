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

So a *properly constructed* V.34 modem and a V.90 modem are the same work.
`test/unit/t_v34call.c` proves sequencing between two hand-brought-up arenas
(findings 780-788) and deliberately does not connect; connecting needs this
span. That is what settled the ordering rather than any preference for V.90.

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

## The caveat on the golden-object oracle

It rests on a blob-constructed object being usable as a reference. If the
object holds many pointers into blob code — the four `Resampler` vtables are
the obvious risk — then driving our code on it is a hybrid that proves nothing,
and every batch's constructor stays untestable until the span is complete.
That measurement is the deciding one; do not plan around the oracle until it
has been made.
