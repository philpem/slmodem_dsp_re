# Reconstructing `v34handshak` one dispatch case at a time

`v34handshak` is 61,541 bytes, the largest function in the object. This is how
to take a piece of it without spending a session, what the harness that makes
that possible does, and what it cannot do.

Read `docs/largefunctions.md` first for *why* the work is split this way, and
`docs/fastpass.md` for how #56, #57 and #58 came to be three tasks rather than
seven. This file is the operating manual.

## The guards, and what each covers TODAY

Read this from the tree, not from here: `grep -n 't3m_notwritten(\|t3c_unwritten(' src/pump/v34/V34hshak.c`.
As of the session that landed rxstate 53's arm:

| guard | still covers | bytes |
|---|---|--:|
| `T3M_UNWRITTEN_TBL1` | **RETIRED.** 81's wrap at 0x66d85 and 86's segment end at 0x66fe9 are written, in the arms, and NEITHER turned out to be a transfer out of the loop -- so the loop's dispatch on an arm's return value has gone too, and with it two of `enum v34tx1_exit`'s three values. Finding F748 | -- |
| `T3M_UNWRITTEN_RXSTATE` | **RETIRED.** rxstate 72 RX_L1 (0x650c6) is written and the guard has no call site left. All five arms of the chain and both transmit-dispatch doors are written; the constant is still in `v34hshak.h` beside the other four that no path reaches | -- |
| `T3M_UNWRITTEN_FSKGATE` | **RETIRED.** The arm at 0x6754b is written and the guard has no call site left. The code is still in `v34hshak.h` beside the other four that no path reaches | -- |
| `T3M_UNWRITTEN_OTHER` | `t3c_unwritten()` at ONE site: the table-3 `default:`, which is unreachable. 0x6d57c and 0x6c8f8 are written, and they were never microstate 44's -- finding F748 | -- |

**THE TABLE-3 `default:` IS NOT WORK AND NEVER GOES AWAY.** Its own comment
says so: the fifteen written arms and the twenty-four shared ones are forty
labels over the forty values the range test admits, so it is unreachable and
is kept because the range test and the label set are two statements of one
fact. So "remove the `PARTIAL` line when the last guard goes" was always the
wrong criterion -- one guard is a permanent structural assertion. The
criterion is **when no REACHABLE arm is unwritten**.

**THAT CRITERION IS MET AND `tools/coverage.py`'s `PARTIAL` ENTRY HAS COME
OUT.** `v34handshak`'s 61,541 bytes now count as translated and `make
coverage` reads **30.1%** where it read 21.7%. The number of guarded reachable
bytes went 11,398 -> 10,310 (finding F725, F53 DET_AB) -> 8,678 (731, 4 RECEIVE)
-> 3,811 -> zero for the rxstate chain (738, 72 RX_L1), and then table 1's two
and the two Modem-on-Hold sites went with findings F748-753. Read the guard
table above from the tree before quoting any of this; the grep at the top of
this section is the check, and one call site is the pass.

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
that is measured over seventeen states rather than assumed (finding F361).

**THE rxstate CHAIN IS 11,537 BYTES OVER FIVE ARMS, AND ALL FIVE ARE
WRITTEN** -- not the inherited "~12.3 KB", which counts the shared tail at
0x62a40 and the 0x64a8f preamble against the arms (finding F716). The number
moved from 11,433 when `cfgsplit.py` stopped dropping 131 bytes of every walk
(finding F737), so **every count in this section is a re-measurement and not a
copy**, taken with barriers at 0x62af1, 0x62a40, 0x629ed, 0x62a02 and 0x62b71.
The last three are not optional and the inherited note did not have them: a
table-1 arm falls out of the per-sample loop at 0x629ed and reaches the whole
chain, so without them every arm reaches every other and `cfgsplit.py` reports
exclusive = 0 for all of them.

```
    0x653e4   rxstate  4 RECEIVE       4,881 bytes, 28 ranges, 165 blocks  DONE
    0x650c6   rxstate 72 RX_L1         3,811 bytes, 24 ranges,  95 blocks  DONE
    0x65473   rxstate 53 DET_AB        1,632 bytes, 10 ranges,  41 blocks  DONE
    0x6754b   the FSK gate's body      1,182 bytes,  7 ranges,  39 blocks  DONE
    0x6752c   rxstate 35 WAIT             31 bytes,  1 range,    1 block   DONE
    0x64a87   the FSK gate's TEST      already written
```

**THE FSK GATE'S BODY DOES NOT AGREE WITH FINDING F719, AND IT IS NOT FINDING
F737.** 719 says 1,089 bytes over 38 blocks; this walk says 1,182 over 39, and
so does the walk done with `cfgsplit.py`'s OLD instruction sizing, which gives
1,181. The old sizing reproduces every other inherited number exactly --
3,806 for rxstate 72, 1,629 for 53 (finding F716's figure, corrected to 1,632
by hand in 727) and 4,875 for 4 RECEIVE -- so the tool is not the difference
here and 737's 131 bytes are not either. The block COUNT is 39 both ways,
which no byte-accounting change can move. 719's figure is not reproducible by
any barrier set or dispatch-target set tried, and nobody has re-derived its
method. Recorded rather than reconciled: that arm is landed and tested, so
the disagreement is about accounting. Finding F747.

**AND 3,811 HAS TWO INDEPENDENT DERIVATIONS**, which is the strongest thing
about it: the handed analysis reached it with two barriers and a restricted
set of other targets, this session's walk with five barriers and every table
target, and they agree on the bytes, the twenty-four ranges and the
ninety-five blocks.

Every callee all five need is already defined in this tree, so unlike table 1
-- where `probe` and `vectpp` had to be recovered before an arm could be
written at all (421, 422) -- nothing here is blocked on a missing function.

The three state words are plain halfwords in the object (finding F213):

```
    obj + 0x3592    microstate      table 3, index = microstate - 41, 41..80
    obj + 0x3594    rxstate         no table: a compare chain
    obj + 0x3596    txstate         tables 1 and 2, index = txstate - 5
```

Neither `V34agc` nor `fskdemodulate` writes any of the three -- swept over the
whole of `.text`, finding F285 -- so a microstate written before the step is
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
LCG -- never zeroed, both sides identical (finding F230) -- aims every pointer
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
at fault (findings F319 and F320, D60 retracted). Bisected against the old
fixture: it is the placement of the five BLOCKS that matters, not of the
object -- wrapping the object alone changes nothing, and `V34HS_LOOSEOBJ=1`
puts side B's object back outside its arena and the sweep still passes.
**A fixture whose two sides differ in their geometry is measuring the
linker.**

`v34hs_compare` compares the whole 44,096-byte object byte for byte with the
thirty-seven pointer fields excluded, every one of those thirty-seven by
offset from its own base, the rest of the arena -- five blocks, seven filler
regions and the space around them -- and both transcripts.
`v34hs_holes_check()` asserts once at the end of a run that every one of the
thirty-seven skips was exercised, so the list cannot go stale unnoticed. The
list was thirty-five until the first case ran `initdigital` inside a step and
the two shell contexts' `coeff` pointers came back as differing object bytes;
finding F734.

**Which block a pointer selects is checked now.** A pointer out of the object
is classified three ways, not two: into its own object (offset compared), into
its own arena (offset compared, which says which block and where in it), or
outside both, which is a library table or function where side A holds ours and
side B the blob's -- two addresses of two copies, which no address comparison
can tell from two different tables. For that last class `t_v34hsstep.c` runs
the whole sweep a SECOND time with the blob's bring-up on both sides, and then
the two must select the identical address. All twelve that qualify do, and
that pass is in `make phase`. Finding F324. This is the gap earlier versions of
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
leaving it at 1 compares the blob's transcript against itself. Finding F365.

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
reconstruction. Finding F356. `V34HS_OURS` still compiles and now sets the
default.

**`v34handshak` IS NO LONGER PARTIAL, and the list below is now a record of
how it got there rather than a restriction on what a test may drive.**
`t3c_unwritten()` still calls `abort`, and there is still exactly one call of
it -- table 3's `default:`, which forty labels over the forty values the range
test admits make unreachable. Every reachable arm of every dispatch is
written:

```
  table 3   the arm 24 states share (0x6590b) and the default (0x65329)
            41 DET_SYNC, 62 RX_PHASE3_CALL, 79 MOH_TONE, 80 MOH_TONE_DROP
            62 RX_PHASE3_CALL, 79 MOH_TONE, 80 MOH_TONE_DROP
            44 DET_INFO COMPLETE -- the bit clock at 0x668c0, the restart
               at 0x6bda0, and all four arms of the accept path: the
               default at 0x6e552, and 0x6f438 (INFO1c), 0x6ed17 (INFO1a)
               and 0x6ea38 (Modem-on-Hold), which are selected by a message
               length of 0x4d, 0x26 and 0x08 (findings F400-406, 440-448)
  table 2   ALL SEVEN targets -- 0x64480, 0x644c9, 0x644d8, 0x644fa,
            0x64509, 0x64518 and the default at 0x62a40, which is also the
            tail every arm of that dispatch falls into. `t_v34hstbl2.c`
            drives every one; the "0x644c9 only" this line used to say was
            the FOURTH stale status line found in this file
  the chain EVERY rxstate: 43 to table 3, 4 RECEIVE (0x653e4, findings
            F731-735), 35 WAIT, 53 DET_AB (0x65473, findings F724-729),
            72 RX_L1 (0x650c6, findings F738-745) and the two default
            doors into table 2.  `t_v34hsrxch.c` drives all eighty-seven
  the rest  NOTHING.  Every reachable arm of every dispatch is written,
            and the only `t3c_unwritten()` left is table 3's `default:`,
            which forty labels over forty values make unreachable
```

**Pick your txstate for the tail you want.** Every table-3 arm ends in the
transmit dispatch, so a microstate case is a microstate arm AND a transmit
arm. txstates 75, 76, 77, 81, 85 and 86 are above table 2's window and select
its default, which is written; anything in 5..74 needs that arm to exist.
`t_v34hst3core.c` uses MOH_SILENCE (81) and says so.

**Aim a pointer with `v34hs_poke_self_ptr`, never `v34hs_poke_int`.** The two
objects are at different addresses, so one address written into both is
precisely the asymmetry findings F319-322 are about.

**And `V34HS_REFINIT=1` does not apply to a case whose arm installs a library
table.** 80's retrain calls `v34handshakinit` from inside the step, so side A
installs ours and side B the blob's, and ten pointers then hold two addresses
of two copies -- the case finding F324 says no address comparison can settle.
Same caveat, same reason, as `v34hs_holes_check`. Finding F359.
**`v34hs_side_a(fn)`, at run time, and NOT `V34HS_OURS`.** This section used
to say "define `V34HS_OURS` and side A becomes `v34handshak`". That cannot be
done: `V34HS_OURS` replaces side A in *every* binary that links this fixture,
so it only works once the whole 61,541 bytes exist, and while the function is
being taken an arm at a time there is no such point — four batches are in
flight, each with a few arms under its own name. `v34hs_side_a` takes a
function pointer, defaults to the blob, and `NULL` puts the blob back.

So a per-case test drives each case **twice**, once with its own entry and
once with `NULL`. That matters: a green ours-versus-blob run says nothing
unless the same seed is green blob-versus-blob, because then the failure could
be the fixture's. `t_v34hsstep.c` never calls it and goes on proving the
fixture is deterministic, address independent and fully seeded — which is what
a per-case agent has to be able to assume before its own failures mean
anything.

### And the door that is not for a case, or for a step, but for a CALL

`test/unit/t_v34call.c` uses this fixture's two sides as the two ENDPOINTS of
one call -- side A originating, side B answering -- and compares RUN AGAINST
RUN rather than side against side. It never calls `v34hs_compare`: two
endpoints hold different objects by design, so that comparison would fail by
construction.

```c
    v34hs_refinit(!ours_on_A);   /* side A: the blob's bring-up or ours */
    v34hs_oursinit(ours_on_B);   /* ...and side B's, the mirror of it   */
    v34hs_setup(0);
    /* then per endpoint: the role at +0x359c, the session block's two
       pointers, and `v34handshakinit(obj, 0)` again with THAT endpoint's
       implementation, because mode 0 calls `v34modeminit` and that branches
       on the role */
```

Per sample it writes +0x260, calls `modem_serrint` and reads +0x25e; per block
it calls `datapumpv34`. The whole call runs four times over the same two
arenas -- ours on both ends, the blob on both, and each mixed pair -- and
every block of every run is compared against the same block of the blob-blob
run. Findings F780-788; 781 is the defect it found at block 0, and 784 is the
one thing about the fixture a whole call needs and a single step does not: the
session block at +0x3548 holds pointers the object dereferences, and the
pseudorandom fill makes them faults.

Three things were added to the fixture for it, all mirrors of what was
already there: `v34hs_oursinit`, `v34hs_in_hole` and
`v34hs_arena_hash`/`v34hs_padding_hash`. `in_hole` is a bitmap now rather
than a linear scan over thirty-seven entries called per byte, which every
other user of this file gets for free.

### And one door that is not for a case at all

`v34hs_entry(a, b, log_a)` replaces the entry point on **both** sides -- ours
on A, the blob's on B -- for a function that is not `v34handshak` but shares
its object. `datapumpv34` is the only user: it is the function that *calls*
`v34handshak`, in the same translation unit, and what it needs from here is
the arena rather than the dispatch. Reusing this fixture instead of building
a second one is findings F319-322; `test/unit/t_v34datapump.c` is the example
and finding F453 is why `log_a` is a parameter.

It does not compose with the two per-case forms. A test uses it alone, and
runs each case a second time with the blob on both sides as its control.

`t_v34hst3mid.c` is the worked example: every path not written records a code
and returns rather than doing something plausible, and the test fails if a
trial reached one. Findings F370-373. Its arms were in a file of their own
until the unify (546-551); the codes are now `v34handshak`'s own and the
return-instead-of-abort is what `v34handshak_unwritten_reset` asks for.

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
  doing the same thing. Finding F290.
- **The two sides must be congruent in memory, not merely equal in it.**
  Identical bytes at two addresses with two different sets of neighbours is
  not enough, and the route that finds out is table 1. It is the blocks the
  object points at that have to be congruent, not the object. Findings F319
  and F322.

## Table 3, the microstate machine -- this is #57

Sixteen targets over states 41..80, 27.6 KB, and it is what the per-case split
was invented for. cfgsplit's exclusive byte counts, and what the harness sees
when each is entered cold with rxstate 43 and txstate 18 (SSEG):

**THE `where` COLUMN IS GONE, AND IS NOT TO BE WRITTEN AGAIN.** Two
reconstructions of `v34handshak` used to exist and an arm added to one was not
added to the other, so the table carried a column saying which file each arm
lived in. Three separate batches wrote that column from their own tree and
each got it wrong in its own way -- 46 was marked `open` because it was not in
the file that batch happened to be editing. Task #25 unified the two
(findings F546-551) and `src/pump/v34/v34hshak_t3mid.c` no longer exists, so
there is one file, one `v34handshak`, and nothing left for the column to say.
Check a row with `grep -n 'case V34HS_' src/pump/v34/V34hshak.c`, or with
`tools/anchorcheck.py`'s `arm_map`, which prints all forty microstates and the
function each dispatches to.

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
| 79 `MOH_TONE` | 0x657ca | 848 | group G |
| 80 `MOH_TONE_DROP` | 0x656e0 | 669 | group G |
| 48 `TX_PHASE3_ANS` | 0x65d30 | 422 | group D |
| 62 `RX_PHASE3_CALL` | 0x65c7a | 310 | **its own** -- 24 B, 2 traces |
| 42 43 45 52 53 54 57 60 61 64..78 | 0x6590b | 19 | group B |

**ALL SIXTEEN TARGETS ARE LANDED**, and 44's is landed in part: `arm_map`
reports forty of forty microstates bound to an arm, and the `default:` label
in the dispatch is unreachable and says so. What is still open in table 3 is
inside 44, not beside it.

Seven distinct behaviours from seventeen representatives -- a property of the
fixture's seed as well as of the object, so a different fill could separate
one more or one fewer. The groups:

```
  A  41, 44                    two real cases that agree cold; 41 has landed
                               (findings F390-396) and its three companion
                               fields are +0xaae2, +0xabe8 and +0x358a
  B  42, 46, 63, and any state outside 41..80 (the default at 0x65329)
  C  47                        }
  D  48, 49, 50, 51, 55, 58    the six that bump the counter at +0xaa78
  E  59                        }
  F  62                        }
  G  79, 80
```

**Group D is six DIFFERENT arms, not one.** 48, 49, 50, 51, 55 and 58 have
six distinct entries in `.rodata+0x3000` -- 0x65d30, 0x66a0d, 0x664b8,
0x65c47, 0x65b72, 0x66003 -- and none of them is 0x6590b. All six are now
reconstructed, and what separates them is `filtdelay` at +0xaa7c (finding F372)
for 49 and 50, and for 55 something that is not in the object at all: whether
`fskdemodulate` moved `fsk.nbits`, read into `%ebx` at 0x64a9e before the call
and compared at 0x65b79 (finding F430). Their agreeing cold
is the fill's doing. Finding F351.

**Group B and group D are the shape of the machine, not a harness defect.**
The arm twenty-four states share is three instructions -- read txstate, jump to
the once-per-block dispatch at 0x62af1 -- and so is the default. Group D's six
increment one counter, test it against one or two thresholds, and jump to the
same place. Finding F288.

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

`src/pump/v34/V34hshak.c` and `test/unit/t_v34hstbl2.c`, 11,009 checks. The
first tier-1 differential test of any part of `v34handshak`. Findings F360-366,
and 591 for the collapse of the last of its three reconstructions onto
`t3m_txblock`/`t3m_tail` -- the dispatch now lives in `v34handshak`'s own file
and `v34handshak_txblock` is a named entry to it for this test alone.

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
move**, so finding F290's count is about the fill and not about the object.
Fifty-FOUR of the seventy entries are the default; finding F286 says
fifty-three. Finding F364.

Reach it with `v34hs_route(V34HS_ROUTE_TXBLOCK, 0)`, which sets the cursor at
the limit and the receiver count to 5 -- or with `V34HS_ROUTE_RXCHAIN` and an
rxstate that is neither 43, 4, 35, 53 nor 72. **There are three doors, not
two**: rxstate above 43 leaves the chain at 0x62a12 for a second chain at
0x62b71 which reaches the same dispatch. All three converge exactly, measured
over seventeen states (finding F361).

Two things a per-case agent on table 1 or table 3 should take from it:

- **The tail at 0x62a40 can erase what an arm decided**, on four conditions
  no arm reads, and at `V34HS_SEED=10` the fill makes it do so on every case.
  Pin the tail's five inputs -- +0x2218, +0x238, +0x23c, the receiver's
  +0x134 and +0x230 -- and the arms' answers stop depending on the seed.
- **The transcript axis is worth nothing here**: the whole closure holds no
  `call` and no debug site, so both sides print zero lines and the transcript
  comparison passes by construction (finding F362).

## Table 1, the per-sample transmit loop -- DONE, arms AND loop

Twenty targets over txstates 5..86, in the loop at 0x62950.

**THE ARMS AND THE LOOP ARE TWO THINGS AND THEY LANDED SEPARATELY.** All
nineteen arms are in `src/pump/v34/v34hstx1.cpp` and compare through
`test/unit/t_v34hstx1.c`, which drives each one directly and lets the blob's
own `v34handshak` supply the loop around it. **The loop that dispatches to
them was written later**, in `v34handshak` itself, and is tested by
`test/unit/t_v34hstb1.c` -- our whole function against the blob's, so no arm
can be compared against the blob's copy of itself. Reading "table 1 is
complete" off the arms retires a guard that is still doing its job; finding
F712.

**`T3M_UNWRITTEN_TBL1` IS RETIRED.** What was left of it -- 81's wrap at
0xc0 -> 0x66d85 and 86's segment end -> 0x66fe9 -- is written, inside
`v34tx1_moh_silence` and `v34tx1_txmd`, because neither block is a transfer
out of its arm: 0x66d85 ends at the loop test and 0x66fe9 at the fall-through
of the block that jumped to it. `V34TX1_MOH_WRAP` and `V34TX1_TXMD_DONE` are
gone, the loop no longer tests what an arm returned, and `enum v34tx1_exit`
has one value. Findings F748 and F750. **It compares,
and it is in the default sweep.** It used not to; findings F319-322 are what
that took and D60 is the retraction. Nothing about the route is special any
more except that it is the one the harness's geometry could break, so if a
case here starts disagreeing, read D61 before reading your own code.

Entered cold with microstate `PHASE1`, rxstate `SILENCE` and a budget of one
sample, **eighteen of the nineteen reachable targets have their own
behaviour** -- better than table 3's seven-from-seventeen or table 2's
four-from-seven, because these arms run a modulator rather than setting a flag
and leaving. The only pair that agrees cold is 24 `TX_DPSK` and 60 `TONE_AB`.
Finding F323 has the per-target table of bytes written and progress code; read
it before choosing what to take first, because the six that write nothing
below +0x234 -- 65, 71, 78, 81, 85, 86 -- are the small ones.

### All nineteen have landed, and this is how a case is landed

txstates 65, 71, 78, 81, 85 and 86 -- the six that write nothing below +0x234
-- then 60, 18, 70 and 51, the four smallest of what those left, then 19, 20
and the entry 5, 54 and 74 share, then 69 and the entry 64 and 68 share, then
67 and 24, the two largest at the time, and then 21 `TRNSEG4`, are in
`src/pump/v34/v34hstx1.cpp` and compare byte for byte through
`test/unit/t_v34hstx1.c`. Findings F340-345, 421-425 and 426-427.

**TABLE 1 IS COMPLETE.** All nineteen targets are written and differentially
tested. The two sentences that used to sit here and below each said the OTHER
arm was the last one left, because 21 and 66 were written in parallel and each
batch wrote its `where` line from its own tree -- the third time that has
happened in this file (microstate 46 was the first). Write it from the tree.

**Two of the eighteen needed a table
before they could be written at all**: `probe`, 64 signed shorts at
`.rodata+0x2c00`, which 51 reads and which this tree did not have (finding
F421); and `vectpp`, 96 shorts at `.rodata+0x2c80`, which 20 reads as forty-eight
FOUR-byte points and which was `static` in V34RX.c (finding F422). 21 needed
neither, because the eight `hsine*` tables it reaches are reached through
`setupreceiver` and are already global in `v34filters.c`. Read both
before estimating what is left -- an arm that reads a table this tree does
not have, or has under the wrong element width, is not the size its byte count
says. **And read the body for an INLINED LIBRARY FUNCTION before estimating it
at all**: 67's 2 KB was mostly `getbit` open-coded, 24's 2.2 KB was `getbit`
open-coded twice, and about a thousand bytes of 21 -- twenty of its twenty-three
block ranges -- is `setupreceiver` inlined, checked store for store against the
standalone function at 0x5f040. All three came out as calls, so the rule is
three for three: **read a long arm against the functions the tree already
has.** 66's four `bitreverse` calls and its `sysdep_memset` are the same
signal (findings F424, F425 and F426).
and the entry 5, 54 and 74 share, then 69 and the entry 64 and 68 share, and
then 67 and 24, the two largest, and then 66, the largest of all, are in
`src/pump/v34/v34hstx1.cpp` and compare byte for byte through
`test/unit/t_v34hstx1.c`. Findings F340-345, 421-425 and 428-429.

**Three of the nineteen needed a
table before they could be written at all, and the third needed its VALUES**:
`probe`, 64 signed shorts at `.rodata+0x2c00`, which 51 reads and which this
tree did not have (finding F421); `vectpp`, 96 shorts at `.rodata+0x2c80`,
which 20 reads as forty-eight FOUR-byte points and which was `static` in
V34RX.c (finding F422); and `cfg->rx_divtab` at +0xaaac, which 66's rate ladder
walks and which the fixture aims at its shared `dummy` block -- whose entries
are so nearly equal that `entry >> 5` is 600 at every index the ladder
reaches, and nine of 66's claims could not fail against it (finding F429). Read
all three before estimating an arm -- one that reads a table this tree does
not have, has under the wrong element width, or has under values too flat to
separate its own arithmetic, is not the size its byte count says.

**And a field an arm both writes and reads cannot be driven by a poke.** 66
stores +0x250 in its first half and the rate ladder in its second half reads
it, so two runs that poked it drove the ladder with a value the arm had
already thrown away -- both anti-vacuity guards holding, the comparison green,
and only `tools/mutate.py` saying so (finding F429). **And read the body for an INLINED LIBRARY FUNCTION before estimating it
at all**: 67's 2 KB was mostly `getbit` open-coded and 24's 2.2 KB was `getbit`
open-coded twice, so both came out as calls (findings F424 and F425).

**An arm whose paths do not all reach `txmit` needs a DIFFERENT anti-vacuity
guard and not a weaker one.** `run_case`'s "the arm advanced the queue to the
limit" is what makes the blob skip the per-sample loop on side A; 24 has two
paths that move the transmit state instead and one that writes a single byte,
and `t_v34hstx1.c` guards those on the state and on that byte. Finding F425.

**AND ONE PAIR OF ENTRIES IS NOT ONE ARM EITHER.** 24 `TX_DPSK` and 60
`TONE_AB` are finding F323's only collision, and it is agreement on one path
rather than identity: it holds on three conditions of the object's and one of
the fixture's, and finding F425 asserts each of the four rather than the
agreement.

**AND ONE TABLE ENTRY IS NOT ONE ARM, in three different ways.** 0x640b4 is
the target for txstates 5, 54 and 74, and the shared prologue re-reads
`txstate` at 0x640f4 and gives each of the three a different tail (finding
F422). 0x635cc, shared by 64 and 68, re-reads it too -- but at 0x636ff, seven
eighths of the way down and only on the pass where `vect_idx` wraps, so most
passes are one body under two indices and the wrap is two behaviours (finding
F423). 0x63d58, shared by 81, 82, 83 and 84, IS ALSO TWO BEHAVIOURS AND THIS FILE
SAID OTHERWISE. Above the wrap it is one body under four indices; at
`vect_idx == 0xc0` it reaches 0x66d85, which re-reads +0x3596 and returns to
the loop test for anything that is not 0x51 -- so 82, 83 and 84 decide nothing
there and only 81 moves the transmit machine. That is 0x635cc's shape exactly
(finding F423), and finding F750 is the correction. Measure it; do not assume
either way because finding F323 lists one representative.

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
finding F342 says why. **Run the case twice** -- once with the arm alone, to
see the count reach the limit and the arm write something, and once through
the fixture -- because an arm that did nothing leaves side A's step to run the
blob's copy of it and the comparison passes for free.

**The diagnostics must be off** on such a test: our code logs to capture
channel 0 and the blob's to channel 1, and a side running both reaches only
one of them. That is finding F341's gap and it also costs two mutations.

**Seed what the arm writes.** Eleven claims were untestable against a cold
object simply because the field already held the value the arm stores --
finding F345 lists them, and it is the first thing to check when a mutation
goes uncaught.

The txstate a table-1 case is driven with IS the case, so unlike #57 there is
no companion-field problem to solve first. What there is instead:

- **The loop does not always terminate.** Its default arm is the loop bottom
  itself, so a txstate with no case of its own spins forever (finding F287,
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
                     positive control finding F319's bisect rests on
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
finding F286's table, not on its own.
