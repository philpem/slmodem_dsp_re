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

### 6. Ghidra, as scaffolding only — `tools/decompile.sh`

Available and now wired up. Measured against `chkForceBaudRate`, which this
tree had already reconstructed and differentially verified by hand, Ghidra
11.4.2 recovered the branch structure exactly, every constant and structure
offset (`obj+0xac3c`, `cfg[0x50] >> 5`), the debug gate as
`1 < _dsplibs_debug_level` — which is `DSPLIB_DEBUG_ON()` — and the calling
convention including `__regparm2` where the object uses it.

It destroyed aggregates. `unsigned char allow[6]` came back as `local_2c`,
`local_28` and `uStack_27`, written through as `local_2c._2_1_ = 1`. **An
array is the thing it cannot see, and arrays are most of this object.** Types
and signatures are gone too — `(int param_1, int param_2)` for what is
`(void *obj, struct v34_dftbin *bins)` — though that is not recoverable from
the object and so not a fault.

So it is good at the half this project finds tedious and bad at the half its
findings are actually about. That division is the whole value.

**On "the decompilation may not match the assembly":** it does not have to,
because it is never what ships and never what is trusted. The differential
test is the arbiter, exactly as before. A mismatch surfaces as a failing test
— the normal case the harness exists for. What *would* do damage is a Ghidra
guess written into a comment or a finding as though it were derived, because
the record is the deliverable. Hence the standing rules in the script header:
every line goes through `tools/dis.py` before it goes into `src/`, and no
name, comment or finding is ever written from decompiler output.

A worked example of the upside: `preempindex` has defeated two attempts here,
both needing its per-baud-rate multiplier table. One run returned it —
`0xd65 -> 0x6626`, `0xc80 -> 0x639f`, `3000 -> 0x656f` — as plain branch
constants.

**Untested: x87.** The object is built `-mfpmath=387` and Ghidra's x87
modelling is its known weak spot. The float-heavy modules are already done, so
this has not been measured. Treat floating-point output as suspect until
someone does.

## What this does not change

The differential test stays the only thing that decides correctness, and
nothing here weakens it: better failure messages, not fewer checks. Item 3
changes what a commit contains, not what it has to prove.
