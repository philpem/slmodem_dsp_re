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
                    otherwise  -> falls into TABLE 2
```

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
makes the comparison a check rather than a tautology while both sides still
call `ref_v34handshak` for the step itself.

`v34hs_compare` compares the whole 44,096-byte object byte for byte with the
thirty-four pointer fields excluded, every interior pointer by offset from its
own base, the four blocks the object points out of, and both transcripts.

### When you land a case

Define `V34HS_OURS` and side A becomes `v34handshak` instead of
`ref_v34handshak`. Every test written against the fixture becomes an ordinary
tier-1 differential test with no other edit anywhere. Until then the harness
is proving *itself*, which is the point: a per-case agent needs to know the
fixture is deterministic, address independent and fully seeded before its own
failures mean anything.

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
| 79 `MOH_TONE` | 0x657ca | 848 | group G |
| 80 `MOH_TONE_DROP` | 0x656e0 | 669 | group G |
| 48 `TX_PHASE3_ANS` | 0x65d30 | 422 | group D |
| 62 `RX_PHASE3_CALL` | 0x65c7a | 310 | **its own** -- 24 B, 2 traces |
| 42 43 45 52 53 54 57 60 61 64..78 | 0x6590b | 19 | group B |

Seven distinct behaviours from seventeen representatives. The groups:

```
  A  41, 44                    two real cases that agree cold
  B  42, 46, 63, and any state outside 41..80 (the default at 0x65329)
  C  47                        }
  D  48, 49, 50, 51, 55, 58    the six that bump the counter at +0xaa78
  E  59                        }
  F  62                        }
  G  79, 80
```

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

## Table 2, the transmit supervisor -- part of #56

Seven targets over txstates 5..74, about 0.3 KB, and the harness separates
four behaviours from seven representatives:

```
   5            0x64480      }  agree cold
  24 51 54 60 74 0x644c9     }
  18 19        0x64518       }  agree cold
  20 21 64 68  0x64509       }
  66 67 69     0x644fa       }  agree cold
  70           0x644d8       }
   6 and 52 others           the default at 0x62a40, its own behaviour
```

Reach it with `v34hs_route(V34HS_ROUTE_TXBLOCK, 0)`, which sets the cursor at
the limit and the receiver count to 5.

## Table 1, the per-sample transmit loop -- BLOCKED, and #56 starts here

**The harness does not prove this route and the committed test does not run
it.** Stepping one sample leaves the two sides differing in the modulator at
+0x2078..+0x25d1 for three of nineteen txstates, and *which* three moves when
code that runs after the step is edited. The result is not a function of the
object. Finding 289 lists the five experiments that rule out our bring-up,
stack residue, the shaping buffer's address, past-the-end reads of the seed
tables, and a short `preemp0`; D60 records it.

`V34HS_TXSAMPLE=1` runs the sweep anyway, which is how to reproduce it.
Finding what the loop reads is the first job of #56.

And **the loop does not always terminate**: its default arm is the loop bottom
itself, so a txstate with no case of its own spins forever (finding 287, D59).
Fifty-seven of the table's eighty-two entries are that default. `v34hs_step`
arms a `SIGALRM` so this is a named case rather than a run that never returns;
`v34hs_route(V34HS_ROUTE_TXSAMPLE, n)` takes the sample budget explicitly for
the same reason.

## The environment knobs

```
  V34HS_DUMP=1       print every case's signature
  V34HS_DIAG=1       print every differing offset, and the seed tables
  V34HS_REFINIT=1    bring side A up with the blob's initialisers too, which
                     separates "the fixture" from "our v34handshakinit"
  V34HS_TXSAMPLE=1   run the per-sample sweep that does not pass
```

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
