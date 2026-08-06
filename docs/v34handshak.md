# Reconstructing `v34handshak` one dispatch case at a time

`v34handshak` is 61,541 bytes, the largest function in the object. This is how
to take a piece of it without spending a session, what the harness that makes
that possible does, and what it cannot do.

Read `docs/largefunctions.md` first for *why* the work is split this way, and
`docs/fastpass.md` for how #56, #57 and #58 came to be three tasks rather than
seven. This file is the operating manual.

## What the function is

Three concurrent state machines and four dispatches, chosen by four guards
read off the prologue at 0x628f0:

```
    v34handshak(obj):
        if  [obj+0x221c] < [obj+0x2aa0]           ; signed 16-bit
                -> TABLE 1, .rodata+0x2da0, on txstate, INSIDE a per-sample loop
        else if [obj+0x264] <= 5                  ; receiver +0x00
                -> TABLE 2, .rodata+0x2ee8, on txstate, once per block
        else
                the rxstate compare chain at 0x62a02:
                    43 RX_DPSK -> V34agc, fskdemodulate, then
                                  TABLE 3, .rodata+0x3000, on MICROSTATE
                     4 RECEIVE -> 0x653e4
                    35 WAIT    -> 0x6752c
                    below 43   -> falls into TABLE 2
                    above 43   -> a SECOND chain at 0x62b71:
                                    53 -> 0x65473    72 -> 0x650c6
                                    otherwise -> TABLE 2, by its other door
```

So table 2 has three entrances and not two; all three converge exactly, and
that is measured over seventeen states rather than assumed (finding 361).

The three state words are plain halfwords in the object (finding 213):

```
    obj + 0x3592    microstate      table 3, index = microstate - 41, 41..80
    obj + 0x3594    rxstate         no table: a compare chain
    obj + 0x3596    txstate         tables 1 and 2, index = txstate - 5
```

Neither `V34agc` nor `fskdemodulate` writes any of the three -- swept over the
whole of `.text`, finding 285 -- so a microstate written before the step is
still there when the dispatch reads it. That is what makes this harness
possible, and it did not have to be true.

**The three names are one table.** `StateName` is indexed by all three
machines, so state *value* 51 is `TX_L1` to the transmit machine and to the
microstate machine and they run different code for it. `include/dsplib/v34hshak.h`
has the eighty-seven names.

## The harness

`test/harness/v34hsstep.h` is the API and is commented; this is the shape.

```c
    v34hs_setup(0);                              /* both objects, brought up */
    v34hs_route(V34HS_ROUTE_RXCHAIN, 0);         /* pick the dispatch        */
    v34hs_state(mst, V34HS_RX_DPSK, txstate);    /* the three halfwords      */
    v34hs_poke_short(0xaae2, whatever);          /* the case's companions    */
    v34hs_step();                                /* one call on each side    */
    v34hs_compare("what this case is", tag);     /* everything, both sides   */
```

`v34hs_setup` fills both objects with varied pseudorandom bytes from a fixed
LCG -- never zeroed, both sides identical (finding 230) -- aims every pointer
field, and then runs `V34InitializeImplementationSpecific` and
`v34handshakinit`. **Side A runs ours and side B the blob's**, which is what
makes the comparison a check rather than a tautology on a case where both
sides still call `ref_v34handshak` for the step itself. On a case that has
been reconstructed, `v34hs_side_a` puts the reconstruction on side A and the
step is differential too -- see "When you land a case".

`v34hs_setup` also gives each side ONE ARENA: the object and all five blocks
it points at, at fixed offsets inside a single 64 KB-aligned block with 32 KB
of filler between and around them, side B's arena a byte copy of side A's.
That is not tidiness. With the two sides as ten separate statics at ten
addresses the linker chose, the per-sample transmit route failed at 23 of 24
object fills, and the harness spent a session concluding that the object was
at fault (findings 319 and 320, D60 retracted). Bisected against the old
fixture: it is the placement of the five BLOCKS that matters, not of the
object -- wrapping the object alone changes nothing, and `V34HS_LOOSEOBJ=1`
puts side B's object back outside its arena and the sweep still passes.
**A fixture whose two sides differ in their geometry is measuring the
linker.**

`v34hs_compare` compares the whole 44,096-byte object byte for byte with the
thirty-five pointer fields excluded, every one of those thirty-five by offset
from its own base, the rest of the arena -- five blocks, seven filler regions
and the space around them -- and both transcripts. `v34hs_holes_check()`
asserts once at the end of a run that every one of the thirty-five skips was
exercised, so the list cannot go stale unnoticed.

**Which block a pointer selects is checked now.** A pointer out of the object
is classified three ways, not two: into its own object (offset compared), into
its own arena (offset compared, which says which block and where in it), or
outside both, which is a library table or function where side A holds ours and
side B the blob's -- two addresses of two copies, which no address comparison
can tell from two different tables. For that last class `t_v34hsstep.c` runs
the whole sweep a SECOND time with the blob's bring-up on both sides, and then
the two must select the identical address. All twelve that qualify do, and
that pass is in `make phase`. Finding 324. This is the gap earlier versions of
this file told you to close with `t_v34hshak.c`'s `compare_table`; it is
closed.

### When you land a case

**`v34hs_side_a(fn)`, not `V34HS_OURS`.** The define points side A at
`v34handshak`, which cannot exist until all four dispatches do -- so it is the
swap for the end of the work, not for a case. `v34hs_side_a` installs one
function for one test: `t_v34hsstep.c` leaves it NULL and goes on proving the
fixture, and `t_v34hstbl2.c` installs `v34handshak_txblock` and drives only
the states table 2 owns. It moves side A's debug capture slot with the
function, which matters: our code writes slot 0 and the blob slot 1, and
leaving it at 1 compares the blob's transcript against itself. Finding 365.

Name the reconstruction after the dispatch, not `v34handshak`.
`tools/coverage.py` files a symbol the blob does not have under "a helper
split out of a larger function", where `v8_handshak_agc` already is, and a
`v34handshak` that silently did nothing for three of its four dispatches is
exactly the wrong-but-plausible artefact CLAUDE.md forbids.

Until a case lands the harness is proving *itself*, which is the point: a
per-case agent needs to know the fixture is deterministic, address independent
and fully seeded before its own failures mean anything.
Call `v34hs_ours(1)` in your test and side A becomes `v34handshak` instead of
`ref_v34handshak`; your test is then an ordinary tier-1 differential test with
no other edit anywhere. The default is off, which leaves the harness proving
*itself* -- a per-case agent needs to know the fixture is deterministic,
address independent and fully seeded before its own failures mean anything.

**It is a run-time switch and not `-DV34HS_OURS`, and the reason is not
style.** One `v34hsstep.o` is linked into every test binary, so the macro
would move side A for `t_v34hsstep.c` too, whose whole claim is a
blob-against-blob property over forty-three cases most of which have no
reconstruction. Finding 356. `V34HS_OURS` still compiles and now sets the
default.

**`v34handshak` is partial and HALTS on an arm nobody has written.**
`t3c_unwritten()` calls `abort`. So a test that turns the switch on must drive
only states some batch has landed, which today are:

```
  table 3   the arm 24 states share (0x6590b) and the default (0x65329)
            62 RX_PHASE3_CALL, 79 MOH_TONE, 80 MOH_TONE_DROP
  table 2   0x644c9 only -- txstates 24, 51, 54, 60, 74 -- and the tail at
            0x62a40 that every arm of that dispatch falls into
  the rest  halts
```

**Pick your txstate for the tail you want.** Every table-3 arm ends in the
transmit dispatch, so a microstate case is a microstate arm AND a transmit
arm. txstates 75, 76, 77, 81, 85 and 86 are above table 2's window and select
its default, which is written; anything in 5..74 needs that arm to exist.
`t_v34hst3core.c` uses MOH_SILENCE (81) and says so.

**Aim a pointer with `v34hs_poke_self_ptr`, never `v34hs_poke_int`.** The two
objects are at different addresses, so one address written into both is
precisely the asymmetry findings 319-322 are about.

**And `V34HS_REFINIT=1` does not apply to a case whose arm installs a library
table.** 80's retrain calls `v34handshakinit` from inside the step, so side A
installs ours and side B the blob's, and ten pointers then hold two addresses
of two copies -- the case finding 324 says no address comparison can settle.
Same caveat, same reason, as `v34hs_holes_check`. Finding 359.

### Three things the fixture already learned so you do not

- **Keep every state word in 0..86 while the diagnostics are on.** `StateName`
  is indexed unbounded (D42) and `v34handshakinit` prints the state it is
  *leaving*, so a pseudorandom halfword is a wild `char *`. The fixture faulted
  inside its own bring-up until it seeded all three first.
- **Aim a pointer before anything runs.** A fixture that lets one through
  faults rather than fails, and a fault has no offset in it.
- **Never put the state word you entered with into a signature.** It sits in
  the object, so a comparison including it calls every case distinct from every
  other -- seventeen microstate targets "separated" perfectly while six were
  doing the same thing. Finding 290.
- **The two sides must be congruent in memory, not merely equal in it.**
  Identical bytes at two addresses with two different sets of neighbours is
  not enough, and the route that finds out is table 1. It is the blocks the
  object points at that have to be congruent, not the object. Findings 319
  and 322.

## Table 3, the microstate machine -- this is #57

Sixteen targets over states 41..80, 27.6 KB, and it is what the per-case split
was invented for. cfgsplit's exclusive byte counts, and what the harness sees
when each is entered cold with rxstate 43 and txstate 18 (SSEG):

| microstate | target | bytes | cold behaviour |
|---|---|--:|---|
| 44 `DET_INFO` | 0x668c0 | 6046 | group A -- writes 23 B |
| 41 `DET_SYNC` | 0x669a4 | 3945 | group A -- writes 23 B |
| 46 `TX_PHASE1_ANS` | 0x65d6d | 3198 | group B |
| 59 `RX_PHASE2_CALL` | 0x662b0 | 2385 | **its own** -- 77 B, 4 traces |
| 58 `RX_PHASE1_CALL` | 0x66003 | 1853 | group D |
| 51 `TX_L1` | 0x65c47 | 1735 | group D |
| 55 `TX_PHASE1_CALL` | 0x65b72 | 1705 | group D |
| 49 `RX_PHASE1_ANS` | 0x66a0d | 1265 | group D |
| 50 `RX_PHASE2_ANS` | 0x664b8 | 1182 | group D |
| 63 `INFODONE` | 0x6591e | 1115 | group B |
| 47 `TX_PHASE2_ANS` (+56) | 0x66834 | 896 | **its own** -- 78 B, 3 traces |
| 79 `MOH_TONE` | 0x657ca | 848 | group G -- LANDED |
| 80 `MOH_TONE_DROP` | 0x656e0 | 669 | group G -- LANDED |
| 48 `TX_PHASE3_ANS` | 0x65d30 | 422 | group D |
| 62 `RX_PHASE3_CALL` | 0x65c7a | 310 | **its own** -- 24 B, 2 traces -- LANDED |
| 42 43 45 52 53 54 57 60 61 64..78 | 0x6590b | 19 | group B -- LANDED |

Seven distinct behaviours from seventeen representatives -- a property of the
fixture's seed as well as of the object, so a different fill could separate
one more or one fewer. The groups:

```
  A  41, 44                    two real cases that agree cold
  B  42, 46, 63, and any state outside 41..80 (the default at 0x65329)
  C  47                        }
  D  48, 49, 50, 51, 55, 58    the six that bump the counter at +0xaa78
  E  59                        }
  F  62                        }
  G  79, 80
```

**Group D is six DIFFERENT arms, not one.** 48, 49, 50, 51, 55 and 58 have
six distinct entries in `.rodata+0x3000` -- 0x65d30, 0x66a0d, 0x664b8,
0x65c47, 0x65b72, 0x66003 -- and none of them is 0x6590b. Their agreeing cold
is the fill's doing. Finding 351.

**Group B and group D are the shape of the machine, not a harness defect.**
The arm twenty-four states share is three instructions -- read txstate, jump to
the once-per-block dispatch at 0x62af1 -- and so is the default. Group D's six
increment one counter, test it against one or two thresholds, and jump to the
same place. Finding 288.

Two things follow, and they are how #57 should be planned:

- **The txstate a microstate case is driven with is part of the fixture.**
  Most arms end in the transmit dispatch, so a microstate case tested with a
  txstate whose table-2 arm does nothing is a case tested against silence.
  `t_v34hsstep.c` holds it at SSEG and says so; a per-case test should choose
  deliberately and record the choice.
- **It is not sixteen equal pieces.** 44, 41 and 46 are 13.2 KB of the 27.6.
  The shared arm is three instructions. Take 62, 79, 80 and the shared arm
  first: they are small, three of them already separate cold, and landing them
  puts `v34handshak` in `src/` so `V34HS_OURS` starts paying.

### Getting a case out of its group

A case in group B or D is not doing nothing -- it is taking its first guard
and leaving. Read the head of its arm and seed what it reads. The heads are
short; 41's is

```
  669ad  movzwl 0xaae2(%ebx),%ebp     ; and compares %dl against 0x72
  669cd  movzbl 0xabe8(%ebx),%ecx     ; and branches if non-zero
  669dc  movzwl 0x358a(%ebx),%eax     ; and branches if non-zero
```

so three companion fields put it somewhere other than where the fixture's fill
happens to send it. `V34HS_DUMP=1 ./build/test/t_v34hsstep` prints the
signature of every state driven, which is how to tell whether a seed moved the
case.

## Table 2, the transmit supervisor -- DONE

`src/pump/v34/v34hstxblock.c` and `test/unit/t_v34hstbl2.c`, 11,009 checks.
The first tier-1 differential test of any part of `v34handshak`. Findings
360-366.

Seven targets over txstates 5..74, about 0.3 KB:

```
   5            0x64480      }  agree cold, separated by microstate 63 and
  24 51 54 60 74 0x644c9     }  +0x359c == 0x66
  18 19        0x64518       }  agree cold, separated by bit 3 of the
  20 21 64 68  0x64509       }  receiver's flags at +0x122
  66 67 69     0x644fa       }  agree cold, separated by +0xe4c != 0
  70           0x644d8       }
   6 and 53 others           the default at 0x62a40, which is also the tail
                             every other arm falls into
```

**Four behaviours cold and all seven distinct once three companion fields
move**, so finding 290's count is about the fill and not about the object.
Fifty-FOUR of the seventy entries are the default; finding 286 says
fifty-three. Finding 364.

Reach it with `v34hs_route(V34HS_ROUTE_TXBLOCK, 0)`, which sets the cursor at
the limit and the receiver count to 5 -- or with `V34HS_ROUTE_RXCHAIN` and an
rxstate that is neither 43, 4, 35, 53 nor 72. **There are three doors, not
two**: rxstate above 43 leaves the chain at 0x62a12 for a second chain at
0x62b71 which reaches the same dispatch. All three converge exactly, measured
over seventeen states (finding 361).

Two things a per-case agent on table 1 or table 3 should take from it:

- **The tail at 0x62a40 can erase what an arm decided**, on four conditions
  no arm reads, and at `V34HS_SEED=10` the fill makes it do so on every case.
  Pin the tail's five inputs -- +0x2218, +0x238, +0x23c, the receiver's
  +0x134 and +0x230 -- and the arms' answers stop depending on the seed.
- **The transcript axis is worth nothing here**: the whole closure holds no
  `call` and no debug site, so both sides print zero lines and the transcript
  comparison passes by construction (finding 362).

## Table 1, the per-sample transmit loop -- this is #56, and it is open

Twenty targets over txstates 5..86, in the loop at 0x62950. **It compares,
and it is in the default sweep.** It used not to; findings 319-322 are what
that took and D60 is the retraction. Nothing about the route is special any
more except that it is the one the harness's geometry could break, so if a
case here starts disagreeing, read D61 before reading your own code.

Entered cold with microstate `PHASE1`, rxstate `SILENCE` and a budget of one
sample, **eighteen of the nineteen reachable targets have their own
behaviour** -- better than table 3's seven-from-seventeen or table 2's
four-from-seven, because these arms run a modulator rather than setting a flag
and leaving. The only pair that agrees cold is 24 `TX_DPSK` and 60 `TONE_AB`.
Finding 323 has the per-target table of bytes written and progress code; read
it before choosing what to take first, because the six that write nothing
below +0x234 -- 65, 71, 78, 81, 85, 86 -- are the small ones.

### Thirteen of the nineteen have landed, and this is how a case is landed

txstates 65, 71, 78, 81, 85 and 86 -- the six that write nothing below +0x234
-- then 60, 18, 70 and 51, the four smallest of what those left, and then 19,
20 and the entry 5, 54 and 74 share, are in `src/pump/v34/v34hstx1.cpp` and
compare byte for byte through `test/unit/t_v34hstx1.c`. Findings 340-345, 421
and 422.

The six still open are 69, 64/68, 67, 24, 21 and 66, which are the six
largest. **Two of the thirteen needed a table before they could be written at
all**: `probe`, 64 signed shorts at `.rodata+0x2c00`, which 51 reads and which
this tree did not have (finding 421); and `vectpp`, 96 shorts at
`.rodata+0x2c80`, which 20 reads as forty-eight FOUR-byte points and which was
`static` in v34rx.c (finding 422). Read both before estimating one of the six
-- an arm that reads a table this tree does not have, or has under the wrong
element width, is not the size its byte count says.

**AND ONE TABLE ENTRY IS NOT ONE ARM.** 0x640b4 is the target for txstates 5,
54 and 74, and the shared prologue re-reads `txstate` at 0x640f4 and gives
each of the three a different tail. 0x635cc, shared by 64 and 68, has not been
read yet; do not assume it is one behaviour because finding 323 lists one
representative. Finding 422.

**`V34HS_OURS` is not how a case lands and cannot be.** It is one `#ifdef` in
one shared harness object, so it demands all forty-three cases at once, and no
case can be the first if landing one requires all of them. What replaces it,
per case, is the loop's own guard at 0x62933: with the queue count already at
the block's limit the blob skips the per-sample loop entirely, so an arm that
leaves the count at the limit can run FIRST on side A and the blob then
contributes exactly the tail.

```c
    v34hs_route(V34HS_ROUTE_TXSAMPLE, 1);   /* count 0, limit 1        */
    v34hs_state(V34HS_PHASE1, V34HS_SILENCE, 65);
    rc = v34hs_step_case(v34tx1_xmit0);     /* A: ours + tail          */
    v34hs_compare("65 XMIT0", tag);         /* B: the blob's + tail    */
```

`v34hs_step_case` installs the arm inside the snapshot, the alarm and the
observation; calling it from the test before `v34hs_step` does not work, and
finding 342 says why. **Run the case twice** -- once with the arm alone, to
see the count reach the limit and the arm write something, and once through
the fixture -- because an arm that did nothing leaves side A's step to run the
blob's copy of it and the comparison passes for free.

**The diagnostics must be off** on such a test: our code logs to capture
channel 0 and the blob's to channel 1, and a side running both reaches only
one of them. That is finding 341's gap and it also costs two mutations.

**Seed what the arm writes.** Eleven claims were untestable against a cold
object simply because the field already held the value the arm stores --
finding 345 lists them, and it is the first thing to check when a mutation
goes uncaught.

The txstate a table-1 case is driven with IS the case, so unlike #57 there is
no companion-field problem to solve first. What there is instead:

- **The loop does not always terminate.** Its default arm is the loop bottom
  itself, so a txstate with no case of its own spins forever (finding 287,
  D59). Fifty-seven of the table's eighty-two entries are that default.
  `v34hs_step` arms a `SIGALRM` so this is a named case rather than a run that
  never returns, and `v34hs_route(V34HS_ROUTE_TXSAMPLE, n)` takes the sample
  budget explicitly for the same reason. Both halves are demonstrated:
  `V34HS_HANG=1 ./build/test/t_v34hsstep` drives txstate 6 and exits 3 naming
  the state.
- **The transcript contributes nothing here.** Every table-1 case prints
  zero diagnostic lines with the diagnostics on, so the separation above rests
  entirely on bytes written, the signature and the progress code. If you want
  a second axis, seed the counter the arm reads: 78's at 0x64139 traces only
  when +0xaa78 decrements to zero, and that is also the path that reaches
  `V34EchoReportCoeff`. Other arms will have their own.
- **Several arms leave the function through another one.** 78 `JaTXMIT`
  decrements the counter at +0xaa78, calls `V34EchoReportCoeff` twice if it
  reaches zero, and then calls `v90Phase34` before rejoining at 0x62d70 --
  so a reconstruction of 78 is mostly a call, and the interesting part is
  elsewhere. Read the arm before estimating it.

## The environment knobs

```
  V34HS_DUMP=1       print every case's signature
  V34HS_DIAG=1       print every differing offset, and the seed tables
  V34HS_REFINIT=1    bring side A up with the blob's initialisers too, which
                     separates "the fixture" from "our v34handshakinit" AND
                     is the run that checks which library table a pointer
                     out of the arena selects
  V34HS_HANG=1       drive a state with no case, to see the alarm fire
```

And five that exist to make the fixture's own claims falsifiable rather than
asserted. If a case of yours disagrees, run these before suspecting anything
else: a case that passes at one layout and fails at another is a fixture
problem, and a case that fails at all of them is yours.

```
  V34HS_SEED=n       a different object fill (0..23 were swept)
  V34HS_SKEW=n       move side B's whole arena n bytes
  V34HS_OBJSKEW=n    move side B's OBJECT n bytes inside its own arena
  V34HS_PADVARY=k    make padding region k differ between the sides
                     (1..7, or 0 for all seven)
  V34HS_NOSCRUB=1    do not scrub 64 KB of stack before each call
  V34HS_LOOSEOBJ=1   put side B's object OUTSIDE its arena, which is the
                     positive control finding 319's bisect rests on
  V34HS_PROBE=1      print the FPU status before each call, whether the two
                     sides were equal after setup, whether the step wrote
                     outside the blocks, and whether a second run of side B
                     at the same address gives the same answer
```

`V34HS_TXSAMPLE` is gone: that route is in the default sweep.

## Reading the object

Use `tools/dis.py`, never raw `objdump`, for anything that touches a table:
these three tables are `R_386_32` relocations against `.text` whose addend is
in the data, and every convenient way of trimming objdump's output drops the
relocation lines and turns a table of pointers into a table of plausible small
integers. That mistake has been made three times here.

`tools/cfgsplit.py --func v34handshak` gives the per-case byte counts, but it
merges the three machines' state values into one numbering -- `51` appears
twice and neither entry says which machine it belongs to. Read it against
finding 286's table, not on its own.
