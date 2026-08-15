> **SALVAGED, AND HALF OF IT IS SUPERSEDED.** This was the only content on
> branch `review/nextsteps-2026-08-11`, which was deleted 2026-08-16 as a
> stray. It is kept because §3 is not recorded anywhere else.
>
> - **§1 and §2 are STALE.** They measure the construction path at 108
>   symbols / 19,704 bytes and argue it should be next. It landed; that is
>   what `docs/remaining.md` and `docs/plan.md` now measure, against master
>   `93270f9`. Do not quote the queue, the batching or the byte counts here.
> - **§3 is NOT superseded** and is why this file survives: the join between
>   `docs/deviations.md`'s Appendix C and findings 1204-1209, which are the
>   same investigation and had never been read against each other. It notes
>   that Appendix C names `MALLOC_PERTURB_` as the decisive experiment while
>   `grep MALLOC_PERTURB docs/findings.md` returns nothing, that four
>   transport-level covariates were measured instead and all four failed, and
>   that 1208 then found the outcome TRIMODAL -- which is what a discrete
>   initial condition produces and a continuous impairment does not.
> - **§4 and §5** are small items; check each against `docs/plan.md` before
>   acting on it.

# Where the reconstruction is, and what to do next

*A review, not a plan of record. Written 2026-08-11 against `ddcfa42`
(`1208: thirty calls at one configuration`). Every figure here was
**recomputed**, not quoted — `docs/vpcmv34main.md` says to, and this document
is the reason why: two of its tables have inverted since they were written.*

Two things are proposed. The first is a **recomputed work queue** for the
V.PCM span, which is the whole of what is left on the critical path to V.90.
The second is a **join between `docs/deviations.md`'s Appendix C and findings
1204–1209**, which are the same investigation and have never been read against
each other.

---

## 1. The baseline, measured rather than assumed

`make phase` at `ddcfa42`, in a clean worktree: **exit 0, 1,293 PASS, 0 FAIL.**
`make coverage` regenerates `docs/coverage.md` byte-identically, so the
committed figures are current and can be quoted.

| | |
|---|--:|
| `.text` | 734,605 B / 1,861 symbols |
| translated | **34.4%** — 252,540 B / 570 symbols |
| tested against the blob | 100% of what can be — 252,503 B / 563 of 570 |
| findings | 1,209 (600 entries); `refcheck.py` 4,217 references, 0 dangling |
| codegen tier | **105 of 386** shared symbols identical on instruction sequence |

Three scoping notes, because a green build is not the same as a green tree:

- **`make phase` does not run everything.** Finding 1152 names the two gates
  outside it: `mutsnap.py --check --strict` and `anchorcheck.py`. The mutation
  snapshot today is **0 current, 72 stale, 0 never recorded, of 72**. Stale is
  not MISSING and is not a failure, but it is also not a baseline — the tree
  is owed one tree-wide re-record once the current merges settle, and
  `vpcmv34main.md` is emphatic that it happens **once, after all merges**.
- **A worktree needs two things the main tree has.** `BLOB` must be absolute
  (`make BLOB=/home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o phase`), and
  `third_party/spandsp` must be symlinked to the main tree's built copy or
  every interop binary fails to link at `t_spandsp_b103`. `.gitignore` already
  anticipates the symlink; nothing tells you to make it. Worth a line in
  `CLAUDE.md`.
- **`CLAUDE.md`'s codegen figure is stale.** It says *"92 of 365 shared
  symbols match"*; `tools/toolchain/ratchet.json` records **105 of 386**. The
  ratchet moved and the prose did not.

### V.34 is finished, and the tree does not say so plainly anywhere

`V34hshak.c` does not appear in `coverage.py`'s *"what is left, by
translation-unit span"*. That span is `v34handshak` — 61,541 bytes over 87
states, the largest function in the object — plus `datapumpv34`, and it is
**fully translated**. The only surviving `t3c_unwritten()` call site is a
`default:` arm the file's own comment documents as unreachable and kept as a
mutation target.

Combined with README step 2 (both endpoints ours, 33,600 bit/s each way, BER 0
over 8,000 blocks), **phase 10 is done**. The README still describes it as *"in
progress — the fast pass"* and the phase table still reads `10 | in progress`.
That should be updated; a reader budgeting the remaining work will otherwise
double-count 61 KB that is already written.

---

## 2. The recomputed V.PCM queue — the part that has inverted

`docs/vpcmv34main.md` plans this span in waves and warns, in its own words,
*"Recompute before quoting — finding 330 invalidated every closure number
written before it, in both directions."* Recomputed, with `build/` fully
populated (finding 271's precondition):

    BLOB=/…/dsplibs.o python3 tools/closure.py <roots> --missing

| root set | unwritten symbols | unwritten bytes |
|---|--:|--:|
| construction + destruction (`dp_vpcm_init`, `vpcm_create`, `VPCMXF_Create`, `VPcmV34Create`, `vpcm_delete`) | **108** | **19,704** |
| the above **plus** `VPcmV34Progress` and the three getters — i.e. a modem that also *runs* | 362 | 213,684 |

The second number is the honest size of V.90/V.92. The first is the finding:
**the entire construction path is under 20 KB, and 69 of its 108 symbols
(9,198 bytes) have a closure of 1 — they can be written today, in any order,
by anyone.**

### What that overturns

`vpcmv34main.md` demotes the construction path to *"Wave 5 — the construction
path, LAST"*, on finding 838's measurement that none of its four entry points
could be compiled. Those numbers have collapsed:

| | finding 838 | today |
|---|--:|--:|
| `dp_vpcm_init` | needs 394 | **108** |
| `vpcm_create` | needs 124 | **104** |
| `VPCMXF_Create` | needs 66 | **59** |
| `VPcmV34Create` | needs 11 | **1 — writable now, and 2,376 B, the largest single item in the set** |

The wave-2 writability table (*"G, H, D, B first"*) is likewise stale: `V90CP`
and `V90ConnectionEvaluator` are still writable, but so are nineteen of
`V90AutoDigitalImpDetector`'s twenty-two symbols, and `V90ConstellationDesigner`
— listed at closure 6 — now has five members at closure 1.

**Neither table should be quoted again.** Recompute; it takes ninety seconds.

### The construction path, by class (108 symbols / 19,704 B)

`cl(lg)` is the closure of the class's largest unwritten member; `cl=1` counts
its members that are writable today.

| class / group | syms | bytes | largest | cl(lg) | cl=1 |
|---|--:|--:|--:|--:|--:|
| free functions and data | 24 | 5,399 | 2,376 | 1 | 17 |
| `V90Demodulator` | 3 | 2,243 | 1,002 | 27 | 1 |
| `V90Equalizer` | 2 | 1,273 | 732 | 1 | 2 |
| `V92Modulator` | 2 | 1,237 | 734 | 11 | 0 |
| `V90Modem` | 2 | 918 | 597 | 34 | 0 |
| `V92EchoCanceller` | 3 | 703 | 313 | 4 | 2 |
| `VPcmFloModem` | 1 | 651 | 651 | 58 | 0 |
| `V92Modem` | 2 | 622 | 393 | 16 | 0 |
| `V90PreFilter` | 2 | 571 | 552 | 1 | 2 |
| `V90Modulator` | 2 | 537 | 338 | 8 | 0 |
| `V90Phase3Demodulator` | 2 | 530 | 365 | 9 | 0 |
| `V90CP` | 2 | 366 | 193 | 1 | 2 |
| `V92Transmitter` | 2 | 353 | 180 | 5 | 0 |
| `GenericToneDetector` | 2 | 307 | 267 | 1 | 2 |
| `V90Phase4Modulator` | 2 | 307 | 213 | 6 | 0 |
| `V90Phase4Demodulator` | 2 | 277 | 225 | 8 | 0 |
| `V92Phase4Modulator` | 2 | 251 | 164 | 2 | 0 |
| *(19 further classes, all ≤ 237 B, 33 symbols, almost all `cl=1`)* | | | | | |

The shape is a **strict chain with a wide, flat base**: 69 leaves, then a
narrowing stack — `V90Demodulator`'s constructor (27), `V90Modem`'s (34),
`VPcmFloModem`'s (58), `VPCMXF_Create` (59), `vpcm_create` (104),
`dp_vpcm_init` (108). Nothing branches. Write the base and the chain falls out
in order.

### Why this is the right next milestone

Because it has an oracle, and it is the last piece of this span that does.

Findings 800–806 established the blob-constructed V.34/V.PCM object as a
differential fixture: 127 allocations, 265,520 bytes live, **two** blob-code
pointers in the whole graph and both replaceable by two stores, no vtable in
the root arena. That gives our constructor a field-by-field reference to be
diffed against — `diff_eq_obj` over 53,848 bytes, exactly as `t_vpcmrun`
already does for `datapumpv34`.

Finishing it means `dp_vpcm_init` builds the entire modem — V.34, V.90, V.92
and the K56Flex husk — out of our own code, with the blob used only as the
thing we are compared against. That is a milestone with a name, it is under
20 KB, and it is the first one since `vpcm_run` that changes what the tree can
claim rather than how much of it is written.

Two things finding 806 says the taker must handle: `VPcmV34Create` leaves
`+0x2218` at 0 and something must write 2; and two constructions must be made
congruent or pointer fields excluded, because 125 heap regions come back at
different addresses.

### Suggested batching — five subagents, one class each, no shared header

`docs/largefunctions.md` measured that delegation is the *only* structural fix
for the context wall, and every one of these is independently verifiable
(`make phase` green on the batch's own machine). One class, one owner, per
`vpcmv34main.md`.

| batch | content | bytes |
|---|---|--:|
| 1 | `VPcmV34Create` alone — 2,376 B, `cl=1`, and the anchor the 806 oracle tests | 2,376 |
| 2 | the free functions and data: `V92createConstellations` / `V92deleteConstellations`, the `V92*FilterCoefficients` pair, `K56FLEX_Create` / `_Delete`, `v92TxPreFilter`, `IIR2100_Coef_[AB]_{8000,9600}`, `v92echoPreFilter_[ab]`, `entFilt{Num,Den}`, `v34initialbauds` | 1,225 |
| 3 | `V90Equalizer` and `V90PreFilter` ctor/dtor — all four `cl=1` | 1,844 |
| 4 | the small-class sweep — the remaining `cl=1` members across ~24 classes, mostly ctor/dtor pairs under 200 B | 3,753 |
| 5 | `V92EchoCanceller`, `V90SpectralShaper`, `V90Demapper`, `V92Phase4Modulator`, `V90Mapper`, `V90BitsToSymbol` (`cl` 2–4) | 1,444 |

Then, serially and by one owner because each unblocks the next:
`V90Phase3Demodulator` → `V90Phase4Demodulator` → `V90Modulator` /
`V92Modulator` → `V90Demodulator` → `V90Modem` / `V92Modem` → `VPcmFloModem`
→ `VPCMXF_Create` → `vpcm_create` → `dp_vpcm_init`.

**Reserve findings 1220–1239** for this work. Master is at 1209 and five
merged branches sit between 1046 and 1204; leave the gap rather than starting
at 1210.

### After construction: the run path, in writability order

The 362-symbol / 213,684-byte figure is what a *working* V.90 costs. The
immediately-writable blocks in it, largest first, are worth naming because
they are what a fan-out would take next:

| class | unwritten | largest | cl(lg) | writable now |
|---|--:|--:|--:|--:|
| `V90AutoDigitalImpDetector` | 16,728 B / 22 | 5,335 | 3 | 19 |
| `V90ConstellationDesigner` | 20,487 B / 8 | 4,887 | 6 | 5 |
| `V90CP` | 7,623 B / 7 | 2,785 | **1** | 6 |
| `V92ModulusEncoder` | 6,910 B / 3 | 4,601 | **1** | 3 |
| `V90ConnectionEvaluator` | 6,767 B / 9 | 3,857 | **1** | **9 — the whole class** |
| `V90MP` | 5,185 B / 8 | 2,597 | 2 | 7 |
| `V92ConvolutionEncoder` | 2,592 B / 8 | 1,558 | **1** | 5 |
| `V90SpectralVerifier` | 2,481 B / 6 | 1,682 | **1** | 5 |
| `V92Jd` | 2,409 B / 8 | 1,003 | **1** | 8 |

`V90Demodulator` (142), `V90Equalizer`'s `process` (64) and `VPcmFloModem`
(160) are the hubs and stay last, exactly as the wave plan says.

### What these numbers do not mean

`closure.py` answers *"what must exist before this links"*, and three caveats
travel with every figure above:

- It is a **link** closure, deliberately pessimistic about run-time
  reachability — it will list a branch nothing takes.
- It cannot see members GCC inlined out of existence: `Scrambler.h`'s six
  template members have no symbol in any object (finding 64). A closure of 1
  is a lower bound on convenience, not a guarantee of triviality.
- Unwritten **bytes** are the blob's, not ours. They size the reading, not the
  writing.

---

## 3. The defect register and the bench are the same investigation

`docs/deviations.md` closes with **Appendix C — would fixing any of this
improve connect reliability or rate?** It ranks the register against exactly
the bench problem findings 1204–1209 have been chasing, and it was written
before them. Nobody has read the two together. Doing so is most of this
section, and it changes what the next bench sitting should do.

**The register is a register of the blob's defects, and the bench is measuring
the blob.** The reconstruction is unfinished, so `slmodemd` on the bench links
`dsplibs.o`. Appendix C is therefore not adjacent evidence — it is a list of
candidate causes for the thing being measured, written from the disassembly of
the code under test.

### What the bench has settled

| | |
|---|---|
| the asymmetry | **confirmed, n = 22.** `our TX` 33600 on 22 of 22; `our RX` median 12000, range 4800–26400 |
| echo | **exonerated.** ERL 16.2–22.9 dB, p = 0.45; and 11 dB less echo on a second modem bought nothing (1206) |
| jitter buffer, per-call ERL, between-modem ERL, clock slip | all proposed, all refuted (1206–1208) |
| pre-CONNECT equaliser error | **survives** — r = −0.689, worst LOO −0.650, p = 0.0025 |
| and it is **trimodal** | 15 of 22 calls in a band 13% wide; 12 of those produce exactly 12000 |
| ruled out as the discrete difference | pre-emphasis index (6 on all 22), `txpreemp` (2 on all 22), symbol rate (3429 on all 22), AGC gain at S-S1 |

### The join, and it is one sentence

**Trimodality is what a discrete difference in initial conditions produces,
and it is not what a continuous channel impairment produces — and Appendix C
already ranks a family of defects that gives V.34 acquisition a discrete,
run-to-run-varying initial condition, with the experiment to test it, and that
experiment has not been run.**

Appendix C item 5 is D29/D30/D32, *"three places where V.34 acquisition starts
from heap garbage"*, and it says in terms: *"This is the cheapest hypothesis to
test and I would test it first."* Its experiment 1 is heap poisoning —
`MALLOC_PERTURB_`, twenty calls, *"one afternoon and it is decisive either
way."* `grep -n 'MALLOC_PERTURB' docs/findings.md` returns nothing. Four
transport-level covariates were measured instead, and all four failed.

D29 is the concrete one and it is not speculative: `V34TimingFiltersInit`
zeroes eighty **shorts** across a region holding forty shorts followed by forty
**ints**, so the upper twenty entries of the timing prefilter keep whatever was
in that memory. It fires on every setup. `t_v34ec` asserts the harness fill
survives, so it is proven live in our build too. **It lands in timing recovery
during acquisition, which is the loop that decides where the equaliser
converges** — the exact quantity 1208 found trimodal.

**And here is the weakness in it, stated rather than left for the next reader
to find.** D29's own entry bounds the damage at *"about 20 symbols"* — 5.8 ms
at 3429 baud — after which the uninitialised entries have shifted out of the
prefilter. Whether a 20-symbol disturbance at the start of timing recovery is
enough to decide which of three states the equaliser settles into is
**unmeasured**, and nothing in the register or on the bench measures it. D29 is
a mechanism with the right shape and an unknown magnitude, not a diagnosis.

That argues for running the experiment, not against it. `MALLOC_PERTURB_` tests
the **family** — D29, D30's cursor seeded from a field not yet written, D32's
stale delay-line shift, and any uninitialised read nobody has catalogued —
because it changes what every such read returns at once. A null result retires
all of them together, which no amount of further reading can do.

### The three experiments now have a priority order, and two are one shift

1. **Poison the heap.** `MALLOC_PERTURB_` at two or three distinct values,
   fifteen calls each, through `testbench/batch.sh` and `batchanalyse.py`
   unchanged. **If the trimodal band positions move with the fill value, the
   D29/D30/D32 family is the answer.** If they do not, the family is out and
   Appendix C item 3 (D137/D61 — placement dependence) moves up. Decisive
   either way, and the analysis code already exists.

   **What the process structure does and does not settle.** Checked rather
   than assumed: `row.sh:159` `setsid`s a fresh `slmodemd` per call and
   `batch.sh` calls `row.sh` once per row, so **every bench call is its own
   process** and no call inherits the previous call's heap. That removes the
   simplest version of the mechanism — but not the mechanism. Pages the kernel
   hands over are zero-filled only until something touches them, and slmodemd
   has done its own start-up allocation before `vpcm_create` asks for
   265,520 bytes across 127 regions. Whether D29's twenty ints land on a
   pristine page or on a recycled one is exactly what varies call to call, and
   it is not predictable from the source.

   This is also why `MALLOC_PERTURB_` is the right instrument rather than a
   proxy: it fills every allocation with a nonzero pattern, so it converts
   *"happens to read zeros"* into *"reads 0xNN"* whether or not the page was
   pristine. A distribution that moves when the fill value changes is a
   distribution that depends on uninitialised memory, and no further argument
   is needed.

2. **Force 2400 baud.** D112 (`PPSEG` adds symbols-at-baud to a count of
   4-sample ticks) is dimensionally correct at 2400 and wrong in proportion to
   baud above it. 1208 reports **3429/3429 on all 22 calls** — so every call in
   the sample sits at D112's maximum error, which is precisely why the sample
   cannot see it. Forcing 2400 makes D112 exactly correct: the rate ceiling
   must drop, and if **the variance collapses with it**, that is D112.

3. **Sweep `IODELAY`.** Lower-priority now: 1208's thirty calls were all at
   240 and connected 73%, so the D77 connect race at the bottom of the band is
   not what these calls are hitting. Still worth doing for the D72 question at
   the top, but it no longer competes with 1 and 2.

### And one caveat of Appendix C's own is now weaker

Appendix C opens by warning that the asymmetry may not be a defect at all —
V.34 negotiates the two directions independently, and *"if the link really is
asymmetric then nothing below applies"*. 1208 weakens that: a channel that
merely differed direction-to-direction would not hold **our transmit at exactly
33600 on 22 of 22 calls** while our receive is trimodal at 4800–26400 with echo
exonerated. Something one-sided and discrete is in our receiver. The appendix's
own ranking is the right list to work down.

---

## 4. Smaller things worth doing, none of them large

- **Update the README's phase table.** Phase 10 (V.34) is done — see §1.
- **Correct `CLAUDE.md`'s codegen figure** to 105 of 386, and add the two
  worktree preconditions (absolute `BLOB`, `third_party/spandsp` symlink).
- **Five stale worktrees can go.** `agent-a493371068ef63603`,
  `-a53829f7f6718b308`, `-a57bebead077b7272`, `-a8aacbf0a764df71a` and
  `-acdfdce647ad7862d` are all fully merged into `master` and all clean —
  checked with `git merge-base --is-ancestor` and `git status --porcelain`.
  `git worktree remove` on each is safe. (Left in place here: another session
  is live in this tree and removing someone else's worktree is not this
  document's call.)
- **The tree-wide mutation re-record** is owed: 72 stale, 0 missing. Once, and
  after the merges.
- **`b103.c +2` still holds 4,471 unwritten bytes** although phase 2 is called
  done, and `Dialer.c +18` holds 16,982 with phase 3 called done. Both spans
  bracket several translation units, so the bytes are probably phase-8 services
  sharing the range rather than unfinished B103 or dialler work — but nothing
  has attributed them, and a phase marked done with unwritten bytes in its span
  is the kind of thing that gets re-derived twice.

---

## 5. If only one thing happens next

**Write the 69 writable symbols of the construction path.** It is 9,198 bytes,
it needs no decisions, it parallelises five ways with no shared header, it has
a measured oracle in findings 800–806, and it is the near end of the only
chain that ends at `dp_vpcm_init` building the whole modem out of our code.

**And if a bench sitting happens before that, spend it on `MALLOC_PERTURB_`
rather than on a fifth covariate.** Findings 1207 and 1208 both end by saying
the sample, not the hypothesis, is what was missing; the sample now exists and
says the outcome is discrete. Appendix C named a discrete cause and the
experiment that tests it in `d7d9adf`, four commits before 1205 landed on the
same day — close enough to miss each other, and they did.
