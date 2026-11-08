# Recovery waves 26–29: first bounded pass

This pass starts from `e66905295df9e49cd63f0f2bd8c73447035d0995` (PR #408).
Issue [#409](https://github.com/philpem/slmodem_dsp_re/issues/409) carries the
experiment contracts and follow-ups. No reconstruction source or declaration
change is retained from these experiments.

The reference SHA-256 is
`1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.
Reconstruction experiments use Gentoo GCC 3.4.2-r2, the retained period flags
and `DSPLIB_REPRODUCE_BUGS`; the executed assembler identifies itself as
2.15.92.0.2, 20040927. Full commands, source/object hashes and function ledgers
are in the session evidence directory listed below.

## Wave 26: the V29 countdown type remains underdetermined

The member is `v29_tx_params::countdown`, two bytes at parameters `+0x16`;
the parent holds the parameters pointer at `+0x20`. Source accesses are in
`V29t_prc.c` and `V29tx.c`; `V29t_stc.c` shares the declaration.

The four training handlers load the field with `movzwl`, test its low half
with a signed branch, and sign-extend a register before comparing against the
sample budget. They reload the field before subtracting the selected count
and storing two bytes. In `TxHdxQuietV29`, for example, those field loads are
at `0xa4f02` and `0xa4f18`. This is evidence for the access and conversion
boundaries, not proof that the declaration was unsigned.

Other accesses do not settle signedness: `TxNextStateV29` stores seven narrow
constants; `TxHdxDataV29` compares against zero and clears the field. The
constructor's countdown clear at `0x9bb23` uses the parameters owner loaded
from parent `+0x20`. Its later `+0x16` clear at `0x9bb79` uses a different
owner loaded from parent `+0x24` and is not a second countdown access.

Two declared families completed:

| Family | Cells including control | Complete objects | Function verdicts |
|---|---:|---:|---:|
| Field signedness × local signedness × captured/compound update | 9 | 27 | 126 |
| Direct member accesses: field signedness × ternary/cap minimum | 5 | 15 | 70 |

Each unchanged control reproduces all 14 production function bodies across
the three TUs. Changing only field signedness leaves every paired body
identical: four pairs/56 functions in the first family and two pairs/28 in
the second. Neither family yields an exact target or explains the native
second-load pattern. A same-sized `TxHdxABV29` candidate still differs in its
instruction graph. The unsigned field edit and all nearby candidates remain
unadopted. A future experiment needs a new discriminator; these results do
not exclude all possible source forms or compiler-state explanations.

## Wave 27: relocation-aware initialiser inventory

`tools/toolchain/datareloc.py` adds a bounded ELF32 REL discrepancy inventory:

```sh
python3 tools/toolchain/datareloc_selftest.py --scratch /mnt/zfs/dev/slmodem/scratch/datareloc-controls
python3 tools/toolchain/datareloc.py ref/slmodemd/dsplibs.o build/partial/dsplibs.o \
    --json /mnt/zfs/dev/slmodem/scratch/datareloc.json
```

It compares section/extent metadata, ordinary payload bytes, relocation field
positions, and resolved target descriptors. Named OBJECT/FUNC targets retain
their symbol identity and bounded interior offset. SECTION targets require a
unique containing symbol. LOCAL identities retain their unique FILE owner;
GLOBAL/WEAK names imply no defining-TU attribution. Uncovered mergeable byte
strings compare NUL-terminated contents, not pointer identity or aliasing.

The measured inventory is:

| Surface | Matched | Different | Unsupported |
|---|---:|---:|---:|
| Non-relocated keys | 662 | 0 | 0 |
| Relocated keys | 96 | 0 | 4 |
| All 762 reference keys | 758 | 0 | 4 |

There are 765 candidate keys and three existing apparatus extras. The
relocated subset contains 677 `R_386_32` entries on each side: 659 in the
resolved subset and 18 in the four unsupported vtables (`Resampler`,
`ResamplerTiming`, `ResamplerTimingOffset`, `V90Resampler`). Those vtables
use COMDAT/linkonce sections, deliberately outside this tool's model.

Calibration passes 22 assertions over 21 assembled ELF32 objects. It includes
an unchanged and repacked positive, a changed named target with **identical
raw table bytes**, addend/string/LOCAL-owner/ordinary-byte negatives, and
unsupported owner, relocation-kind, alias, boundary, extent and COMDAT cases.
A plain neighbour of a COMDAT object still matches. WEAK identities compare
without FILE ownership, and the CLI rejects an empty reference denominator.
These apparatus controls use host GNU assembler
2.42; they make no reconstruction-compiler claim.

An independent review confirmed the original 12-assertion inventory and
identified missing controls. The expanded controls cover its COMDAT,
repeated-FILE-owner, WEAK and boundary observations. A four-byte relocation
starting up to three bytes before an object overlaps its first byte; the
tool rejects that boundary rather than silently discarding it.

Descriptor matches do not establish function-body equivalence, global pointer
aliasing, original source syntax or partial-object byte identity. Unsupported
and ambiguous records are not matches. The strict partial comparator is
unchanged. This pass found no adopted data-source correction.

## Wave 28: refresh residues, then investigate observation timing

The fresh census remains 1852 names: 1161 EXACT, one UNRESOLVED, 35 REGALLOC,
93 BYTES and 562 SIZE; grade 0-or-1 is 1197. The rescreen examines all 655
names outside that set. LCS ranking is bounded to 500 instructions on each
side: 595 names qualify and 60 are outside that ranking domain.

The first instruction-band selection requires absolute instruction-count
delta 9–40 and LCS at least 80% of the larger body. It selects
`VPcmV34GetVisualDiagnostics` and `V34GiveINFO0dBits`. The byte-test inventory
separates accumulator-byte, other-byte-register and memory forms. It reports
18 functions/34 missing distinct `(form, mask)` pairs, or 21 functions/131
deficits when repeated occurrences are counted. These counting contracts
must not be substituted for one another or presented as progress against an
older differently extracted queue. Seven controls check form separation,
wide-test exclusion and occurrence counting. Similarity and these queues are
triage only.

The first source experiment selects `V34GiveINFO0dBits`. The native function
reads session `+0x6120` for separate diagnostic arguments (`0x8258`,
`0x828a`), after the decoding loops (`0x81ae`), and for the final remoteShort
diagnostic (`0x820b`). It delays the full-width remoteShort load until the
other conjunction terms have passed (`0x834d`/`0x8361`). Current source
captures the variant and both decoded scalar values early.

The first family crosses captured/live variant expressions, shared/branch-local
assignment, and saved/reselected final diagnostic values: nine objects and
63 function verdicts including control. The second crosses eager/direct word
uses with conjunction/explicit flag stores: five objects and 35 verdicts.
Both controls reproduce all seven production bodies. There are no exact
targets. The 755/758-byte near-match still has eager loads and materialised
booleans; the 758-byte candidate has 118 differing bytes and collateral in
two neighbours. Neither is adopted. Repeating live expressions also allows
GCC to reuse a value; it does not by itself reproduce every native load.

This closes two source families, not the wider timing question. The next
discriminator must explain why the original retained the final session load,
using owner/type/alias evidence or period RTL. Artificial volatile accesses
and further size fitting are not justified. Closed allocation-only families
and the parked RD_create/draft-303 work remain closed or parked.

## Wave 29: distinguish component histories from planted boundaries

The first audit covers the six magnitude functions retained by PR #408:

| Function | Existing evidence and its boundary |
|---|---|
| `FP_Pow` | `t_fp_math`: stateless signed-input sweeps, including both sides of `0x4000`; no internal history is needed. |
| `FPM_FSE_receive` | `t_fpm_fse_recv`: init/receive/free sequences with a declared component configuration. Runs with a non-null `poke` and manually swept internals remain exploratory probes. |
| `V22_FSE_receive` | `t_v22recv`: init installs storage; the fixture supplies the slicer/quadrant, then drives per-symbol and ragged sample histories. |
| `FSEv22_decision24` | The V22 receive history invokes the real slicer. `t_v22dec`'s coordinate/previous-quadrant grid is separately a direct component probe; it includes previous values the slicer never writes. |
| `band_pass` | `t_dtmfrx`: reset, rate/sensitivity setup and sequential tones/silence/clipping. Its random seeded-object cases remain a separate input domain. |
| `V27RX_eq_train` | `t_v27fax::run_eq_one` seeds counters, angles, tap count and callback state, then replaces installed callbacks with a sentinel. These runs alone do not prove constructor reachability of the measured boundaries. |

The fresh `make phase` run passes 394 period binaries with zero failures and
all structural checks. Its fixture logs report 32,767 / 76,460 / 66 checks for
the three `FP_Pow` input sweeps; 1,442 checks across the first three unpoked
FPM receive sequences; 169,201 checks in each V22 per-symbol rate history;
16,050 checks for DTMF tones/silence/clipping; 305,184 checks in the direct
V22 decision24 grid; and 6,977 in the direct V27 training probe. Those are
assertion counts for their stated fixture domains, not counts of distinct
reachable states or newly added coverage.

Executing a function through a valid component history does not prove every
promotion or threshold boundary was reached. The next V27 fixture should
start from `V27RX_create`, retain the native callback chain through
`V27RX_epoch_det` to `V27RX_eq_train`, and record actual angle/error boundary
observations before claiming lifecycle coverage. Existing planted results
remain useful fidelity probes and are not weakened or relabelled as reachable.
No test or mutation campaign is added merely to repeat an existing assertion.
Issue [#410](https://github.com/philpem/slmodem_dsp_re/issues/410) tracks that
constructor-to-training witness and its acceptance criteria.

## Evidence and continuation

Session evidence lives under
`/mnt/zfs/dev/slmodem/scratch/independent-recovery/`: `w26-matrix/`,
`w26-live-matrix/`, `w26-verification.log`, `w27-datareloc-final.json`,
`w27-controls-final.log`, `w28-rescreen.json`, `w28-rescreen-forms.log`,
`w28-info-matrix/`, `w28-info-direct-matrix/` and `w26-29-phase.log`.
The handover in that directory
records gates, delivery and next discriminators. Claims above use explicit
bounded denominators; none upgrades a clean inventory or a nearer size into
an equivalence claim.
