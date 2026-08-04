# The V.34 fast pass

**Applies to tasks #33 and #35–#45. Deferred work is task #47.**

Phase 6 is ~110 KB of code — `v34handshak` alone is 61 KB across 87 states.
At the pace phases 1–5 were done it is many sessions. This is the agreed
change of sequencing, decided after V.34's first 4.5 KB.

It is **sequencing, not a drop in quality**. Nothing that establishes
correctness is skipped.

## What every module still gets

1. Read from the disassembly, not from a summary of it.
2. Written in C.
3. **A tier-1 differential test against the blob.** Non-negotiable, and not
   overhead: it is what makes writing fast. `V34TimingFilter` took three
   attempts and each fault was localised in one run. Without it there is no
   way to know the code is right and no way to find out later.
4. A one-line entry in `docs/deviations.md` for anything that looks wrong,
   marked `unmeasured`.
5. Brief structural comments — what the object is, what the fields are, what
   the loops do.

## What is deferred to #47

- **Coefficient derivations.** Tracing a table to its closed form serves
  *regeneration* at a different sample rate. Until the 8 kHz retarget, a
  byte-exact copy is byte-exact and the differential test proves it with no
  derivation at all.
- **Reachability measurement** for deviations. The entry is cheap; measuring
  whether it fires is not.
- **File-header rationale** and findings prose.

## Why the one-line deviation entries are not deferred

An unrecorded observation is unrecoverable — it will not be noticed twice.
D28 is the precedent: entered cheaply, later measured, then retracted. That
is the process working, and it only works if the entry exists.

## The one rule that is not relaxed

**Nothing is committed that has not passed a differential test**, and nothing
is committed that is wrong-but-plausible. If a function cannot be made to
pass, it is left out and the attempt is recorded. Two functions were reverted
under exactly this rule during the first V.34 pass.

Run `make phase`, not `make test`.

## What the task numbers refer to

**The numbering is used throughout this tree and defined nowhere in it** —
not in `README.md`, not in the plan, not in any document here. Written down
because three sessions share this history and `#39` has already been quoted
in a hand-over summary by someone who could not look it up.

Only what the repository itself settles is recorded. Everything below is
sourced; nothing is inferred from adjacency of the numbers.

| | what it is | where that comes from |
|---|---|---|
| #23 | removing the limit finding 70 puts on `t_v8create`'s comparison | finding 70 |
| #33, #35–#45 | the V.34 fast pass | the line at the top of this file |
| #36 | `V34RX.c`'s remaining functions | finding 117 lists what was left in it |
| #38 | `V34hshak.c`'s support functions — that TU except `v34handshak` | this session's brief; findings 170–149 |
| **#39–#45** | **`v34handshak` itself**, 61,541 bytes over 87 states, split seven ways | findings 117 and 144, `include/dsplib/v34hshak.h` |
| #47 | the deferred work listed above | this file |
| #49 | check whether `demapFrame` can produce an out-of-range group | finding 129 |

`#34`, `#37`, `#40`–`#44`, `#46` and `#48` are named nowhere. `#34` is
outside the fast pass's stated range; the rest fall inside a range and have
no content recorded against them individually.

### The state-to-task split for #39–#45 is unassigned

Nothing records which of the eighty-seven states belong to `#39`, and it
**cannot be read off the state list**. What a dispatch case owns has to come
from the control-flow graph — that is the whole argument in
`tools/cfgsplit.py`'s header, which exists because the obvious estimate
(distance to the next jump-table target) produced three different 45–55 KB
"states", none of which exist.

**What is now settled, and was not when this section was written.** The
three state words are named — +0x3592 microstate, +0x3594 rxstate, +0x3596
txstate (finding 180) — and `StateName` is emitted, so `cfgsplit`'s
per-state byte counts can be read against the author's names rather than
against indices. The three machines are concurrent, so a per-state split has
to say which machine a state belongs to before it can say which task.

`cfgsplit` has not been run on `v34handshak`. The recorded runs are
`rxtiming` (finding 121k), `decodeDepth` (finding 131) and one that reported
a whole function unreached. Pointing it at `v34handshak` needs its three jump
tables located first, and `--entries` was needed for both of the functions it
has been run on — so **locating the dispatch and running `cfgsplit` is the
first job of #39, not a prerequisite for starting it.** Until that is done,
"#39" names a share of the work and not a set of states.

### And now it has been run, and the split is by machine

`cfgsplit --func v34handshak` finds the three tables by itself, and finding
213 says which machine feeds each: two of them are the transmit state and
one is the microstate, while the receive state has no table at all. So the
split is **by machine, not by state number** — a state *value* means
different things to different machines, and 51 is `TX_L1` to two of them.

```
  table 1  +0x2da0  txstate, inside the per-sample loop   ~21.1 KB, 25 cases
  table 2  +0x2ee8  txstate, once per block                ~0.3 KB, 16 cases
  table 3  +0x3000  microstate, after fskdemodulate       ~27.6 KB, 16 cases
  no table          rxstate, by compare chain             inside the 12,290
                                                          cfgsplit calls shared
```

Three pieces, then, and not seven. The rest of "#39–#45" is the
prerequisites: `v34handshak` reaches 64 functions this tree has not written,
33,406 bytes once its own 61,541 are set aside — 14 C at 17,403 and 50 C++ at
16,003. That is the *transitive* closure, not a direct-callee count: it
bottoms out there and does not pull in the rest of the C++ half
of `VPcmV34Main.cpp`.

It was written here that every test links all of `$(OBJ)`, so **none of the
three pieces can be committed until all of it exists** — the same answer
this tree gives for `CALLPROG_Progress`, `b103_process` and
`FPM_iir_filt_block`, at sixty times the size.

**That does not follow, and finding 214 is why.** The link constraint is
real, but it comes from `symmap.py` renaming *every* defined blob symbol to
`ref_*`, which is a choice made when the harness was built for leaf
functions. Rename only what we define and an unwritten callee resolves to
the blob's own copy for both sides at once. Spiked on branch
`symmap-scaffold-spike`: 62 of 62 binaries pass, and a caller of the
unwritten `probeselect` links and runs. The closure of all 64 unwritten
callees is 111 functions with no store to `.bss` or `.data`, so sharing one
physical copy carries nothing between the sides.

Not landed, because it spends a free invariant — today "the suite links"
proves everything reachable from what we have written *is* written. Whether
to spend it, and what ratchet replaces it, is the open decision. What is
settled is that the 16 KB of V.90/V.92 C++ is a **scheduling** question and
not a precondition.

### The decision, and what it does to #59's ordering

Finding 215 settles it: the scaffold is **not** landed, because V.90/V.92 is
the project's end goal and the prerequisite set is bounded, so the ordering
costs nothing that the invariant does not buy back.

**With that decided, the C++ is a precondition again — for six of #59.**
Finding 217 names them: `indicateJaTransmission`, `V34SetINFO1aBits`,
`V34GiveINFO1dBits`, `VPcmV34InitiateRetrain`, `v90Phase34` and
`k56FlexPhase34` each reach a `VPcmV34Main.cpp` method, and a caller whose
callee has been renamed to `ref_*` leaves an undefined symbol that breaks all
62 binaries, not just its own. So **#60 comes before those six**;
`datapumpv34` comes after #56–#58; and `getbit`, `ApplyBulkDelay` and
`getMPrecvdBits` are file-local, have no `ref_` alias, and are blocked on a
harness change rather than on code.

**#59 is therefore not one sitting.** About 8.7 KB of it is available and
8.2 KB is not, and the hand-over that said "nothing here is blocked" was
reading the same list finding 215's blanket claim was.

**The available 8.7 KB is done** — all seven functions, with differential
tests and mutation sets (findings 216 to 219). What is left of #59 is the
8.2 KB that needs #60, #56–#58, or a harness change first.

### The renumbering, and the two task stores

`#39`–`#45` named a seven-way split by state that finding 213 shows does not
exist. They are superseded by a three-way split by machine plus the
prerequisites:

| old | new | what it is |
|---|---|---|
| #39 | **#56** | `v34handshak` part 1 — txstate, tables 1 and 2, ~21.4 KB |
| #40–#44 | **#57** | `v34handshak` part 2 — microstate, table 3, ~27.6 KB |
| #45 | **#58** | `v34handshak` part 3 — rxstate by compare chain, ~12.3 KB |
| — | **#59** | the 17.8 KB of C prerequisites |
| — | **#60** | the 16 KB of V.90/V.92 C++ from `VPcmV34Main.cpp` |
| — | **#61** | the two-instance transcript oracle |

`#45` was "V.92 modem-on-hold states" and does **not** map cleanly onto #58;
the MOH states are dispatched from the microstate table, so most of #45 is
inside #57. Nothing is lost, but a hand-over quoting "#45" should be read as
"the MOH share of #57", not as #58.

**A second task store exists.** The `v34hshak` session numbers its own work
`#11`–`#22`, and those are different tasks from `#11`–`#22` here — this
store's `#11` is the Bell 103 rate conversion. Any hand-over quoting a task
number must say which store it means. This is the same collision that took
`204` and `205` twice (finding 214's neighbours, 212 and 213, are the
survivors), and the same fix applies: write the mapping down where three
sessions can read it, which is here.
