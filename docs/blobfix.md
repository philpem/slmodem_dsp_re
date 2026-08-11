# Fixing defects in the blob we still depend on

`docs/deviations.md` is a register of what is wrong with `dsplibs.o`, and
until this document everything in it was either reproduced faithfully or
mitigated in the host. The reconstruction is not finished, so anything running
today still links the blob, and a defect the register describes is a defect
that ships.

This is the first entry that changes that: **two defects repaired in the
object, by script, with the repair read back out of the linked output.**
`slmodemd/dsplibs.o` is not modified — its md5 is still
`1fd60268a1dcf5392f5520791f7a7059` and `.text` is still
`d25e747af8636e4ee2250d33737bbc17`, which is also the `.text` of all three
copies the fork ships (`docs/forkblob.md`). Both fixes are off unless asked
for, and `make phase` runs with neither.

    tools/blobfix.py       the mechanism
    test/blobfix/run.sh    the acceptance test        (`make blobfix-check`)
    make blobfix           writes build/blobfix/{blobfix.c,dsplibs_fixed.o}

---

## The two mechanisms, and what each one can express

There are exactly two ways to change a linked object you do not have the
source of.

**A — weaken and override.** `objcopy --weaken-symbol=NAME` makes the blob's
definition weak; a strong definition linked ahead of it wins, including for
references *inside the blob itself*. Reaches `GLOBAL` symbols only, and
replacing a function means reproducing all of its behaviour.

**B — binary patch.** Rewrite instructions. Reaches anything, including code
with no symbol, and can be surgical. It is also how the fork shipped a patch
that does nothing: at `rebuildJMSequence+0x136` it overwrites the `xor`
preceding a `setne`, and the `setne` overwrites it two instructions later.
Applied, inert, and nothing in their build would ever say so.

The choice between them is not a matter of taste, and the rule is sharper than
"code patch for code, data override for data":

> **Patch code when the defect is a DECISION. Override data when the defect is
> a MISSING VALUE — because a code patch can only redistribute values that
> already exist.**

D1 is the clean demonstration. Its table needs the value 32768 at index 192.
The last entry the object actually has, 191, is 32703. No clamp, no index
rewrite, no rearrangement of eleven bytes of instructions can produce 32768
from a table that does not contain it — and the clamp is not a hypothetical
repair, it is the one the sibling function `FPM_sqrt_dp` already has, pinned
at 191. Applying it here changes the answer on all 85 inputs that reach the
end. **Mechanism B cannot express the fix at all**, so for D1 the data
override is not the cheaper option; it is the only faithful one.

The reverse case is D70, assessed at the end of this document: a missing
bounds check is a decision, the values it needs all exist, and a data override
there would be actively wrong.

---

## D1 — `FPM_sqrt_table`, the rehearsal

The register's entry, in one line: `FPM_sqrt` indexes a 192-entry table with
an expression that reaches 192, on 85 of the 32768 Q15 inputs, and gets away
with it because `.rodata` puts `FPM_div_table` immediately afterwards and
`FPM_div_table[0]` is 32768 — exactly the value index 192 should hold, since
that index is sqrt(256/256) = 1.0.

That coincidence is why it was chosen to go first. **Fixing it changes nothing
observable.** Correct behaviour is known and identical, so a mechanism that
works must produce a bit-identical result on every input in the domain, and
any difference at all is proof the mechanism is broken. There is no way for a
plausible-looking wrong answer to hide.

It also means the obvious test is worthless. Comparing results proves nothing,
because the results are the same either way — which is precisely the shape of
the fork's dead patch, where everything looked right and nothing had happened.
The test has to observe the ACCESS.

## D4 — `FPM_div_table`, the one that matters

Same defect, same mechanism, opposite consequences. `FPM_div` indexes a
128-entry table with `((mantissa + 0x80) >> 8) - 0x80` over a normalised
mantissa in `[0x8000, 0xffff]`, so the index reaches 128. What follows
`FPM_div_table` in `.rodata` is `FPM_xor_table`, whose first word is **0**,
where the generator gives 16384. So 255 of the 65535 denominators — including
511, 1023 and 2047 — get a reciprocal of zero, and the AGC block that asked
for one multiplies itself to silence (finding 40).

The difference that matters for the rules: **fixing D4 genuinely changes
behaviour.** `src/dsp/fpm_div.c` reproduces the zero deliberately and must go
on doing so, because the differential tier's job is to prove we behave like the
blob. The fix therefore cannot live in `src/`; it lives here, off by default,
which is exactly the case the opt-in rule was written for.

### The 129th value, and where it comes from

`floor(2^30 / ((i + 0x80) * 0x100))` reproduces **all 128** of the blob's own
entries with zero mismatches, and `blobfix.py` re-checks that against the
object's bytes on every run before it will emit anything. Continued to i=128
the same expression gives `2^30 / 65536 = 16384` — 1.0 in the Q14 the table is
written in, which is the reciprocal of 1.0, which is the value index 128
means. It is generated, not read from an address the object did not intend to
be read.

D1's 193rd entry is derived the same way and reproduces all 192:
`floor(32768 * sqrt((i + 64) / 256))`, giving 32768 at i=192.

**The generator check is a gate, not a formality.** If a formula fails to
reproduce even one existing entry, the tool refuses to emit the table rather
than shipping a plausible extrapolation from a rule the object disagrees with.

---

## What the tool does, and what it proves about itself

    tools/blobfix.py generate --outdir DIR --fix D1 --fix D4

1. reads the table out of the blob's `.rodata`
2. regenerates every existing entry and **aborts** unless all match
3. writes `DIR/blobfix.c`, a strong definition one entry longer
4. runs `objcopy --weaken-symbol=` into `DIR/dsplibs_fixed.o`
5. **verifies that output**, which is the part that matters:
   - every section's bytes identical to the input, `.text` and `.rodata`
     hashed and printed
   - all 18,317 relocations identical once resolved to
     `(offset, type, symbol name)` — compared semantically because `objcopy`
     renumbers the symbol table, so raw `.rel` bytes differ by design and a
     byte comparison would drown a real difference in thousands of false ones
   - every symbol identical in name, value, size, type and section, with
     exactly the named ones changing `GLOBAL` → `WEAK`

Step 5 has one accepted exception, named rather than tolerated: a modern
`objcopy` drops the 57 `SECTION` symbols the 2003 binutils emitted for
`.rel*`, `.symtab`, `.strtab` and `.shstrtab`. They carry no value, no size
and no bytes. The fork's shipped blob lost 56 of them the same way. Anything
else disappearing is still a failure.

So "no instruction was touched" is measured here, not asserted — which for
mechanism A is the whole safety argument, because the fix works by changing
what a reference resolves to and must change nothing else.

### Reading the link back

    tools/blobfix.py checklink BINARY --fix D1 --fix D4

For each site that indexes the table — derived from the blob's own
relocations, not from a list written in the tool, so a site nobody remembered
cannot go unchecked — it disassembles the instruction in the **linked output**,
extracts the displacement the linker actually wrote, and checks three things:

- the displacement equals the address of the replacement table
- the highest index the expression can reach is inside that table's size
- the instruction's bytes are the blob's, **outside the relocated field**

That last check is narrowed to the one instruction on purpose. Comparing a
whole function fails honestly-but-uselessly: `FPM_div` calls
`dsplibs_debug_printf`, and the linker has resolved that too.

`checklink` is the check that exists because of `rebuildJMSequence+0x136`. A
fix here is not believed because it was applied. It is believed because the
output was read.

### And it is shown to fail

A gate nobody has watched reject something may be passing because it is
broken — `extcheck.py` printed "(none)" through four dead versions. So
`blobfix.py tamper` damages a copy of the fixed binary in each of the two ways
`checklink` claims to catch, and the acceptance test requires both to be
caught:

    checklink d1/tampered-displacement --fix D1: fail as expected
        FAIL: linker wrote 0x080fd282, table is at 0x080fd280

    checklink d1/tampered-opcode --fix D1: fail as expected
        FAIL: instruction differs from the blob's outside the relocated field
              0f b7 b4 01 80 d2 0f  vs  0f b7 b4 00 00 00 00

The second is worth reading closely: one flipped bit turned
`movzwl (%eax,%eax,1)` into `movzwl (%ecx,%eax,1)`, which halves the index and
picks up an unrelated register. That is precisely a patch that looks applied
and does something else, and the arm that catches it had never fired in a
passing run until it was made to.

---

## The test, and the question the brief asked

> *If you cannot build such a test, say so: that is a real finding about what
> we can and cannot verify.*

It can be built, three independent ways, and all three fire.
`make blobfix-check` builds six probes — a control linking the blob untouched,
and five configurations — and runs twenty checks. Each way has a blind spot
the others do not.

**STATIC.** `checklink` against the control **fails**, and against the fixed
object **passes**. This is a test that fails without the fix, at link time,
with no execution at all.

**SENTINEL.** The replacement table's new last entry is poisoned with 0x1234
and the whole domain is swept. The inputs whose result moves are exactly the
inputs that read that entry: **85 for D1, 255 for D4**, matching the register's
numbers, arrived at by measurement and without the test knowing the index
expression. For D1 the invariant checked is stronger than "returns the
sentinel", because `FPM_sqrt` shifts the entry down by half the exponent — it
is that one shift *k* explains both sides at once, `32768 >> k` becoming
`0x1234 >> k`, which says the only thing that changed about the read is the
datum it landed on. This check needs no debugger and must never be skipped.

**WATCHPOINT.** A hardware read watchpoint on the byte the blob overruns
*into* — `FPM_div_table[0]` for D1, `FPM_xor_table[0]` for D4. The control
takes 85 and 255 hits respectively; the fixed objects take **zero**. This is
the only check that observes the original defect directly rather than by
substitution, and it observes it on the shipped configuration, not on the
sentinel build. It degrades to `SKIP` where ptrace or a debug register is
unavailable, never to a false pass.

The watchpoint runs against the SINGLE-fix builds deliberately. In the
combined build the name `FPM_div_table` resolves to the replacement, so a
watchpoint set on it would watch the wrong address and report zero hits for
entirely the wrong reason.

### What none of this can see, and it is worth knowing

**No memory checker can find either defect.** The read leaves the bounds of a
*symbol* while staying inside a valid, mapped, correctly-permissioned
`.rodata`; valgrind sees an ordinary load, and ASan cannot instrument a
prebuilt object it has no source for. That is why these two survived from 2003
and why the register found them by reading the index expression against the
symbol size instead. Any future entry of this shape will have to be found the
same way — `nm -S` and the arithmetic, not a sanitiser.

### Both tables have TWO consumers, and the bound covers both

A table's `reach` is a claim about every function that indexes it, not about
the one the defect was noticed in. Checked rather than assumed, because the
sqrt pair is the worked example of the two differing: `FPM_sqrt_dp` truncates
`x >> 15` to sixteen bits and can drive its index *negative*, and only an
unsigned clamp saves it.

- **`FPM_sqrt_table`** — `FPM_sqrt` overruns; `FPM_sqrt_dp` clamps at 191
  (`cmp $0xbf`) and cannot reach 192 whatever its argument. So "all 32768 Q15
  results bit-identical" is a `FPM_sqrt` claim, and `FPM_sqrt_dp` is covered
  by its clamp rather than by the sweep — the longer table is provably a no-op
  for it.
- **`FPM_div_table`** — `FPM_div` and `FPM_div_32` (`0xa6c90`). The second
  forms the same index expression from `x >> 16` after normalising until bit
  31 is set, so its mantissa is in `[0x8000, 0xffff]` and its index runs
  0..128 exactly. **It does not clamp**, and it has no truncation hazard,
  because the mantissa comes from an already-normalised 32-bit value.

`FPM_div_32` has **no reconstruction**, so no differential test in this tree
reaches it and `test/blobfix` is the only thing that drives it. It is swept
over the mantissa rather than the input — `x = m << 16` for m in
`[0x8000, 0xffff]` enters already normalised and covers every mantissa the
function can form — which makes the result a statement about the whole index
domain rather than a large sample: exactly **128** of the 32768 mantissas,
`0xff80..0xffff`, form index 128, all move from 0 to 16384 with the shift
untouched, and the watchpoint on `FPM_xor_table[0]` goes 128 → 0.

### What D1's fix does NOT fix, and must be disclosed

`FPM_sqrt` above `0x7fff` is a different entry — D2 — and the index there can
reach 448. Unpatched, that reads whatever the blob's `.rodata` holds past
`FPM_div_table`. Patched, it reads past *our* 386-byte array instead. Both are
undefined and neither is defensible; what changed is only which undefined
bytes are read. **The override does not narrow D2 and slightly alters it**,
and the honest statement is that D1's repair is bounded by D1's domain: the
Q15 contract, `0x0000..0x7fff`, which is what the sweep covers and what every
caller passes.

`FPM_div` has no analogue, because its mantissa is normalised into
`[0x8000, 0xffff]` before the index is formed, so 0..128 is the whole range
and 129 entries close it. `FPM_sqrt_dp` is unaffected either way: it clamps at
191, and the reconstruction keeps that clamp at 191 rather than raising it to
the longer table's limit, because the clamp is observable behaviour.

### One honest gap in the argument

The sentinel proves the read comes from our table; it does so on a build whose
last entry is 0x1234, not on the build that ships. The inference from one to
the other is sound — same object, same code, same link, differing in one
datum — but it is an inference. The watchpoint closes it by running on the
shipped build itself, so the gap only matters where the watchpoint is skipped.
In that environment `make blobfix-check` still passes on nineteen checks and
reports the twentieth as skipped, and the honest reading of that run is
"applied and consistent" rather than "observed".

---

## Using it

`build/blobfix/dsplibs_fixed.o` and `build/blobfix/blobfix.c` are what
`slmodemd` links in place of `dsplibs.o`. **Order on the link line does not
matter** — both were tried and the strong definition wins either way, because
these are object files and not archive members, where it would.

This tree does not write into `../slmodemd` and has not done so here, so
wiring it up is a two-line change belonging to whoever owns that Makefile, and
it is deliberately left undone. Both fixes remain opt-in on this side too:
`make blobfix BLOBFIX="--fix D4"` builds only the one that changes behaviour.

---

## Next candidate: D70, `selectFilter`'s unclamped row — ASSESSED, NOT DONE

`selectFilter`'s ISDN and PBX arms take the row out of the registry and index
a coefficient bank with no bound applied (D70, finding 234). Measured here:

    _ZN12V90PreFilter18preFilterCoefType1E   0x0c00   2480 bytes  GLOBAL
    _ZN12V90PreFilter18preFilterCoefType2E   0x15c0   2480 bytes  GLOBAL
    _ZN12V90PreFilter18preFilterCoefType3E   0x1f80   4960 bytes  GLOBAL

Type1 ends at 0x15b0 and Type2 begins at 0x15c0, so an overrun off the end of
Type1 crosses sixteen bytes of alignment padding and then reads **another
bank's coefficients**. Plausible floats, no fault, wrong filter — the D4
pattern, not the D1 one. The arms that do clamp are in the same function
(`cmp $0x1d`, `cmp $0x31`, `cmp $0x13` at `selectFilter+0x6c`, `+0x11c`,
`+0x125`), so as with `FPM_sqrt_dp` against `FPM_sqrt`, the correct bound was
known to the author and one path omits it.

**Mechanism A is the wrong tool here, and not merely a clumsy one.** The
symbols are `GLOBAL`, so it would reach them; that is the trap. The defect is
a missing decision, not a missing value — there are no correct coefficients
for row 100, so a longer bank could only be filled with invention. Worse, the
banks are *adjacent by design of the link*, and finding 235 records that the
reconstruction's own tests had to enumerate `(bank, row)` candidates precisely
because one bank's high rows and the next bank's low rows resolve to the same
addresses in the blob. Replacing one bank symbol with a standalone array
destroys that adjacency, so a read that today lands in Type2 would land
somewhere else entirely. **A data override would change behaviour on exactly
the input in question, to a value we made up.** That is the worst outcome
available, and it is reachable by copying this document's D1 recipe without
re-asking which kind of defect it is.

So D70 is a mechanism-B candidate if it is done in the object at all — insert
a clamp, in a function of 800 bytes with two sites to patch — and the honest
first question is whether it should be done in the object at all, because
D70's own entry already names the better answer: **validate the registry value
in the host, before it reaches the object.** That is one comparison in
`slmodemd`, it needs no `objcopy`, no disassembly and no `checklink`, and it
fixes every unclamped path at once including the one through `getV90Capability`
that `selectFilter` cannot reach.

The rule that falls out of the comparison, and the reason D70 is left undone
rather than done badly:

| the defect is | the fix is | do it |
|---|---|---|
| a missing VALUE in a table | a longer table | **A**, override the data |
| a missing DECISION on an input the caller controls | a check | **host-side**, before the object sees it |
| a missing DECISION on an input the caller cannot reach | a clamp in the code | **B**, patched by script and `checklink`ed |
| behaviour the object gets wrong wholesale | a replacement function | **A** on the function — and reproduce *all* of it |

Nothing in this batch used mechanism B. It is not established here, and the
next entry that needs it should expect to build `checklink`'s equivalent for
instructions — a disassembly of the patched output compared against the rule
that generated the patch — before trusting a single byte of it.
