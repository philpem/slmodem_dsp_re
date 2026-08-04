# Reconstructing a function too large to hold in one sitting

Functions over about 6 KB have repeatedly cost a whole session and not been
finished. This is what was measured, what the cause turned out to be, and
what to do about it in the order the leverage falls.

## What it is not

The obvious theory is that the disassembly does not fit. Measured, it does:

| function | bytes | disassembly | tokens |
|---|--:|--:|--:|
| `v34handshakinit` | 2,629 | 591 lines | ~5,700 |
| `modulatevector` | 3,388 | 833 lines | ~7,400 |
| `receiver` | 4,326 | 1,057 lines | ~9,400 |
| `probeselect` | 6,173 | 1,523 lines | ~14,500 |
| `v34handshak` | 61,541 | 13,823 lines | ~131,000 |

One clean read of the largest function in the object is ~131 K tokens. Even
that fits. `probeselect`, sitting exactly on the 6 KB wall, is 14.5 K — three
of them would not trouble a session.

So the first read is not the cost. **The re-reads are**, and they are driven
by failures that do not say where they are. A debug loop that re-reads two
regions per iteration across twenty iterations is 100 K+ tokens of re-reading
the same disassembly, and that scales with disassembly density — which is why
6 KB is the wall rather than 60 KB being one.

## What it is

**Half the differential checks in the suite discard the identifier they
compute.**

`diff_eq_int(fmt, got, want, input)` passes `input` to `fmt`. Of 1,667 call
sites, **819 give a format with no conversion in it** — so the argument is
formatted by nothing and silently dropped. The commonest shape is the
byte-compare loop, which several tests open-code:

```c
for (i = 0; i < (int)sizeof(da); i++)
        diff_eq_int("decoder state", ((unsigned char *)&da)[i],
                    ((unsigned char *)&db)[i], i);        /* t_v34rx.c:187 */
```

It computes the offset, hands it over, and prints:

```
t_v34rx.c:188: decoder state  got 68, reference 136
t_v34rx.c:188: decoder state  got 51, reference 119
t_v34rx.c:188: decoder state  got 34, reference 102
```

Six of those in a row and not one says which byte. Everything the failure
knew — which field, which sample, which iteration — is thrown away at the
point of printing, and the session earns it back by re-reading the
disassembly. That is the loop that eats the context.

## The fixes, most leverage first

### 1. Print the input when the format did not consume it

One `if` in `harness.c`, no call sites touched, all 819 fixed:

```c
if (strchr(fmt, '%') == NULL)
        fprintf(stderr, " [input %ld]", input);
```

```
t_v34rx.c:188: decoder state [input 680]  got 68, reference 136
```

Cost: five lines. This is the whole of the diagnosis above, undone.

### 2. `diff_eq_obj` — the byte loop as a helper that names the field

The open-coded loop has three faults beyond the dropped offset: it counts one
check per byte (1,948 for one object), it reports one failure per differing
byte so a four-byte field floods the ten-line report cap, and an offset is not
a field.

```c
diff_eq_obj("after process", struct v34_receiver, &da, &db, 137);
```

```
t_v34rx.c:26: after process [input 137]: struct v34_receiver+680..+683
              got 44 33 22 11, reference 88 77 66 55
    which field: tools/whichfield.py struct v34_receiver 680
```

Differences are coalesced into runs, the first is reported first because
everything after it is consequence, and one object is one check.

`tools/whichfield.py` resolves the offset against the DWARF our objects
already carry (`-g` is already in `CFLAGS`), through nested structs and
arrays:

```
$ tools/whichfield.py struct v34_receiver 680
struct v34_receiver + 680  ->  pad_2a8[0]                  (unsigned char, +680)
```

`pad_2a8` is itself the answer in that case: the divergence is in a region
the reconstruction has not modelled as fields yet.

### 3. Per-dispatch-case testing, for the state machines

This is the one that matters for `v34handshak`, and it is **feasible** — the
state words are plain fields at known offsets in an object the test allocates
(`obj+0x3592` microstate, `+0x3594` rxstate, `+0x3596` txstate, finding 213),
so a test can write one and call the function once.

That turns #57's 27.6 KB / 16 real cases into 16 independently verifiable
units of ~1.7 KB each — session-sized, and each one committable on its own
rather than the whole machine having to work before anything passes.

The unknown is per case: a case entered cold needs whatever companion fields
it reads to be set up, and finding that out is the work. But it is bounded
work per case, which is the point.

### 4. Fan-out, only after 3

One subagent per dispatch case, each holding one case's disassembly, is a
direct answer to context — but only once each case is independently
*verifiable*. Fanning out over units that cannot be tested alone just moves
the integration problem to the end where it is worse. Sequence it after 3.

### 5. Not recommended without a decision: a decompiler

No decompiler is installed. Ghidra headless or angr would give a first-cut C
to correct rather than transcribe, which is a large lever on raw throughput.

It cuts against `fastpass.md`'s standing rule — *read from the disassembly,
not from a summary of it* — and decompiler output is exactly a summary, one
that is confidently wrong about types, signedness and fixed-point scaling in
the ways this object is full of. If it is ever used it should be as
scaffolding that every line is then checked against the disassembly, and the
differential test remains the only thing that decides. That is a method
change and belongs to whoever owns the method, not to a session that finds
itself short of context.

## What this does not change

The differential test stays the only thing that decides correctness, and
nothing here weakens it: better failure messages, not fewer checks. Item 3
changes what a commit contains, not what it has to prove.
