# Waves 36–37: visual owner boundaries and constructed component histories

Base: `e000ad3775faea412ecc03242459a7a270caaa30` (PR #414). These waves follow
Astra's review in issue [#409](https://github.com/philpem/slmodem_dsp_re/issues/409):
complete the four-cell PCM-subowner/resampler-base cross and supplement the
synthetic visual grid with constructor-grounded component histories.

## Wave 36: a four-cell owner/base cross

The retained wave-35 source and period flags are frozen. The predeclared axes are:

- PCM: flattened `xf->echoCanceller` member reads versus an explicit pointer to
  that existing `V92EchoCanceller` subowner.
- Resampler: receiver-rooted versus enclosing-owner-rooted nested member reads.

Four cells compile successfully: **4 objects / 12 function verdicts**, zero
rejections and four distinct emissions. The unchanged control is byte-identical
to the complete production object. Twenty-three layout assertions per object
pass (92 total). Three native-positive/production-negative boundary controls
demonstrate that the declared signatures distinguish the existing mismatch.

### Native address and width witnesses

The native function loads the session pointer from owner `+0x3548` at `0x721a`
and derives the receiver from owner `+0x264` at `0x720c`. It then forms the PCM
echo subowner with `+0x6bd0` at `0x7238`, reads its count at `+0x14` (`0x723e`)
and coefficient pointer at `+0x20` (`0x7251`). The retained source names that
existing subowner explicitly. Complete count/pointer addresses remain
session `+0x6be4/+0x6bf0`; coefficient accesses remain four-byte floats.

The two fallback timing reads are owner `+0x43c` at `0x73e3` (four-byte integer)
and owner `+0x434` at `0x7425` (signed two-byte load). Their previous receiver
addresses are identical: `0x264+0x1d8=0x43c` and `0x264+0x1d0=0x434`. The retained
nested-member spelling restores both native load encodings without changing
owner, byte address, width or signedness.

Only three declared instruction regions change: timing phase, timing offset and
PCM subowner formation/dependent reads. PCM still uses a candidate `lea` and
different scratch registers where the native uses `add`; this is partial base
recovery, not complete instruction identity or unique source syntax.

### Complete-TU collateral and limits

The target remains **1006 bytes against native 1023**. Both neighbours remain
unchanged: exact `VPcmV34GetSNR` (99 bytes) and non-exact
`VPcmV34GetDiagnostics`. All 40 relocation records, symbol identities/bindings
and non-text section payloads are identical across the source cross. Fourteen
sections and 25 symbol records are inventoried. Only `.text` payload changes;
the nine switch-entry call-reach mappings and all wave-35 boundaries are retained.

The production compile reproduces all three reviewed candidate canonical bodies.
Whole exact and grade-0-or-1 gain/loss sets are empty: **1197 grade-0-or-1 / 1852
names**. Partial-link comparison remains **DIFFERENT**, including raw bytes;
positioned matches move **76775→76774 / 943398**. Candidate `.text` remains 711415
bytes; exact-position relocation matches remain 1160/18317 with 18301 candidate
records. Counts alone are not whole-object or relocation-identity claims.

No further pointer/cast family is opened. The unchanged size and one-byte
positioned-match decrease are disclosed; retention rests on native boundary
witnesses, not a similarity score.

## Wave 37: construct owners, then use exported component APIs

`t_v34diag::run_visual_history` supplements the existing seeded fixture.
Each side registers its actual VPCM operations through `dp_vpcm_init`, creates
its runtime, and invokes the registered V.34 create operation. A declared host
shim supplies codec/rate/delay information. The resulting native constructor
state is status 0, caller role `0x65`; no internal status, role, history, tap
count or coefficient is planted to enter a visual path.

Exported `V34EchoCleanUp` and `V34EqualizerCleanUp` establish reset state. Echo
input/filter/adaptation calls and equaliser input/adaptation calls then create
signed coefficients. These are **constructed-owner histories with caller-supplied
DSP-component inputs**, not modem-generated adaptation-error, handshake or
waveform histories.

Selectors 1, 5 and 6 are exercised before and after those inputs, with output
capacities 0, 1, 7 and 192: **24 paired trials, 48 diagnostic calls, 6800 explicit
component calls and 278 new assertions**. Overall fixture assertions increase
from 47283 to 47561; the existing 3600-trial synthetic dispatcher grid is unchanged.

Known answers are derived from native reset/update arithmetic independently of
the visual implementation. After the echo history, tap 143 is near `+4` or far
`-8`, with the other observed taps zero. Updated equaliser points are `(-4,-12)`,
except centre tap 40 `(8188,-12)`; the reset centre real component is 8192.
Checks cover complete output/guards, root-state immutability and the constructed
echo/equaliser owners. Pointer normalisation touches copies only. Heap children
outside those comparisons are not exhaustively compared.

The negative control routes only OURS' near selector 5 to far selector 6. All
trials still execute and **2/278 assertions fail**, observing `-8` instead of the
independently predicted `+4` at output byte 1148. The perturbation is removed;
the restored focused period run passes **1/0**. The initial fixture-only C-linkage
failure is preserved as invalid apparatus work, not counted as a passing run.

PCM echo/status 2 and selector 8 are not reached by this constructed history.
They require their own native-backed input histories; existing seeded coverage
is not relabelled as lifecycle evidence.

## Validation and continuation

Independent read-only review checked both packets, native instructions, layout
proofs, full-TU collateral, fixture validity, assertion arithmetic and the firing
wrong-owner control. The final parent `make phase J=8` passes **394 period
binaries, zero failures**, plus all structural checks. The existing binary is
extended, so the binary denominator stays 394. Experiments and tests use Gentoo
GCC/G++ 3.4.2-r2, executed assembler/linker 2.15.92.0.2 and bug-reproduction flags.

Artifacts under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/` include
`w36-owner/REPORT.md`, matrix/ELF/access ledgers, before/after partial comparisons,
final census and `parent-verification.log`; `w37-history/notes.md`, native captures,
focused and negative-control logs; and `w36-37-phase.log`. The session handover
records delivery. Next work should establish the missing PCM/selector histories
or diagnose the first visual block/tail-layout compiler divergence, rather than
repeat this completed four-cell owner/base family.
