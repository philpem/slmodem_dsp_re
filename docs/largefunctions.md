# Reconstructing a function too large to hold in one sitting

Functions over about 6 KB have repeatedly cost a whole session and not been
finished. This is what was measured, what the cause turned out to be, and
what to do about it in the order the leverage falls.

## What it is not

The obvious theory is that the disassembly does not fit. Measured, it does:

| function | bytes | disassembly | tokens |
|---|--:|--:|--:|
| `v34handshakinit` | 2,629 | 591 lines | ~5,700 |
| `receiver` | 4,326 | 1,057 lines | ~9,400 |
| `probeselect` | 6,173 | 1,523 lines | ~14,500 |
| `v34handshak` | 61,541 | 13,823 lines | ~131,000 |

One clean read of the largest function in the object is ~131 K tokens against
a 1 M window. `probeselect`, sitting exactly on the 6 KB wall, is 14.5 K.

Nor is it tool output. Across one whole reconstruction session (`78622533`,
1,826 turns) every disassembly tool together — `dis.py`, `objdump`,
`cfgsplit`, `relocscan`, `readelf`, `callgraph` — produced **153 KB of
output, 12% of all tool results**. `dis.py` alone was 7.7%.

## What it is, measured

**Context is cumulative output.** Every token generated stays in the window
for the rest of the session. Session `abff4cf1`, which reached the 1 M wall:

| turn | context | cumulative output | ratio |
|--:|--:|--:|--:|
| 25 | 69,945 | 12,453 | 5.62 |
| 100 | 143,961 | 86,842 | 1.66 |
| 300 | 359,920 | 281,435 | 1.28 |
| 508 | 623,348 | 579,161 | **1.08** |

The ratio converges to 1. Early turns carry the fixed cost of the system
prompt and the first file reads; after that, **context growth is output
growth**, essentially one for one.

So the wall is a *turn count* wall. Output averages 1,100–1,800 tokens per
turn across every session measured, which puts the limit at roughly 500–900
turns whatever the turns are about. The sessions that died had 400–600; the
ones that finished had 10–25.

Five sessions have reached it: peaks of 1.04 M, 946 K, 946 K, 916 K, 722 K.
Every one is a long session, not a session that read something large.

**This corrects the first version of this document**, which named the 819
discarded identifiers below as *the* cause. They are a contributor — an
uninformative failure costs several investigative turns — but the mechanism
is turn count, and a fix that saves turns is worth more than a fix that saves
bytes.

## The fixes, in the order the measurement puts them

Everything here is judged by **turns saved**, because turns are what the
window is spent on.

### 1. Give the work to a subagent — the only structural fix

A subagent's turns do not accumulate in the parent's window. The parent pays
for the prompt and the final report; the two hundred round trips in between
cost it nothing. That is the one change that alters the exponent rather than
the constant, and it is why a 6 KB function is a subagent-sized job and not a
session-sized one.

It is only safe where the unit is independently *verifiable* — the subagent
must be able to run `make phase` and see its own work pass. That is true for
every function in #59 and #60 today, and true for the state machines only
after item 2.

### 2. Per-dispatch-case testing, for the state machines

**Feasible**: the state words are plain fields at known offsets in an object
the test allocates (`obj+0x3592` microstate, `+0x3594` rxstate, `+0x3596`
txstate, finding 213), so a test can write one and call the function once.

That turns #57's 27.6 KB / 16 real cases into 16 independently verifiable
units of ~1.7 KB — each committable on its own instead of the whole machine
having to work before anything passes, and each one a subagent under item 1.

The unknown is per case: a case entered cold needs whatever companion fields
it reads set up first, and finding that out is the work. Bounded per case,
which is the point.

### 3. Fewer, larger tool calls

One session made **760 Bash calls averaging 1,458 characters of output**.
Many were single greps whose answers were needed together. Ten greps is ten
turns and ten lots of reasoning; one script that prints all ten answers is
one. This is worth more than it sounds: at ~1,400 output tokens per turn,
saving 100 turns saves 140 K of window.

### 4. Make failures say where they are

These do not change the mechanism, they reduce the number of investigative
turns each failure costs — which is worth having, and is what the rest of
this document was originally about.

**Print the input when the format did not consume it.** 819 of 1,667
`diff_eq_int` call sites pass an identifier to a format with no conversion in
it, so it is silently dropped. Five lines in `harness.c`, no call site
touched:

```c
if (strchr(fmt, '%') == NULL)
        fprintf(stderr, " [input %ld]", input);
```

```
t_v34rx.c:188: decoder state [input 680]  got 68, reference 136
```

**`diff_eq_obj`** replaces the byte-compare loop several tests open-code. It
coalesces differing bytes into runs, so one wrong 32-bit accumulator is one
report and not four flooding the ten-line cap; it counts one check per object
rather than 1,948; and it reports the first difference first, because
everything after it is consequence.

```
after process [input 137]: struct v34_receiver+680..+683
        got 44 33 22 11, reference 88 77 66 55
    which field: tools/whichfield.py struct v34_receiver 680
```

`tools/whichfield.py` resolves the offset against the DWARF our objects
already carry. Where it lands in a `pad_*` region, that is the answer: the
divergence is somewhere not modelled as fields yet.

### 5. Checkpoint so that running out is cheap

The limit is per session, so the cost of hitting it is the cost of *resuming*.
A running decode file — field offsets settled, cases done, what was tried and
failed — written as the work proceeds turns a hard wall into a boundary. The
task descriptions in #56–#61 were written this way deliberately.

### 6. Needs a decision: a decompiler

None is installed. Ghidra headless or angr would give a first-cut C to correct
rather than transcribe — a large lever on output volume, which is exactly what
the measurement says the window is spent on.

It cuts against `fastpass.md`'s standing rule, *read from the disassembly, not
from a summary of it*, and decompiler output is exactly a summary — one that
is confidently wrong about types, signedness and fixed-point scaling in the
ways this object is full of. If used at all it should be scaffolding whose
every line is then checked against the disassembly, with the differential test
still the only thing that decides. That is a method change and belongs to
whoever owns the method.

## What this does not change

The differential test stays the only thing that decides correctness, and
nothing here weakens it: better failure messages, not fewer checks. Item 3
changes what a commit contains, not what it has to prove.
