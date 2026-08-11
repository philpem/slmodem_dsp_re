# `VPcmV34Main.cpp` — the plan for the last big span

290,315 bytes over 739 symbols: V.90, V.92, K56Flex, and the construction path
that builds all of them *and* V.34. The largest single thing left, and the only
route to 56k.

**The decision is taken.** This is being reconstructed, in parallel batches, by
agents in worktrees. What follows is the split, the order, and the things that
have already cost this project time.

## Two INDEPENDENT tracks, and only one of them is this document

Findings 800-806 established that the blob's own constructor works as a
differential fixture, so **V.34 does not need this span written**. That splits
the remaining work in two, and they do not block each other:

| track | what | needs this span? |
|---|---|---|
| **A — finish V.34** | connect, then carry data both ways at BER 0 (task #81) | **no** |
| **B — V.90 / V.92** | this document | yes, all of it |

Track A's blocker is not here: `t_v34conn.c` never runs V.8 before entering
`datapumpv34`, so each endpoint waits for a negotiation the other never sends.
V.8 is complete and already selects `DP_V34` and returns `DPSTAT_CHANGEDP`
(`src/v8/v8proc.c:165`); the gap is in the test, not the reconstruction.

Run the two tracks in parallel. Nothing in track A waits on a byte of this
span, and treating them as one queue is what made the ordering wrong twice.

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

**RECOMPUTED, AND THE PLANNED FIGURES BELOW WERE WRONG IN BOTH DIRECTIONS.**
The right-hand columns are what `tools/closure.py` reports after a full build;
the `already` column is what it CANNOT see, because `Scrambler.h` defines six
members as inline template functions and GCC inlines them, so `build/src/**/*.o`
contains no symbol for them at all (finding 64, `nm` over every built object).

| group | planned | recomputed | already | owner |
|---|--:|--:|--:|---|
| `V90Parameters` | 12,004 / 7 | 12,004 / 7 | | D |
| `V92Parameters` | 1,964 / 5 | 1,964 / 5 | | D |
| `Resampler`, `ResamplerTimingOffset`, `ResamplerTiming`, `V90Resampler` | 4,981 / 32 | 5,030 / 34 | | A |
| `Scrambler`, `Descrambler` | 2,242 / 34 | 2,242 / 34 | 287 / 6 | B |
| `FloatARMA`, `Psd` | 1,983 / 7 | 1,983 / 7 | | C |
| **total** | **23,174 / 85** | **23,223 / 87** | **287 / 6** | |

So wave 0's genuinely unwritten span is **22,936 bytes over 81 symbols**.

**These are shared types and base classes, and they are nobody's.** Half the
constructors in the span take a `V90Parameters *`, and the four `Resampler`
classes carry the vtables. `docs/v90rest.md` records that the V.90 line split
by CLOSURE and put two batches on the same header; the rule that came out of it
is one class, one owner, and the corollary is that a type everyone includes has
to land before anyone forks.

### Wave 1 — the construction path, and the oracle it may unlock

| group | planned | recomputed |
|---|--:|--:|
| free functions (`dp_vpcm_init`, `vpcm_create`, `VPCMXF_Create`, `VPcmV34Create`, …) | 31,127 / 69 | **31,072 / 65** |
| `VPcmFloModem` | 7,020 / 6 | **7,020 / 6** |
| `V90Modem`, `V92Modem`, `K56FlexFloModem` | 2,598 / 13 | **2,598 / 13** |
| **total** | **40,745 / 88** | **40,690 / 84** |

Big enough to split in two if it resists. It goes early for a reason beyond
dependency: **the blob's own constructor is aliasable** — `ref_dp_vpcm_init`,
`ref_vpcm_create`, `ref_VPCMXF_Create`, `ref_VPcmV34Create` all exist, because
the Makefile globalizes file-locals before renaming. If a blob-constructed
object can be used as a reference, every later batch can diff its constructor
output field-by-field against it instead of being untestable until the whole
span works. Whether that holds is being measured; see the caveat below.

#### CORRECTION — WAVE 1 CANNOT GO FIRST, and the byte count does not show it

**The split above is by CLOSURE, not by WRITABILITY, and for this wave those
are very different things.** Every wave-1 symbol was re-run through
`closure.py <name> --missing` and asked whether its own closure is size 1
(finding 838):

    dp_vpcm_init   needs 394 more     vpcm_create     needs 124
    VPCMXF_Create  needs  66          VPcmV34Create   needs  11
    VPcmFloModem::ctor needs 65       V90Modem::ctor  needs  38

**Not one of the functions this wave is named after can be compiled today.**
`VPcmV34Create` is the closest at 11 symbols, and all eleven are `reset` and
`enterChannelVerification` members of WAVE-2 classes — taking them would break
one-class-one-owner for eight future batches. So the construction path is the
LAST thing in this span that can be written, not the first, and the ordering
argument above survives only as the oracle argument, which findings 800-806
delivered without needing the span written at all.

About **14 KB of wave 1 is writable today** and it is all leaves: the FFT pair
`realfft`/`four1` (which is also finding 876's block on `Psd::process`), the
V.92 CP/DIL packers, the rate-renegotiation pair, the diagnostic printers, and
about 2 KB of coefficient tables.

### Wave 2 — the receive chain, and it is NOT a free-for-all

**ORDER BY WRITABILITY, NOT BY SIZE.** Wave 1 was planned by byte count and
none of its four entry points could be compiled at all (finding 838). The same
question was then asked of wave 2 — take each class's largest unwritten method
and run `closure.py <symbol> --missing`, which says how many symbols must exist
before that one can build:

| batch | class | largest unwritten | closure still needs |
|---|---|--:|--:|
| G | `V90CP` | 2,785 B | **1** — writable now |
| H | `V90ConnectionEvaluator` | 3,857 B | **1** — writable now |
| D | `V90AutoDigitalImpDetector` | 5,335 B | **3** |
| B | `V90ConstellationDesigner` | 4,887 B | **6** |
| F | `V90Phase4Demodulator` | 3,252 B | 20 |
| C | `V90Phase3Demodulator` | 8,616 B | 22 |
| A | `V90Equalizer` | 9,364 B | 65 |
| E | `V90Demodulator` | 7,276 B | 145 |

So the fan-out order is **G, H, D, B first** — those four are writable today or
nearly so — then F and C, then A, and `V90Demodulator` LAST of the receive
chain, because it is the class everything else feeds. The original A-to-I
lettering was by byte count and is kept only so earlier notes still resolve;
ignore it for scheduling.

A closure of 1 means the symbol itself: nothing blocks it. A closure of 145
means `V90Demodulator` is a hub, not a leaf, and starting there would stall.

### Wave 5 IS DONE — `dp_vpcm_init` builds the whole modem out of this tree

**Landed 2026-08-11.  `tools/closure.py dp_vpcm_init --missing` reports 0
symbols and 0 bytes.**  The last nine symbols were 3,348 bytes:

    V90Modem::V90Modem            597    VPCMXF_Create      495
    V90Modem::~V90Modem           321    VPCMXF_Delete      109
    VPcmFloModem::VPcmFloModem    651    vpcm_create        969
    vpcm_delete    110   vpcm_op    24   dp_vpcm_init        72

Four suites, 178 mutations, 0 NOT caught: `v90modemctor` (26: 24/0/2),
`vpcmctor` (32: 31/0/1), `vpcmxfcreate` (24: 21/0/3), `vpcmdp` (96: 88/0/8).
Findings 1330-1344; deviations D235, D236, D237.

Three things the next reader of this file should have:

- **Finding 806's question is answered and its premise was wrong.**  Nothing
  in the construction path writes the V.34 object's `+0x2218`; it is a
  run-time handshake state and `datapumpv34` at .text+0x71bfa is what writes
  2.  Finding 1331 has the enumeration of all eleven writers.
- **`sizeof(VPcmFloModem)` is 0x7f68 and `V90Modem` has no `pad_` left.**
  Findings 1332 and 1333; the header floors both files used to carry are gone.
- **D237 is the only difference left and it is a symbol-table one.**  The
  blob's `~VPcmFloModem` `D1`/`D2` are called by nothing in the blob; ours are
  inlined into `VPCMXF_Delete` and emitted nowhere.

The plan that used to be here is kept below, unedited, because the ordering
argument it makes is the one that turned out to be right.

### Wave 5 — the construction path, LAST

What this document originally called wave 1. Moved here, unchanged in content,
because finding 838 measured that none of it can be compiled until the classes
below it exist:

    dp_vpcm_init  needs 394 more    vpcm_create    needs 124
    VPCMXF_Create needs  66         VPcmV34Create  needs  11

Its ~14 KB of writable leaves have already been taken (the FFT pair, the V.92
CP/DIL packers, the rate-renegotiation pair, the diagnostic printers). What
remains is the construction path proper and it is genuinely last.

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

### THE CONFIGURATION IS NOW DERIVED — and the call still does not connect

Done, findings 820-825, and `docs/configuration.md` has the field-by-field
result. The route was not the object: **`slmodemd`'s own source survives**, and
it is the other half of the ABI this object was partially linked against.
`MDMPRM_DPRUNTIME` is `m->dp_runtime`, which is `dp_runtime_create(m)`, which
is a function *in this blob at 0x58e0 that only the host calls* — so it is in
no datapump's closure, which is why nobody had looked at it. It is a
136-byte `struct _tagModemParameters`, the type the mangling of
`VPcmFloModem`'s constructor names, and it is now reconstructed and
differentially tested. `MDMPRM_DSPINFO` is a 16-byte `struct dsp_info`.
MIN_RATE and MAX_RATE are `MODEM_MIN_RATE` 300 and `MODEM_MAX_RATE` 56000; and
`max_frag` 48 is `MODEM_FRAG`, so the guard value was the real value.

**Every parameter was then swept against the whole 1,600-block call, and five
of the six are INERT.** The host's rate window lands at runtime +0x30/+0x34
and the rate machinery reads +0x38/+0x3c, which `vpcm_create` writes as
literals. `t_v34conn.c` now uses the derived configuration and **every recorded
literal is unchanged from finding 902** — same trajectory, same stop, same
zero rates.

So the configuration is retired as the suspect for finding 902, and finding
908's second item is what is left: **there is no V.8**, and phase 2 is where
V.34 uses what V.8 negotiated.

## Where wave 0 got to

**The two parameter blocks landed first and alone, and their layout is now
frozen.** `include/dsplib/V90Parameters.h` (0x558, 342 slots) and
`include/dsplib/V92Parameters.h` (0xdc, 55 slots) carry the ORIGINAL AUTHOR'S
OWN NAMES for 291 and 54 of them. Findings 860-862.

They came out of a shape nobody had looked at: `loadParams(char *)` is 7,894
bytes of nothing but 295 straight-line calls to `Vparser_read_int` and
`Vparser_read_float`, each carrying the parameter's name as a relocation and
its offset as a `lea` displacement. Both callees are three-byte stubs, so the
member has no behaviour at all — and it is a complete, self-describing field
map, which nothing else in the object is. `tools/vparse.py` reads it.

Three measurements agree and none disagrees: `loadParams`'s (name, offset,
type), `setToDefault`'s (offset, width, value) from a separate walk of a
separate function, and the `sysdep_malloc` immediately before each constructor.
Last field plus four is the malloc size in both classes.

**`make params` is a new phase gate and it exists for a specific hole.** A
header with no member defined is invisible to `test`, to `coverage.py` and to
`check64` alike, so a later batch could move every field and the tree would
stay green. The gate re-extracts the map from the blob at gate time and
compares it with the header text, AND compiles 397 emitted `offsetof`
assertions. Both halves were shown to fire; the second was added because the
first could not see a deleted `unnamed_*` slot, which moves every field after
it while every comment stays put.

Two things any later batch must know:

- **The layout is not open for revision.** If you find evidence a field is
  wrong, that is a conversation and not an edit; `make params` will fail the
  build for the whole tree, which is the intent.
- **Fifty-one V.90 fields are `unnamed_*` on purpose.** `setToDefault` writes
  them and `loadParams` never reads them, so they have no recoverable name.
  They are four bytes each and in the right place. Twenty-five run
  consecutively from +0x300 to +0x360 and are very likely one array; naming
  them needs a reader, not a writer.

### What each wave-0 batch landed

| batch | classes | landed | suites | findings |
|---|---|---|---|---|
| B | `Scrambler`, `Descrambler` | all 28 unwritten members, 1,955 B | `scrambler` 36: 33 caught, 0 uncaught, 3 equivalent | 869-872 |
| C | `FloatARMA`, `Psd` | `FloatARMA` whole; 5 of `Psd`'s 6 | `floatarma` 30: 27/0/3, `psd` 14: 14/0/0 | 873-876 |
| D | `V90Parameters`, `V92Parameters` | 10 of 12 members, 4,542 B | `v90params` 37: 34/0/3, `v92params` 11: 10/0/1 | 877-879 |

Three things they settled that are not in any of their own files:

- **`Vparser_read_int` and `Vparser_read_float` are three-byte stubs**, so both
  `loadParams` members have no observable behaviour. Batch D built the
  `objcopy --weaken-symbol` oracle anyway, ran it, and compared all 349 logged
  triples against `vparse.py`'s static walk: identical on reader, name and
  offset. That takes the abstract interpreter out of the trusted path. It also
  shows why the member is still not written — **our `loadParams` would BE
  `vparse.py`'s output**, so our-log-against-blob-log compares the extraction
  with a copy of itself. The differential content was obtained without writing
  it. If a later batch does land it, use `ld --wrap` rather than
  `--weaken-symbol`: it reaches both sides and touches neither `src/` nor the
  reference object.
- **`Psd::process` is blocked twice** and clearing either alone does not
  unblock it: it calls `realfft` and `four1`, which are unwritten wave-1 free
  functions whose only definitions in the reference object are `ref_`-renamed;
  and two of its arms compute log10 with `fldlg2`/`fyl2x`, which GCC emits only
  under `-funsafe-math-optimizations`. Finding 876.
- **Nine of `V90Parameters`'s fifty-one `unnamed_*` slots are floats** and are
  still declared `int`, annotated in the header with the value the object
  stores. Finding 878, and the header says why the retype was deferred and who
  should do it.

### And what the merges themselves cost

Four shared files conflict on every wave-0 merge and three of them must be
resolved by UNION. `tools/offcheck.py`'s `SKIP_HEADERS` is the sharp one: a
single long line that every batch appends a header name to, where taking one
side entire drops another batch's entry and the symptom is
`N of N annotations do not match the layout` — a message about offsets,
produced by a parse error. `test/mutations/suites.json` is the same shape and
worse in one way: one batch reformatted the whole file, so the conflict covered
every line and taking its side would have silently dropped the batch that
merged before it. `docs/coverage.md` is the one file where taking a side entire
is right, because it is regenerated. Finding 700 in four instances in one
afternoon.

**Record each batch's own mutation suites by name at merge time** —
`tools/mutsnap.py --update <suite> <suite>` re-runs only those mutations and is
not the tree-wide sweep this document forbids. A registered-but-unrecorded
suite is `MISSING`, which unlike staleness is a hard failure and is
unambiguously a defect: the record cannot detect a change to a suite it has
never seen. Two batches were denied permission to do this and correctly did not
hand-write the file.
