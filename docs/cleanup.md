# The readability pass: what, why not yet, and where it fits

*Companion to `docs/plan.md`, which says what is left to RECONSTRUCT. This says
what is left to make the reconstruction READABLE — and, more importantly, which
parts of it must not be touched and why.*

*Measured at `52e5aced`, 65.0% translated. Re-measure before acting: every
count here moves as batches land.*

## The invariant that governs all of it

**`compare.py` must not move.** The reconstruction's second tier asks whether
the same compiler, given our source, emits what the original's compiler emitted.
A readability change that alters code generation has destroyed evidence to make
a file read nicely, which is the wrong trade in this tree.

That splits the work in two, and the split is not obvious:

- **FREE.** A named constant, an enum, a renamed field, a rewritten comment.
  These are compile-time substitutions; the object code cannot know. If
  `identical` or `same_size` moves after one, a TYPE changed and it is a defect,
  not a cleanup.
- **NOT FREE, and mostly forbidden.** Anything that changes an EXPRESSION —
  a shift rewritten as a divide, a reassociation, a hoisted subexpression, a
  loop reshaped. §2 is the worked case.

Measure before and after **from your own runs**, and diff the identical-symbol
*lists*, not the counts: a count can gain four and lose four and look untouched.

## 1. Magic numbers — 10,119 distinct values, 30,678 occurrences

Against 1,760 constants already named. The ten most frequent are not noise:

    452  8192      250  16384     230  0x10      177  128
    159  4096      156  0x20      145  0x18      129  0x14
    125  12288     112  2400

`8192`, `16384`, `4096` and `12288` are Q15 scale factors and V.32/V.90
constellation coordinates — `DECv32_IMAP16` is ±4096 and ±12288 exactly, and
`DECv32_MAG9600` is their L2 norms. Those have real names and deserve them.
`0x10`, `0x14`, `0x18`, `0x20` are almost certainly structure offsets in code
that should be using a field.

**The rule: name a constant when the NAME carries a derivation the number does
not.** `V32_SYMBOL_NOCARRIER` earned its name because 0x10 is "seventeen map
entries, minus one" (finding 3647) — the name records why. A `#define EIGHT 8`
records nothing. Where the derivation is genuinely interesting, it belongs in a
finding and the constant cites it.

**Do not sweep this file-wide.** 30,678 occurrences is not a task, it is a
category; most of those numbers are table data, Q15 literals in expressions the
object fixes exactly, and loop bounds that are already obvious. Name the ones a
reader would otherwise have to derive.

## 2. Shifts — 862 right, 203 left. LEAVE THEM ALONE.

**This one is settled already and the answer is no.** Finding 1044 measured it:
a signed `/ 2^k` must round toward zero, so GCC emits a fixup before the shift,
where `>> k` on the same value is a bare `sar`:

    k >= 2   sar $0x1f,%A ; and $(2^k - 1),%A ; add %B,%A ; sar $k,%A
    k == 1   shr $0x1f,%A ;                     add %B,%A ; sar $1,%A

Verified by building both shapes with this project's own flags. **In the whole
1.2 MB object there are exactly SIX signed divides by a power of two**, all
`/ 2`, at six named addresses (`V90Equalizer::V90Equalizer` ×2,
`V90TRN2Design`, `modulatevector` ×2, `_send_silence_state_init`).

So the object's choice between `>>` and `/` is **forced, detectable, and
already recorded**. Rewriting one of our shifts as a divide would move codegen
and destroy the evidence; rewriting a divide as a shift would silently change
rounding for negative values. Findings 1184 and 1312 are two more sites where
the distinction turned out to be the whole point.

**What IS allowed:** say what the shift means in a comment, or introduce a
named constant for the shift COUNT where it encodes a scale (`Q15_SHIFT`,
`>> LOG2_SYMBOLS_PER_FRAME`). The expression stays; the intent gets stated.

## 3. Comments — 4,154 address-citing lines across 260 files

**Provenance has a lifespan, and this is the section that has to change twice.**

While a unit is being reconstructed the address is load-bearing: it is how any
reader verifies a claim against the object, and several findings exist only
because someone could go back and re-read the instructions at a cited address.
**Once a unit is verified, the address stops being evidence and becomes
archaeology** — and the code has to be maintainable by someone who does not have
`dsplibs.o` and never will.

So there are two states, and a defined transition.

### While reconstructing: intent first, provenance second

Not

    /* Reimplementation of the function at 0x0a9300. */

but

    /*
     * Convert a 16-bit phase into a sine/cosine pair, Q14, by table lookup
     * with a quadrant sign fix-up.  The quadrant index is deliberately
     * UNMASKED, reproducing the object -- see D392.
     *
     * blob 0x0a9300; the sign tables are .data 0x081dc and 0x081e4.
     */

### After verification: the provenance moves out of the source

**The acceptance test is: could a competent engineer extend this function
without the blob?** Add a rate, change a filter length, fix a real bug — using
only the source, the headers and the registers. If the answer is no because the
comment only says where the code came from, the comment has not been written
yet.

When a unit passes that test:

- **One provenance line survives per function**, and it is a citation, not a
  derivation: `blob 0x0a9300` and the findings/deviations that govern it. That
  is enough to re-verify, and it is the level `git log` and `docs/findings.md`
  cannot replace.
- **Address-laced prose in the body goes.** Detailed derivations already live in
  `docs/findings.md` — that is what the register is FOR, and duplicating it into
  a comment means two copies that drift. Cite the finding number instead.
- **What replaces it is the thing a maintainer needs**: what the function is
  for, what its inputs mean and what units they are in, what invariants the
  caller must hold, and what the surprising parts are and why they are that way.
  A reproduced defect is a surprising part and must stay, but stated as
  behaviour with a deviation number, not as an anecdote about an address.

`tools/tuattrib.py` already maintains the symbol-to-original-file index, so the
per-symbol provenance is recoverable from the tree without being carried in
every comment.

### When the transition happens

**Per unit, at the same moment as the rest of the readability pass** — when the
unit closes. "Verified" means **behavioural equivalence**: every symbol written,
`make phase` green, and the codegen difference either zero or explained.

**Byte-identity is the ideal, not the gate**, and the distinction matters
because the two are measured differently and the looser one is the one usually
quoted. Three levels of sameness, on this tree today:

| | count | ignores |
|---|--:|---|
| behavioural equivalence | the differential gate | everything but observable output |
| mnemonic-identical (`compare.py`) | 410 | registers, displacements, all operands |
| **byte-identical** | **238** | nothing |

238 is a floor — the pairing that produced it matched 907 symbols where
`compare.py` pairs 1,094 — and of the 389 same-size symbols it could check, 61%
were byte-perfect. So where the SIZE is right the bytes usually are too, and the
410 − 238 gap is functions whose instruction sequence is exactly right and whose
register allocation is not.

That gap is where the ideal meets finding 614, which classes register allocation
as FREE and warns that chasing it "means permuting source until the output
matches, which is fitting the compiler, not recovering the source". Both stand,
because they are about different things:

- **byte-identity as a SCORE** — track it; it says how close to perfect we are
  where the mnemonic count flatters us;
- **byte-identity as a target to permute source towards** — forbidden. A
  function that matches only after locals were shuffled to please the allocator
  is fitted, not recovered.

The way to raise the score is better SOURCE RECOVERY — the right expression
shape, operand order and types — which is what the forced/free rule already
directs effort at. `V90CP`'s three small members came out byte-identical on the
first try because the declarations were right, not because anyone aimed at the
bytes.

**The whole-tree pivot is a separate, later decision** and it is the owner's:
at some point the reconstruction stops being a reconstruction and becomes the
source, and that is when the remaining address citations are worth a final
sweep. Recording it here so it is not forgotten, not proposing it now.

## 3a. Casts — 1,157 pointer, 4,804 width, and only 24 are the bad kind

**A cast is a claim, and in this tree it is usually a claim that a DECLARATION
is wrong.** Used sparingly it states something the type system cannot; used
freely it hides a type error behind a syntax that compiles.

Measured in `src/`:

| kind | count |
|---|--:|
| pointer casts `(T *)` | 1,157 |
| — of which type-punning `*(T *)&` | **24** |
| width / truncation `(short)`, `(int)` | 4,804 |
| — of which adjacent to a shift (the Q15 idiom) | 394 |
| `(float)` / `(double)` | 189 |
| bare `(unsigned)` | 430 |

**Three categories, and only one of them is cleanup.**

**FORCED — keep, and say why.** The object truncates, sign-extends or widens,
and the cast is how C spells that. A `(short)` reproducing a `cwtl`, a
`(unsigned short)` reproducing a `movzwl`, `(int)a * b >> 15` reproducing a
Q15 multiply — these are EVIDENCE, and findings 613, 614 and 2702 are the tree
arguing about exactly this class. Removing one changes behaviour. **The 4,804
width casts are mostly this, and are NOT a cleanup target** — sweeping them
would be actively harmful.

**PAPERING OVER A WRONG DECLARATION — remove, by fixing the declaration.** The
24 type-punning sites are the extreme case: `*(int *)&o->f25d0` writes four
bytes through a field modelled as narrower, which says the field is declared
wrong. The fix is the declaration — a wider type, an array, or a union, which is
defined C where a pointer cast is not. §3a's test: **if removing the cast
changes nothing the object does, the cast was hiding a type error.**

**CONVENIENCE — remove.** A cast written to silence a warning or to make a call
site compile is the worst kind, because it converts a real mismatch into
something that looks deliberate.

**The rule:** every cast should survive the question *"what forces this?"* with
an answer that is the object's behaviour or a genuine limit of C. "It made the
types line up" is not an answer — it is a declaration bug wearing a cast.

Note the shape of the numbers: 1,157 pointer casts is a lot, but only 24 are
punning. The rest are `void *` from allocators and dispatch-table casts, most of
which are legitimate. **Do not sweep by count.** The 24 are a task; the 1,157
are a category to be careful in.

## 4. Naming — 304 offset-named fields, 189 bare `fNNNN`, 148 unnamed flags, 90 `pad_*`

Governed by `docs/plan.md` §3, which stands unchanged: **name inside the batch
that owns the struct**, evidence order is a format string that prints the field,
then a callee or caller that types it, then usage inference — and **a wrong name
is worse than a pad**, because no test can fail on it.

Two live examples of the restraint that rule asks for: `V90CP` keeps eighteen
offset names because no string names them (finding 3540), and `V92CP::+0x910`
stayed unnamed for a whole batch until `infoToBits` settled it as `msgLen`.

## 5. Everything else worth doing

- **Parameter names.** Unmeasured. A function whose parameters are `a1, a2, a3`
  is as opaque as a field called `f25d0`, and the same evidence order applies —
  a caller that types it, a format string that prints it.
- **File headers.** Every `src/` file should open with what the module IS.
  `src/dsp/fpm_div.c` and `src/dsp/fpm_log10.c` are the models: they explain the
  algorithm, the defect they reproduce, and why.
- **`DSPLIB_REPRODUCE_BUGS` coverage.** Only THREE sites use the switch today
  (`fpm_div`, `fpm_log10`, `FSE_decision_16pt`) against dozens of 🐛 entries in
  `docs/deviations.md` that say "reproduced, not fixed". The register promises
  "anyone linking this library for real gets the fix". That promise is currently
  kept in three places. Auditing which of those 🐛 entries SHOULD be behind the
  switch is a real task and belongs to the owner, not to a cleanup agent.
- **Dead scaffolding.** `tools/` grew several one-shot scripts this week. Worth
  a sweep once the reconstruction slows.

## Where this fits in the work

**Not as a phase, and not at the end.** Per translation unit, when that unit
CLOSES — the same rule `plan.md` §3 already applies to naming, for the same
reasons and one more:

1. **The evidence is freshest in the batch.** A format string that names a field
   is in hand while the function using it is being written, and expensive to
   recover later. Finding 3303 is the negative proof: `v34_shell::pad_000`
   looked like 2,560 bytes of opportunity and was a double count of a region
   `v34_object` already models — only the batch that knew the struct could tell.
2. **A file still being written will churn.** Cleaning it twice is waste.
3. **It avoids finding 3511.** A cleanup pass over a file another agent is
   writing is precisely the shape that merged with no git conflict and did not
   compile. A closed unit has no owner, so it is safe.

So: **when a class or file reaches "complete", it gets a readability pass in the
same batch, before the branch lands.** 51 of 98 C++ classes are complete today,
and those are where a standalone pass is safe. Everything else waits for its
reconstruction batch.

**The one exception** is `src/dsp/` — `fpm_div`, `fpm_log10`, `fpm_sqrt`,
`fpm_phasor` and their kin are finished, heavily reasoned about, and already
carry the best comments in the tree. They are the template. Reading them is the
fastest way to see what "done" looks like.
