# Deviations from the original `dsplibs.o`

Every place the reconstruction does not behave identically to the original,
and every defect found in the original along the way.

The project's contract is **functional equivalence**: same input, same output.
So each entry here has to justify itself. An entry is only acceptable if it is
one of:

- **Bit-exact, different structure** — observable behaviour is identical over
  the whole legal input domain; only the implementation differs. Safe.
- **Out-of-contract divergence** — behaviour differs only for inputs no caller
  can produce. Must state precisely where the boundary is.
- **Deliberate fix** — the original is wrong and we are not reproducing it.
  Must state the impact and why reproducing the bug would be worse.  Every
  one of these is behind `DSPLIB_REPRODUCE_BUGS`, so bit-exactness against
  the blob stays provable: the differential tier defines it and everything
  else, including anyone linking this library for real, gets the fix.
- **Added hardening** — a check the original does not have, on an input no
  caller produces.  Not behind the define, because there is nothing to
  reproduce: the original's behaviour on that input is a fault, and a
  differential test cannot compare against a fault.  Must say what the input
  is and why no caller can produce it.

Anything that does not fit one of those is a reconstruction error, not a
deviation, and belongs in the issue list rather than here.

**Status key:** ✅ verified bit-exact over the stated domain · ⚠ deliberate
behavioural difference · 🐛 defect in the original · ❌ retracted

Retracted entries are kept, not deleted. A claim about the original that
turned out to be wrong is worth as much as one that held, and more if the
route to it was a mistake worth not repeating.

## The rule that governs every fix proposed here

**The reconstruction reproduces the object faithfully, bugs included.** A
defect recorded here must NOT be quietly corrected in `src/` — the
differential test would fail, and rightly, because behaving differently from
the blob is the one thing the reconstruction may not do.

So a fix has to take one of three forms, and every entry below names which:

| form | where it lives | example |
|---|---|---|
| **host-side** | `slmodemd` / `d-modem` | clamp a parameter before it reaches the object |
| **opt-in extension** | reconstruction, behind a flag off by default | the missing `FPM_div` table entry |
| **blob-side override** | a NEW object derived from the blob, opt-in | `docs/blobfix.md`: D1 and D4 |
| **documentation only** | this file | a defect nobody can trigger in practice |

An opt-in extension must be off by default and must leave the differential
test passing when off. That is the price of being a replacement rather than a
fork.

**The fourth form is new (task #99) and does not relax the rule.** It exists
because the reconstruction is unfinished, so a defect recorded here is a
defect that ships in whatever links the blob today, and the first three forms
could not reach a table that is one entry too short. It works by weakening the
blob's symbol and linking a longer table in front of it —
`slmodemd/dsplibs.o` is never modified, its md5 is checked, and every fix is
selected by name and off unless asked for. `make phase` runs with none of them
and must go on doing so. Read `docs/blobfix.md` before adding one: the
mechanism suits a **missing value** and is actively unsafe against a **missing
decision**, and D70 is the worked example of the difference.

## Two orthogonal axes, and they answer different questions

Every entry carries a **status** — is the claim about the object true, and how
do we know — and, from D70 on, a **reachability grade**: can it happen on a
real call. They are independent, and confusing them is how a register turns
into a to-do list nobody should do.

* **Status** — `🐛` defect in the original · `⚠` deliberate difference · `✅`
  verified bit-exact · `❌` retracted · CONFIRMED (reproduced, with numbers)
  against SUSPECTED (mechanism identified, not demonstrated).
* **Reachability** — **FIRES TODAY** on an ordinary call or a shipped
  configuration · **NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES** ·
  **LATENT**, the mechanism is in the object and no path to it is known ·
  **CANNOT FIRE**, a gate or an arm no input reaches.

A CONFIRMED defect that CANNOT FIRE is documentation. A SUSPECTED one that
would FIRE TODAY is the most urgent thing in the file. Appendix A audits
status for D1–D64; Appendix B does reachability for D70 onwards.

**`docs/fixlist.md` no longer exists.** It was a second register created
without noticing this one, and everything in it — the 92 entries below, the
governing rule above and the reachability axis — was folded in here. There is
one register.

---

## D1 — `FPM_sqrt`: table declared one entry too short 🐛 ✅

**Module** `src/dsp/fpm_sqrt.c` · original `fpm_sqrt.c`, `.text 0x0a9d70`,
table at `.rodata 0x0c520`

**The defect.** `FPM_sqrt` normalises its Q15 argument into `[0x4000, 0x7fff]`
and indexes its table with `((mantissa + 64) >> 7) - 64`. Over the legal Q15
domain that expression reaches **192**, but `FPM_sqrt_table` holds 192 entries,
indices 0..191. The original reads one element past the end of its own table.

Not a corner case: **85 of the 32768 Q15 inputs land on index 192**, so it
fires in ordinary operation.

**Why it never caused a failure.** The read lands on the first `unsigned short`
of `FPM_div_table`, which immediately follows in `.rodata` and holds 32768 —
*exactly* the correct value for `sqrt_table[192]`, since that index corresponds
to `sqrt(256/256) = 1.0` and 1.0 in Q15 is 32768. The out-of-bounds read
returned the right answer by coincidence.

**What we do.** Add the missing 193rd entry, generated by the same formula as
every other entry. Bit-exact with the original across the entire Q15 domain,
but correct by construction rather than by adjacency.

**Risk if left alone.** The value is only correct because of where the linker
happened to place `FPM_div_table`. Regenerating the tables, reordering
`.rodata`, or changing the sample rate would move it, and the failure would be
a subtly wrong square root at the top of the range — the hardest kind to trace.

**Corroborated by its sibling.** `FPM_sqrt_dp` indexes the *same table* and
does bounds-check it:

```
cmp $0xbf,%ax
jbe use_it
mov $0xbf,%eax          /* clamp to entry 191, the last of the original 192 */
```

So the correct bound was known to whoever wrote these; the 16-bit version
simply omitted the check its 32-bit counterpart has. That makes D1 an
oversight rather than a deliberate trick, and removes any doubt about fixing
it.

Note the reconstruction keeps `FPM_sqrt_dp` clamping at **191**, not at the
193-entry table's new limit — the clamp is observable behaviour, not an
implementation detail, and raising it would diverge from the blob.

**Now also fixed IN THE BLOB, opt-in** (task #99, `docs/blobfix.md`, finding
F1200). `make blobfix BLOBFIX="--fix D1"` weakens `FPM_sqrt_table` and links a
193-entry replacement over it; `slmodemd/dsplibs.o` is untouched. All 32768
Q15 results are bit-identical to the unpatched object, and a hardware read
watchpoint on `FPM_div_table[0]` takes 85 hits before the fix and **zero**
after — which is what makes it a fix rather than a rearrangement, since the
answer alone cannot tell the two apart. D2 is NOT narrowed by this and is
slightly altered by it; see `docs/blobfix.md`.

---

## D2 — `FPM_sqrt`: clamped above Q15 range ⚠

**Module** `src/dsp/fpm_sqrt.c`

For inputs above `0x7fff` the normalisation leaves the mantissa in
`[0x8000, 0xffff]` and the index runs as high as 448, far past any table. The
original reads whatever `.rodata` contains there.

The reconstruction clamps the index instead. This is an **out-of-contract
divergence**: `FPM_sqrt` takes a Q15 fraction, so `0x0000..0x7fff` is the whole
legal domain, and the differential test covers exactly that. Reproducing the
original here would mean reproducing dsplibs' `.rodata` layout, which is
neither achievable nor desirable.

**If a caller is ever found passing more than `0x7fff`**, that caller is the
bug, and this entry should be revisited.

**Contrast with `FPM_sqrt_dp`, which is reproduced rather than guarded.** That
function truncates `x >> 15` to 16 bits, so inputs at or above `0x80000000`
lose their top bit and drive the table index negative — which its unsigned
clamp then pins to 191. That case is *not* guarded here, because unlike
`FPM_sqrt`'s Q15 contract there is no domain ruling it out: `FPM_rms` is the
only caller, its accumulator passes `0x80000000` at 73 full-scale samples, and
every call site passes a runtime sample count. Reachability cannot be
established statically, so matching the blob is the only defensible choice.

The distinction is the rule this project uses: guard only where the contract
makes the input impossible; otherwise reproduce.

---

## D3 — `FixedRC` modes 0 and 1 not implemented ⚠

**Module** `src/core/fixedrc.c` · original `FixedRC.c`, `.text 0x0b0fd0`

`RcFixed_Create(0)` and `RcFixed_Create(1)` build a converter in the original,
using a **different state layout** from every other mode: a 40-byte block with
three sub-allocations (6, 174 and 30 bytes) and three separate coefficient
tables at `.rodata 0x10f0e`, `0x10e60` and `0x10e30`, rather than the 420-byte
polyphase state the other 18 modes share.

This reconstruction returns `NULL` for both.

**Why that is safe.** Nothing can request them:

- `RcFixed_Check_Combination()` begins its table scan at index 2, so it can
  never return 0 or 1.
- Every call site inside `dsplibs.o` passes a **literal** mode: `dp_wrapper_create`
  passes 2 and 7, `call_create` passes 2, 3, 4 and 5. None passes 0 or 1.
- `RcFixed_Check_Combination` is exported but **never called** anywhere in the
  object, so the only paths to `RcFixed_Create` are those literals.

Modes 0 and 1 are the plain x4 and /4 ratios, which modes 8 and 9 also provide
through the ordinary polyphase path. They look like an earlier implementation
that was superseded and left in place.

**If a caller is ever found passing 0 or 1**, this becomes a real gap and the
40-byte path must be reconstructed. `t_rcresample` asserts the current shape
explicitly — that we decline and the original does not — so a change on either
side fails the build rather than passing silently.

---

## D4 — `FPM_div` reads past its table and returns a zero reciprocal 🐛 **CONFIRMED HARMFUL**

**Module** `src/dsp/fpm_div.c` · original `fpm_div.c`, `.text 0x0a6bf0`,
table at `.rodata 0x0c6a0`

**The defect.** `FPM_div` normalises the denominator into `[0x8000, 0xffff]`
and indexes its table with `((mantissa + 0x80) >> 8) - 0x80`. That expression
runs 0..**128**, but `FPM_div_table` holds **128** entries, 0..127.

It is reached whenever the normalised denominator is `0xff80` or above —
**255 of the 65535 possible denominators, 0.39%** — including values as
ordinary as 511, 1023, 2047 and 65535.

**Why this one is serious.** It is the same defect as D1, but without the luck.
There, the word past the end happened to be exactly the right value. Here the
read lands on `FPM_xor_table[0]`, which is **0**, where the correct entry would
be **16384**.

So for those 255 denominators the original returns a reciprocal of **zero**,
and any division built on it collapses to zero. `FPM_div` exists to let callers
multiply instead of divide, so a zero reciprocal silently turns a division into
a zero result rather than raising anything.

**What we do: reproduce it.** The table here has a 129th entry of 0 — not a
coefficient, but the neighbouring table's first word. That follows the
project's rule (guard only where the contract makes the input impossible), and
nothing rules these denominators out.

`t_fpm_div` sweeps all 65536 denominators and **asserts that exactly 255 yield
a zero reciprocal**, so this cannot quietly regress into a tidied version that
returns 16384.

**Decision: reproduce, do not fix.** Raised with the user and delegated back.
The reasoning:

- The stated goal is a drop-in replacement that behaves as the blob does. A
  reconstruction that is *better* than the original is a different artefact,
  and mixing the two goals makes it impossible to tell a reconstruction error
  from an intentional improvement.
- Every test in this project is differential against the blob. Fixing D4 means
  those 255 denominators can no longer be tested at all — we would be asserting
  a value with nothing to check it against, in the one module where we have
  proof the original is wrong.
- The fix is one table entry and is fully documented here. It costs nothing to
  apply later, once there is an independent reference (tier-3 interop) that can
  actually validate the corrected behaviour.

So the correct sequence is: stay faithful now, get SpanDSP interop working,
*then* revisit — at which point the improvement can be measured rather than
assumed. Filed against task 5.

**If modem behaviour is ever traced to a division collapsing to zero**, this is
the first place to look: `FPM_div` sits under both `FPM_AGC` and the V.22
demodulator.

**HOW WIDE IS IT? Every datapump below V.34, not just Bell 103.** The 46-of-700
figure in finding F40 was measured on a Bell 103 capture, and that made it easy
to read this as a Bell 103 defect. It is not. Counted from the blob's own
relocations rather than from the reconstruction, so unreconstructed callers are
included:

```
  FPM_div  <- FPM_AGC_agc, FPM_atan, V32FP_status          3 callers, 5 sites

  FPM_AGC_agc   <- DemodDataB103 (x2), RxDetMarkB103,
                   DemodDataV21, DemodDataV22, Detect_v22,
                   v23FP_rx_progress (x2), BwChDem_Progress,
                   DemodDataV17, DemodDataV27, DemodDataV29, DemodDataV32,
                   DataCarrierDetectV17/V27/V29,
                   RxHdxNoSignal, RxHdxPhsReversal, RxHdxTone
                                                          17 callers, 19 sites
  FPM_atan      <- FPM_FSE_receive, FPM_SRE_recover,
                   V22_FSE_receive, V22_SRE_recover         4 callers
  V32FP_status  <- v32_data, v32_process                    2 callers
```

So the zero reciprocal is reachable from **Bell 103, V.21, V.22, V.23, V.17,
V.27, V.29 and V.32** — through the shared AGC in every case, and through two
further paths in V.22 (the fractionally spaced equaliser and the symbol-rate
recovery, via `FPM_atan`) and V.32 (the status path). V.34, V.90 and V.92 do
not use `FPM_div`; they have their own arithmetic.

**What that does and does not license.** The shared *code path* is measured and
certain. The 46-of-700 *rate* is not transferable: it depends on the level
statistics each caller presents, and only the Bell 103 AGC path has been
driven. `FPM_atan` and `V32FP_status` pass denominators from different
quantities entirely and neither has been swept. So: eight datapumps are
exposed, one has a measured failure rate, and the other seven are unquantified
rather than safe. Finding F40 and the caller counts above are the evidence;
`tools/relocscan.py`-style reasoning applied to `readelf -rW` is how they were
obtained.

**Table derivation**, exact for all 128 entries:
`table[i] = trunc(2^30 / ((i + 0x80) * 0x100))` — truncated, like the sine
tables; rounding differs on 58 of them.

**Now fixable IN THE BLOB, opt-in** (task #99, `docs/blobfix.md`, finding
F1201). "Reproduce, do not fix" above governs `src/` and is unchanged — the
differential tier still proves we return zero exactly where the blob does, and
`t_fpm_div`'s assertion that exactly 255 denominators yield a zero reciprocal
still stands. What has changed is that the *shipping* object no longer has to.
`make blobfix BLOBFIX="--fix D4"` weakens `FPM_div_table` and links a 129-entry
replacement whose last entry is **16384**, generated by the table's own
formula continued to i=128 after that formula is re-checked against all 128 of
the blob's own entries. Measured: exactly 255 denominators change, every one
of them from a reciprocal of 0 to 16384, with the returned shift untouched;
and a hardware read watchpoint on `FPM_xor_table[0]` takes 255 hits before and
**zero** after.

This is the first entry in this register whose defect is repaired rather than
recorded, and the sequence the decision above asked for is satisfied in
substance: the fix is off by default, so nothing that is tested against the
blob sees it, and it is available to anyone running a modem for real.

---

## D5 — `FPM_TONE_delete` frees what `create` may not have allocated 🐛

**Module** `src/dsp/fpm_tone.c` · original `.text 0x0aad00` (delete),
`0x0aaa00` (create)

The two do not agree on ownership.

```
create:  if (owned && len > 0)  allocate the four buffers
delete:  if (len > 0)           free the four buffers, then free the object
```

`create` only allocates when it allocated the object itself. `delete` has no
ownership test at all: it frees the buffers on `len > 0` alone, and frees the
object unconditionally.

So a caller that supplied its own state with a positive `len` would have four
never-allocated pointer slots passed to `free`, and its state freed even if it
lived on the stack or inside a larger structure.

**Unreachable in practice.** Every call site passes NULL to `create` — the
four in `B103FP_create` and the one in `FPM_FSM_init`, which passes the
existing pointer that is NULL on a zeroed state. So `owned` is always 1 and
the two halves agree.

**Reproduced, not fixed**, per the project rule: nothing establishes that a
caller-supplied state is impossible, only that none exists today. Adding an
ownership flag to `delete` would also change the object layout, which is
observable.

**If a caller-supplied state is ever introduced**, this must be revisited
first — it is a double-free and a free-of-non-heap in one.

---

## Bugs found in the reconstruction (not deviations)

Recorded because how they were caught is worth remembering.

### R1 — `RcFixed_Resample` ignored its output limit

The incoming value of `*out_count` is an output **cap**, read before the
function zeroes it. Conversion stops when either the input runs out or that
many samples have been produced.

The first implementation ignored it entirely and ran until the input was
exhausted. `t_rcresample` did not catch this because every case passed `0` —
and `0` behaves as "no limit", since the original compares for *inequality*
after producing a sample, so the count can never equal 0 again. Both readings
are real and both are now reproduced.

It surfaced only when `dp_wrapper` — the one caller that passes a genuine cap —
disagreed on 2 of 14 cases. The lesson: a test that only ever passes the
degenerate value of a parameter is not testing that parameter. `t_rcresample`
now drives real caps (1, 7, 192, exactly one fragment, and larger than
possible) and asserts the cap is respected.

A second, smaller error in the same fix: the limit test belongs at the *bottom*
of the loop, because the original always produces one output before applying
it. Putting it at the top made a limit of 0 mean "produce nothing" and broke
all 18 modes at once — loudly, which is the good kind of wrong.

---

## Open questions

Not deviations yet — things that need resolving before the affected modules can
be called done.

### Q1 — the resampler's per-mode filter designs *(ordering resolved)*

The mode-to-table map, the storage rule (`bytes = taps * up * 2`), Q14 scaling
and now the **coefficient ordering** are all established — see
`docs/coefficients.md`. The ordering was settled by reading
`RcFixed_Resample`'s address arithmetic: phase-major, reverse-tap, confirmed
independently by the frequency response.

Mode 3 (9600 -> 8000) is fully characterised: a 161-tap Type-I linear-phase
prototype, passband flat to 3400 Hz, stopband >= 49.6 dB from 4400 Hz — a
deliberate voiceband design rather than a generic anti-alias filter.

What remains is fitting each prototype to a named design so a generator can be
written. Until that is done the tables must not be copied in as opaque bytes:
that would defeat the purpose of recovering derivations, since a copied table
can only be resampled at 8 kHz, never re-designed.

### Q2 — x87 equivalence regime — **RESOLVED: bit-exact is achievable**

Settled by `GenericIIR<float, double>`, the first float module through the rig.
It agrees with the original **bit for bit** — compared as raw bit patterns, not
with a tolerance — across 31,200 samples of recursive filtering, where any
rounding difference compounds rather than cancels. Covered: biquad, 4th order,
pure FIR, both `den[0]` conventions, tight buffers that force frequent history
compaction, and block processing at three chunk sizes.

So float modules are held to **bit-exact**, the same standard as fixed point.
The per-class tolerance table in the plan can be simplified accordingly; no
tolerance-based test has been needed.

**The one thing that matters for getting there.** The original mirrors its
accumulator to memory after every multiply-accumulate (`fstl 0x2c(%ecx)`),
which looks like per-iteration rounding to double. It is not: `fstl` stores
*without popping*, so the running sum stays in the x87 register at 80-bit
extended precision. Rounding to double happens exactly once, at the end.

Reconstruct that as a plain `double` accumulator and you round every step;
reconstruct it with `-ffloat-store` and you additionally round every *product*,
which the original never does. Both are wrong, and both hide on short filters —
the error only appeared on an 8-tap FIR, at 35 samples in 3200. The correct
form accumulates in `long double` and rounds once on the way out, which is what
x87 does naturally.

**Guidance for later float modules:** read whether the original's spill is
`fstl` (mirror, stays 80-bit) or `fstpl` (store and pop, genuinely rounds).
That single distinction decides the accumulator type, and it is invisible in
the C.

## D6 — `AGC_DEF_ALPHA`'s slow pair is copy-pasted, breaking unity gain 🐛 💤

**Dormant: nothing selects the affected element.** Recorded because it is one
config field away from being live, and because it is the only thing that could
make `FPM_AGC_agc`'s shift go negative (finding F29).

`FPM_AGC_agc` smooths its level estimate with

```
level = (alpha[0] * level + beta[0] * measured) >> 15
```

which has unity DC gain exactly when `alpha + beta == 32768`. `AGC_DEF_ALPHA`
and `AGC_DEF_BETA` are 4-byte objects — two Q15 coefficients each, a fast
"acquisition" pair at `[0]` and a slow "tracking" pair at `[1]`. There are 18
such objects across the blob (`readelf -sW dsplibs.o | grep AGC_DEF`), one per
translation unit that declared them file-static.

Element `[0]` is 16384/16384 in **every** one of them — always exactly unity.
Element `[1]` is not:

| object | α[1] | β[1] | α+β | DC gain |
|---|--:|--:|--:|--:|
| `.rodata:0x9e2c` / `0x9e28` | 29491 | 3277 | 32768 | 1.000 ✓ |
| `.rodata:0xab10` / `0xab0c` | 29491 | 3277 | 32768 | 1.000 ✓ |
| `.rodata:0xb298` / `0xb294` | 29491 | 3277 | 32768 | 1.000 ✓ |
| `.rodata:0xa100` / `0xa0fc` (`_v21`) | 32604 | 164 | 32768 | 1.000 ✓ |
| `.data:0x77bc` / `0x77b8` | 32604 | 164 | 32768 | 1.000 ✓ |
| `.data:0x7810` / `0x780c` (Bell 103) | 32604 | 1638 | 34242 | **1.045** ✗ |
| `.data:0x778c` / `0x7788` (V.23) | 32604 | 1638 | 34242 | **1.045** ✗ |
| `.data:0x7794` / `0x7790` (V.23) | 32604 | 1638 | 34242 | **1.045** ✗ |
| `.data:0x7664` / `0x7660` (global, V.32's TU) | 30491 | 2277 | 32768 | 1.000 ✓ |

**That last row said 32604 / 34881 / 1.064 ✗ until finding F1621 read the
object for it.** It is 30491, three readings agreeing (`readelf -x .data` at
0x7660 gives `0040e508 00401b77`), so the pair sums to exactly 32768 and is
correct. The count is **three** broken pairs, not four. The row mattered more
than one row of a table normally would, because the next sentence derives the
intended value from it and the object already holds that value.

The three wrong ones all carry α = 32604, which is `32768 - 164` — the value
that belongs with the β of the `0x77b8` pair. Someone copied that TU's α while
changing β. The intended values are plainly `32768 - β`: 31130 for β = 1638,
and 30491 for β = 2277 — which is what the V.32 pair above already has.

**Why it is dormant.** All 6794 section-relative `R_386_32` relocations in the
object were resolved; every config field that points at one of these objects
points at **element 0**. Element 1 is present in all 18 and selected by none —
and four of the 18 are not pointed at by anything at all.

```sh
python3 tools/relocscan.py ../slmodemd/dsplibs.o --into AGC_DEF
```

**What would happen if it were selected.** A DC gain of 1.045 makes the level
estimate climb without bound on a steady input. `FPM_AGC_agc` truncates it to
`short` each block, so it would eventually wrap negative, `FPM_div` would see a
denominator with its top bit set, `norm` would be 0 and `shift` would be −1 —
which the apply loop turns into a **left shift by 31** via `movzbl` and x86's
five-bit shift mask. The AGC would go from "slightly too much gain" to
"garbage" in one block.

**Reproduced as-is.** `src/pump/b103/b103_agc_cfg.c` carries `{16384, 32604}`
and `{16384, 1638}` verbatim, and the shift mask is reproduced in
`src/dsp/fpm_agc.c`, so the two agree even along the path neither takes.

## D7 — RETRACTED: the channel filter's history *is* cleared ❌

**This entry claimed a defect in the original that does not exist.** It is kept
rather than deleted because the way it was reached is worth more than the
claim was.

### What it said

That `B103FP_create` allocates the 84-byte history for the receive channel
bandpass and never clears it, so the first `bpf_taps` output samples after a
create depend on whatever the allocator left behind.

### Why that looked true

The differential test failed, and failed in the one way that admits no other
explanation: two objects built by the **same** `B103FP_create` with the
**same** arguments filtered identical input differently. That really did
happen, and uninitialised memory really was the cause.

### What was actually wrong

The test built **loopback** objects and installed a bandpass into them by
hand, because at the time `B103_CFG`'s `call_type` was not understood
(finding F35). `B103FP_create` clears that buffer in the branch that installs
the filter — and loopback does not install one, so the memset never ran.

Reading the branch settles it. The caller path:

```
8ecfb:  dsp->bpf = B103_BPF_CALLER ; dsp->bpf_taps = 40
8ed1e:  mov  $0x50,%eax                  /* 80 bytes = 40 taps */
8ed39:  call sysdep_memset               /* (dsp->bpf_hist, 0, 80) */
```

and the answer path allocates a **larger** buffer for its longer filter and
clears all of it:

```
8eed3:  movl $0x68,(%esp)                /* 104 bytes, not 84 */
8eea5:  mov  $0x64,%eax                  /* 100 bytes = 50 taps */
```

So each configuration allocates enough and clears exactly what it uses.

### Measured, against a deliberately dirtied heap

```
type 0 (originate): taps=40  first 40 history words: 0 non-zero
type 1 (answer):    taps=50  first 50 history words: 0 non-zero
type 2 (loopback):  taps= 0  first 42 history words: 3 non-zero
```

Loopback is dirty and always was — but its `bpf` is NULL and its `bpf_taps`
is zero, so nothing ever reads it.

### The lesson, which is the reason this stays

A differential failure proves the two sides *differ*. It does not prove the
original is at fault, and it does not prove the input state was one the
original can produce. Here the state was one it cannot: a loopback object
with a bandpass bolted on. The check that would have caught it immediately —
build the object the way the library builds it, and only then compare — is
now what `t_b103link` does for the configuration itself.

`t_b103hdx` still zeroes the history before comparing, and still should: it
uses loopback objects for the state-machine tests, so the buffer genuinely is
dirty there. The comment saying `B103FP_create` does not clear it has been
corrected.

---

## D8 — `B103FP_delete` frees the object even when the caller supplied it 🐛

The same defect as D5, one level up, and found the same way it should be: by
counting allocations rather than by reading code.

`B103FP_create(state, cfg)` follows the library's "pass NULL to allocate"
idiom — a caller may supply its own storage and get it back. `B103FP_delete`
does not honour that. It ends with an unconditional tail call:

```
8f003:  mov  %ebx,0x10(%esp)        /* ebx is the object */
8f007:  add  $0x8,%esp
8f00a:  pop  %ebx
8f00b:  jmp  sysdep_free
```

There is no ownership flag on the object, and no branch guarding that free —
both exit paths reach it.

**Measured**, with the harness's allocation tracking:

```
type 0, NULL state:  create allocs=28  after delete live=0 frees=28 bad_free=0
type 0, own state:   create allocs=27  after delete live=0 frees=27 bad_free=1
```

The 28th allocation on the NULL path is the object itself; on the caller-
supplied path there are only 27 allocations but still 28 frees, and the extra
one is the caller's buffer. With a real `free()` that is heap corruption if
the buffer was malloc'd by the caller, or an abort if it was on the stack.

Note the sub-objects are *not* affected: 27 of the 28 balance exactly on both
paths, so every `FPM_*_create`'s own ownership flag is honoured correctly.
It is only `B103FP_delete`'s treatment of the outermost object that is wrong.

**Reachable?** Nothing inside `dsplibs.o` passes a non-NULL state to
`B103FP_create` — `b103_create` passes NULL — so as shipped this never fires.
It is a latent trap for any new caller using the documented idiom.

**Reproduced.** The reconstruction frees unconditionally too, and
`t_b103alloc` asserts the counts above **including the `bad_free`**, so the
defect cannot be quietly tidied away. The harness swallows frees of pointers
it never handed out rather than passing them to `free()`, which is what lets
the test observe this instead of crashing on it.

---

## D9 — `Dual_TONE_detect` truncates its loop counter to 16 bits 🐛 💤

**Status:** reproduced, unreachable as shipped.

`Dual_TONE_detect`'s sample loop increments its index and then sign-extends
the low 16 bits back into it:

```
    7e602:  inc    %eax
    7e606:  cwtl
    7e60b:  cmp    %ebp,%eax          ; against `count`, a full 32-bit int
```

So the index counts 0, 1, ... 32767, −32768, ... and never reaches a `count`
of 32768 or more. The loop would not terminate.

Everything else in the function treats `count` as a full `int` — both hold
timers are advanced by it before the loop, in 32-bit arithmetic — so this is
the compiler faithfully reproducing a `short i` in the source, not a
deliberate narrowing.

**Reachable?** No. The only caller is `CALLPROG_Progress`, working in blocks
of at most a few hundred samples, and a 32768-sample block would be four
seconds of audio at the 8000 Hz this module runs at.

**Reproduced.** The reconstruction declares `short i` for the same reason and
the same effect. Not fixed: unlike D4 it cannot fire, and the rule from D4 is
that reproducing a defect is free exactly when it is unreachable.

---

## D10 — RETRACTED: the `CP_*` filter designs are not dead ❌

**This entry claimed the four `CPfiltrs.c` designs are unreferenced. They are
referenced, by `cadence_create`, along with all nine `Filter_*` symbols in
Elliptic1/2/3.c.** Twenty-one tables, all used, all by one function.

### Why that looked true

A query that asked which functions relocate against each symbol returned
nothing. It was malformed, and returned nothing for every input — including
inputs that certainly do have references. "No results" was read as "no
references" rather than as "the query did not work", which is the whole of the
mistake.

### The correct answer

```
  CP_100_550_{a,b,scales}          <- cadence_create
  CP_276_504_{a,b,scales}          <- cadence_create
  CP_350_600_{a,b,scales}          <- cadence_create
  CP_450_630_{a,b,scales}          <- cadence_create
  Filter_100_550_{a,b,scales}      <- cadence_create
  Filter_276_504_{a,b,scales}      <- cadence_create
  Filter_350_500_{a,b,scales}      <- cadence_create
  toneiir_configuration_allpass    <- cadence_create
```

This also explains what the parameters recovered in finding F44 select.
`GetDialToneCallProgressFilterIndex`, `GetBusyToneCallProgressFilterIndex`,
`GetRingbackToneCallProgressFilterIndex` and
`GetCongestionToneCallProgressFilterIndex` choose *which* design a given tone
is detected with, and `GetDialToneFilterSubindex` chooses which of the seven
variants within a `Filter_*` family. The tables are not leftovers; they are a
per-country filter bank, and the homologation data picks from it.

### The rule this earns

Twice in this phase a negative result from a hand-written `readelf`/`objdump`
query has been believed: once for relocations inside the toneiir configuration
(finding F46), once here. **A query that returns nothing must be shown to
return something for a case known to have results before its silence is
evidence of anything.** Both mistakes would have been caught by one control
input.


---

## D11 — the calling tone's phase scaling is out by a factor of eight 🐛

**Status:** reproduced, not fixed.

`GenerateCallingTone` keeps a phase accumulator that wraps at 0x4000 and
indexes `TONE_read`, whose cycle is 0x800, with

```
    7e0b0:  movswl 0x0(%ebp),%edx     ; phase, 0..16383
    7e0b8:  add    $0x20,%edx          ; round to nearest
    7e0bb:  sar    $0x6,%edx           ; -> 0..256
```

16384/2048 is 8, so the shift should be 3. At 6 the index only ever reaches
256, so the generator sweeps the first eighth of a cosine — 0° to 45° — and
back. The output never goes negative.

**The correct version is in the same object.** `DialerProgress`'s DTMF
generator uses the identical idiom on an identical 14-bit accumulator:

```
    DTMF          idx = (phase + 4)  >> 3      correct
    calling tone  idx = (phase + 32) >> 6      wrong
```

Both round to nearest — 4 is half of 8, 32 is half of 64 — so the rounding was
adjusted to match the shift, which makes the calling tone's version look
considered rather than mistyped. But 3 is the shift that maps a 14-bit
accumulator onto `TONE_read`'s 2048-point cycle and 6 is not: the DTMF tones
land within 0.06% of nominal (finding F58) and the calling tone emits a
sawtooth.

That removes the last charitable reading of this entry. It is not a limit of
the technique, and not something nobody in the codebase understood — two
functions in the same phase, one right and one wrong.

**Measured**, at level −12 over 4096 samples:

```
    mean 29347 of a 32540 peak      range 23007 .. 32540
    negative samples 0 of 4096      AC rms 2760, 9.4% of the mean
```

So 90% of the amplitude is DC, which a line transformer removes, leaving a
tone about 20 dB weaker than intended and shaped like a sawtooth rather than a
sine. The repetition rate is unaffected — the accumulator still wraps at the
right interval — which is why this is easy to miss.

**Reachable? Yes, on shipped hardware.** `CALLPROG_Dial` enables the generator
for two values of `GetCallingToneFlag` (parameter 27), not one:

```
    7a882:  movl $0x0,0x60(%ebx)     ; flag == 0  -> off
    7a99e:  movl $0x1,0x60(%ebx)     ; flag == 1  -> ON
    7a611:  cmp  $0x2,%eax
    7a614:  sete %al                 ; flag == 2  -> ON, anything else off
```

Reading only the `sete` gives "enabled when the flag is 2", which is what an
earlier version of this entry said and is wrong: the flag == 1 case is a
separate branch that sets the same field.

slmodemd ships fifty country parameter sets. Forty-nine carry
`CallingToneFlag = 0`; **`params014`, CZECH_REPUBLIC (id 0x002e), carries 1**.
No set carries 2. So the generator is off in forty-nine configurations and on
in one, and everything below happens on a real Czech line.

**Not fixed.** It does not stop a call completing -- it degrades an optional
courtesy signal that the far end may ignore anyway -- but it is not
unreachable, so this is a deliberate decision to stay faithful rather than the
free choice D4's rule describes. `t_callingtone` asserts the measurements
above so a later edit cannot change the output silently.

---

## D12 — the calling tone's period counter double-counts across a transition 🐛

**Status:** reproduced, not fixed.

The generator clamps its per-iteration bound against the block size rather
than against the samples remaining in the block:

```
    end = min(remaining, count)      /* an index bound, not a length */
    ...fill from i to end...
    remaining -= end                 /* but only end - i were written */
```

On the first pass `i` is zero and the two agree. After a cadence transition
inside a block, `i` is not zero, and the next period is charged for the
samples the previous one already emitted.

**Measured**, tone burst 5760 samples, silence 16800:

```
    block 64 (divides 5760)      silence 16800 samples   correct
    block 50 (leaves 10 over)    silence 16790 samples   10 short
```

The shortfall is exactly the overhang of the previous burst, so the error is
bounded by the block size — under 0.3% of a period at the 48-sample blocks
`call_run` uses. The burst that *ends* is always correct; it is the one that
follows that is clipped.

**Not fixed**, and asserted by `t_callingtone`.

---

## D13 — the calling tone's level control does almost nothing 🐛

**Status:** reproduced, not fixed.

`ResetCallingTone` turns a level into an amplitude with

```
    amplitude = FP_Pow((level * 154791) >> 14)
```

`FP_Pow` is exp() in Q14 (finding F9), so turning decibels into a linear ratio
wants a multiply by ln(10)/20, which is 1886 in Q14 — `FP_Pow(level * 1886)`
would have been right. What is there is `level * 9.45`, about 200 times too
small.

**Measured** across the whole range of the signed char it takes:

```
    level -128   amplitude 15216      0.93 of full scale
    level  -12   amplitude 16270      0.99
    level    0   amplitude 16384      1.00
    level  127   amplitude 17626      1.08
```

1.4 dB from end to end, where the argument spans 255 dB. Asking for −12 dB
gives 0.99 of full scale instead of 0.25.

The visible consequence is an overflow. The sample computation is

```
    buf[i] = (short)((amplitude * TONE_read(...)) >> 13)
```

which doubles — correct if `amplitude` were a proper fraction, but it is
always near 16384, so the product reaches 32768 and wraps to −32768 in the
16-bit store. At level 0 that happens on 40 samples in every 4096; at −12 it
does not happen at all, which is what identifies it as an overflow rather
than a scaling choice.

**Reachable** wherever D11 is: see its entry. Note also that `ResetCallingTone`
is called on *all three* of `CALLPROG_Dial`'s branches, including the one that
disables the generator, so the amplitude is computed whether or not it is used.

**Where the level comes from.** S-register 221, fetched through a callback
that `call_create` fills in with `call_GetSRegister`, a fourteen-byte tail-call
to `modem_get_sreg`. That namespace was unidentified when this entry was first
written and is now settled (docs/parameters.md); the index is valid and in
range, so there is no additional fault here. `CALLPROG_Dial` narrows the
result with `movsbl`, so an S221 of 128 or more arrives negative -- which,
given the 1.4 dB span, moves the amplitude by well under a decibel.

**Not fixed**, and both the level range and the overflow are asserted by
`t_callingtone`.

---

## D14 — `toneiir_create` reads its envelope before writing it 🐛 💤

**Status:** reproduced, harmless.

`toneiir_create` finishes by performing `toneiir_reset`'s body inline, and
that body's first act is to carry the current band envelope into `env_prev`:

```
    7c37a:  movzwl 0x96(%ebx),%edi     ; edi = env_band
    7c38c:  mov    %ax,0x96(%ebx)      ; env_band = 0
    7c398:  mov    %di,0x98(%ebx)      ; env_prev = the old env_band
```

In `toneiir_reset` that is exactly right and is the whole point: the interval
just finished becomes the one the next interval is compared against. In
`toneiir_create` there is no interval just finished. On the allocating path
`env_band` is whatever `sysdep_malloc` returned, and that value lands in
`env_prev`, where the first verdict's stability test compares against it.

**Consequence:** at most one wrong verdict, 62.5 ms after the filter is built,
and only if the junk happens to fall in the narrow band that changes the
outcome. The stability test needs `env_band >= env_prev * 0.7` **and**
`env_prev >= env_band * 0.7`, so a large junk value fails it and a zero one
fails it too; the first interval is much more likely to report ABSENT than
PRESENT either way, and `cadence_progress` needs 35 consecutive intervals
before it reports anything.

**Reproduced**, and the test pre-fills both objects with the same pattern so
the field is comparable at all — `t_toneiir` asserts that `env_prev` comes out
holding the pre-fill, which is what proves the read happens.

---

## D15 — a null dial string grades VALID 🐛 💤

**Status:** reproduced, unreachable as shipped.

`AnalyseDialString` sets its grade to `VALID` before checking its argument,
and the null check falls straight into the return:

```
    7aa18:  mov  $0x3,%ebp          ; grade = VALID
    7aa21:  mov  $0x3,%edx          ; and the return value
    7aa26:  mov  %eax,0xa0(%esi)    ; d->last_digit = -2
    7aa2c:  je   7aaa0              ; s == NULL -> return edx, which is 3
```

So `IsDialStringInvalid(d, NULL)` answers "not invalid" and the caller
proceeds to dial nothing. The length check immediately below gets this right
-- a string longer than the buffer grades `FATAL` -- which is what makes the
null case look like an oversight rather than a decision.

**Reachable?** No. The only callers pass `modem_get_param(MDMPRM_DIALSTR)`,
which slmodemd answers with `m->dial_string`, an array inside the modem
structure rather than a pointer that can be null.

**Reproduced**, and `t_dialer` drives NULL explicitly so the behaviour is
pinned rather than merely inherited.

## D16 — the V.21 offer can never be withdrawn 🐛 💤

`V8UpdateModemParameters` intersects the received menu with the local one by
clearing a bit for every modulation the far end did not offer. The third
modulation octet carries V.23 in bit 6 and V.21 in bit 7, and the object
tests them like this:

```
    74bd5:  movzwl (%esi,%eax,2),%edx    ; the 10-bit character, raw
    74bd9:  test   $0x40,%dl
    74bdc:  jne    74be6
    74bde:  andb   $0xef,0x1(%edi)       ; V.23 not offered -- clear it
    74be2:  movzwl (%esi,%eax,2),%edx
    74be6:  test   $0x3,%dl
    74be9:  jne    74bf0
    74beb:  andb   $0xdf,0x1(%edi)       ; V.21 not offered -- clear it
```

Both tests are against the raw sequence word rather than the octet, and the
two differ by the framing: a word is the octet shifted up one with a stop bit
in bit 0. The V.23 test survives that, because bit 6 of the word is bit 5 of
the octet and nothing else lands there. The V.21 test does not: `word & 3`
takes in the stop bit, which is set in every character V.21 can carry, so the
test is never zero and the `and` is dead code.

The V.21 bit that should have been consulted is bit 1 of the word. `& 3`
instead of `& 2` is a one-character slip.

**Effect.** `cm->b1` bit 5 stays set whatever the far end said, so the modem
believes V.21 is on the menu after a negotiation in which it was not offered.

**Reachable?** Yes, trivially -- any far end that omits V.21. Measured
against SpanDSP in `t_spandsp_v8sock`: with SpanDSP offering V.21 and V.32,
V.34 and V.23 are correctly dropped and V.21 correctly kept, so the defect
does not show there. It would show against a far end offering, say, V.32
alone.

**Harmless in practice**, which is presumably why it survived: V.21 at 300
bit/s is the fallback every V.8-capable modem supports, and the handshake has
just finished conducting a conversation in it. A modem that negotiated V.8 at
all has demonstrated V.21.

**Reproduced.** `t_v8jm` pins the behaviour, and the reconstruction carries
the same `& 3`.

## D17 — loopback calls a receive handler that was never installed 🐛 💤

`B103FP_create` sets `hdx->tx` unconditionally and `hdx->rx` on exactly one
arm -- originate with `cfg.f10 == 0`, which installs `RxHdxDataB103`
directly:

```
    8ebbc:  movl $0x0,0x10(%ecx)      <== R_386_32 RxHdxDataB103
```

Everywhere else `hdx->rx` is whatever the allocator handed back. That is
fine for two of the three modes, because their first transition installs one:

```
    B103OriginateNextState  START -> hdx->rx = RxDetMarkB103
    B103AnswerNextState     START -> hdx->rx = RxDetMarkB103
    B103LocLoopNextState    START -> tx, tx_blocks, rx_count, substate
                                     -- and nothing else
```

`B103LocLoopNextState` does not install a receive handler until `CARRDET`,
one transition later:

```
    8f754:  movzwl 0x2(%ecx),%edx
    8f758:  movl   $0x0,0x8(%ecx)     <== R_386_32 TxHdxMarksB103
    8f75f:  movw   $0x1,0x14(%ecx)    ; substate = CARRDET
    8f765:  lea    (%edx,%edx,1),%eax
    8f768:  mov    %ax,0x4(%ecx)      ; tx_blocks
    8f76c:  mov    %dx,0xc(%ecx)      ; rx_count
    ...                               ; +0x10 is not written
```

And `B103FP_modem` calls the handler without checking it:

```
    n = fp->hdx->rx(fp, rx_in, staging, n_rx);
```

So a loopback object faults on the *first* block it is given. The transmit
loop runs `TxHdxStartB103`, which takes START to CARRDET and installs no
receive state, and the receive loop then calls through whatever was in the
allocation.

**Reachable?** No. `b103_create` sets `cfg.call_type` from its `caller`
argument, which is 0 or 1 -- originate or answer -- and never the value that
selects loopback. The only way in is to hand `B103FP_create` the library
default `B103_CFG_data`, whose `call_type` *is* loopback, which nothing in
the object does.

**How it was found.** Not by reading: by filling fresh allocations with
`0xa5` instead of letting them be zero. With a zeroing allocator the
uninstalled handler is NULL, `t_b103hdx` guarded on `== 0`, and the whole
thing read as a deliberate "no handler in this state". With a fill it is a
jump to `0xa5a5a5a5`, which is what it always was.

**Reproduced.** `t_b103hdx` now asserts that loopback has installed nothing
one transition after START -- pinning the defect rather than avoiding it --
and then takes the extra transition before driving the receive states, which
also gained coverage of `RxHdxDataB103` under loopback that the old guard
had been silently skipping.

## D18 — `RcFixed_Reset` checks for null and the original does not ⚠

**Module** `src/core/fixedrc.c` · original `FixedRC.c`, `.text 0x0b0e10`

The original dereferences its argument on the first instruction:

```
    b0e10:  sub    $0x1c,%esp
    b0e13:  mov    0x20(%esp),%eax      ; the handle
    b0e1f:  mov    (%eax),%edx          ; ->kind, unconditionally
    b0e21:  test   %edx,%edx
```

`RcFixed_Delete`, twenty bytes earlier in the same file, does check
(`test %ebx,%ebx; je`). The reconstruction checks in both.

**Not behind `DSPLIB_REPRODUCE_BUGS`**, unlike every deliberate fix, because
there is nothing to reproduce: the original's behaviour on null is a
segmentation fault, and a differential test cannot compare our return against
that. The one input that distinguishes the two implementations kills the
reference, so `t_rcresample` deliberately does not drive it and says so.

**Reachable?** No. Every caller of `RcFixed_Reset` in the object holds a
handle it built itself; nothing passes a pointer that can be null.

**Why keep it.** The contract is functional equivalence over inputs a caller
can produce, and this is outside that domain in the only direction that
matters -- ours is defined where the original is not. Removing it would buy
bit-exactness on an input that crashes.

This is the first entry of its category. Others are likely: the
reconstruction was written by people reading assembly, and a null check is
the kind of thing that gets written without noticing the original had none.
Finding the rest is its own task rather than a claim made here.

## D19 — the V.23 transmitter's mute reports samples where everything else reports bits 🐛 💤

**Module** `src/pump/v23/v23tx.c` · original `v23tx.c`, `.text 0x0873b0`

`v23FP_tx_progress` writes the number of bits it finished through its
`consumed` argument, and that is how the caller knows to advance its own bit
stream. Every path does this except the one-shot mute, which reports the
sample count instead:

```
   873c9:  mov    0x14(%ebx),%eax      ; the mute flag
   873cc:  test   %eax,%eax
   873ce:  je     8743d                ; not muting: the normal path
   873d6:  mov    %edi,%eax            ; edi = count, in SAMPLES
   873e0:  movw   $0x0,(%esi)          ; ... write count zeros ...
   873eb:  mov    %edi,0x18(%esp)      ; consumed = count
   873f3:  movl   $0x0,0x14(%ebx)      ; clear the flag
```

A caller that advanced its bit stream by the reported figure would skip
`count` bits instead of the zero it actually sent -- for the 160-sample block
the datapump uses, 160 bits rather than 0.

**Reproduced**, not fixed: the reconstruction returns the same number.

**Reachable?** Only if something sets the flag, and `v23modem.c` now says
what that flag *is*: the same configuration byte that arms the 2100 Hz answer
tone. `CreateV23Modem` passes it straight to `v23FP_tx_create` as the mute,
so any answering configuration arms it for exactly one block.

That also says exactly when it fires. A configuration with the byte set
starts in state 0, and `V23ModemMain` returns before reaching the transmitter
in states 0 and 1 -- so the data transmitter is not called at all until three
seconds of answer tone and a silence have gone by. The mute is still armed
when it finally runs, eats that block, and reports `count` bits consumed when
it sent none. How much that costs depends on whether the layer above is
offering data three seconds into an answering sequence, which is a question
for `v23.c` -- as is whether any shipped configuration sets the byte at all.
`t_v23tx` drives the path directly and pins the behaviour either way.

## D20 — the V.23 transmitter can hold an uninitialised bit ⚠

**Module** `src/pump/v23/v23tx.c` · original `v23tx.c`, `.text 0x0873b0`

`v23FP_tx_create` leaves the object's `held` field alone, which is safe
because it is only read when `resume` is set and only the tail of
`v23FP_tx_progress` sets that -- writing `held` in the same breath.

Except on one path. If `progress` is called with `count == 0`, the sample
loop never runs, `%ebp` is never loaded with a bit, and the tail still
executes:

```
   8743d:  test   %edi,%edi            ; count
   8743f:  je     8748f                ; straight to the tail
   8748f:  cmpw   $0x0,0x8(%ebx)       ; remaining != 0 after create
   874ea:  movl   $0x1,0x10(%ebx)      ; resume = 1
   874f5:  mov    %ebp,0xc(%ebx)       ; held = whatever %ebp holds
```

The next call then transmits a whole bit period at a frequency chosen by an
uninitialised register.

The reconstruction initialises the bit to zero, so a `count == 0` call holds
a space rather than something arbitrary. **Not behind
`DSPLIB_REPRODUCE_BUGS`**: there is nothing to reproduce -- an uninitialised
register has no defined value to match, and reading one in C is undefined
rather than merely unpredictable.

**Reachable?** Not from `V23ModemMain`, which is the only caller and passes
its own block size. It is reachable by anything else linking this library.

**Why keep it.** Same argument as D18: ours is defined where the original is
not, and the only input that tells them apart is one where the original has
no behaviour to be equivalent to. `t_v23tx` does not drive `count == 0` and
says why.

## D21 — V.23's two receivers disagree about the width of one configuration field 🐛 💤

**Module** `src/pump/v23/v23rx.c` · original `v23rx.c`, `.text 0x086d30`

`struct v23_cfg` carries one carrier-loss timeout at `+0x08`, and both of
V.23's receivers copy it into their own object at create time. They do not
agree about how much of it to copy:

```
; v23FP_rx_create
   86f10:  mov    0x8(%eax),%ecx        ; the whole 32 bits
   86f25:  mov    %cx,0x238(%ebx)       ; ...stored as 16

; BwChDem_Create keeps all 32
```

So one configuration value produces two different timeouts. Below 65536 they
agree exactly. At or above it they do not merely differ, they fail in
*opposite* directions:

- the forward channel's limit wraps to 0, so `silence_limit <= silence` is
  true on the first quiet block and it gives up the moment the line dips;
- the backward channel never gives up at all. Its test is
  `(int)(unsigned short)bw->silence >= bw->silence_limit` against a counter
  that steps by 20 with 16-bit wrap, so the largest value it can ever take is
  65520. Any limit above that is unreachable and the demodulator holds a dead
  line for ever.

The unit is milliseconds — both counters are charged 20 per call, which is
one 160-sample block at 8 kHz — so the boundary is 65.5 seconds of tolerated
silence. That is already a long time for a modem to hold a dead line, which
is why this is dormant rather than live: `v23.c` configures 700 ms.

**Reproduced**, not fixed: the reconstruction truncates in `v23rx.c` and does
not in `bwchdem.c`, exactly as the original does. Widening the field would be
a behaviour change on an input the original handles differently, and the two
sides of a differential test would stop agreeing.

**Reachable?** Only from a configuration that asks for a 65-second timeout or
longer, and `v23.c` — the only caller — asks for 700 ms. So nothing in the
shipped library reaches it. `t_v23rx` drives the truncating path directly
with a limit of 200 ms and pins it, and `t_v23dp` checks that both receivers
come out of `v23_create` holding the same 700.

## D22 — `CreateV23Modem` builds nothing at all when handed storage 🐛 💤

**Module** `src/pump/v23/v23modem.c` · original `v23modem.c`, `.text 0x086890`

Every other create in this library follows the same shape: allocate if the
caller passed NULL, then build. `CreateV23Modem` puts the *building* inside
the allocation branch:

```
   868a2:  test   %ebx,%ebx
   868a4:  je     869f4                 ; NULL -> allocate AND construct
   868aa:  ...                          ; non-NULL enters here
   ...
   869f4:  movl   $0x28,(%esp)
   869fb:  call   sysdep_malloc
   86a00:  mov    %di,(%eax)            ; mode -- inside the branch
   86a1a:  call   BwChDem_Create        ; and both sub-objects
   86a4f:  call   v23FP_tx_create
   86a57:  jmp    868aa                 ; only now join the common tail
```

So `CreateV23Modem(&my_modem, mode, cfg)` returns `&my_modem` with the
answer-tone sequence set up correctly and with `mode`, `tx` and `rx` holding
whatever was in that memory. The first `V23ModemMain` then reads `mode` to
decide which receiver it has and dereferences `rx` to ask it about carrier.

**Reproduced**, not fixed: the reconstruction builds inside the same branch.
An added guard would be a behaviour change on an input the original does
handle — it returns, it does not fault — and the differential test can
compare what both sides leave behind, which it does.

**Reachable?** Only from a caller that supplies storage. `v23_create` is the
only caller and is not reconstructed yet; every other create in this tree is
called with NULL. `t_v23modem` drives the path with a known fill pattern on
both sides and compares all forty bytes, so whichever way `v23.c` turns out,
the behaviour is pinned.

## D23 — the answer-tone states size the transmit buffer by the receive count 🐛 💤

**Module** `src/pump/v23/v23modem.c` · original `v23modem.c`, `.text 0x086b10`

`V23ModemMain` takes a sample count for each direction. In data it uses them
correctly. In the two states before it, both writes to the *transmit* buffer
are sized by the *receive* count:

```
   86b35:  mov    0x48(%esp),%ebp       ; rx_count
   ...
   86cf4:  movswl %bp,%ecx              ; state 0: the answer tone
   86cf7:  mov    %ecx,0x8(%esp)        ; ...FPM_TONE_generate's length
   ...
   86c71:  test   %ebp,%ebp             ; state 1: the silence
   86c80:  movw   $0x0,(%edx)           ; ...zeroing rx_count samples
```

The transmit count at `+0x40` is untouched until data. So a caller whose
receive frame is longer than its transmit frame overruns the transmit buffer
during the answer tone, and one whose receive frame is shorter leaves the
tail of it holding whatever was there before -- three seconds of a stale
buffer going down the line.

The elapsed-time bookkeeping uses the same figure, so the *duration* of the
sequence is measured in receive samples too. That part is arguably right:
it is the far end's clock that matters.

**Reproduced**, not fixed.

**Reachable?** Not from `dp_wrapper`, which delivers one 160-sample frame in
each direction and therefore makes the two counts equal. Reachable by
anything else driving `V23ModemMain` directly. `t_v23modem` passes equal
counts, as every real caller does, and says so.

## D24 — V.23 reports the same line rate in both directions 🐛

**Module** `src/pump/v23/v23.c` · original `v23.c`, `.text 0x004da0`

V.23 is asymmetric by definition: 1200 bps one way, 75 the other. On the edge
into carrier, `v23_process` computes one rate and sets both parameters to it:

```
   4ec1:  mov    $0x2,%eax
   4ec6:  mov    %eax,0x4(%esp)         ; MDMPRM_TX_RATE
   4eca:  cmp    $0x1,%edx              ; caller
   4ecd:  sbb    %ebx,%ebx
   4ecf:  and    $0xfffffb9b,%ebx       ; -1125
   4ed5:  add    $0x4b0,%ebx            ; +1200  -> 75 or 1200
   4edb:  mov    %ebx,0x8(%esp)
   4ee5:  call   modem_set_param
   4eea:  mov    %ebx,0x8(%esp)         ; the SAME value
   4eee:  mov    $0x1,%ebx
   4ef3:  mov    %ebx,0x4(%esp)         ; MDMPRM_RX_RATE
   4f02:  call   modem_set_param
```

`%ebx` is callee-saved, so the second call passes exactly what the first did.

Worked through, the one number it computes is always the **receive** rate:

| `caller` | end | receives | transmits | TX reported | RX reported |
|---|---|---|---|---|---|
| 0 | host | 75 | 1200 | **75** | 75 |
| non-zero | terminal | 1200 | 75 | **1200** | 1200 |

So `MDMPRM_RX_RATE` is right at both ends and `MDMPRM_TX_RATE` is wrong at
both, by a factor of sixteen in opposite directions.

**What it does not break.** Transmit pacing does not come from this. The core
is asked for exactly as many bits as `V23ModemMain` reported the transmitter
consumed last block, which is derived from the bit-period table and is
correct. What is wrong is the rate the core *reports* -- the CONNECT message
and anything sized from it.

**Reproduced**, not fixed: the reconstruction sets both to the same value.
The correct pair is not guessable from the object either, since nothing in it
records which figure was meant to be which.

**Reachable?** Every V.23 call, at both ends, on the first block after
carrier. This is the one entry in this register that is not dormant.
`t_v23dp` asserts the wrong value deliberately -- a reconstruction that
quietly fixed it would still pass a comparison against a reference that does
not.

---

## D25 🐛 the V.34 detector's two sections scale differently

**Where:** `src/pump/v34/detector.c`, `tone_detect`.

**What the original does:** divides section 1's output by 16 by shifting the
32-bit accumulator and then truncating to 16 bits, and divides section 2's
output by 16 by truncating to 16 bits and then shifting. Finding F90 has the
two instruction sequences.

**What we do:** the same, both of them, in the same order.

**Why it is a defect:** the two orders are not the same function. Once the
accumulator leaves 16 bits, section 2's output folds where section 1's would
have scaled, so the level fed to the integrator -- and therefore the
detector's verdict -- is wrong in a way that depends on how far the
accumulator overflowed rather than on how much signal there was.

**Reachable?** Only above 16-bit accumulator range, which a detector fed by a
working AGC does not reach. `t_v34det` drives it deliberately (the
resonators there are deliberately soft precisely so the behavioural
assertions measure the filter and not this), and both sides agree byte for
byte throughout. Dormant in normal operation.

**Not fixed**, and not behind `DSPLIB_REPRODUCE_BUGS`: there is no way to
tell which of the two orders was intended, so there is no "fixed" version to
put behind the flag. Changing either one would change which signals the
handshake believes it has heard, and no tier-2 peer exists for V.34 to say
which answer is right.

---

## D26 🐛 `V34EchoCleanUp` does not clear the delay line

**Where:** `src/pump/v34/v34filters.c`, `V34EchoCleanUp`.

**What the original does:** rewinds the cursor to the start of the delay
line and zeroes the coefficients (both halves) and the tap history, for
`taps` entries each. It does **not** touch the `dlen` entries of the delay
line itself.

**What we do:** the same.

**Why it might matter:** the next `V34EchoFilter` convolves freshly zeroed
coefficients against whatever the previous connection left in the line.

**Reachable?** Yes, on every reset — but harmless, and provably so: the
coefficients are zero at that moment, so the first output is zero whatever
the line holds, and by the time adaptation has moved a coefficient far enough
to matter the line has been overwritten by `V34EchoUpdateDelayLine`. The
window is `taps` samples wide and the output over it is zero.

**Not fixed.** `t_v34ec` asserts the line survives, so a reconstruction that
quietly zeroed it would fail rather than pass.

---

## D27 🐛 `V34EchoFilter` wraps its read index only once

**Where:** `src/pump/v34/v34filters.c`, `V34EchoFilter`.

**What the original does:** computes `cursor + lag + taps - 1`, compares it
against the end of the delay line, and subtracts the length **once** if it is
past. An index more than one full length past the end stays past the end and
the read leaves the buffer.

**What we do:** the same, single subtraction.

**Reachable?** Now bounded. `V34InitializeImplementationSpecific` sets
`dlen = 0x678` (1656) and `taps = 0x90` (144) for both cancellers. The read
index is `cursor + lag + taps - 1` with `cursor` at most `dlen - 1`, so one
subtraction suffices whenever

```
    lag  <=  dlen - taps + 1  =  1656 - 144 + 1  =  1513
```

A `lag` above 1513 would in any case be asking for a sample older than the
delay line holds, so the single wrap covers the whole domain in which the
function returns anything meaningful. **Dormant**, unless `V34RX.c` passes a
`lag` that is already out of range — which remains worth confirming when it
lands, but is no longer an open question about this function.

**Not fixed.** A second wrap would be a different function, and with no
tier-2 peer for V.34 there is nothing that could say which one the caller
wants.

---

## D28 ~~🐛 `V34EchoReportCoeff` scans `taps` coefficients and prints 144~~ RETRACTED

**Where:** `src/pump/v34/v34filters.c`, `V34EchoReportCoeff`.

**What the original does:** decides whether there is anything to report by
scanning `(taps / 6) * 6` coefficients, and then dumps a **hardcoded** 144 of
them, six to a line, regardless of `taps`. A canceller with fewer than 144
taps has its coefficient array over-read.

**What we do:** the same, with the 144 named `V34_ECHO_REPORT_TAPS`.

**Reachable?** Only with `dsplibs_debug_level > 1`, which slmodemd ships at
zero, so it cannot fire on a working modem. The read is also of the
library's own allocation rather than of anything a caller owns.

**RETRACTED.** 144 is not a magic number: it is `taps`.
`V34InitializeImplementationSpecific` sets both echo cancellers' tap counts
to `0x90` — 144 — at 0x71dc6 and 0x71e1e. So the dump is sized to the array
exactly, and there is no over-read.

The scan is not odd either, once the same number is applied: `(144 / 6) * 6`
is 144, so the rounding-down that looked like a mismatch is a no-op at the
only tap count the object ever uses.

This entry was filed as a defect on the strength of "the two loops use
different bounds", without measuring what `taps` actually is. The bug policy
requires measuring whether a defect fires, and this one was registered before
that was done. Left in the register rather than deleted, because a retraction
is more useful than a gap — see D7 and D10.

The reconstruction is unchanged: it still spells the dump length as
`V34_ECHO_REPORT_TAPS`, which is correct and now documented as equalling
`taps` rather than as an unexplained constant.

---

## D29 🐛 `V34TimingFiltersInit` zeroes half the prefilter state

**Where:** `src/pump/v34/v34filters.c`, `V34TimingFiltersInit`.

**What the original does:** after clearing the IIR state, runs one loop
zeroing **eighty shorts** from +0x024:

```
   72700:  movw   $0x0,0x24(%ecx,%eax,2)
   72709:  cmp    $0x4f,%ax
   7270d:  jbe    72700
```

Two arrays live in that region. The high-pass history is 40 **shorts** at
+0x024, and the prefilter state is 40 **ints** at +0x074 — `V34TimingPrefilter`
addresses it with `(%esi,%edi,4)` and it abuts the coefficient pointer at
+0x114 exactly, so both the type and the length are pinned.

Eighty shorts is 160 bytes: the whole high-pass history, and then the first
**twenty** of the prefilter's forty entries. The upper twenty are left
holding whatever was there.

**What we do:** the same bytes, written as two loops — one over `hist` and
one over the first twenty entries of `pre_state` — because a single loop over
`hist` would be indexing past an array in C even though it is exactly what the
object emits.

**Why it looks like a slip rather than a design:** the loop bound reads as
"the length of the thing at +0x24", and it would be correct if both arrays
were shorts. `V34TimingPrefilter` then convolves 40 taps against a state
whose upper half was never initialised.

**Reachable?** On every call to `V34TimingFiltersInit`, which is once per
setup. The prefilter's output is wrong until 20 more complex samples have
shifted through — about 20 symbols. Whether that matters depends on what
V34RX.c does with the prefilter during acquisition, which is not
reconstructed; **the consequence is unmeasured** and recorded as such.

**Not fixed.** `t_v34ec` asserts the upper twenty entries still hold the
harness fill after init, so a reconstruction that helpfully zeroed all forty
fails rather than passes in the binary-comparison tier.

**Fix.** The default (shipping/interoperability) build now clears all forty
prefilter entries. `DSPLIB_REPRODUCE_BUGS` retains the object's eighty-short
extent, preserving bit-exact comparison. This follows the same ifdef polarity
as the other deliberate fixes: real modem builds do not consume heap contents
as timing-filter history.

**Performance result (2026-08-19): no effect in the seeded model.** With
`CHAN_SEED=20260819`, 70 ms delay and 18 dB SNR, the blob-compatible and fixed
builds produced byte-for-byte identical Phase-4 decisions: answer `equerr`
4627, origin `equerr` 4197, rate index 4 / 9600 bit/s at both ends. The fix
remains worthwhile hardening, but this controlled run disproves it as the
cause of that particular equaliser/rate gap. It must not be cited as evidence
of a performance gain without a different, reproducible channel outcome.

---

## D30 🐛 `V34InitializeImplementationSpecific` seeds `cursor` from the old `dline`

**Where:** `src/pump/v34/v34filters.c`.

**What the original does:** for each echo canceller, reads the current
contents of the `dline` field and stores that into `cursor`, a few
instructions before overwriting `dline` with the correct base:

```
   71d85:  mov 0x80bc(%eax),%edx     ; the OLD dline
   71da3:  mov %edx,0x80b8(%eax)     ; cursor = it
   71daf:  mov %ecx,0x80bc(%eax)     ; dline = obj+0x81f8
```

The obvious intent is `cursor = dline`, and on any re-initialisation that is
exactly what it computes, because the old base and the new base are the same
address. On a **first** initialisation the field has never been written, so
`cursor` is seeded from uninitialised memory.

**What we do:** the same — `obj->echo0.cursor = obj->echo0.dline;` placed
before the assignment to `dline`, which is the same read-before-write.

**Reachable?** The bad value exists only between this function and the first
`V34EchoCleanUp`, which unconditionally rewrites `cursor` with `dline`, and
every setup path calls it. Nothing dereferences `cursor` in between.
**Dormant.**

**Not fixed.** `t_v34fsk` asserts `cursor` still holds the harness fill after
initialisation, so a reconstruction that helpfully set it to the new base
fails rather than passes — the same shape of check as D26 and D29.

---

## D31 ~~`V34SetupModulator`'s V.90 arm is unreachable~~ RETRACTED

**Where:** `src/pump/v34/v34filters.c`, the 3200-baud case.

**What the original does:** the 3200 case looks like it chooses between a V.34
configuration (0x20 taps from `tx3200c1_for_v34`) and a V.90 one (0x40 taps
from `tx3200c1_for_v90`, with `V90EchoPrefilterCoeff` as the pre-emphasis).
The branch that would select the V.90 arm is

```
   732f1:  ...                       ; entered via je from cmp $0xc80,%ebx
   732f7:  movl $0x1,0x4(%ecx)
   732fe:  movl $0x3,0x8(%ecx)
   73305:  je   7333e                ; <- always taken
```

**`movl` does not set flags.** So the `je` inherits ZF from the
`cmp $0xc80,%ebx` that selected this case in the first place — which was
equal, or control would not be here. The branch is unconditionally taken and
the V.90 arm is dead.

**What we do:** take the V.34 arm unconditionally, and say why.

**Consequence:** `tx3200c1_for_v90` (384 bytes) and `V90EchoPrefilterCoeff`
(84 bytes) are never installed by this function.

**They are not dead data.** V.90 is planned for a later phase, so those
tables have a reader that has not been reconstructed yet. The likely reading
is therefore not "someone forgot a `cmp`" but "the V.90 configuration is
selected somewhere else, and this arm is a leftover from when it was
selected here". Re-open when `VPcmV34Main.cpp` lands — if V.90 sets the
modulator up by another route, this arm is vestigial rather than broken and
the entry should be retracted.

**Not fixed.** A missing `cmp` cannot be guessed at: there is no way to know
which comparison was intended, and inventing one would change which shaping
filter a 3200-baud V.90 connection uses.

**How it was found:** by the differential test, after two wrong guesses at a
carrier-based condition. The test disagreed at exactly one (baud, carrier)
pair each time, which is what pointed at the branch rather than at the
tables.

### RETRACTED — the arm is live and `v90` selects it

The listing above is missing its first line. The object has

```
   732f1:  mov  0x40(%esp),%ecx
   732f5:  test %edi,%edi            ; %edi is the `v90` argument
   732f7:  movl $0x1,0x4(%ecx)
   732fe:  movl $0x3,0x8(%ecx)
   73305:  je   7333e                ; v90 == 0 -> the V.34 arm
```

and `test` is the flag-setter. Everything after it here followed from the
missing line: the reasoning about `movl` and inherited flags is correct and
was applied to the wrong instruction. `72d57: mov 0x50(%esp),%edi` is what
makes `%edi` the fifth argument; the prologue's frame layout is in finding
F216.

Fixed in `v34filters.c`: `v90` now selects 0x40 taps, `tx3200c1_for_v90`,
`V90EchoPrefilterCoeff` and 14 in `fc8c`. `t_v34ec.c`'s state sweep takes
`v90` as a dimension, which is what would have caught this and did not exist
— the only sweep that varied it compared transcripts, and `v90` is printed
before the switch.

This is the fourth retraction, after D28, D34 and D47, and the first of them
that was a wrong reading of an instruction rather than a claim about
reachability that later expired.

---

## D32 🐛 `V34ModulatorProcess` seeds its delay-line shift from a stale register

**Where:** `src/pump/v34/v34filters.c`, the two shift loops at the end.

**What the original does:** after producing its output samples it advances the
symbol delay line by one, each half separately, writing `%ebp` into index 0:

```
   735b2:  movswl 0xcbc(%edi,%edx,2),%ebx   ; read work[i]
   735ba:  mov    %bp,0xcbc(%edi,%edx,2)    ; write the carry
   735c7:  mov    %ebx,%ebp                 ; carry = the old value
```

`%ebp` is zeroed at function entry, but the row loop reuses it — it last
holds `sine[phase]` from the final iteration (set at 0x734dc, and untouched
between there and the shift). So a **carrier sample is pushed into a symbol
delay line**.

**What we do:** the same, keeping the last `sine[phase]` explicitly so the
value written is the value the original writes.

**Why it does not break anything:** index 0 is overwritten by the incoming
symbol at the top of the next call, before the convolution reads it. The
garbage never reaches the filter. The second shift inherits the first's final
carry, which is a real delay-line value, so the imaginary half is seeded with
something meaningful by accident.

**Reachable?** Every call. **Observable?** Only by inspecting the object
between calls, which `t_v34ec` does — it compares the whole modulator,
including `work_cbc`, so a reconstruction that seeded the shift with zero
would fail.

**Not fixed.** Writing a zero would be tidier and would differ from the
object in a field the differential test reads. There is nothing to gain: the
value is dead before it is used.

---

## D33 🐛 `updateAlpha` divides by zero for `energy = -1`

**Where:** `src/pump/v34/v34rx.c`.

**What the original does:** normalises `energy` upward until bit 30 is set,
then divides `1 << (shift + 21)` by `(energy + 0x8000) >> 16`.

For `energy = -1` the normalising loop does not run — bit 30 is already set
in `0xffffffff` — and `(-1 + 0x8000) >> 16` is **zero**. The `idiv` faults.

**What we do:** the same. The reconstruction divides by the same value and
faults identically.

**Reachable?** `energy` is an energy estimate, so a caller producing -1 would
already be wrong. **Unmeasured** — the callers are `rxinit`, `agcadapt` and
`adaptecho`, none of which is reconstructed yet. Re-open when they are.

**Not fixed.** There is no correct value to substitute: the function's own
overflow guard (`if (quotient < 0) quotient = 0x2000`) shows the author
thought about the divide's range and did not guard this case, so any clamp
would be invention. `t_v34rx` documents the domain in a comment rather than
driving it.

---

## D34 ~~`rxinit` seeds the AGC integrator from a stale return register~~ RETRACTED

**Retracted.** There is no such defect. `rxinit` writes `agc_accum` from
`%eax`, and `%eax` is cleared by `xor %eax,%eax` two instructions before the
first of the three stores it participates in:

```
   5ac09:  mov  $0x4000,%ecx
   5ac0e:  mov  $0x4000,%edx
   5ac13:  xor  %eax,%eax           <-- here
   5ac15:  mov  %cx,0x218(%ebx)
   5ac1c:  xor  %ecx,%ecx
   5ac1e:  mov  %dx,0x1f2(%ebx)
   5ac25:  xor  %edx,%edx
   5ac27:  testb $0x8,0x122(%ebx)
   5ac2e:  mov  %ax,0x138(%ebx)     <-- agc_accum = 0
   5ac35:  mov  %cx,0x134(%ebx)     <-- agc_level = 0
```

The claim was that `%eax` still held `V34InitHilbertFilter`'s tail-called
`sysdep_memset` return.  It does not: the three registers are loaded, used
once each, and zeroed, and I traced that correctly for `%ecx` and `%edx`
while attributing `%eax` to the call.  The zeroing of `%eax` sits BEFORE the
stores rather than between them, which is the only thing that made it look
different from the other two.

Found by the `rxtiming` differential test, which drives `rxinit` from a
different starting state and disagreed by exactly this field.  The `rxinit`
test had not caught it because it *skipped* the byte and asserted the
Hilbert-address theory instead of comparing against the blob -- see finding
F122.  This is the second retracted deviation after D28, and both were filed
on a reading rather than a measurement.

---

## D35 ⚠ `setupreceiver` has a 2743-baud arm nothing can select

**Where:** `src/pump/v34/v34hshak.c`, `setupreceiver`'s symbol-rate switch.

**What the original does:** the switch has six arms — 2400, 2743, 2800, 3000,
3200 and 3429 — reading the rate from +0xaa96 and installing four timing
constants for it.  2743 baud is V.34's optional sixth symbol rate, and its
constants (0x36b0 for the increment, 0x1f40 for the phase) are as specific as
any of the others, so the arm is real code rather than a compiler artefact.

**Nothing writes 2743 into +0xaa96.**  `setfinalrate` is the function that
sets that field, and its own switch has five arms: 2400, 2800, 3000, 3200,
3429.  There is no rate code for 2743 on either side of it.

**What we do:** reproduce the arm, including its constants.

**Why it is not filed as a defect:** the field is global state, and
`v34handshak` and `probeselect` both write in that region; whether either can
put 2743 there has not been measured, so "unreachable" would be a claim this
project has not earned.  What is measured is that `setfinalrate` cannot.
`unmeasured` — task #47, or whenever `probeselect` lands.

**How it was found:** by writing the two functions in the same session and
noticing the switches did not have the same arms.

---

## D36 🐛 `preempindex`'s "index is 0" branch cannot be taken

**Where:** `src/pump/v34/v34hshak.c`, `preempindex`.

**What the original does:** the search counter starts at 5 and is incremented
at the TOP of the loop, before the multiply and before the limit test:

```
   60c30:  mov  $0x5,%ebx
   60c40:  lea  0x1(%ebx),%eax       ; i++
   60c43:  movswl %ax,%ebx
   60c46:  mov  %edx,%eax
   60c48:  imul %esi,%eax            ; x *= ratio
   60c4b:  sar  $0xe,%eax
   60c4e:  movswl %ax,%edx
   60c51:  cmp  %di,%dx
   60c54:  jg   60ccc                ; x > limit -> exit
   60c56:  cmp  $0x9,%bx
   60c5a:  jle  60c40
   ...
   60ccc:  cmp  $0x5,%bx             ; <- i is 6 or more, always
   60cd0:  je   60cf4                ;    so never taken
```

so `%bx` is at least 6 wherever the `cmp $0x5` can be reached.  The branch it
guards prints `V34PREEMPHASIS, - index is 0, baudrate= %d` and returns 0, and
neither can happen.

**What we do:** reproduce the branch, unreachable and all, so the control
flow matches and the third debug string keeps its call site.

**Consequence:** none — `preempindex` cannot return 0.  Its range is 6..10.

**Reading:** an earlier version of the loop very likely tested before
incrementing, which would have made `i == 5` mean "the first multiply already
passed the limit".  Moving the increment to the top changed the meaning of
every exit and the guard was not updated with it.

**How it was found:** by writing the loop out and asking what value `i` could
hold at each exit, which is the sort of question the three debug strings —
"index is 0", "index is %d", "index is 10" — make worth asking.

---

## D37 ⚠ `preempindex` reads two uninitialised registers for an unknown rate

**Where:** `src/pump/v34/v34hshak.c`, `preempindex`'s symbol-rate switch.

**What the original does:** five arms — 2400, 2800, 3000, 3200, 3429 — each
loading a starting measurement into `%edx` and a Q14 ratio into `%esi`.
**There is no default arm.**  For any other baud rate control falls straight
through to `mov $0x5,%ebx` and the loop runs on whatever the caller left in
those two registers: `%esi` is callee-saved and still holds the caller's
value, `%edx` is caller-saved and holds whatever was last in it.

**What we do:** zero both, which makes the loop run to its ceiling and return
10.  This is a **deliberate behavioural difference** and the only available
one: there is no value that reproduces "whatever the caller had", and leaving
a C variable uninitialised would be undefined behaviour rather than an
imitation of the original's.

**Where the boundary is:** exactly the five listed rates are identical; any
other argument differs.  The differential test sweeps the five and does not
sweep anything else, and says why.

**Reachability: unmeasured.**  Nothing in the object calls `preempindex` —
no relocation and no direct call — so there is no call site to check the
argument against.  Task #47.

**How it was found:** by reading the switch and noticing the fall-through
target was the loop rather than a default arm.

---

## D38 ⚠ `V34GiveProbeResults` moves its doubles as bytes, not through the x87

**Where:** `src/pump/v34/v34info.c`, `V34GiveProbeResults`.

**What the original does:** copies each of the 25 doubles with `fldl` /
`fstpl` — an x87 load and store.

```
   8c90:  fldl  (%edx)
   8c95:  fstpl 0xa258(%ecx,%eax,8)
```

**What we do:** copy the eight bytes.  The source stride is 44, which is not
a multiple of 8, so every other source double is only 4-byte aligned;
`fldl` does not mind that and a `double *` in C is not allowed to be that
lax, so the copy is written a byte at a time.

**Where the two differ:** `fld` QUIETS A SIGNALLING NaN — it raises the
invalid-operation exception and stores the quiet form — and a byte copy
carries the payload through unchanged.  For every finite value, every
infinity and every quiet NaN the two are identical, which is the whole
domain a probe measurement can occupy.

**Reachability: unmeasured.**  The source record is built by
`VPcmV34Main.cpp`'s C++ half, which is not reconstructed, so whether a
signalling NaN can reach it has not been established.  The differential test
drives finite values only.  Task #47.

---

## D39 🐛 `edprintf`'s length guard is one byte short of its buffer

**Where:** `src/core/encode.c`, `edprintf`.

**What the original does:** encodes into `cEncodedTemp`, which is 0x10e =
270 bytes, and guards the length with

```
   b0a27:  lea  0x8(%eax,%eax,1),%eax    ; 2 * strlen(temp) + 8
   b0a2b:  cmp  $0x10e,%eax
   b0a30:  jbe  b0a5b                    ; <= 270 is accepted
```

The 8 is the four-byte prefix `$!$ ` plus the four-byte suffix `????`, and
2 per source character is the encoding.  **It does not count the
terminator.**  At the boundary — `strlen(temp) == 131`, giving exactly 270 —
the prefix and 262 encoded characters end at index 266, `strcat` appends four
more to 269, and the NUL it writes goes to index 270.  One past the end.

**What is actually there.**  `cEncodedTemp.1` is at .bss+0xa00 and the next
object, `iEncodeOffset`, is at .bss+0xb10 — so bytes 0xb0e and 0xb0f are
alignment padding and the stray NUL lands in them.  The original overruns and
gets away with it.

**What we do:** size the array 271.  The guard is reproduced exactly, so
every accepted length is accepted and every refused one refused, and every
character of every string is at the same index; the only difference is that
the terminator has somewhere legal to go.  Writing one byte past a C array
is undefined however harmless it is in the object's own layout, and the
alternative — shrinking the guard by one — would change which messages get
encoded and which are replaced by "too long print string".

**Reachability: measured.**  It needs a formatted message of exactly 131
characters, which `vsnprintf`'s 0x100 cap admits.  `t_encode` drives
`strlen` from 128 to 135 and both sides agree throughout, so the boundary is
tested rather than avoided.  Whether any real caller emits exactly 131
characters is not measured; 145 functions call `edprintf` and none of them
is reconstructed.

---

## D40 ⚠ `dsplib_encode_plain`: a switch the original does not have

**Where:** `src/core/encode.c`, `edprintf`.

**What it is:** an added global, zero by default, that makes `edprintf`
print the readable message instead of the encoded one.  Finding F175 is why
it exists: 145 functions report through this channel and none of what they
say is legible, which makes the least-understood half of the object also the
half whose diagnostics are useless.

**This is an ADDED FEATURE, not a fix.**  The original is not wrong; it is
doing what it was built to do.  So it is not behind `DSPLIB_REPRODUCE_BUGS`,
which is for defects — and it does not need to be, for two reasons.

**One: with diagnostics off it does not execute.**  The switch is read only
inside the `dsplibs_debug_level > 1` gate, which is the last thing `edprintf`
does.  `dsplibs_debug_level` ships at zero, so on a working modem the branch
is never reached and the instruction stream is the object's.

**Two: with the switch ON, only the printed string changes.**  The encoding
still runs in full — `iEncodeOffset` is reset and advanced identically and
`cEncodedTemp` is filled identically — so nothing downstream can tell.  In
particular `cEncodeChar`, which shares that counter, returns the same
characters either way.  `t_encode` asserts exactly this: each message is run
with the switch off (compared against the blob) and again with it on, and the
key position is read back after both and must agree with the blob's.  A
version that short-circuited the encoder when plain mode was on — the obvious
way to write it — fails that check.

**One deliberate difference in plain mode.**  A message too long for the
encoded buffer is replaced by "too long print string" in the channel; plain
mode prints the message instead, because `temp` holds it and it is at most
255 characters.  So plain mode shows messages the encoded channel drops.

**The alternative that changes nothing at all** is `tools/eddecode.py`, which
decodes captured logs after the fact and is the only option for logs that
came from the original binary.

---

## D41 ✅ The descramblers' refill shift is masked to five bits

**Where:** `src/pump/v34/v34scram.c`, `descram_tail`.

**What the original does:** after flushing a word it puts the caller's bits
back at the top of the register with

```
   57e89:  sub  $0x10,%eax          ; count + nbits - 16
   57e8f:  sub  %edi,%ecx           ; ... - nbits, i.e. the ORIGINAL count - 16
   57e95:  shl  %cl,%ebp
```

and `shl %cl` masks the count to five bits on x86.  The subtraction is
negative whenever a caller passes more than sixteen bits against a register
holding fewer than sixteen — which is reachable for `nbits > 16`, since the
flush only needs `count + nbits > 31`.

**What we do:** mask with `& 31` explicitly.  On the 32-bit target the two
are the same instruction and the same result; in C, shifting by a negative
or over-wide count is undefined, and this reconstruction is meant to be
64-bit-clean, where a compiler is entitled to do something else with it.

**Bit-exact over the whole domain.**  Not a fix and not a behavioural
difference: it spells out what the hardware was already doing.  It is here
because a reader who removed the mask would not be able to tell from the
disassembly that anything had changed.

**Reachability of the negative case: unmeasured.**  Every caller is inside
`v34handshak`, which is not reconstructed.  The differential test drives
`nbits` from 1 to 16, where the shift cannot go negative.

---

## D42 ⚠ `StateName` is indexed with nothing bounding the index

**Where:** `src/pump/v34/v34hshak.c`, `hs_setstate`, and every one of
`v34handshak`'s 533 uses of the same table.

**What the original does:** loads `StateName[state]` with `mov
0x6c00(,%eax,4),%esi` after a plain `movswl` of the state word. There is no
comparison against 87 anywhere near any of the sites, in either function, and
the index is sign-extended from a `short` — so a negative state word indexes
*backwards* out of `.data`.

**What we do:** reproduce it. The alternative is a bounds check the object
does not have, which would change the control flow the debug level gates.

**Why it is not filed as harmful:** every store to the three state words in
this function is a literal in 0..86, and the sites are all gated on
`dsplibs_debug_level > 1`, which ships at zero. Reaching a bad index needs
`v34handshak` to have written one first, and that is 61 KB not yet
reconstructed. Compare finding F129, which is the same shape in
`demapFrame`. `unmeasured` — task #47, or #39–#45 when the writer lands.

**What it costs the test.** The transcript sweep in `t_v34hshak.c` stops at
86 for this reason: an out-of-range state word would have both sides read
past their own copy of the table, into different memory, which compares
nothing. So the sweep proves the 87 entries and says nothing about the
89th — which is the honest limit and not an oversight.

**How it was found:** by writing the sweep and having to choose its bound.

---

## D43 ⚠ `v34handshakinit`'s timer stride, 431,488, is not a round interval

**Where:** `src/pump/v34/v34hshak.c`, `v34handshakinit`'s opening block.

**What the original does:** four `int` fields reached through `obj + 4` —
+0x238, +0x23c, +0x244 and +0x248 of the object. A positive +0x248 is
subtracted from +0x238 and the difference kept if it is at most **95,999**
unsigned, otherwise the pair is reset to 0 and **-960,000**. +0x244 then
takes a copy of the base and +0x23c takes the base plus **431,488**.
`VPcmV34SetV90RateReneg` writes the same three fields with the same two
constants through the same `obj + 4` base, which is what says they are one
group.

**Which sample rate to test them against is not a guess.**
`docs/rate_assumptions.md` R-1 records that V.34/V.90/V.92 run at the HOST
rate directly, and the host rate is 9600; the 8 kHz retarget is the other
rate this tree will care about. Bell 103's 7200 (R-8) is that pump's FSK
core and has nothing to do with V.34. So the list is 9600 and 8000, and the
claim below is about those two and not about "any rate".

**Two of the three constants are round at both, and one is round at
neither.** 95,999 is one short of 96,000 — exactly 10 s at 9600, or 12 s at
8000 — and -960,000 is ten times that, so 100 s and 120 s. At 9600, the rate
V.34 actually runs at, the pair is a clean 10 s and 100 s. 431,488 is
44.947 s at 9600 and 53.936 s at 8000, and factors as 2^7 × 3371 with 3371
prime, so it is not a round number of samples at either, nor a power-of-two
fraction of one.

**What we do:** copy all three. No reading of the third is offered.

**Why it is recorded rather than solved:** the obvious move is to hunt for a
sample rate that makes 431,488 come out round and then assert that rate.
That is backwards — the rate is already fixed by R-1, and the two constants
that ARE round agree with it. Recorded so the next reader does not spend the
same hour on it, and so that a later finding can retract this entry the way
D28 was retracted. `unmeasured` — task #47.

**How it was found:** by pinning the constants before the states, as the
brief for `v34handshakinit` asked.
## D44 ⚠ `t3`'s tail stays at -1 for the three largest ring sizes

**Where:** `src/pump/v34/v34shell.c`, `initG248` and `initV34`.

**What the original does:** copies `xyz`'s block for the ring size into `t3`
and stops.  For sizes 15, 17 and 18 the block is SHORTER than the
8(n-1)+1 entries the sequence needs, because `xyz` stops where the
cumulative count would pass `INT_MAX` (finding F182) -- 69, 58 and 56
entries.  Everything above that keeps `preinitV34`'s fill of -1.

**What we do:** the same.

**Reachable?** The sizes are, and often: `MMaxTable` produces all three.
Whether `shellDemapper` then INDEXES into the -1 region is **unmeasured** --
it reads `t3[d1 + d2]` with nothing bounding the sum, which is finding F129.
On the TRANSMIT side this is settled and not a deviation at all:
`modulatevector` is now reconstructed and passing, its search over `t3`
compares unsigned, and `0xffffffff` is what stops it -- so the fill is a
working sentinel there, demonstrated rather than predicted.  The entry stays
open only for the receive side.  Re-open with `demapFrame` driven at ring
size 15 or above.

**Not fixed.** Extending the table would be invention, and on the transmit
side it would break a sentinel the object depends on.

---

## D45 ⚠ `scrambleGP*` ORs two fields that overlap

**Where:** `src/pump/v34/v34shell.c`.

**What the original does:** forms each new register word as
`(bitbuf << 2) | (scr[n] >> 16)` and `(bitbuf << 7) | (scr[n] >> 16)`.  The
shifted term keeps bits 2..31 (or 7..31) and the second is sixteen bits
wide, so the two OVERLAP in bits 2..15 and 7..15.  A concatenation, which is
what the shape suggests, would not.

**What we do:** the same OR, in the same order.

**Reachable?** The overlap is structural, so it happens on every call; what
is **unmeasured** is whether a set bit ever lands in both terms at once, and
therefore whether the OR is ever distinguishable from the concatenation the
code reads as.  Deciding it needs the bit ordering these functions use,
which is task #47's.

**Not fixed.** It is reproduced exactly and the differential test passes over
24 calls per generator, so if the overlap carries, it carries identically.

---

## D46 ⚠ `initG248` runs 2^32 times for a ring size of zero

**Where:** `src/pump/v34/v34shell.c`.

**What the original does:** computes `2 * (count - 1)` unsigned and uses it
as the second loop's upper bound.  A `count` of zero makes that
`0xfffffffe`, and the loop counts up to it.

**What we do:** the same, with unsigned arithmetic so the wrap is defined
rather than undefined.

**Reachable?** Not from `initV34`, which is the only thing that sets `count`
and takes it from `MMaxTable` or `MMinTable`, both of which bottom out at 1.
But `initG248` is a global symbol with NO caller in the object, so nothing
enforces the invariant at the boundary -- **unmeasured** for any future
caller.  The fixture drives 1..18 and asserts the range.

**Not fixed.** A guard the original does not have would be invention, and
the reachable range is provably safe.

---

## D47 ~~`fa16` is 24 for the 16-state code and 32 and 64 for the others~~ RETRACTED

**Filed** as an unexplained break in a pattern: +0xa16 is set alongside the
convolutional code pointer -- 24 with `Convolve16`, then 32 with
`Convolve32` and 64 with `Convolve64` -- and 24 is not 16, not `depth << 5`
for any depth, and not a rounding of either.  The entry said "nothing
reconstructed reads +0xa16 yet".

**Retracted:** it is not a state count, so there is no pattern to break.
`modulatevector` reads it, twice, and both readings say the same thing:

```
   state = (state ^ conv[idx] ^ ((state & 1) ? fa16 : 0)) >> 1
```

`fa16` is the convolutional encoder's FEEDBACK MASK -- the generator
polynomial, XORed in when the bit shifted out is set.  32 and 64 are single
bits because those two codes have one feedback tap; 24 is `0b11000` because
the 16-state code has two.  The value tracks the code because it IS the
code, and 24 is the only one of the three that shows it.

`modulatevector`'s second reading is a direct comparison, `fa16 == 64`,
which selects a hand-unrolled six-register form of the same recurrence over
`fa2c[0..5]` instead of the shift-and-mask loop.  Two spellings of one
encoder, and the 64-state one is the one worth unrolling.

This is the third retraction after D28 and D34, and like both of those it
was filed on a reading rather than a measurement -- here, on the absence of
a reader.  "Nothing reads it yet" is a statement about the reconstruction,
not about the object, and it expires the moment the next function lands.

## D48 ⚠ `VPcmV34SetV90RateReneg` winds `v90_receiver` backwards

**Where:** `src/pump/v34/v34pcmif.c`, `VPcmV34SetV90RateReneg`.

**What the original does:** assigns `+0x24c` unconditionally — 11 when
`rrn_type` is zero and 15 otherwise — with `cmp $1; sbb; and; add`, which is
the compiler's spelling of a two-way choice and not of a ratchet.

**Why that is worth an entry:** every other writer of that field advances it.
`V34XF_IndicateJdReceived` sets 3, `V34XF_IndicateDilReceived` 6, and
`V34XF_IndicateTrn2dReceived` is explicitly a four-rung ratchet that never
moves the value down — 10, 14, 18, 20. Its own diagnostic calls the field
`v90Receiver` and the ladder reads as progress through phase 3. A rate
renegotiation arriving after TRN2d has reached 18 or 20 therefore moves it
*back* to 15, and after Jd has reached 3 it moves it *up* to 11 or 15 without
the intervening messages having arrived.

**What we do:** reproduce the assignment. The reading that makes it sensible
is that a renegotiation genuinely restarts phase 3, so 11 and 15 are entry
points into the ladder rather than violations of it — but nothing in the
object says so, and `V34XF_IndicateTrn2dReceived` went to the trouble of a
ratchet, which is what makes the plain assignment worth recording.

**Reachability:** not measured. `unmeasured` — task #47.

**How it was found:** by reading the three request entry points together;
the field's own header comment in `v34fsk.h` describes a counter "the
handshake's C++ side ratchets forward", and this writer does not.

## D49 ⚠ `chkForceBaudRate`'s V.90 arm writes a local it then abandons

**Where:** `src/pump/v34/v34pcmif.c`, `chkForceBaudRate`.

**What the original does:** all three arms of the fork store into the local
six-byte array — `movb $0x1,0x25(%esp)` on the V.90 arm at 0xa4d1, `movb $0x0`
on the K56Flex arm, `movb $0x1` on the third — but only the last two go on to
use that array. The V.90 arm sets `edi` to `p3548 + 0x217` instead, so its
store at index 5 is never read.

**Why that is worth an entry:** the store is not a compiler artifact that can
be argued away. The other two arms write the same byte for a reason, and the
value the V.90 arm writes is the same one the "neither receiver" arm writes,
which is what a hoisted `allow[5] = !k56flex_receiver` would produce — so the
likeliest reading is that the source computed the flag before choosing the
array and the choice made it dead. That is a guess about the source, and what
the object states is the dead store.

**What we do:** reproduce it, with a comment saying it is dead. It is filed as
an equivalent mutation in `test/mutations/v34pcmif.json`: removing it survives
the whole-object comparison, which is the evidence that nothing observes it,
and the entry is what stops that survival reading as a gap in the tests.

**Reachability:** the arm is reached — the sweep in `t_v34pcmif.c` drives all
three — but the store's *effect* is unreachable by construction. `unmeasured`
in the sense the other entries mean it: nothing measures whether a real call
takes the V.90 arm.

**How it was found:** reading the three arms against each other while writing
the function; the mutation was added to check the reading and survived, which
is the confirmation.

## D50 ⚠ `VPcmV34InterpretMohMessageBits` matches five messages at 16 bits and two at 8

**Where:** `src/pump/v34/v34info.c`, `VPcmV34InterpretMohMessageBits`.

**What the original does:** loads the message's first short with `movzwl` and
then tests it two different ways. MHreq, the two MHnacks, MHcda and MHfrr are
`cmp $0x33,%ax` and friends — sixteen-bit equalities. MHack and MHcld are
`and $0xf0,%edx; cmp $0x50,%edx` — a mask that keeps four bits and discards
everything above bit 7 as well as the low nibble.

**Why that is worth an entry:** the two kinds of arm disagree about the high
byte. `0x0150` decodes as an MHack with a time-out code of 0 and `0x0133` does
not decode as an MHreq — it falls through to "Illegal MH message detected"
and is forced to MHnack. Nothing masks the value before the switch and nothing
bounds it, so which behaviour a caller gets depends on whether the message
type it received happens to be one of the masked pair.

**What we do:** reproduce both spellings exactly, and sweep all 65,536 first
shorts rather than the 256 a byte-wide reading would suggest. That sweep is
the only thing that distinguishes this from a version that masked once at the
top, which is what a reconstruction reading the arms in isolation would write.

**Reachability:** the message arrives from the far end through the V.34
handshake's receive path, and nothing between there and here narrows it to a
byte in anything reconstructed so far. Whether a real peer can send a first
short above 0xff is not settled. `unmeasured` — task #47.

**How it was found:** by writing the arms out and noticing that two of the
seven used `%edx` (the masked copy) where five used `%ax`.

## D51 ⚠ `settxlevel`'s two dB loops accumulate at different widths

**Where:** `src/pump/v34/v34hshak.c`, `settxlevel`.

**What the original does:** applies the power reduction one dB at a time, with
one loop per direction. Reducing:

```
   62651:  imul $0x390a,%esi,%esi     ; x 0.8912
   62659:  sar  $0xe,%esi             ; 32-bit throughout
```

Raising:

```
   626c2:  imul $0x47cf,%esi,%eax     ; x 1.1220
   626ce:  sar  $0xe,%eax
   626d4:  movswl %ax,%esi            ; TRUNCATED TO A SHORT
```

**Why that is worth an entry:** the loop that can make the accumulator grow is
the one that discards its high half, and the loop that only ever shrinks it
keeps thirty-two bits. A scale of 30000 with a reduction of −1 dB gives
20549 rather than 33660: the product `30000 * 0x47cf >> 14` is 33660, which
does not fit a signed short and wraps. The reduction is negative only when
`GetVPcmMinimalTxPowerReduction` returns a negative value and a V.90 receiver
is running, and the scale is whatever `VPcmV34SetTxScale` or a previous call
left — 0x16a1 from the former, which is 5793 and survives four dB of gain
before it wraps.

**What we do:** reproduce both widths, with the cast written out. The sweep in
`t_v34hshak.c` drives the starting scale to 0x7fff and 0x7ffe as well as to
0x16a1, so the wrap is a tested case rather than an inferred one.

**Reachability:** the arm is reachable — the sweep reaches it — but whether a
real session presents a scale large enough to wrap it is not measured.
`unmeasured` — task #47.

**How it was found:** by reading the two loops side by side while writing
them; the asymmetry is one instruction and neither loop is wrong on its own.

## D52 ⚠ `probeselect`'s two flat power requests also say they are not asking

**Where:** `src/pump/v34/v34hshak.c`, `probeselect`'s sensitive-ISP arms.

**What the original does:** the two arms that a very small `snr_l1` selects —
0x60fd3 asking for 7 dB and 0x613ba asking for 9 — each print "V34PROBE,
asking for a power reduction of %d" and then fall through into 0x61051, which
is the head of the arm that prints "V34PROBE, not asking for power reduction"
and "V34PROBE, rx->gain=%d ,(obj->rxinfo0.data[1]&0x80)=%d".

So a run at debug level 2 that has just asked the far end for 9 dB says, on
the next line, that it is not asking for anything. The ORDINARY request path
at 0x60f15 does not: it jumps past 0x61051 to the band-edge section, so only
the two flat arms contradict themselves.

**Why that is worth an entry:** the messages are the only externally visible
difference between the arms, and a reconstruction that read the fall-through
as "and then the not-asking arm" — rather than as a shared tail — would
produce the same object state and a different transcript. It is also a real
diagnostic defect: anyone reading the log for whether a reduction was
requested gets both answers.

**What we do:** reproduce it. Both flat arms `goto not_asking` and the
ordinary one does not.

**Reachability:** reached. `t_v34hshak.c`'s sweep drives `pcfg[0x50]` bit 4
and `snr_l1` across both thresholds, and the transcript comparison covers the
result at levels 2 and 3.

**How it was found:** by following the jump at 0x6103a, which goes to a label
whose first instruction is a debug gate rather than to the band-edge section
the ordinary path uses.

## D53 ⚠ `probeselect`'s "index is 0" is unreachable in all five rate arms

**Where:** `src/pump/v34/v34hshak.c`, `probe_preemph`.

**What the original does:** each rate's pre-emphasis search keeps its counter
in `%ebx`, preset to 5, and advances it BEFORE the comparison that can leave
the loop:

```
   6128a:  movswl %si,%ebx           ; ebx = ebx + 1
   6128d:  sar    $0xe,%edi
   61290:  movswl %di,%edx
   61293:  cmp    %cx,%dx
   61295:  jg     623a8              ; the early exit
```

so the counter is 6..10 at that exit and 10 at the other. Every arm's exit
block then begins `cmp $0x5,%bx; je <index 0>`, which cannot hold. The string
on that path, `V34PREEMPHASIS, - index is 0, baudrate= %d`, is one of three
and is the only one nothing can print.

**Why that is worth an entry:** it is the same shape as D31, which declared a
branch dead from the instructions around it and was wrong. This one is
recorded only after measurement, twice over.

The sweep in `t_v34hshak.c` runs four thousand probes and asserts that indices
6 through 10 were all returned and that 0 through 5 never were. If the reading
is wrong, that assertion fails.

**And the compiler agrees.** `gcov` over the instrumented tree reports the
`i == 5` test executed 6,938 times and its whole body as NOT EXECUTABLE:

```
    19587: 1686:		if (x > ref) {
     6938: 1687:			if (i == 5) {

**THE CONSEQUENCE, and it took the hardware bench to see it (finding F1471).**
This entry treated the dead branch as a curiosity about an unprintable string.
It is not. In V.34 the RECEIVER chooses the FAR TRANSMITTER's pre-emphasis
filter, and index 0 means flat. **A modem whose search cannot return anything
below 6 cannot ask for a flat line, ever, on any channel.** Every index this
bench has logged across every call is 6 (2445 times) or 7 (35 times).

On the same two FXS ports, minutes apart, a 1998 SupraExpress asks the same
Courier for filter 0 where we ask for 4 in the Courier's own units — and the
request arrives: our received copy of that transmitter runs 1.5 dB hotter at
2800-3400 Hz than the un-preemphasised reference.

**APPLIED, 2026-08-16, and it is now the default.** Shape matching over all
eleven templates replaces the counter in every build that does not define
`DSPLIB_REPRODUCE_BUGS` — so the differential tier still proves bit-exactness
against the blob, and the interop tier, the bench and anyone linking this
library for real get the fix. `probe_preemp_shape` in `v34hshak.c`.
`DSPLIB_V34_BLOB_PREEMP=1` forces the counter at runtime for A/B work; that is
a bench convenience and not part of the contract.

**WHAT CHANGED THE VERDICT.** The paragraph below said not to apply this
without measuring, and that was right. What has since been measured:

  * given a channel that is the exact inverse of template i, the selector
    answers i — 11/11 at all five symbol rates. The counter manages 2–3 of 11
    (finding F1961).
  * on the bench's OWN ATA, the correct answer is index 0 at every symbol rate
    below 3429, because the conformance band stops short of the 3450 Hz cliff
    and the line is flat. The counter asks for 6 or 7 and so ADDS 1.5–3 dB of
    tilt to a flat channel (1961).
  * with 6 dB of tilt in the emulator the selector reaches a Table 3 index the
    counter cannot, and the equaliser's off-centre tap energy falls 16% across
    three seeds (1961).
  * a regression test pins identity, noise robustness and interferer rejection
    (1962), and caught a real defect — an unrejected outlier moved the fit
    eight indices — before it reached the bench.

**STILL NOT MEASURED: a rate improvement against real hardware.** On this bench
the whole effect is ~0.13 dB (1956) because the path is flat and negotiates
3429, the one rate where the counter is near-optimal. The fix is applied
because rejecting five of eleven filters is a defect and the evidence above
shows the replacement is correct and robust — not because a bench call got
faster. It has not.

**SUPERSEDED PROPOSAL, kept for the reasoning.** Make index 0
reachable, so a channel that needs no pre-emphasis is told so. It belongs with
the 8000 samp/s work and the floating-point defects on the list of things the
reconstruction may deliberately do better, and it obeys the same rule as all
of them: behind `DSPLIB_REPRODUCE_BUGS`, off by default, differential tier
unchanged and still bit-exact against the blob.

**DO NOT APPLY IT ON THE STRENGTH OF THIS ARGUMENT ALONE.** That the request
is wrong is measured. That fixing it improves the connection is NOT: the gap
to explain is equerr ~2700 against a threshold of 205, and 1.5 dB of tilt is
unlikely to be all of it. The experiment is #89 -- our datapump in slmodemd,
the same call made twice differing in one branch, on real hardware. Applying
a fix whose benefit has not been measured is how a reconstruction acquires
behaviour nobody can justify later.
        -: 1688:				if (DSPLIB_DEBUG_ON())
        -: 1689:					dsplibs_debug_printf(
        -: 1690:					    "V34PREEMPHASIS, - index is 0, "
```

GCC proved the branch unsatisfiable and folded the body away, which is the
same conclusion reached from the object's instruction order and reached
mechanically. Note the consequence for the other checks: the site does not
appear in `debugcov`'s dead-site list, because gcov marks it non-executable
rather than executed-zero-times, so that count is NOT evidence here either
way.

**What we do:** reproduce the branch, because the object has it five times
over and removing it would be a claim about the compiler rather than about
the code.

**Reachability:** the arms are reached — all five, per the sweep's counters —
and this branch inside them is not.

**How it was found:** by transcribing the loop and noticing that the
increment sits above the exit test rather than below it.

## D54 ⚠ `probeselect` finds the minimum shift signed and keeps it unsigned

**Where:** `src/pump/v34/v34hshak.c`, `probeselect`'s band normalisation.

**What the original does:**

```
   61076:  movzwl 0xe(%edi,%eax,4),%ecx
   6107b:  movswl %cx,%ebp           ; SIGNED for the comparison
   6107e:  cmp    %ebx,%ebp
   61080:  jge    61085
   61082:  movzwl %cx,%ebx           ; UNSIGNED for the assignment
```

so a bin whose `shift` is negative wins the comparison and is then stored as a
number above 32767. Every bin is subsequently reduced by that, which with
16-bit wrap-around adds rather than subtracts.

**Why that is worth an entry:** the normalisation is supposed to leave each
bin's level relative to the quietest, and one negative shift inverts it — the
whole ladder below then reads a band that looks nothing like the one measured.
`dftenergy` writes `shift`, and finding F212 established that the neighbouring
`energy` genuinely does go negative from a seeded accumulator, so "a negative
shift cannot happen" is not something this reconstruction can assert.

**What we do:** reproduce both widths. The sweep leaves one shift in
thirty-two wide open rather than narrowed towards zero, so negative ones occur
and the comparison is tested.

**Reachability:** reached in the sweep. Whether a real probe produces a
negative `shift` is not measured — that is a question about `dftenergy`'s
inputs. `unmeasured` — task #47.

**How it was found:** by the two different widening instructions on the two
sides of one comparison, which is the same thing finding F212 records for the
DFT bin's thresholds.

## D55 `FloatIIR` reports nothing when its history allocation fails

`FloatIIR::FloatIIR` calls `sysdep_malloc` for the history buffer and, on
failure, completes anyway: `m_hist` stays null, `m_pos` is still set, and the
constructor returns. The first `process` then dereferences null.

`reset` and the destructor both guard on `m_hist`, so the author knew it could
be null; only `process` does not. Reproduced, not fixed. `unmeasured` --
whether an allocation of a few hundred bytes ever fails here is a question
about the caller, not about this class.

## D56 `~FloatIIR` frees the history without clearing the pointer

`m_hist` is passed to `sysdep_free` and left as it was, so a second
destruction double-frees. Harmless in the original's usage, which constructs
these once, and reproduced. `unmeasured`.

## D57 `FloatIIR`'s history compaction underflows when there are no taps

The compaction loop is a do-while whose counter starts at `m_ncoeff - 1` with
no entry guard, so `m_ncoeff == 0` gives `-1` as unsigned and 2^32 iterations
walking backwards out of the buffer.

`m_ncoeff` is `ncoeff & ~3`, so this needs a filter asked for fewer than four
taps -- degenerate, but reachable through the constructor and through
`setCoefficients`. Reproduced as written; `t_floatiir` deliberately does not
drive it, and says so, rather than hanging the suite to prove a point.

## D58 `FloatIIR`'s two accumulators are not observably two

NOT A DEFECT -- an equivalent-mutant record, filed here because the tree has
nowhere better for "this looks like it must matter and does not".

`FloatIIR::process` splits its four-way unrolled dot product between two
accumulators and adds them at the end. Floating-point addition is not
associative, so a single-accumulator build should differ. It does not: that
mutant passes all 289,028 checks in `t_floatiir`, including 262,144 samples
through poles near the unit circle with inputs spanning two decades.

Sixteen products of similar magnitude sum inside the x87's 64-bit significand
without rounding, so both orders are exact and the single rounding to float at
the end sees the same number. A standalone experiment did separate them, 3
samples in 200,000, but only by letting two independent filters diverge with
input magnitude growing without bound -- not this filter.

**What was held fixed**, since an equivalent-mutant claim is worthless
without it: bounded input, sixteen or fewer taps, coefficients of similar
magnitude. A caller that violates any of those could see the difference, and
the reconstruction is written the original's way regardless.

## D59 ⚠ `v34handshak`'s per-sample dispatch does not terminate on an unhandled state

`unmeasured` for reachability in a working modem.

The transmit dispatch at 0x62966 is inside a loop whose test is the block its
own default arm lands in (0x629e0), and the default advances nothing. So with
the cursor at +0x221c below the limit at +0x2aa0 and a txstate outside the
twenty-five the table has a body for, the function spins forever.

Fifty-seven of the table's eighty-two entries are that default. Nothing in
the object appears to put the transmit machine into one of them with the
cursor low, so this is not believed to be reachable in service; it is
trivially reachable from a test that writes the state word, which is why
`test/harness/v34hsstep.c` arms a `SIGALRM` around every step. Finding F287.

## D60 ~~⚠ The per-sample transmit loop's result is not a function of the object~~ RETRACTED

`unmeasured` for what it actually reads.

Stepping one sample of the per-sample transmit loop from two objects holding
identical bytes at different addresses leaves them differing in the modulator
at +0x2078..+0x25d1, for three of the nineteen txstates that have a body --
and WHICH three changes when code that runs after the step is edited.

Ruled out by experiment: our bring-up versus the blob's, 64 KB of scrubbed
stack, a shared shaping buffer, past-the-end reads of the fixture's seed
tables, and a short `preemp0`. Finding F289 has the detail. Whatever is left
is read by the object and is not in the object, so the loop's output is not
reproducible from its input alone.

This blocks differential testing of table 1, which is #56. It does not affect
table 2 or the microstate table.

### RETRACTED -- it was a function of the object, and of where the fixture put it

`test/harness/v34hsstep.c` gave side A a `struct v34_object` and side B an
`unsigned char[]`, and the five blocks each object points at five more pairs
of statics, all at addresses the linker chose. Two sides identical in every
byte and in nothing else. One arena per side -- object and blocks at fixed
offsets in a 64 KB-aligned block, side B's a byte copy of side A's whole
arena -- and all nineteen of table 1's reachable targets compare, over
twenty-four object fills, ten placements, five object skews and eight
neighbourhoods, against 23 of those 24 fills failing on the old fixture.
Bisected: it is the placement of the five BLOCKS, not of the object.
Findings F319 and F322.

The two facts this entry rested on were both correct. "Which txstates
diverge moves when code that runs after the step is edited" is exactly what a
layout dependence looks like when the layout is the linker's, and it was read
as evidence about the object. This is the fifth retraction after D28, D34,
D47 and D31, and the first where the defect was in the fixture that was
looking for it.

What is left of it is D61, which is much smaller.


## D61 ⚠ What the per-sample transmit loop reads outside the object has not been named

`unmeasured` -- the sensitivity is gone, the datum is not identified.

D60 is retracted: table 1 compares once the two sides have congruent memory
images (finding F319). What that does not do is say what the old layout was
feeding the loop. The bounds measured while closing it (finding F322):

- not within 32 KB of any block in either direction -- `V34HS_PADVARY` makes
  each padding region differ between the sides and the sweep still agrees;
- not an out-of-bounds WRITE either -- the step writes zero padding bytes on
  all 43 cases;
- not the object-to-block geometry, the absolute address, the alignment, the
  object's own placement or the object's contents;
- and not any one block's neighbourhood -- wrapping `shaped`, `pcm`, `cfg` or
  `dummy` alone in the old fixture leaves every fill failing. It takes all
  five together.

**`sess` is the thread to pull.** It is the one single-block wrap that moved
anything: five of eight fills failing where the other four left all eight.
That is not a fix and it is not proof of anything on its own -- moving one
block moves the layout, and this defect moves with the layout -- but it is the
only signal in the bisect pointing at a particular block, `sess` is the
largest of the five at 0x6200, and it is the one the object reaches through
+0x3548 on the way to a PCM receiver and the configuration at +0xac3c. Anyone
who takes this further should start there.

So the half of the fixture responsible is named -- the placement of the five
blocks the object points at -- and the datum it was feeding the loop is not.
This does not block #56: the route compares, and every one of table 1's
targets is testable through it. It is filed so that a case which starts
disagreeing again is looked for here rather than in the reconstruction.

## D62 ⚠ Three self-allocating constructors check a `sysdep_malloc` the original does not

**Modules** `src/dsp/fpm_mtd.c` (`FPM_MTD_create`), `src/dsp/fpm_tone.c`
(`FPM_TONE_create`), `src/pump/b103/b103fp.c` (`B103FP_create`)

The first **added hardening** entries in this register, found by auditing for
them rather than by a test failing — which is the point, because a check the
original does not have only diverges on an input the differential tier cannot
drive.

All three follow the library's "pass NULL and I will allocate" convention, and
all three of ours do:

```c
if (state == NULL) {
	state = (struct fpm_mtd *)sysdep_malloc(sizeof(*state));
	if (state == NULL)
		return NULL;		/* <-- ours; not the original's */
	owned = 1;
}
```

**The original does not test the result, and the disassembly is unambiguous
about it.** Each of the three tests its object parameter at the top of the
function, and the self-allocating branch rejoins the flow *after* that test:

| function | tests the parameter at | allocation branch rejoins at | what it skips |
|---|---|---|---|
| `FPM_MTD_create` | `+0x0f` `test %ebx,%ebx` | `+0x17` | its own null test |
| `FPM_TONE_create` | `+0x1d` `test %esi,%esi` | `+0x25` | its own null test |
| `B103FP_create` | `+0x18` `test %esi,%esi` | `+0x20` | its own null test |

`B103FP_create` settles it beyond argument: at `+0x827` it writes
`movl $0x0,0x50(%eax)` and `movl $0x0,0x54(%eax)` **through the malloc result,
before rejoining at all**. Those two stores are the original's and are
reproduced; the null test that would have to precede them is not there.

**Why this is hardening and not a fix.** The input is `sysdep_malloc` failing,
and the original's behaviour on it is a null dereference. There is nothing to
reproduce — a differential test cannot compare against a fault — so this is
not behind `DSPLIB_REPRODUCE_BUGS`, per this file's own taxonomy. Callers that
supply their own object never reach the branch at all.

**Scope of the audit that found these, so the next reader knows what was NOT
checked.** Every `sysdep_malloc`/`sysdep_calloc` site in `src/` was enumerated
— 59 of them — and the twelve whose result our source tests were compared
against the original function. Eight are the original's own checks and are
correct. `LowPassFIR::design`'s four-argument form checks its own window
allocation and that is also the original's, proved by a mutation. These three
are the exceptions. Separately, all 33 null-ish guards outside
create/init/delete were examined: most are integer value tests rather than
pointer guards (`FPM_sqrt`'s `x == 0`, `FPM_div`'s `denom == 0`), and every one
that really is a pointer guard — the six in `dialer.c` and `pulse.c`, and
`V90Phase3Modulator::resetDILGenerator` — is the original's, each with a
matching `test`/`je` in the first dozen instructions.

`unmeasured` — whether an allocation of this size fails in service is a
question about the host, not about these three functions.


## D63 ⚠ `Resampler::resample` forces two roundings the compiler will not emit

**Module** `src/pump/v90/Resampler.cpp` · original `VPcmV34Main.cpp`,
`_ZN9Resampler8resampleEPKfjPfRj`, `.text 0x034da0`

**Bit-exact, different structure.** The function computes two polyphase inner
products and interpolates between them. GCC 3.4 ran out of x87 registers
across the second and spilled the first sum to a `float` stack slot --
`fstps 0x34(%esp)` at `.text+0x34f76`, and `fstps`/`flds 0x30(%esp)` at
`+0x34fee`/`+0x34ff6` for the second. **A spill to a `float` slot rounds.**

The tree compiles `-mfpmath=387` with `-fexcess-precision=fast` and
deliberately no `-ffloat-store`, and a modern GCC keeps both sums at 80 bits
right through the interpolation. That is not below the noise floor: rounding
`y0` before `y0 + (y1 - y0) * frac` moves the stored output sample by up to
one ULP, and the differential tier compares bit patterns.

**What is written instead.** A `volatile float` staging function, `round32`,
applied at exactly the two points the object spills and nowhere else. The
accumulation inside each loop stays at 80 bits on both sides, and so does
everything after the two roundings.

**Why not a source spelling.** Declaring the sums `float` does not restore the
rounding, and neither does an inlined `float`-returning helper; both were
tried and neither emits a store. The rounding is a property of GCC 3.4's
register allocator rather than of the source, so no spelling of the source
recovers it and "act on what the compiler was FORCED to encode" does not
reach it. The alternative was a tolerance on `out[]`, which this tree does
not do.

**Domain and evidence.** Exact, not approximate: 15,491 `t_resampler.cpp`
checks over eight resampling ratios -- 1:1, 4:1 up, 2.5:1 down, one-sample
calls, a short ring that wraps, a run started inside the look-ahead arm and
one whose credit exceeds the input on every pass -- agree with the blob bit
for bit on every output sample, on `nOut` and on the whole object after each
call.

**Not general.** `ResamplerTiming::timingCorrection`, forty lines away, wants
the opposite treatment: it has no spill to a stack slot anywhere, every
intermediate is used again from the 80-bit register AFTER its member store has
rounded it, and `-ffloat-store` would break it. That is why the forcing is
per-expression here and not a flag on the file.


## D64 🐛 `Resampler::resample` reads one sample past the end of its input

**Module** `src/pump/v90/Resampler.cpp` · original `VPcmV34Main.cpp`,
`_ZN9Resampler8resampleEPKfjPfRj`, `.text 0x034da0`

**Defect in the original, reproduced deliberately.** The function ends, on
EVERY return path, with

    350aa   mov  (%ebx),%esi          ; *in
    350af   mov  %esi,0x14(%edi,%edx,4)   ; pending[pendingCount++]

and `in` has already been advanced past the last sample the loop consumed. So
when a call consumes all `n` inputs -- which is the common case at a 1:1 or
downsampling ratio -- the sample it carries into `pending` is `in[n]`, one past
the caller's buffer. The look-ahead arm reaches the same element by a second
route: `history[historyIndex] = *in` at `.text+0x34f8c`, taken whenever the
upper interpolation branch would be branch `phases`.

**Not fixed, and not behind `DSPLIB_REPRODUCE_BUGS`.** The value is carried
into the resampler's state and reaches the next call's output, so a "fix" would
have to invent a replacement sample and every output after it would diverge
from the blob. There is no defensible substitute: the original's behaviour here
IS the filter's state.

**What a caller must do.** Pad the input buffer by one element and initialise
it. `test/unit/t_resampler.cpp` allocates `rin[RS_IN + 1]` and fills the pad
for exactly this reason; without it the two sides disagree about uninitialised
memory rather than about the resampler, which reads as a reconstruction error
and is not one. `V90Equalizer` and everything else in
`dp_vpcm_init`'s closure that drives a resampler inherits this requirement.

**Evidence it is the original's and not a transcription slip.** The store is
outside the loop and unconditional in the object; both return paths
(`.text+0x350ba` and `+0x351ae`) fall through it, and `pendingCount` is
incremented without a bound check even though `pending` is five elements. With
`n >= 1` the queue never holds more than one sample, so the array is not
overrun -- only the input is.


---

# Appendix A — is each entry MEASURED, or is it an assertion?

*Scope: **D1–D64**, the entries that existed when this appendix was
written. D70 onwards arrived with the task #98 sweep and are audited in
Appendix B, on reachability rather than on measurement.*

*Task #87. Appended rather than written into the 64 entries above, because
this file is shared with other sessions and an append does not conflict where
64 in-place edits would — finding F622's "Owed" paragraph. The mechanical half
regenerates with `tools/debugcov.py --deviations --no-build`; the judgement
below is not mechanical and does not.*

Every entry above is a CLAIM about behaviour. `docs/fastpass.md` deferred the
follow-up — is the claim measured, or has nobody driven it — and this is it.

## What the two instruments can and cannot say

`tools/devaudit.py` answers a NECESSARY condition: does a compiled test object
reference the `ref_` alias of the function the entry names? A "no" is
conclusive; a "yes" only says the function is linked, not that the deviant
ARM fired.

`tools/debugcov.py --deviations` is the stronger instrument added here. It
takes the `D<n>` references `src/` already carries, keeps only those in a file
the entry itself names, and reports gcov's verdict on the enclosing function:
every line and arc covered, or dead code, or **compiler-folded**. That last
one is the only place the pass turns a necessary condition into a sufficient
one, and it is what settles D36 and D53.

**Neither instrument can see a missing-guard defect.** D5, D8, D18 and D62 are
all "the original does not test something", and an absent branch has no arc to
be untaken and no line to be dead. Those four are measurable only by counting
allocations, which is what `harness_alloc` exists for. This is why D5 read as
covered under both instruments and was in fact a gap.

## `unmeasured` is used above in two different senses

Twenty-two entries carry the word, and separating them is most of the work.

- **Nothing drives this path** -- the sense this appendix is about, and the
  only one a test can close. D5, D40 and D56 were this.
- **Nothing establishes whether a real session gets here**, or what a constant
  means. D43's 431,488, D48's ladder, D50's high byte, D51's wrap and D54's
  negative shift are all of this kind: the arm IS driven and compared against
  the blob, and what is open is a question about callers that are not
  reconstructed, or about the original author's intent. No test in this tree
  can close those, and writing one would not be progress.

An entry in the second sense is MEASURED for the purpose of this audit and
still correctly says `unmeasured` in its own text. The two are not in conflict,
and reading them as one is how a register of 64 claims turns into a to-do list
of 22 that nobody should do.

## The classification

**MEASURED — 33.** A named test drives the deviant path, not merely the
function containing it.

> D1 D3 D4 D8 D11 D12 D13 D14 D15 D16 D17 D19 D22 D24 D25 D26 D29 D30 D32 D35
> D36 D39 D43 D48 D49 D50 D51 D52 D53 D54 D58 D63 D64

Three of those deserve naming because the evidence is unusual. **D36** and
**D53** are confirmed by the COMPILER: gcov emits no code at all for the body
each entry calls unreachable, and `debugcov.py`'s `folded` classification
finds both mechanically. **D49** is measured by an equivalent mutation that
survives — `test/mutations/v34pcmif.json` — which is the correct instrument
for a dead store, since nothing observable changes by construction.

**UNMEASURABLE — 16.** The deviant path cannot be compared against the blob,
and no test should be written for it. Four distinct reasons:

- *The original has no behaviour to compare against.* D2 (above Q15 the
  original reads whatever `.rodata` holds), D18, D20, D33, D55, D62 — the
  distinguishing input segfaults or is undefined on the reference side.
- *A comparison of a hang.* D9, D46, D57, D59 — driving these makes one or
  both sides loop 2^32 times or spin for ever. `test/harness/v34hsstep.c`
  already arms `SIGALRM` for D59's sake.
- *Ambiguous alias or internal table* — finding F622's two blind spots. D6:
  `ref_AGC_DEF_ALPHA` exists at six addresses because the name is file-static
  in six translation units, so a test declaring it binds to whichever the
  linker picks; measured by hand out of the object instead, and the absence of
  an automated guard is correct. D42: the in-range case is thoroughly driven
  by the transcript sweep with no test naming `ref_StateName`, and the
  out-of-range case has the two sides reading different memory.
- *Out of domain, or blocked on unreconstructed code.* D27 (a single wrap
  provably suffices for every `lag` the delay line can answer), D37, D44
  (receive side needs `demapFrame`), D61.

**UNMEASURED BUT DRIVABLE — 5 open, 3 closed by this task.**

Closed: **D5**, **D40** and **D56** — see the findings.

Open, with what each needs:

| entry | the path nothing drives | what it needs |
|---|---|---|
| D21 | the two V.23 receivers diverging at a carrier-loss limit of 65536 or more | `t_v23rx` pins the truncating path at 200 ms, far below the boundary. Both sides are deterministic at `0x10000`; a new differential case at that limit measures it. |
| D23 | `V23ModemMain` with `rx_count != tx_count` | a padded transmit buffer, D64's precedent — otherwise the two sides disagree about memory past the end rather than about the function. |
| D38 | a signalling NaN through `V34GiveProbeResults` | the ASSERT-THE-DIVERGENCE shape, which only D3 uses: `fld` quiets an sNaN and a byte copy does not, so a test here must assert the two sides differ. A different contract from the rest of the suite, and the reason this is recorded rather than written. |
| D41 | `descram_tail`'s refill shift going negative | needs `nbits > 16` against a register holding fewer than 16 bits. The sweep drives 1..16, where it cannot. |
| D45 | whether a set bit ever lands in both terms of `scrambleGP*`'s OR | not a new fixture at all — a property computable over the calls the existing sweep already makes. The cheapest of the five. |

**RETRACTED — 7.** D7 D10 D28 D31 D34 D47 D60. Not claims about current
behaviour; kept because the route to a wrong claim is worth as much as a right
one.

33 + 16 + 8 + 7 = 64, each entry in exactly one row. D40 is counted as a GAP
and not as measured: `t_encode` drove its general claim on the ACCEPTED path,
and the one arm nothing drove is the interaction -- the too-long path WITH the
switch on. That arm is not a corner of the claim, it is the only place the
claim could have been false, and D40's own safety argument does not reach it
(finding F1066). An entry is measured when its own claim is driven, not when
its function is.

## One entry whose own expiry condition has now fired

**D35** says of the 2743-baud arm that "whether either can put 2743 there has
not been measured", and names its own trigger: "`unmeasured` — task #47, or
whenever `probeselect` lands". `probeselect` has landed. All three writers of
`+0xaa96` now exist in `src/pump/v34/v34hshak.c`, and none can write 2743 —
see finding F1063. The entry is no longer resting on the absence of a writer.


---

# Part II — the defect sweep of `docs/findings.md` (task #98)

Everything that was in the deleted `docs/fixlist.md`, folded in here and
given register numbers.

**What was swept.** Every line of `docs/findings.md`: 37,637 lines, 721
numbered finding headings over 717 distinct numbers — only 1, 2, 3 and 4
collide, and those four are sub-headings inside finding F39, so any citation of
5 or above is unambiguous. The three defects that had reached the fix list
before this were found by grepping for "past the end", "overrun", "out of
bounds" and "no bounds check"; most of what follows uses none of those words.

**Numbering.** This block starts at D70 and the five numbers between it and
D64 are deliberately left unused. Five other sessions were in flight in
sibling worktrees when it was written and none had claimed past D64; the gap
is there so that a concurrent claim does not collide. (Written without
spelling those five numbers out, because `refcheck.py` reads a bare `D` and
digits as a citation and would call each of them dangling — the same trap that
made `make phase` red before this task started.)

**92 entries, D70–D161**, by reachability:

| grade | entries | what it means for a fix |
|---|---|---|
| FIRES TODAY | 14 | host-side or opt-in extension, and worth doing |
| NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES | 26 | host-side validation, usually |
| LATENT | 31 | documentation only until a path is found |
| CANNOT FIRE | 21 | documentation only, for ever |

Where an entry restates a defect D1–D64 already covers, it says so and defers
to the older entry for the mechanism: the new number carries the reachability
and the fix class, not a second account of the bug.

---

## D70 🐛 `selectFilter`'s ISDN and PBX arms do not clamp the row

*Task #98 sweep, from fix list §0.2. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED.*

**Finding F234** (`docs/findings.md:13472`). Both arms take the row straight
out of the registry and hand it to a bank with no bound applied, so a registry
value of 100 **reads 80 bytes past the end of a 2,480-byte bank**. Reproduced
literally in the reconstruction's tests.

A third unclamped path exists — the automatic arm skips its clamp when no
reference loop was selected — but it is unreachable through `selectFilter`,
which writes 0 to `refLoop` first. `getV90Capability` calls `autoSelection`
without that store and is presumably how it was meant to be reached, so the
path may become live when that function is reconstructed.

*Status:* reproduced faithfully. Host-side mitigation is to validate the
registry value before it reaches the object. Nothing does that today.

**Assessed as a blob-side fix and DELIBERATELY NOT DONE** (task #99,
`docs/blobfix.md`, finding F1203). The three banks are `GLOBAL`, so the
override mechanism that repaired D1 and D4 would *reach* them — and that is
the trap, not the opportunity. This defect is a missing DECISION, not a
missing VALUE: there are no correct coefficients for row 100, so a longer bank
could only be filled with invention, and replacing one bank symbol also
destroys the adjacency finding F235 records the reconstruction's own tests
having to reason around (`preFilterCoefType1` ends at 0x15b0, `Type2` begins
at 0x15c0). An override here would change behaviour on exactly the input in
question, to a value we made up. Mechanism B — a clamp, patched into the two
sites — is the only object-side form that fits, and the host-side mitigation
named above is still better than either, because it also covers the
`getV90Capability` path `selectFilter` cannot reach.

---

## D71 🐛 a one-sample overrun in the receive path becomes a wild pointer within six more

*Task #98 sweep, from fix list §0.3. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED.*

**Finding F123** (`docs/findings.md:7157`). The receive buffer holds fourteen
shorts. The fifteenth pull overwrites `f128` while `rxtiming` is looping on it,
which lets the loop run longer, which pulls more, which reaches `rx_samples` at
the nineteenth and corrupts the cursor's low half; the twentieth dereferences
whatever that made.

This is the object's own layout, so the original has the same bound. It holds
because `f1ae < f1b0` keeps pulls to at most one per output in normal
operation, two only while the timing loop is slewing. **It is a constraint on
the caller, not a bug in the callee** — but it is one nobody has written down
anywhere the caller can see, and the margin is six samples.

*Status:* the constraint is checked against `receiver` (task #48). Worth an
assertion in the reconstruction's debug build so a future caller cannot violate
it silently.

---

## D72 🐛 echo canceller history buffer overrun is unreachable at any real IODELAY

*Task #98 sweep, from fix list §1. **Reachability: CANNOT FIRE.** Status:
CONFIRMED. Fix class: documentation only. Settled by task #97; the five numbers
and the bound are finding F1188.*

**Where** `V92EchoCanceller`, blob 0x111e0 (ctor), 0x10f90 (`resetEchoHistory`),
0x10af0 (`setEchoDelay`).

**The mechanism (CONFIRMED present).** The history buffer at `+0x24` is
allocated **once, in the constructor**, with a length `N` at `+0x1c`:

```
    delay0 = params[+0x70]  V92_ECHO_INITIAL_DELAY = 840
    L      = params[+0x6c] & ~3   V92_ECHO_FILTER_LENGTH -> +0x14 = 180
    N      = (L-1 + delay0) + 2*blk + floor((L-1 + delay0)/blk)*blk + extra
    +0x24  = malloc(N * 4)   floats
```

with the constructor's 3rd/4th arguments `blk = 40` and `extra = 199`, read at
the two `VPcmFloModem` call sites (finding F1188).  `setEchoDelay` later changes
the delay and tap count **without reallocating**, and `resetEchoHistory`
(verified) zeroes `echoLength` floats of `+0x24` with the loop bound compared
only to the index, never to `N`:

```
    echoLength = (L >> 1) + echoDelay + params[+0x74] = echoDelay + 76
    for (i = 0; i < echoLength; i++) +0x24[i] = 0.0f;
```

So the unclamped loop is real: D72's mechanism stands.

**Why it CANNOT FIRE.** `N >= 2239` for any `blk >= 1`
(`N = 2237 + 2*blk - (1019 mod blk)`; shipped `blk = 40` gives `N = 2298`).
`echoLength = echoDelay + 76` is invariant across both writers, and echoDelay
is **largest at construction** (delay0 = 840 -> echoLength = 916) and only
falls thereafter.  `setEchoDelay` is driven from `MDMPRM_IODELAY` via
`echo_delay = IODELAY + 60`, so at slmodemd's validated cap IODELAY = 240,
echoDelay = 300 -> echoLength = 376.  Max echoLength over all reachable states
is 916 < 2239 <= N.  Overrunning needs echoDelay > N-76, i.e. IODELAY > ~2103 --
about 9x above the validated 0..240 range (D74), and the buffer stays
over-provisioned even with no host cap.

**What this corrects.** The earlier "why it matters now" tied the V.32 cliff
(failure at echo_delay 240/300, D74) to this overrun; **refuted** -- the
overrun is nowhere near the reachable band, so the V.32 cliff has another
cause.  The hoped-for pincer does not exist: the usable band is bounded below
by D77's connect race (IODELAY 86) and is **not** bounded above by this
mechanism.

**Fix class: documentation only.** A CONFIRMED defect that CANNOT FIRE is
documentation. The opt-in "size the history buffer for the transport's delay"
idea is still worth having -- it is what would let the echo canceller cover a
SIP jitter buffer, which the object was never built for (its whole reachable
delay range is a hybrid's echo, ~11-31 ms) -- but that is a capability
addition, not a bug fix: nothing on the faithful path triggers the overrun.

**The SIP echo has since been measured, and it is out of reach of BOTH
cancellers.** Finding F1205: over the bench's SIP path the reflection arrives at
205.62 ms, while `V34EchoFilter`'s delay line holds 172.5 ms (D27) -- so no
`lag` reaches it, and the sizing question above is a structure change rather
than a setting. Note the arithmetic on THIS page is `V92EchoCanceller`'s and
does not carry over: the V.34 datapump shares neither the structure nor the
`echo_delay = IODELAY + 60` mapping.

## D73 🐛 `DP_V32BIS` (132) never connects

**MISFILED, per `docs/deviation-triage.md` family 10 — the object cannot
distinguish 32 from 132.** `dp_v32_init` (0x4bb3-0x4bd8) registers id 0x20 and
id 0x84 against the SAME `struct dp_operations` at `.data+0x48`; `v32_create`
(0x4560) stores the `dp_id` argument at `dp+0` (0x45cc) and never reads it
again, there is no `cmp $0x84` anywhere in 0x4560-0x4bb0, and the 14400 ceiling
at 0x45ba is unconditional. The bench symptom may be real; the object is not
where it comes from. Test: call `ref_v32_create` with 32 and with 132 and diff
the two 0x348-byte objects.

*Task #98 sweep, from fix list §2. **Reachability: FIRES TODAY.** Status: SUSPECTED.*

**Where** the blob's datapump registration, reached through slmodemd's
`AT+MS=132`.

`DP_V32BIS` has not connected in any configuration tested — before the ATA's
T.38 fix, after it, and at every IODELAY value. slmodemd reports no datapump
at all for those calls: `DP_ESTAB` straight to `DP_DISC` at the timeout, never
a `CONNECT`. `DP_V32` (32) connects on the same path and negotiates **14400**,
which is above V.32's own 9600 ceiling — so the blob's "V.32" driver is
already doing V.32bis rates and the separate 132 entry may be a registration
that does not work.

Nothing appears to need it, given 32 reaches 14400. Task #96; answerable from
the disassembly with no calls.

---

---

## D74 🐛 `MDMCTL_IODELAY` was a hard-coded constant

**ITS "FIXED" STATUS IS NOT TRUE OF EITHER HOST TREE**, per
`docs/deviation-triage.md` family 10. `grep -rn SLMODEMD_IODELAY` over the
parent repository returns NOTHING; `d-modem/slmodemd/modem_main.c:941-953`
returns a hard-coded 48 in a file dated six days AFTER this entry was written,
and `slmodemd/modem_main.c:682` still returns 0. The default of 120 appears
nowhere in `docs/findings.md` -- finding F1026 recommends 240. This entry is
also MISFILED (its own *Where* field names a host file, not the object) and
should merge into D77, as D77 instructs.

*Task #98 sweep, from fix list §3. **Reachability: FIRES TODAY.** Status:
CONFIRMED, **REOPENED 2026-08-23** — was "CONFIRMED, FIXED", and that was not
true of any tree.*

**The reopening, measured rather than asserted.** Three checks over the parent
repository, none of which needs the modem:

    grep -rn SLMODEMD_IODELAY .            nothing, in either host tree
    d-modem/slmodemd/modem_main.c:941      case MDMCTL_IODELAY: ... ret = 48;
    slmodemd/modem_main.c:682              case MDMCTL_IODELAY: ret = 0;

So the environment override this entry records as the fix **does not exist**,
the fork returns a hard-coded 48, and upstream returns 0 with the real
expression commented out beside it. (Do not confuse these with the
`MDMCTL_IODELAY` arms at `modem_main.c:539` and `:654`, which return
`dev->delay` — those are the device path, not the VoIP shim, and both trees
have them.)

**And the fix's own numbers do not survive the check either.** This entry says
the usable band is "roughly 88–150" and that the fix defaults to 120. The
shipping fork sits at **48**, below that band; upstream sits at 0, far below
it. Yet this repository's own bench record has completed V.34 calls against
that fork. Both cannot be right, and the entry cannot be closed until one of
them gives:

- if the band is right, the fork is running outside it and the calls that
  succeeded did so despite the setting, not because of it;
- if the calls are right, the band was measured on something this path does
  not reproduce, and finding F1022's threshold of 86 is measuring something
  else again.

**One `--log` line settles it** — the derived `ext_delay`, `filtdelay`,
`dmadelay` and `echo_delay` are all printed, and the fork's own comment at
`:941` predicts 4, 47, 1548 and 108 for an iodelay of 48. Read them off a live
call before touching anything.

**This entry is also MISFILED.** Its *Where* names a host file, not the
object, so it is not a deviation of `dsplibs.o` at all and belongs with D77,
as D77 instructs. Reopening it does not fix that; merging it into D77 is the
tidy-up, and should carry this evidence with it.

**Where** `slmodemd/modem_main.c`, D-Modem fork.

One number sets the V.34 pipeline latency and the echo canceller's reach, and
the right value depends on the transport. It was `48`, then `240` — both
hard-coded, and both wrong for this path. Measured: 240 breaks V.32 outright
and costs V.34 a rate; the usable band is roughly 88–150.

**Fixed** in D-Modem fork `09ca128c`: `SLMODEMD_IODELAY` overrides at runtime,
validated 0..240, traced with all three derived quantities, default 120.

---

---

## D75 🐛 💤 a dead patch in `dsplibs.o.mod`

*Task #98 sweep, from fix list §4. **Reachability: CANNOT FIRE.** Status: CONFIRMED.*

`cryan209/D-Modem` ships a patched blob whose four capability gates include one
that does nothing: at `rebuildJMSequence+0x136` the patch overwrites the `xor`
PRECEDING the `setne`, which then overwrites it two instructions later. The
same intent at `+0x7e8` replaces the `sete` itself and works. Recorded in
`docs/forkblob.md` and findings F1140-1146. We do not ship that blob and there
is nothing for us to fix; it is here so nobody re-derives it.

The authority is **finding F1145** (`docs/findings.md:37495`), which traces all
three predecessors of `0x75d80` by hand and names the address the edit should
have used: `0x75d8e`, not `0x75d86`. Nothing in the fork's link line includes
`.mod` today, so it affects nobody.

---

## D76 🐛 `FPM_div` reads past its table and drops the call

*Task #98 sweep, from fix list §5.1. **Reachability: FIRES TODAY.** Status: CONFIRMED. Already in this register as D4 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: opt-in extension.*

**Finding F40.** A level estimate of 4088 normalises to mantissa `0xff80`,
which indexes one past `FPM_div`'s table; the reciprocal comes back zero, the
AGC gain becomes zero, and the block is multiplied to silence. **It fires on 46
of the 700 blocks in one capture**, and the Bell 103 receiver loses lock and
never recovers. Both implementations agree over 116,954 checks and lose lock at
the same bit.

*Trigger:* real signal. Scaling the input by a power of two leaves the
normalised mantissa identical, so every power-of-two gain re-triggers it.
*Fix class:* **opt-in extension** — the missing table entry, exactly as D1's
was added for `FPM_sqrt` (D1). This is the most severe defect in the object.

---

## D77 🐛 V.34 cannot connect below `IODELAY` 86, and one shipped driver answers 0

*Task #98 sweep, from fix list §5.2. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: host-side.*

**Findings F960, F1022, F1026** (and 962). Arm 47 `TX_PHASE2_ANS` reads an
all-ones `fsk.sr` from a carrierless slicer as "repeated INFO0" during the
window in which the far end is *correctly* silent, and loses the race by one
block — about 22 samples out of 200. The wait it must sit out is
`0x5f - filtdelay`, so `filtdelay >= 57` is the threshold, which is
`IODELAY >= 86`. Measured: **85 fails and 86 connects**, with 86..240 all
connecting.

`slmodemd`'s **socket driver** — the one a SIP deployment inherits — returns
**0** for `MDMCTL_IODELAY`, with the real expression commented out beside it at
`modem_main.c:682-686`. A V.34 call on that driver cannot complete phase 2, and
says nothing about why beyond *"Repeated info0 is detected"*.

*Fix class:* **host-side**, and D74 is that fix: `SLMODEMD_IODELAY` now
overrides at runtime with a default of 120. D74 records the fix; this entry
records the threshold it has to stay above and the reason. **The two are one
defect — do not file them separately.**

---

## D78 🐛 `MDMPRM_MIN_RATE` and `MDMPRM_MAX_RATE` reach nothing

**MECHANISM REFUTED, per `docs/deviation-triage.md` family 10.** The
`+0x30`/`+0x34` pair is NOT write-only: `VPcmV34InitiateRetrain` divides both
by 2400 at 0x66b5/0x66c5 into `+0x21c`/`+0x220` and clamps the max to 14
(0x66fb-0x6709), and 0x6870/0x6877 and 0x6acc/0x6acf pass the pair to
`K56FlexFloModem::setMinMaxRates` and `V90ConstellationDesigner::
setMinMaxRates` -- mangled names that TYPE the field. The `+0x38`/`+0x3c`
literals 4800 and 33600 are a different quantity, read by
`V90Parameters::setToDefault`, and are exactly ITU-T V.90 Table 9's upstream
rate window. **The citation drifted the day before this entry was filed:**
finding F1020 (commit `2c98999f`, 2026-08-10) corrects 823/824 in these words,
and this entry is commit `7db93c88`, 2026-08-11.

*Task #98 sweep, from fix list §5.3. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Findings F823, F824.** The host's rate window lands at runtime `+0x30`/`+0x34`,
which one debug `printf` reads and nothing else. The pair the rate machinery
divides by 2400 is `+0x38`/`+0x3c`, and `vpcm_create` writes those as the
**literals 4800 and 33600** on both arms at 0x3b90. Measured by re-running the
whole 1,600-block call once per parameter: `MIN_RATE 300 -> 2400 same`,
`MAX_RATE 56000 -> 33600 same`, with the constructor claims firing to prove the
parameter really arrived. Five of the six parameters are inert for a V.34 call.

*Trigger:* every call. Any host or user that narrows the rate window is
ignored. *Fix class:* **documentation only** — the object cannot be made to
read the other pair without diverging. Say so in the host, so nobody tunes a
knob that does nothing.

---

## D79 🐛 `GetDialToneFilterSubindex` is hardcoded to zero, and 21 filter banks are dead

**MISFILED, per `docs/deviation-triage.md` family 10 -- this is a HOST
integration gap.** `modem_homolog.h:94` carries `//u8
DialToneFilterSubindex;` commented out and `modem_param.c:172-173` is a literal
`return 0;`. The object does the right thing with the answer it is given, and
finding F49 measured bank 3's fallback landing on its own nearest equivalent.
The entry's own fix line says it: "Nothing in the object needs to change."

*Task #98 sweep, from fix list §5.4. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: host-side.*

**Findings F49, F50.** The field is commented out of `slmodemd`'s
`struct homolog_params` and the parameter is answered with a literal
`return 0`, so every bank selection falls back. Three banks of seven
progressively wider bandpasses are fully designed, fully shipped and never
selected. Measured by calling `ref_cadence_create` and reading the pointer it
installed.

*Fix class:* **host-side** — restore the field. Nothing in the object needs to
change.

---

## D80 🐛 the dial-tone threshold conversion wraps, and a country table can disable detection

*Task #98 sweep, from fix list §5.5. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: host-side.*

**Finding F60.** `cadence_create` converts `GetDialToneDetectionThreshold` from
dB to a linear threshold with no clamp and no monotonicity. Measured through a
detector built by `CALLPROG_Create`:

```
  parameter    0     1     5    10    20    30    40    50    60   100
  threshold  16319 16324 16340 16356 16378  560   102    0   8192  7765
  asserts?     no    no    no    no    no   yes   yes   yes   no    no
```

Below about 30 the threshold wraps to roughly 16,320, which no signal reaches,
so the detector is permanently deaf; at 50 it is 0, so it asserts on
everything; 60 and 100 wrap back up.

*Trigger:* the country parameter. Realistic values are 30 to 50, so the shipped
tables sit inside the working band by convention, not by validation.
*Fix class:* **host-side** — validate the parameter to 30..50 before it reaches
the object.

---

## D81 🐛 a loud busy tone is not detected

*Task #98 sweep, from fix list §5.6. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: **documentation.*

**Finding F47.** The cadence detector's IIR cascade has about 42 dB of passband
gain and only takes it back out at the end, so above roughly 8,000 amplitude it
wraps internally, the envelope stops being steady, the stability test fails and
the tone is reported absent. Measured by driving `CP_450_630` with a 550 Hz
tone: at 12,000 the envelope runs 6,995..17,438 and the verdict is 4 present,
36 absent. The modem waits instead of redialling.

*Trigger:* an amplitude a real line can produce. *Fix class:* **documentation
only** unless the level upstream can be shown; whatever holds the level down is
outside this module and is not identified.

---

## D82 🐛 the calling tone is built for 9600 Hz and runs at 8000

*Task #98 sweep, from fix list §5.7. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: **opt-in.*

**Finding F45.** All three of `CallingTone.c`'s constants encode 9600 Hz while
call progress runs at a fixed 8000 (finding F41): the phase step of 2219/16384
is 1300.2 Hz at 9600 and **1083.7 Hz at 8000**; the on time is 0.72 s against
V.25's 0.5–0.7; the off time is 2.10 s against 1.5–2.0. All three are outside
spec as shipped.

*Trigger:* one of slmodemd's fifty country configurations —
`CZECH_REPUBLIC` — turns the calling tone on. *Fix class:* **opt-in
extension** if anyone wants a conformant calling tone; the faithful path keeps
the original constants. See also D11 and D13, which are the shape and the level
of the same tone.

---

## D83 🐛 `modifier_validation` is tested the wrong way round

*Task #98 sweep, from fix list §5.8. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F52.** `AnalyseDialString` makes an unknown dial-string character
INVALID when `modifier_validation` is zero and TOLERABLE when it is set —
turning validation *on* makes the parser more forgiving. **Twenty of the fifty
shipped countries set it**, so those twenty accept dial strings they were
configured to reject.

*Fix class:* **documentation only** in the object; a host that cares can
withhold the flag.

---

## D84 🐛 `receiver`'s complex predictor rounds the real axis the wrong way

*Task #98 sweep, from fix list §5.9. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F140.** The imaginary accumulator starts at `+0x2000` and the real one
is formed as `b.hist_i - (0x2000 + a.hist_q)`, so the rounding constant is
applied with the wrong sign on the real axis and it rounds half an LSB the
wrong way — on every symbol, at both call sites (precoding at 0x5c100 and the
adapting predictor at 0x5c5eb). The finding rules out intent:
"deliberate-looking-but-not". Reproduced.

*Fix class:* **documentation only.** Half an LSB per symbol on the real axis is
not separable from the rest of the receive chain without a full-path
measurement nobody has made.

---

## D85 🐛 the slow AGC pair does not sum to unity

*Task #98 sweep, from fix list §5.10. **Reachability: FIRES TODAY.** Status: CONFIRMED. Already in this register as D6 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: **opt-in.*

**Finding F622**, corroborated by **finding F30**. The Bell 103 and V.23 copies
of `AGC_DEF_ALPHA` carry `16384, 1638` where the alpha should be 31130, so the
slow pair sums to **34242 rather than 32768** — a DC gain of 1.045. The v21
copy has the correct shape. Finding F30 adds independent evidence that it is a
copy-paste slip: `FPM_TONE_detect` writes `31130 * x + 1638 * y` longhand for
the same smoother, and 31130 + 1638 is 32768 exactly. That heading names D6.

*Trigger:* live in the Bell 103 and V.23 receivers. *Fix class:* **opt-in
extension**; the register entry exists, the reachability is what this adds. No
automated guard is possible — `ref_AGC_DEF_ALPHA` is file-static in six
translation units, so a test naming it binds to whichever the linker picks.

---

## D86 🐛 the V.21 offer can never be withdrawn

*Task #98 sweep, from fix list §5.11. **Reachability: FIRES TODAY.** Status: CONFIRMED. Already in this register as D16 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Finding F75.** The test that should clear the V.21 bit reads the framing stop
bit as well, so it is never true and the JM carries V.21 whatever the far end
offered. Found by running a real negotiation against SpanDSP with a
deliberately narrowed offer.

*Fix class:* **documentation only** — reproducing it is what keeps the
handshake bit-exact, and no interop failure has been traced to it.

---

## D87 🐛 the restart's inlined `SetINFO0dBits` lost its guard

*Task #98 sweep, from fix list §5.12. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F402.** Microstate 44's restart path contains a hand-inlined copy of
`V34SetINFO0dBits` at 0x718fc that has **no `v90_receiver` test** — the
`movw $0x1e,0x18(%eax)` at 0x71903 sits before the diagnostic check at 0x71909
and is reached unconditionally — and its string is `"SetINFO0dBits  \n"`, not
the real function's `"V90, setINFO0dBits\n"`. So the object's two writers of the
answering side's message length disagree about when it applies. The test drives
the answering side with `v90_receiver` explicitly zero and a mutation that adds
the guard back is caught.

*Trigger:* a CRC failure in `DET_INFO` on the answering side with no V.90
receiver. *Fix class:* **documentation only** — this is precisely the
wrong-but-plausible tidy the tree's rule forbids.

---

---

## D88 🐛 `shellDemapper` indexes four tables with unclamped running sums

*Task #98 sweep, from fix list §6.1. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F129.** `t1` (+0xa48), `t2` (+0xb48) and `t3` (+0xc48) are 128 entries
each and are read at `t1[c]`, `t2[d1]`, `t3[d1 + d2]`, where the two clamps the
function applies are to the *addends*, not to the index; both correlation loops
also walk down from `t[d]`, so an over-large `d` reads below the table as well.
`grid` is the fourth: `decodeDepth` indexes 529 entries with
`(23*hi + lo + 0x408) >> 2`, and "a parameter of 200 — unremarkable on its
face — gives an index near 1200". **Driving it segfaulted** on the first sweep.

*Trigger:* not established. The caller is `demapFrame`, which is not
reconstructed, so whether a real received frame can produce an out-of-range
group is exactly what has not been measured. The absence of any clamp is not in
doubt. *Fix class:* **documentation only** until `demapFrame` lands.

---

## D89 🐛 `initdigital` reads `divtab[-1]`, and indexes `rx_divtab` from two unvalidated halfwords

*Task #98 sweep, from fix list §6.2. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Findings F186, F306 and F736.** Two separate unclamped indexes in one function,
found three times from three directions.

`divtab[bits + 14*use_max - 1]` is evaluated **before** the zero-rate test, so a
zero rate in mode 0 gives index -1 and reads one entry before the table —
"unclamped and reproduced", with the fixture forced to relocate the table into
a scratch array so the read is defined on both sides. The function's own
`"ZERODIV expected!"` guard exists for that case and the lookup is hoisted
above it.

On the receive side, `movswl -0x2(%edx,%ebp,2)` at 0x59c0f takes `%ebp` as
`rxbits + 14 * rx_use_max` from two plain halfwords at +0xaa98 and +0xaaa6, with
no clamp, into a table of 8,192 shorts. **It segfaults inside the blob, on both
sides**, when those fields are left at a pseudorandom fill.

Finding F306 is the third witness and the one that names the cost: "a varied
fill is a random address indexed by a random 16-bit number", under a heading
that says both of the things the fixture had to seed "were crashes before they
were tests". Two separate fixtures — `t_v34shell.c` and this one — bound the
index by hand because the object does not.

*Trigger:* a rate configuration the object does not validate. The fixtures pin
`rxbits` to 0..15 and `rx_use_max` to 0 or 1 and say so; nothing in the object
does. *Fix class:* **host-side** — the rate fields cross the host boundary.

---

## D90 🐛 `StateName` is indexed unbounded and the result goes to `vsnprintf`

*Task #98 sweep, from fix list §6.2a. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Already in this register as D42 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Finding F290.** With the diagnostics on, `v34handshakinit` announces each
transition by printing the state it is LEAVING; `StateName` is indexed with
nothing bounding the index, so a state word outside 0..86 produces **a wild
`char *` handed to `vsnprintf`**. Observed, not reasoned: "the fixture faulted
inside its own bring-up until it seeded them."

*Trigger:* any of the three state words outside 0..86 at a transition, with
`dsplibs_debug_level` non-zero. slmodemd ships it at zero, so it cannot fire on
a working modem. *Fix class:* **documentation only**; the register entry `D42`
notes the out-of-range case cannot be compared against the blob because the two
sides read different memory.

---

## D91 🐛 `VPcmV34InitiateRetrain` skips its validation exactly when a receiver is up

*Task #98 sweep, from fix list §6.2b. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding F313.** The function has two switches on the same `requestedDp`
argument:

```
    switch one   VALIDATES, and only runs when v90_receiver == 0 && dp != 0.
                 Its default demotes an unknown code to 0 and says so.
    switch two   DISPATCHES.  Its default clears BOTH receiver counters.
```

So an unknown datapump code means opposite things according to a field the
caller does not pass. Asked for with no V.90 receiver it is demoted to 0, the
arm that *keeps* a running receiver; asked for while one is up it is not
validated, not complained about, and reaches the dispatch intact — **where it
tears both receivers down.** Only 0, 34, 56, 90 and 92 are handled.

The same arm carries a second: the `dp == 0` path tests `v90_receiver > 0` with
`jle`, not `!= 0`, so a negative counter — which `VPcmV34SetV90RateReneg` can
produce, `D48` — is left where it is rather than pulled up to 1.

*Trigger:* the caller, with an out-of-range `requestedDp`. *Fix class:*
**host-side** — validate the datapump code before it reaches the object.

---

## D92 🐛 microstate 44's accept arm copies past its ten-short buffer and then reads the byte it corrupted

*Task #98 sweep, from fix list §6.3. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F444.** The default accept arm copies `nbytes = ceil(bits/8)` shorts
into the ten-short array at +0xabae with no bound. Index 10 lands on `nbytes`
itself, index 12 on the record's length, index 14 on `local_short` and **index
15 on `is_short` at +0xabcc — which the arm then reads at 0x6e824 to choose the
microstate it leaves in**. So the successor state is picked out of memory the
copy has just overwritten. All of it is inside the object, the differential
comparison sees every byte, and it passes: the blob does exactly this. A
mutation that bounds the loop at ten is caught.

*Trigger:* by message length — any accepted message above 64 bits that is not
one of the three sized lengths 0x4d, 0x26 and 0x08, which dispatch elsewhere. A
110-bit message gives `nbytes` = 16 into 10 slots. That is a length the far end
chooses. *Fix class:* **documentation only** in the object; worth an assertion
in the debug build.

---

## D93 🐛 `ApplyBulkDelay`'s bound is unsigned and `getbit`'s index has none

*Task #98 sweep, from fix list §6.4. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED (compare) / SUSPECTED (reach). Fix class: documentation only.*

**Finding F227.** Two in one finding.

`ApplyBulkDelay` compares the delay against `bulk_len` with `jb`, not `jl`. A
negative `bulk_len` is huge unsigned, so it accepts every delay and the clear
runs off the end of the ring. The instruction is read off the object; nothing
reconstructed writes `bulk_len`, so **no call site is known to reach it**, and
the tests keep it positive deliberately because the overrun would be identical
on both sides and would corrupt the object under comparison while proving
nothing.

`getbit`'s `word[10]` at +0xaa3c is ten entries because the next named field
starts there: "**the array length of ten is adjacency, not a bound** — nothing
in `getbit` checks the index". `v34handshak` loads the record pointer at
+0xaa6c thirty-six times and hands it to `getbit`.

*Fix class:* **documentation only**, plus a debug assertion. Neither has a
reachable caller today.

---

## D94 🐛 the V.8 CM/JM collector checks its bound after the read

*Task #98 sweep, from fix list §6.5. **Reachability: REACHABLE FROM A CONFORMANT PEER -- upgraded 2026-08-24 from "needs a caller or configuration nothing validates".** Status: CONFIRMED. Fix class: a bound, host-side; NOT YET WRITTEN.*

**THE UPGRADE, FROM THE RECOMMENDATION RATHER THAN FROM A GUESS.**
`docs/deviation-triage.md` establishes that no clause caps what this collector
must accept: V.8 §5.2 permits "any number of extension octets" and §6.6 permits
multiple concatenated NS blocks, so a 255-octet NS block alone expands to about
408 octets against a fifteen-word array. The peer does not have to be hostile
or even unusual -- it has to be verbose, and the Recommendation allows it. That
is a different class of entry from "a malformed peer could", and it is why this
is one of the two the triage put at the top.

**What is still true from the original note:** it does not fire in normal
operation, because every fifteen-word message begins with the marker and the
marker resets `fdbc` to 1. The defect needs a stream that stops sending the
marker, which §5.2 and §6.6 between them permit.*

**Finding F73.** At 0x78070 the collector reads `word[fdbc]` and only *then*
tests `fdbc <= 14`. `word[]` has fifteen entries, so `fdbc == 15` reads the CRC
field beyond it, and the matching path raises `fdbc` with no cap at all.
Measured rather than assumed: it does not fire in normal operation because
every fifteen-word message begins with the marker and the marker resets `fdbc`
to 1.

*Trigger:* **a peer.** It needs a stream that never sends the marker again and
whose characters go on matching the transmit fields past the array — i.e. a
broken or hostile far end. *Fix class:* **documentation only**; the read is of
the object's own storage.

---

## D95 🐛 `LowPassFIR` leaks its window on every rejected design

*Task #98 sweep, from fix list §6.6. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: opt-in extension.*

**Finding F602.** The four-argument `design(nTaps, cutoff, type, gain)` allocates
a window buffer, passes `adopt = 1` so the primitive takes ownership, and
**leaks that buffer whenever the primitive rejects the arguments** — a bad
cutoff or fewer than two taps — including `nTaps == 0`, where it calls
`sysdep_malloc(0)` first and leaks that. Four more in the same class: a rejected
design leaves `taps` uninitialised while the wrapper discards the return code;
`cutoff == 0` is accepted and writes the x87 indefinite into every tap; neither
`sysdep_malloc` in the primitive is checked; and the `const T *window` in the
mangling is a lie the object casts away, adopts and frees.

*Trigger:* any caller passing a bad cutoff or fewer than two taps — ordinary
argument values, not corner cases. *Fix class:* **opt-in extension** for the
leak; the rest is **documentation only**.

---

## D96 🐛 `vpcm_create` dereferences two host answers without validating either

*Task #98 sweep, from fix list §6.7. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding F801.** `MDMPRM_DPRUNTIME` is stored at +0x28 and **dereferenced
immediately** (`movl $0x0,0x78(%eax)` at 0x3ad2); `MDMPRM_DSPINFO` is stored at
+0x24 and `vpcm_delete` writes two words through it at 0x3ded — **SIGSEGV in
`ref_vpcm_delete+29`**. Both faults were found by running, not by reading.

*Trigger:* the host. Any `modem_get_param` that answers either call with
anything but a valid buffer crashes the blob, on create or on teardown.
*Fix class:* **host-side** — the host must supply real buffers; the sizes are
not documented anywhere.

---

## D97 🐛 `DialerCreate`'s `strcpy` has no bound of its own

*Task #98 sweep, from fix list §6.8. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F54.** The dial string is copied into a 100-byte field with an
unbounded `sysdep_strcpy`. What keeps it inside is a length check *inside
`AnalyseDialString`*, a hundred lines away in another function that does not
say so — and that holds only because `FATAL` is 0 and therefore satisfies the
`<= INVALID` early return. Give `FATAL` a value above `INVALID` and the copy
overflows.

*Trigger:* not today. *Fix class:* **documentation only**; it is a constraint on
whoever edits the grade enum, and nothing states it.

---

## D98 🐛 a zero break time gives a pulse dialler that sends no pulses

*Task #98 sweep, from fix list §6.9. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding F53.** With `break` zero, `elapsed < break` is never true, the line is
never interrupted, and the digit still counts down to completion on the make
timer alone — so the dialler reports success and dials nothing. Nothing in the
code guards against it, and `t_pulse` asserts it in that direction rather than
avoiding the case.

*Trigger:* a country table with break = 0. *Fix class:* **host-side** — validate
the country table. See also findings F56 and F59 in D118 for two more of the
same shape.

---

## D99 🐛 `getConstellationMask` clears eight entries and can write sixteen

*Task #98 sweep, from fix list §6.10. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding F829.** The shift is `mov %dl,%al; shr $0x4,%al` — 8-bit, with no
subsequent masking — so **a table byte of 0x80 or more addresses `mask[8]` to
`mask[15]`, eight entries the function never cleared and the caller may not
have sized for**. Both halves are the object's: it clears eight and can write
sixteen. `t_v90cmask` drives it with a 24-entry buffer and asserts that entries
8..15 are reached.

The same function's `which` clamp is `cmp $0x6,%esi; setl %dl; neg %edx; and
%edx,%esi` — a **signed** test, so a negative `which` passes straight through
and indexes before `distinctIndex[]`. Nothing in the object guards it.

*Fix class:* **host-side** if the buffer is caller-supplied, **documentation
only** otherwise; the caller is not reconstructed.

---

## D100 🐛 two empty constellations are declared identical having compared nothing

*Task #98 sweep, from fix list §6.11. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F828.** `getConstellationsIndex` uses the loop cursor as its equality
verdict, so a length of zero leaves `n = 0`, the test compares 0 against 0, and
the two constellations are declared duplicate without a byte being read.
`t_v90cmask`'s shape 2 asserts the blob returns 1 for exactly that.

*Trigger:* any caller with `constellationSize[k] == 0` for two or more entries.
*Fix class:* **documentation only.**

---

## D101 🐛 `V92deleteConstellations` and `V92deleteFilterCoefficients` leave ten dangling pointers

*Task #98 sweep, from fix list §6.12. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F831.** "**Neither delete writes anything.** There is not one store in
either function; the freed slots keep their stale pointers." Each free is
guarded by `if (p != 0)`, which the omission defeats: a second delete
double-frees and any later read is a use-after-free.

*Fix class:* **documentation only** — a helpful `= NULL` is exactly the
wrong-but-plausible change the rule forbids. Same shape as D56 (`~FloatIIR`)
and finding F55 below.

---

## D102 🐛 `CALLPROG_Delete` leaves three of five sub-object pointers dangling

*Task #98 sweep, from fix list §6.13. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F55.** Of the four sub-objects it frees, only `busy` and `f70` have
their pointers cleared; `dial`, `band` and `dtmf` are left dangling, so a
second `CALLPROG_Delete` on the same object frees them again. `call_delete`
calls it once — but `CALLPROG_Delete` is a global symbol and the asymmetry is
invisible from outside.

*Fix class:* **documentation only.**

---

## D103 🐛 the caller-supplied `FPM_TONE` path frees five pointers it never obtained

*Task #98 sweep, from fix list §6.14. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Already in this register as D5 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Finding F1067.** Measured on both sides: `create` with supplied state does
`allocs=0 frees=0`, and `delete` then does `bad_free=5`. The finding sharpens
the register entry in a way worth carrying: it is **as much a create-side
precondition as a delete-side double free**. The caller-supplied path is
unusable without four caller-allocated buffers at `len*2`, `(len+extra)*2`, 10
and 8 bytes, "and that requirement is documented nowhere" — a caller following
the library's own "pass your own storage" idiom faults long before `delete`
gets its chance.

*Fix class:* **documentation only**, and the sizes above are the documentation.

---

## D104 🐛 `V90Equalizer::reset`'s cursor clamp does not clamp at length zero

*Task #98 sweep, from fix list §6.15. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding F1108.** `cursor = min(cursor, linearEquLength - 1)` is compiled with
`jae`, so a zero-length equaliser computes 0xffffffff and the clamp is a no-op:
the 1.0f tap lands at whatever cursor was asked for. Spelling the comparison
signed breaks **648 checks**, so the unsigned form is the object's beyond doubt;
the length-zero consequence is reasoned.

*Fix class:* **host-side** — the length is a parameter. No caller is named.

---

## D105 🐛 `FloatARMA` with zero denominator taps

*Task #98 sweep, from fix list §6.16. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED (store) / SUSPECTED (loop). Fix class: documentation only.*

**Finding F873.** With `nDen == 0` the constructor still executes
`m_a[0] = 0.0f` — 0x4731e and 0x47379 are both unconditional — writing four
bytes through a `sysdep_malloc(0)`. `t_floatarma` constructs that shape and
does not drive `process` on it, where the carry-tail loop's `dec %edx; jne`
count underflows: `FloatFIR`'s zero-tap hazard in a second class.

*Fix class:* **documentation only.**

---

## D106 🐛 `refLoopsType2` is terminated only by the linker's zero padding

*Task #98 sweep, from fix list §6.17. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: already handled.*

**Finding F234.** Five of the six loop arrays end with an all-zero record inside
the symbol. `refLoopsType2` is 2,244 bytes — exactly 33 records — and **not one
of them has the zero first byte the counting loops stop on**. What stops them
is the 28 bytes of `.data` alignment padding between the end of the symbol at
0x6124 and `refLoopsType1` at 0x6140, which happen to be zero.

This is finding F13's pattern in a second place: correct only because of what
the linker put next, and it breaks the moment the tables are regenerated or
reordered. Our copy carries a 34th all-zero record, and `t_v90pftab.cpp`
asserts the counted length is 33 on both sides.

*Fix class:* **already handled** in the reconstruction; **documentation only**
for the original.

---

## D107 🐛 the DIL segment search runs one past its row when `dilCount` is zero

*Task #98 sweep, from fix list §6.18. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F232.** `resetDILGenerator` finds which G.711 segment `dilLevel[0]`
falls in and reads the level back with `movzwl`. After any non-empty expansion
the level is in 0..0x7fff and inside the first seven boundaries, so the row's
last entry and the index one past the end of the row are both unreachable —
"**`dilCount` = 0 leaves the field unwritten, and *that* is the path where the
search sees whatever was already there**". The test seeds it across the
boundaries in both laws and **asserts it reached index 7, index 8 and a
negative value**, so the out-of-row index is reproduced.

*Trigger:* a descriptor with `dilCount == 0`, plus whatever the field happened
to hold. *Fix class:* **documentation only.**

---

## D108 🐛 `v23FP_rx_progress` leaves the caller's output and its own status unwritten

*Task #98 sweep, from fix list §6.19. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F82.** `*nbits` is written only on the paths that return 0, so a
caller must initialise it — its twin `BwChDem_Progress` clears it in the
prologue, which is how the asymmetry shows. And neither give-up path stores the
status 2 into the object; the 2 goes straight into `%eax`, so **the object's
status field still claims the receiver is running** after it has given up. A
caller that reads the field rather than the return value is told the wrong
thing.

*Fix class:* **documentation only**, and the differential test works round it
by poisoning `*nbits` before every call.

---

## D109 🐛 the V.23 acquisition gate counts detections, not consecutive ones

*Task #98 sweep, from fix list §6.20. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F81.** `v23FP_rx_progress` omits the `rx_state = 0` reset its Bell 103
twin performs on a failed detect, so the gate accumulates non-consecutive
detections however far apart they fall, and once the counter reaches 10 it
never returns. What compensates is the detector's 0.885 energy ratio — its
configuration, not the gate.

*Trigger:* any input producing two 1300 Hz-dominant blocks at any separation.
*Fix class:* **documentation only.**

---

## D110 🐛 `four1` and `realfft` validate nothing

*Task #98 sweep, from fix list §6.21. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F832.** No power-of-two check, no null check and no length check in
either body; the first loop runs on the raw argument. The compiler's
`-freciprocal-math` rewrite of `A/len` to `A*(1.0/len)` is exact **only**
because every length a caller passes is a power of two; on any other length the
two forms disagree and the twiddle seeds change as well. The only in-object
caller found is `Psd::process` with `m_length`, which nothing validates either.

*Fix class:* **documentation only.**

---

## D111 🐛 the receive-rate completion has no default arm

*Task #98 sweep, from fix list §6.22. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding F428.** The five-way at 0x62f9c is the only writer of the rate and its
floor, and it has **no default**: on any `rx_baud` outside 0x960, 0xaf0, 0xbb8,
0xc80 and 0xd65 the object falls into 0x62fce reading two never-initialised
stack slots, and `v34handshak`'s prologue at 0x628f0 writes 0x74, 0x4c and 0x78
and nothing else. About 1,600 bytes of rate-ladder arithmetic then runs on
them, and the results are written into the capability record at +0xaa3c and the
receive context at +0xe84.

A second read-before-write in the same arm is prevented only by arithmetic: the
floor is always below the rate for all five recognised pairs, so the ladder
turns at least once and writes the term that 0x631a1 reads. On an unrecognised
baud the two defects compound.

*Trigger:* an `rx_baud` the caller supplies and the object does not validate.
*Fix class:* **host-side.**

---

---

## D112 🐛 `PPSEG` adds two different units together

*Task #98 sweep, from fix list §7.2. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`PPSEG` adds two different units together (finding F1043).** At 0x68154 a
count of symbols at the *negotiated* baud is summed with `filtdelay`, a count
of 4-sample ticks. The sum is dimensionally correct **only at 2400 baud**; at
3429 the term should be `filtdelay * 3429 / 2400`, which at `IODELAY` 216 is
127 where the object uses 89 — an error of 38 symbols in an echo-adaptation
start delay. Finding F1046 states the position: latent at every rate but 2400,
no fixture reaches `PPSEG` at another rate, must be reproduced and not
repaired. **Unobserved, not unreachable** — essentially every real connection
negotiates something other than 2400.

---

## D113 🐛 `updateAlpha` divides by zero

*Task #98 sweep, from fix list §7.3. **Reachability: LATENT.** Status: SUSPECTED. Already in this register as D33 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**`updateAlpha` divides by zero (finding F127, `D33`).** `(1 << (shift+21)) /
((energy + 0x8000) >> 16)` traps for `energy` in [-0x8000, -1]: the
normalisation loop stops immediately because bit 30 is already set in a
negative value, so no shift rescues it. The entry condition is that
`V34EchoEstimateDelayLineEnergy`'s sum of squares has already overflowed
negative — a second defect upstream.

---

## D114 🐛 The slicer's distance metric wraps at 16 bits

*Task #98 sweep, from fix list §7.4. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The slicer's distance metric wraps at 16 bits (findings F110 and F119).**
The squared distance is shifted logically and then truncated, so a
constellation point far enough from the target wraps to a small distance and
can win. Whether it fires with a real V.34 constellation is unmeasured. **The
metric exists twice** — inlined in `decoderv34` as well as in `decision` — so a
fix to one would not reach the other.

---

## D115 🐛 `detectRetrainReq` widens the two sides of its compare differently

*Task #98 sweep, from fix list §7.5. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`detectRetrainReq` widens the two sides of its compare differently
(finding F212).** All four sites are `movzwl` on the energy and `movswl` on the
threshold, so an energy short that has wrapped negative reads as ≥ 32768 and
clears any threshold, and a negative threshold rejects every energy. Reachable
in the object's own arithmetic — `0x04000000` in both accumulators is the
corner exactly — but "a sweep of all 32767 amplitudes of a 1200 Hz sine gave no
negative energy at all". The function also has no caller inside the object.

---

## D116 🐛 The low-level block counter is unsaturated and compared signed

*Task #98 sweep, from fix list §7.6. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The low-level block counter is unsaturated and compared signed (finding
F360).** The counter at +0x0234 advances on every block below the level floor,
is cleared only by a block at or above it, and its test against 0x257f is
signed — so after 2^31 sub-floor blocks it wraps negative and the condition
stops being reported. 2^31 *calls*, not loop iterations.

---

## D117 🐛 `B103FP_create`'s 700-tick floor is an unsigned compare on a signed quotient

*Task #98 sweep, from fix list §7.7. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`B103FP_create`'s 700-tick floor is an unsigned compare on a signed
quotient (finding F36).** A negative `tone_timeout_ticks` divides to a negative
`t`, which as unsigned is enormous, passes the `jae`, and is stored as a
negative short — the clamp is meant to enforce a floor and does not. Not
reachable from `B103_CFG`, where 14000/20 is exactly 700 and the clamp is a
no-op.

---

## D118 🐛 Two country parameters hang the dialler

*Task #98 sweep, from fix list §7.8. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Two country parameters hang the dialler (findings F56 and F59).**
`GetPulseDialDigitPattern` is validated against 1, 2 and 3 only; any other
value — including the zero a table that never considered it would hold — leaves
the pulse count at the -1 the state initialised it to, and
`IsPulseDialerReady` counts down past zero and does not terminate. A zero
`GetDTMFDialSpeed` makes the DTMF burst skip its sample accounting, so the
digit never finishes and the tone is emitted for ever. Neither is reachable
from any of the fifty shipped tables — unreachable by *data*, not by code, so
D98's host-side validation should cover all three.

---

## D119 🐛 Three leaf defects in V.8

*Task #98 sweep, from fix list §7.9. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Three leaf defects in V.8 (finding F62).** `v8_absfn(-32768)` returns
-32768, because negating it overflows and the result is narrowed back to a
short. `v8_copycoeff` counts with a short, so a count above 32767 never
terminates. `v8_crc` compares its bit argument sixteen bits at a time, so a
value that is non-zero overall but zero in its low half counts as a zero bit.
None is reachable from the signal path.

---

## D120 🐛 `Dual_TONE_detect`

*Task #98 sweep, from fix list §7.10. **Reachability: LATENT.** Status: SUSPECTED. Already in this register as D9 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**`Dual_TONE_detect` (finding F42, `D9`).** The energy floor
`Dual_TONE_create` installs is **1** — not 1000, not a fraction of full scale —
so the "no signal" branch is very nearly unreachable and a detector fed
anything at all reports 1 rather than 0. Separately, the sample index is
truncated to 16 bits every iteration by a `cwtl` inside the loop, so a block
longer than 32767 samples loops for ever. Nothing in the library passes one,
and nothing validates it either.

---

## D121 🐛 `vpcm_run`'s training timeout never clears its counter

*Task #98 sweep, from fix list §7.11. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`vpcm_run`'s training timeout never clears its counter (finding F982).**
An unchanged progress code 0 counts towards a 3,000-block deadline at 0x4274
(`cmp $0xbb8`) and on expiry sets mode -1, "vpcm: train timeout!", **without
resetting the counter — so every block after the deadline fails again**. The
error return is `mov $0xffffffff` at 0x42b8, a raw -1 that is not a `DPSTAT_*`
code at all. Both are inside the host contract; the 4,000-block fixture never
accumulates 3,000 unchanged blocks.

---

## D122 🐛 A renegotiation at the end of the range carries the previous request

*Task #98 sweep, from fix list §7.12. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**A renegotiation at the end of the range carries the previous request
(finding F189).** A step that would leave the bounds writes **nothing** rather
than clamping, so `rate_want` keeps what it held; the handshake is still torn
down and the attempt still counts, with the stale value as its target. A
negative `rate_want` is the "no target" encoding, so a stale value is read as a
real rate index.

---

## D123 🐛 The FSK delay line is aliased onto the echo canceller's coefficients

*Task #98 sweep, from fix list §7.13. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The FSK delay line is aliased onto the echo canceller's coefficients
(finding F100).** `fskdetect`'s 49-entry delay line and `echo0 + 0x0c` are the
same memory, proved from two independent address readings. `V34EchoCleanUp`
zeroes all 144 entries of `coeff_frac` and `V34EchoAdapt` writes every one of
them, so they cannot both be live. The reading that fits is deliberate reuse —
phase 2 carries the INFO messages, data mode adapts — but **nothing in the
object enforces the separation**, and a retrain that re-enters phase 2 with the
canceller live is the case to check.

---

## D124 🐛 The V.23 answer tone is in spec by frame quantisation

*Task #98 sweep, from fix list §7.14. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The V.23 answer tone is in spec by frame quantisation (finding F84).**
The silence is `rate/20` = 400 samples = 50 ms, against V.25's 75 ± 20. It
lands in spec only because the silence state uses `<=` where the tone state
uses `<`, and because `dp_wrapper` delivers 160-sample frames, so 400 samples
takes three frames to exceed: 480 samples, 60 ms. **A different frame size puts
it back out of spec.**

---

## D125 🐛 `V90Demodulator::reset` sizes the first block from the previous configuration

*Task #98 sweep, from fix list §7.15. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`V90Demodulator::reset` sizes the first block from the previous
configuration (finding F608).** `reset` at 0x1c038-0x1c04c calls `agc.reset()`
and only *then* writes `blockLen` from `params+0x64`; `reset` has already
copied the OLD `blockLen` into `count`, so the first block after the first
reset runs for the constructor's 500 samples rather than the configured length.
It self-corrects on later resets.

---

## D126 🐛 `hamming` and `blackman` divide by zero at one tap

*Task #98 sweep, from fix list §7.16. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`hamming` and `blackman` divide by zero at one tap (finding F246).** Both
compute `1 / (n - 1)`, which for `n == 1` is +infinity, and `0 * infinity` is
the x87 indefinite: both write **0xffc00000** into `w[0]`. There is no guard,
adding one breaks the match, and the test drives `n == 1` deliberately and
compares as bits. Any caller asking for a one-tap window gets a NaN.

---

## D127 🐛 `autoSelection` lets a NaN win the search

*Task #98 sweep, from fix list §7.17. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`autoSelection` lets a NaN win the search (finding F236).** The running
best is updated on `fcom %st(2); fnstsw; sahf; jae`, and `fcom` sets C0 for an
unordered result as well as for a less-than one, so a NaN distance takes the
update arm where C's `<` does not. **A NaN is reachable because the measurement
is whatever Phase 2 left in memory and the six differences are taken from it
unchecked.** Found by the test, not by reading — in the arm where the
measurement was left as seeded pseudorandom bytes.

---

## D128 🐛 `tone_detect`'s second section folds instead of scaling

*Task #98 sweep, from fix list §7.18. **Reachability: LATENT.** Status: SUSPECTED. Already in this register as D25 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**`tone_detect`'s second section folds instead of scaling (finding F90,
`D25`).** Section 2 truncates the accumulator to 16 bits and *then* shifts by
4, where section 1 shifts first; on loud input it wraps. "There is no reading
under which both are intended. One is a slip." Both orders behave identically
over the levels a correctly-AGC'd detector sees.

---

## D129 🐛 `modem_serrint`'s 60-tap FIR path feeds the adaptation at full width

*Task #98 sweep, from fix list §7.19. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`modem_serrint`'s 60-tap FIR path feeds the adaptation at full width
(finding F128).** The accumulator is used at 32 bits for the products and only
the queue store narrows it, so a loud enough sample drives both echo
cancellers' error terms and the leaky energy estimate with a value outside a
short. Reproduced. Requires receiver flag bit 11; the Hilbert branch is the one
the receiver normally takes.

---

## D130 🐛 Nothing initialises the history-ring index, and the store precedes the range test

*Task #98 sweep, from fix list §7.20. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Nothing initialises the history-ring index, and the store precedes the
range test (finding F781).** At 0x5d051 `hist_2f58[idx]` is written, and only at
0x5d04c/0x5d059 is `idx + 1` tested against 0x257 — unsigned. Nothing in the
object initialises `f2aa6`, and a whole call from a fixture that fills the
object with pseudorandom bytes drives it negative on the first sample.

---

## D131 🐛 `FPM_FSD_demodulate` discards input past its bit cap

*Task #98 sweep, from fix list §7.21. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`FPM_FSD_demodulate` discards input past its bit cap (finding F32).** The
loop stops once `max_bits + 1` bits have been written and throws the remaining
samples away rather than holding them over. Bell 103's ceiling is 8 bits from
64 samples and `DemodDataB103` feeds 48 at a time, so it never bites in normal
use — safe by the fragment size the one in-library caller happens to use.

---

## D132 🐛 `FPM_sqrt_dp`'s mantissa can be 17 bits

*Task #98 sweep, from fix list §7.22. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`FPM_sqrt_dp`'s mantissa can be 17 bits (finding F21).** After
normalisation `x >> 15` reaches 0x1ffff, which does not fit the
`unsigned short` it is stored in and wraps for `x >= 0x80000000`. The same
shape as `FPM_sqrt`'s one-past-the-end read. `FPM_rms` is the only caller found
and its accumulator is bounded by the /36 scaling, so it may be unreachable.

---

## D133 🐛 The AGC's block partition can exceed what `FPM_rms` is dimensioned for

*Task #98 sweep, from fix list §7.23. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The AGC's block partition can exceed what `FPM_rms` is dimensioned for
(findings F28 and F29).** The folded block reaches `block_len + block_len/2 - 1`
= 53 samples where `FPM_rms`'s 1/36 headroom is sized for 36, so at 53 samples
a block above about 82% of full scale sticks at 32703 and the AGC applies less
gain reduction than it should — wrong in the safe direction. Separately, a
`count` below `block_len/2` is passed through completely untouched: not gated,
not scaled, and for Bell 103 that is any call of 1..17 samples.

---

## D134 🐛 `initTxSequence` reads a fourth byte it does not own

*Task #98 sweep, from fix list §7.24. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`initTxSequence` reads a fourth byte it does not own (finding F66).** The
original tests `*(int *)cm & 0x80008` — a single 32-bit read across a
three-byte flag structure, correct only by accident of layout. The
reconstruction writes it as the two byte tests it plainly is.

---

## D135 🐛 The A-law boundary row's last entry can never be reached

*Task #98 sweep, from fix list §7.25. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**REFUTED BY `docs/deviation-triage.md`, family 7.** Both halves fail against
the object. The two loads are `movzwl` — `0x2aea5` and `0x2b022` — not
`movswl`, so the search exits at index 7 on **every** top-segment level
(a valid A-law level is a positive `alaw2linear` result, 0..32256, and any
level in 16385..32256 exits there). And "both users" is seven references
across five functions: `updateCodeSegmentPointer`, `resetDILGenerator`,
`generateDIL`, `generateV90Symbol` (twice) and `generateV92Symbol` (twice).
The residual true fact — that an unsigned load lets a slot content of 32,769
or more fall out at index 8 — belongs to D107. **The claim originates in
finding F231, which does not account for the `movzwl`, so the correction is
owed there too.** Status here was already SUSPECTED rather than CONFIRMED,
which is what a suspected claim is for.

**The A-law boundary row's last entry can never be reached (finding F231,
`docs/findings.md:13235` — the material is in that finding's body, not in its
heading, which is about `callgraph.py`).**
`codeSegmentsBoundriesLookupTable`'s A-law row ends at 32768, which
does not fit a short, and the comparison against it is signed — so the last
segment boundary is a test no 16-bit level can satisfy. Both users index the
table at `8 * pcmType + segment` and the A-law arm is live.

---

## D136 🐛 `evaluateRxJMSequence`'s second matching loop never clears its flag

*Task #98 sweep, from fix list §7.26. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`evaluateRxJMSequence`'s second matching loop never clears its flag
(finding F73).** Two extension-matching loops that look identical: the first
keeps its "something has matched" flag in the field `febc` and clears it before
every marker word; the second keeps it in a stack local set up once before the
loop and **never cleared**. Once anything has matched, a later marker whose
first character is wrong is abandoned rather than scanned through. Reachable
from the wire; the finding does not claim the author thought it wrong.

---

## D137 🐛 The blob disagrees with itself on the receive path

*Task #98 sweep, from fix list §7.27. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The blob disagrees with itself on the receive path (findings F736 and
F754).** At two fixture fills, byte-identical inputs give different results on
**both sides** — the control cases fail too, so it is not the reconstruction.
`V34HS_PROBE` reports 0 object bytes and 0 padding bytes differing after setup
and side B re-run at its own address differs in 0 bytes, so each side is
deterministic and the inputs are identical. It survives `REFINIT`, `EQPTR`,
`SKEW`, `NOSCRUB` and `PADVARY`, is geometry-sensitive (moving side B out of
its arena halves it), and lands in four modelled receiver fields. **The cause
is not established; what is left is a read of memory outside the object.** Any
arm that calls `receiver` inherits it.

This is the same shape as `D61` and probably the same defect. **Findings F319,
F322 and F324** measured it on the transmit side: byte-identical objects placed
differently in memory give different answers, and `v34handshakinit` left
+0x0a28 and +0x2608 holding `ref_Convolve32` on one side and
`ref_Convolve32 + 0x40` on the other — the same table, sixty-four bytes in,
from identical inputs and identical code. Congruent wrappers for the five
pointed-to blocks take it from 23 of 24 fills failing to 0 of 24. Ruled out:
our bring-up against the blob's, stack residue, x87 residue, absolute address,
alignment, object placement, object-to-block distance, and block neighbourhood
contents within 32 KB. **What is NOT established is which byte it reads**, and
finding F322 says so in terms — do not quote this as if the mechanism were
known. Finding F319 retracts `D60`, which had read the same evidence as a
fixture artefact.

---

## D138 🐛 Two idle-symbol emitters hardcode the scrambler polarity

*Task #98 sweep, from fix list §7.29. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Two idle-symbol emitters hardcode the scrambler polarity (findings F280
and F303).** `k56FlexPhase34` case 4 hardcodes the generator as the literal 1
and `v90Phase34` case 5 hardcodes it as the literal 0, where `txmitdibit`
passes `tx_scrambler_mode(o)` — bit 0 of `f25c2`, the calling/answering
polarity. The argument is an absence and it is checked in both: "nothing in
these 721 bytes loads +0x25c2 at all", "nothing in these 1,358 bytes loads
+0x25c2". Mode 0 is the calling station's polynomial, so the emitter agrees for
every object whose polarity bit is clear and disagrees for every one where it
is set. Neither advances `f25c6`, so the differential quadrant the handshake
carries does not move across an idle symbol, and neither does differential
encoding at all. **Two siblings hardcoding opposite literals is what makes this
read as a slip rather than a constant**; the record transcribes it without
calling it one.

---

## D139 🐛 `bits[7] &= 0xdf` takes the whole high byte with it

*Task #98 sweep, from fix list §7.30. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`bits[7] &= 0xdf` takes the whole high byte with it (finding F312).**
`V34SetINFO1aBits`'s INFO1d arm sets the bit with `bits[7] |= 0x20` and clears
it with `bits[7] &= 0xdf` — and the clear is `and $0xdf` on a zero-extended
short, so it is not the set's inverse: it destroys bits 8..15. A test whose
message shorts never carry a high byte cannot tell the difference, and none
does. Finding F335 narrows it — the reader side takes the bit only through
`testb $0x20` on the low byte — but does not close the writer-side clobber.
Whether the protocol ever puts anything in that high byte is not established.

---

## D140 🐛 `SineWave::generate` advances the phase once more than it emits

*Task #98 sweep, from fix list §7.31. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`SineWave::generate` advances the phase once more than it emits (finding
F601).** The advance sits between the `cmp` and the `jb` and the wrap happens
only at the end, so a long call accumulates angle: measured **1.86 rad over
96,000 samples in one call against 0.03 rad over 2,000 calls of 48**.
`generate(out, 0)` is not a no-op — it still rewrites `phase`. **No caller is
named anywhere in the record**, which is why it is graded LATENT rather than FIRES TODAY: it is
one of four weak class templates with a single instantiation, and until the
caller is found the drift has no measured consequence. The same finding carries
two more of the class: the parallel differential coders' constructor does not
call `reset`, so `size_` is 0 and a freshly built coder processes nothing until
somebody resets it, and `reset` fills only the new width, leaving capacity
beyond it stale.

---

## D141 🐛 Two producers on the transmit queue account differently

*Task #98 sweep, from fix list §7.28. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Two producers on the transmit queue account differently (finding
F116).** `txwritequeue` adds four to `count` per call; `txmit` open-codes the
same enqueue and adds **one per sample**. The two agree only while the
modulator returns exactly four samples per call, which is measured at 2400 baud
and not fixed for the other six rates `V34SetupModulator` handles.

---

---

## D142 🐛 💤 `probeselect` can never select pre-emphasis index 0 — the counter increments before the first test, so it exits at 6..10 and the `i == 5` arm is dead

*Task #98 sweep, from fix list §8.1. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings F218, F219 (`D53`).** Measured twice: a sweep asserting 6..10 returned and 0..5 never, and gcov marking the body NOT EXECUTABLE after 6,938 executions of the test.

---

## D143 🐛 💤 `preempindex` has the same shape in a second function, and can only return 6 or more

*Task #98 sweep, from fix list §8.2. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings F197, F620 (`D36`).** Confirmed by construction in finding F1061: the guard is unreachable at 5 and reachable at 6.

---

## D144 🐛 💤 `setInitialPhase`'s divide-by-zero guard tests a sum of two constant polynomial values that runs 7,632..11,772

*Task #98 sweep, from fix list §8.3. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F242.** Computed exhaustively; dead in the original too.

---

## D145 🐛 💤 microstate 58's third block is unreachable after its own second block, so the error-recovery reset there is dead

*Task #98 sweep, from fix list §8.4. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F431.** Reasoned from what the second block writes; recorded as an equivalence so it expires loudly.

---

## D146 🐛 💤 five of six `hs_setstate` guards in microstate 44 compare against a state the dispatch has already made impossible

*Task #98 sweep, from fix list §8.5. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F448.** Reasoned from what selects the arm.

---

## D147 🐛 💤 the `+0xaaa6 == 0` test at 0x62fd5 sits forty bytes after the literal 1 is stored there unconditionally

*Task #98 sweep, from fix list §8.6. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F428.** Two addresses, nothing in between; the suite records it equivalent.

---

## D148 🐛 💤 `loadModemParamsData`'s two absolute-value operations can never do anything — the quantity comes from an unsigned divide

*Task #98 sweep, from fix list §8.7. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F879.** Proved over the whole domain; both mutations recorded equivalent.

---

## D149 🐛 💤 `VPcmV34Create`'s K56flex arms are unreachable (one relocation, and the caller can pass only 0, 1 or 2), and `vpcm_create`'s failure path is dead because both of the callee's exits are `xor %eax,%eax`

*Task #98 sweep, from fix list §8.8. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F1119.** Read from the disassembly plus a relocation count.

---

## D150 🐛 💤 the INFO1a arm calls `detectorinit` twice with only the warm-up differing and nothing reading between, so the first call — and the warm-up of 10 it installs — is discarded

*Task #98 sweep, from fix list §8.9. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F442.** Reasoned from the callee writing nine fields and reading none.

---

## D151 🐛 💤 `setfinalrate` has no arm for receive rate codes 1, 6 and 7 and silently leaves the previous rate in place; its own comment says so

*Task #98 sweep, from fix list §8.10. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F442.** Read from the disassembly and the original's comment.

---

## D152 🐛 💤 `CALLPROG_DIALING` (code 3) is computed and then filtered out by a whitelist of 4, 12 and 14, so it is never reported

*Task #98 sweep, from fix list §8.11. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F60.** Read from the tail's message filter.

---

## D153 🐛 💤 `toneiir_create(state, NULL)` builds a filter whose numerator is all zeros, so the default configuration cannot pass a signal

*Task #98 sweep, from fix list §8.12. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F46.** From relocations at 0x61a0, 0x61a4, 0x61c8 and 0x6244.

---

## D154 🐛 💤 K56flex is shipped as names only — constructor, destructor, resets, phase-3 entry and `setMinMaxRates` are bare `ret`, and the demodulator returns the constant 5

*Task #98 sweep, from fix list §8.13. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings F1090, F381, F318.** All 24 symbols disassembled with sizes.

---

## D155 🐛 💤 and its callers treat it as live: `k56FlexPhase34`'s Ja and MP completion arms (78 and 57 bytes) can never execute, because both bit sources are three bytes of `xor %eax,%eax; ret`

*Task #98 sweep, from fix list §8.14. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F281.** Measured twice — the byte-level stub, and 55 lines added to coverage of which 48 are covered, the seven uncovered all in these two arms.

---

## D156 🐛 💤 the ANSam phase-reversal detector's window is wider than the spacing real ANSam produces, so its report has never fired

*Task #98 sweep, from fix list §8.15. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F169.** Observed never to fire in any test.

---

## D157 🐛 💤 `FloatFIR`'s one-at-a-time tail can never run — every writer of `taps` masks it to a multiple of four

*Task #98 sweep, from fix list §8.16. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F234.** Reasoned from the writers; corroborated by a surviving mutation.

---

## D158 🐛 💤 txstate 71 runs the scrambler, stores the quadrant, and then transmits `vect4[0]` — the load at 0x64257 has no index register where 86's at 0x63e40 scales by four

*Task #98 sweep, from fix list §8.17. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F340.** Read from the two addressing modes; the arm is differentially tested.

---

## D159 🐛 💤 fifty-seven of table 1's eighty-two dispatch entries point at the per-sample loop's own bottom, so a txstate with no arm never advances and the loop **spins forever**

*Task #98 sweep, from fix list §8.18. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Already in this register as D59 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Findings F287, F420.** Demonstrated, not argued: `V34HS_HANG=1 ./build/test/t_v34hsstep` drives txstate 6 `ANSAM` with the cursor below the limit and the run exits 3. The harness arms `SIGALRM` around every step because of it.

---

## D160 🐛 💤 `probeselect` has no 2743-baud arm, and `chkForceBaudRate` writes `allow[0]` and `allow[1]` that nothing reads — one standard rate the object can be configured for and can never select

*Task #98 sweep, from fix list §8.19. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding F218.** Two independent readings; `D35` is the same gap from the other side.

---

## D161 🐛 💤 `vpcm_create`'s max-rate clamp at 0x3b65 is 0xdac0 = 56000, the same number as slmodemd's `MODEM_MAX_RATE`, so it never fires under the shipped host

*Task #98 sweep, from fix list §8.20. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings F823, F824.** Driven: raising the host ceiling to 64000 makes the clamp fire.

---

## D162 🐛 `V92Jd`'s constructor leaves one constellation bit unwritten

**Where:** `src/pump/v90/V92Jd.cpp`, `V92Jd::V92Jd(V90Parameters *)`, from
0x11c80.

**What the original does:** stores a literal 0 into `bits[47]` and never
writes `bits[48]`, so that byte keeps whatever was in the storage the object
was built over.

**Why it looks wrong:** `V90Jd`'s constructor, which is otherwise the same
function, fills BOTH of those bytes — `bits[47]` from
`V34_PHASE4_CONSTELLATION` and `bits[48]` from `V34_RRN_CONSTELLATION` — and
the header's bit map calls the pair "constellation size, 2 bits".  A two-bit
field with one bit initialised and one bit inherited is the shape of a slip.

**Reachable?** On every construction.  Whether it can be OBSERVED is a
different question and is not settled here: `packJdData` is not written yet
and may fill `bits[48]` before anything transmits the vector.  **Unmeasured**,
and it stays that way until that member is read.

**Not fixed.** `t_v92jd.cpp` seeds the slot with varied bytes and compares the
whole object, so a reconstruction that helpfully cleared `bits[48]` fails
rather than passes.  Finding F1223.
## D163 🐛 💤 `V90CP::printNofRecievedMpMpNot` prints `"V90MP: received %d MP, %d MPNot"` — the CP class's debug line names the other class

*V.90/V.92 message-parameter batch. **Reachability: unmeasured** — diagnostic only, and only above `dsplibs_debug_level > 1`. Status: CONFIRMED — the literal at `.rodata.str1.4+0xd6b0` is byte for byte `V90MP`'s at `+0x5a34`. Fix class: documentation only; reproduced, not corrected.*

**Finding F1239.**

**RENUMBERED.  This entry was committed as `D162` on its own branch** and so
was the `V92Jd` entry above it: three of the nine parallel construction-path
batches independently picked 162 as the next free number, which is what a
shared append-only register does when only the finding numbers are blocked
out.  D-number blocks were assigned to the remaining batches when the second
collision surfaced.  CLAUDE.md's rule for a renumbering is that the entry says
what it used to be called, because a reference that still resolves but now
points at the wrong entry is the one thing `tools/refcheck.py` cannot catch.
## D164 ⚠ 💤 `V90SpectralVerifier`'s constructor initialises +0x28 but not +0x20 or +0x24 -- the accumulation counter and the running flag `startAccumulation` and `process` both test -- so a verifier that is constructed and never `reset()` runs on allocator garbage

*Spectral-group lifecycle batch. **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: one store.*

**Finding F1240.** Asserted in test/unit/t_v90spectral.cpp against the seeded
bytes, so the omission is measured; whether any caller reaches an accumulation
without a `reset()` first is not.

**RENUMBERED at the merge**, from `D162`, the number this batch committed on its own branch: three of the nine parallel construction-path batches picked the same
next-free number out of this file, and the two before it took D162 and D163.
CLAUDE.md's rule is that the entry says what it used to be called, because a
citation that still resolves but now points at the wrong entry is the one
thing `tools/refcheck.py` cannot catch.

---

## D165 ⚠ 💤 `V90SdDetector`'s constructor stores its third float argument into +0x10 and no member of the class ever reads it

*Spectral-group lifecycle batch. **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: none until a reader turns up.*

**Finding F1241.** All six members were scanned for a load at +0x10 and none
has one; a seventh reader outside the class would have to reach it through a
`V90SdDetector *`, which has not been swept.

**RENUMBERED at the merge**, from `D163`, the number this batch committed on its own branch: three of the nine parallel construction-path batches picked the same
next-free number out of this file, and the two before it took D162 and D163.
CLAUDE.md's rule is that the entry says what it used to be called, because a
citation that still resolves but now points at the wrong entry is the one
thing `tools/refcheck.py` cannot catch.
## D190 🐛 💤 `V90Phase3Modulator`'s constructor stores its `V90Parameters *` at +0x50 and no other symbol of the class reads it — the V.92 sibling's equivalent field IS read, by `reset`

*Batch: the two Phase 3 modulator constructors. **Reachability: unmeasured.** Status: unmeasured — what reads +0x50 from OUTSIDE the class was not looked for.*

**This entry was numbered 162 in this register before merge and was renumbered
to 190** on the coordinator's assignment, three batches having picked the same
next-free number independently. Nothing outside this batch ever cited it, and
the one finding that does — 1257 — was updated in the same commit. (The old
number is spelled out rather than written in its usual form because
`tools/refcheck.py` reads any `D` followed by digits as a live reference and
would report it dangling.)

**Finding F1257.** Measured, not inferred: every one of the nineteen
`V90Phase3Modulator` text symbols was disassembled and searched for a `0x50`
displacement. Three hit, and only three — the two constructor copies, both
`mov %reg,0x50(%ebx)` with `%ebx` as `this`, and `reset`, whose `mov
0x50(%esp),%ebp` is a stack slot and not the object at all.
## D170 🐛 `V92deleteConstellations` and `V92deleteFilterCoefficients` free all ten arrays and null none of them, so `V92ParamsInfo` comes back from either deleter holding ten dangling pointers

*V.92 leaf allocators. **Reachability: `unmeasured`.** Status: CONFIRMED from the disassembly — there is no store to any of +0x5c..+0x68 or +0x84..+0x98 in either function. Fix class: documentation only until a caller is found that deletes without freeing the block.*

**Finding F1226.** The one caller in the object, `V92Modem`'s destructor, frees the block itself immediately afterwards, so the dangling values are never read there. Reproduced, and `t_v92alloc.c`'s `run_delete_live` asserts they survive.

**This entry and D171 were drafted under two higher numbers and renumbered before the branch left its worktree**, so no citation to the old pair exists anywhere and CLAUDE.md's renumbering rule has nothing to protect. The old-to-new mapping is in the commit message and deliberately not here: `refcheck.py` reads a bare `D` and digits as a citation, a retired number would be reported dangling, and writing it without the `D` is worse — a bare number is read as a FINDING reference and both of these resolve to real and unrelated findings, which is the silent failure CLAUDE.md warns about.

---

## D171 🐛 `V92createConstellations` and `V92createFilterCoefficients` store ten `sysdep_malloc` results without testing one of them, and their caller carries on regardless

*V.92 leaf allocators. **Reachability: `unmeasured`.** Status: CONFIRMED — six and four consecutive `movl $size` / `call` / `mov %eax,off(%ebx)` with no `test` between. Fix class: documentation only.*

**Finding F1226.** The contrast is inside the same object: `vpcm_create` DOES test what `K56FLEX_Create` returns, at .text+0x3b31, and branches into a failure unwind.

*Numbering: D170 and D171 are the block this batch was assigned, and the gap below them is not this batch's to fill. Three sessions independently picked the number after D161 as "the next free one", which is exactly why blocks are now handed out rather than taken; the numbers between D161 and D170 are reserved for resolving those collisions. Bare `D` and digits read as a citation to `refcheck.py`, so they are not spelled out here — the same trap the D64 gap note records.*
## D180 🐛 `V92Precoder` and `V92PreFilter` run a filter constructor over `sysdep_malloc`'s return without checking it, so a failed allocation constructs a `FloatFIR` through a null pointer

*Constructor batch, task "V.92 coder and pre-filter constructors". **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: needs a decision — the allocator never fails in the harness.*

**Finding F1245.** `movl $0x14,(%esp); call sysdep_malloc; call FloatFIR::FloatFIR` with nothing between the two, four times across the pair. `FloatFIR`'s constructor stores five fields, so the fault is at the first store and not deferred. The same shape as D5's family and the same as `FloatFIR`'s own unchecked history allocation.

---

## D181 🐛 💤 `~V92Precoder` and `~V92PreFilter` do not null what they free, and `V92Precoder::reset(V92MappingParams *)` is the one writer that skips those two words

*Constructor batch, task "V.92 coder and pre-filter constructors". **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: documentation only unless a caller is found that resets after destroying.*

**Finding F1245.** The destructor leaves +0x68 and +0x6c holding freed addresses; `reset` rewrites +0x04..+0x64 and +0x70, +0x74 and deliberately leaves those two alone, which is right for a live object and would use a dangling pointer on a destroyed one. `V92Transmitter::~V92Transmitter` frees the precoder immediately after destroying it, so no path in the object reaches it — hence 💤.

---

## D175 ⚠ `V90Equalizer`'s constructor stores fifteen `sysdep_malloc` returns and checks none of them, then hands them to `reset` to write through

`FloatFIR`'s constructor tests its one allocation; this one tests none of its fifteen, and its last act is a tail call to `reset`, which writes through six of them. A negative `LINEAR_EQU_HISTORY_LENGTH` reaches the same place by another route: the length is a SIGNED divide by two, so the allocation size is enormous, the return is null, and `reset` writes through it. Reproduced as written (finding F1230). `unmeasured`. *(Drafted under one of the three numbers immediately after D161 -- the obvious next-free ones, which three parallel batches all picked at once -- and renumbered into this batch's assigned block before it was committed. Nothing cites the draft numbers; the numbers between D161 and D175 are reserved for resolving collisions already committed.)*

---

## D176 🐛 `V90PreFilter`'s "table length" banner prints the maximum index, not the length

*Titled "`V90PreFilter`'s constructor can never select the last entry of
`dataBase`" until 2026-08-23. That title was the defect claim itself and it
is refuted below; the number is kept and the old wording recorded here so
citations still resolve to something that makes sense.*

**REFUTED BY `docs/deviation-triage.md`, family 3, from the object.** The
`dec %ebx` at `.text+0x44dd9` converts a COUNT into a MAXIMUM INDEX and the
compare that consumes it is `jle`, not `jl`. `dataBase` is 612 bytes at
`.data 0x6760` with a 0x24 stride — seventeen entries, sixteen named, index 16
the empty terminator — so the loop leaves 16, `dec` gives 15, and
`codecType == 15` (`"Squeezer_545A_ITE"`, the last real entry) is **accepted**.
No named entry is unreachable. What IS off by one is the diagnostic, which
prints the maximum index under the label "table length"; that is a real
tier-1 defect and it belongs with the other diagnostics. **The error
originates in finding F1233's step 4, which states the same thing, so the
correction is owed there too.** The rest of 1233 stands.

The text below is the original claim, kept because a wrong route is worth as
much as a right one:

The table is walked to its first empty name and the count is decremented before `codecType` is compared against it, so the highest index the constructor will accept is `count - 2`, and an index of `count - 1` -- a real, named entry -- is rejected with "External Hardware Codec Index exceeds table length" and replaced by 0. The message prints the decremented number as "table length", which is where the off-by-one shows. Reproduced (finding F1233). `unmeasured`. *(Drafted under one of the three numbers immediately after D161 -- the obvious next-free ones, which three parallel batches all picked at once -- and renumbered into this batch's assigned block before it was committed. Nothing cites the draft numbers; the numbers between D161 and D175 are reserved for resolving collisions already committed.)*

---

## D177 `V90PreFilter`'s out-of-range banner has one doubled `*`

`.rodata.str1.4+0xb904` is 102 characters of `*#` except at +0xb930, where the pattern reads `*#**#*`. Cosmetic, in the object's own bytes, transcribed rather than tidied. `unmeasured`. *(Drafted under one of the three numbers immediately after D161 -- the obvious next-free ones, which three parallel batches all picked at once -- and renumbered into this batch's assigned block before it was committed. Nothing cites the draft numbers; the numbers between D161 and D175 are reserved for resolving collisions already committed.)*


---

## D178 🐛 `V90PreFilter`'s constructor bounds `codecType` from above and not from below

The range check is `if (codecType > count - 1)`, and there is no other. When `HW_CODEC_TYPE` is negative the constructor stores its own `__tHardwareCodecTypes__` argument straight into `codecType` with no floor, so a negative argument reaches the field intact -- and `isV90WithEia6`, `autoSelection` and `selectFilter`, all three already written in `src/pump/v90/V90PreFilter.cpp`, index `dataBase[codecType]` with no gate of any kind. The constructor is therefore where a wild table index is STORED, and the reads that follow it are at the shipped debug level, not behind `dsplibs_debug_level > 1` like the constructor's own `dataBase[codecType].name`.

Reproduced (finding F1233). `t_v90prefilter.cpp`'s constructor sweep deliberately stops at zero: a negative index has each side reading below the base of its OWN table, so the object comparison and the transcript would both fail for a reason belonging to the harness rather than to the function, and the test would be reporting the fixture. `unmeasured`.

---

## D179 🐛 `V90Equalizer` with fewer than four taps allocates nothing and then writes to it

`linearEquLength` is `len & ~3`, so any `len` below four gives zero, and the constructor asks `sysdep_malloc` for zero bytes. Its tail call to `reset` then clamps the cursor with `if (linearEquLength - 1 < cursor)` -- unsigned, so `0xffffffff < 0` is false, the clamp does not fire, and `linearEquCoefs[0] = 1.0f` is written into a zero-length block. Both halves are the object's: the mask is in the constructor and the clamp is in `reset`, and neither is wrong on its own.

Reproduced, and DRIVEN: `t_v90equ.cpp`'s constructor sweep includes lengths 0 and 1, so both sides take this path in every trial that uses them. It survives only because glibc's smallest chunk has twelve usable bytes. `unmeasured`.
---

## D185 🐛 💤 `GenericToneDetector`'s constructor divides by its tenth argument twice with no zero guard, so a `blockLen` of 0 traps before the object exists

*The constructor/destructor batch. **Reachability: unmeasured.** Status: CONFIRMED from the disassembly. Fix class: documentation only — reproduced, not repaired.*

**Finding F1253.** Both sites are `div %edi` at 0x10704 and 0x1071b with `%edi` loaded straight from the argument slot at `0x58(%esp)`, and there is no test of it anywhere in the 267 bytes. `test/unit/t_gtonedet.cpp` sweeps seven nonzero divisors and says in its file comment that zero is excluded on purpose rather than avoiding it quietly. Nothing in this tree constructs a `GenericToneDetector`, so which callers exist and what they pass is unmeasured — that is what the marker means here, and it is why the entry claims a trap rather than a live defect.
## D195 🐛 `VPcmV34Create`'s second `sysdep_memset` clears 0x79c bytes at +0x264 that the first one, 0xac4c bytes at +0, has already cleared — the receiver is zeroed twice and no path reaches the second without the first

*Batch: `VPcmV34Create`. **Reachability: unmeasured.** Status: unmeasured. Fix class: documentation only.*

**Finding F1260.** Transcribed and kept; `test/mutations/vpcmcreate.json`'s "the redundant second memset is dropped" is the recorded survivor that says it is unobservable.

---

## D196 🐛 `VPcmV34Create` re-loads `sess + 0x612c` between the two byte stores it makes through it, so the two stores are not guaranteed to reach the same record

*Batch: `VPcmV34Create`. **Reachability: unmeasured.** Status: unmeasured. Fix class: documentation only.*

**Finding F1260.** Read from the disassembly; the reconstruction keeps both loads rather than folding them.

---

## D275 🐛 `V92Jd`'s two receive directions share one pair of state bytes, so they cannot run concurrently

**Where:** `src/pump/v90/V92Jd.cpp`, `unPackJdData` and `unPackJdPhaseData`.

**What the original does:** both unpackers store their state to +0x00 and
+0x01 (`mov %dl,(%ebx)`, `mov %al,0x1(%ebx)` in each) while switching on two
DIFFERENT state words, +0xd4 and +0xd8.

**Why it looks wrong:** a data bit fed part-way through a phase message
advances the phase message's own run length and payload count.  The two
directions are separately switched and jointly stated.

**Reachable?** Only if a caller interleaves the two unpackers.  **Unmeasured**
-- nothing this tree has written does, and whether the object's own callers can
is not established.  The offsets are measured; that a receiver cannot run both
at once is the one-line inference from them.

**Not fixed.**  Finding F1397, which is this entry's text; allocated here from
the leaf-sweep batch's unused block.

---

## D276 🐛 A preamble longer than seventeen 1 bits desynchronises the Jd message

**Where:** `src/pump/v90/V90Jd.cpp` and `V92Jd.cpp`, the unpackers' state 0/1.

**What the original does:** state 0 leaves on the seventeenth 1 bit
(`cmp $0x10,%dl; jbe`); state 1 treats a further 1 as an error and resets with
`movb $0x0,(%ebx)` -- ZERO, not one -- so the offending bit is NOT counted
toward the next run.

**Why it looks wrong:** a transmitter whose preamble runs to eighteen ones
leaves the receiver needing a fresh seventeen.  Counting the offending bit
would have resynchronised on the next.

**Reachable?**  **Unmeasured** against a real peer; V.90's own preamble length
is not established here.  Driven in both test files, so the behaviour is
reproduced deliberately rather than by accident.

**Not fixed.**  Finding F1397.

---

## D277 🐛 The Jd unpackers' accepting state has no exit, and completes again every 256 bits

**Where:** `src/pump/v90/V90Jd.cpp` state 8; `V92Jd.cpp` states 9 and 8.

**What the original does:** the accepting state only increments `unpack[1]`.
Nothing writes the state word and there is no transition out.

**Why it looks wrong:** a caller that keeps feeding bits after a completed
message gets a spurious completion every 256 bits, on stale payload, because
the byte counter wraps back to 0x34.

**Reachable?**  On any caller that does not stop or reset at completion.
**Unmeasured** -- whether the object's callers do is not established.  The
tests' `late_complete` counter asserts exactly this behaviour.

**Not fixed.**  Finding F1397.

**And one hazard that is deliberately NOT an entry:** `vec[unpack[1]] = bit`
is bounds-checked nowhere in any of the three unpackers, and it is
UNREACHABLE in normal use -- from a constructed or reset object the counter is
capped by each storing state (16 / 27-28 / 32 / 48), and the only state that
walks it past 71 is the accepting one, which never returns to a storing state
without a reset.  Written down because the next reader will see the unbounded
index and reach for a fix.

---

---

# Part III — looked at and judged NOT a defect

This is part of the deliverable, not an offcut. A register that only
records hits cannot be audited, and four of the items below had already
been filed as defects somewhere before being read properly.

## A retraction that is still cited as live (D28)

**7.1 `D28` is RETRACTED and finding F180 still cites it as live.** Finding F180
says `V34EchoReportCoeff` "dumps a hardcoded 144 coefficients whatever `taps`
says (D28), which out of the shared 32-entry array runs 48 shorts past the end
of the struct". **Finding F98 retracted that**, and so does the register:
`V34InitializeImplementationSpecific` sets both cancellers' tap counts to 0x90
— 144 — at 0x71dc6 and 0x71e1e, so the dump is sized to the array exactly and
there is no over-read. `(144 / 6) * 6` is 144, so the scan's rounding-down is a
no-op too. **This is not a defect.** It is listed here because it is the one
place in the record where a retracted claim still reads as current, and because
it was one of this task's seven starting pointers.

## The rest, in two kinds

Two kinds, and they carry different information.

### It looked like a defect in the object and is not

* **Finding F297** — "Had `nofBits` been anywhere earlier the object would
  overrun its own buffer on a descriptor it can represent". It is not
  anywhere earlier: `bitVector` holds 2,700 entries and the largest descriptor
  the fields can describe is 2,654 bits. A counterfactual, and in fact the
  finding's point is that two independently derived offsets agree.
* **Finding F180 / `D28`** — see the retraction note above. Retracted by finding F98 and by the
  register; `taps` **is** 144, so there is no over-read.
* **Finding F230** — "the object is compared whole, and so is a guard past its
  end". Test methodology: the guard exists so that a store overrunning the
  object fails rather than passing silently.
* **Finding F394** — "the staged clear runs to eleven words rather than ten".
  A surviving *mutation* of the reconstruction, not the object; the eleventh
  word is +0xabc2, which the statement after the loop zeroes anyway.
* **Finding F423** — "the run at +0x25da wrong has +0x25d8 PAST the end". A
  deliberately chosen test seed to separate two guards in series.
* **Finding F139** — its own heading says `receiver` "found a bug in
  `V34TimingFilter`". Read in full, the bug is the **reconstruction's**: the
  object multiplies the register the store came out of, which still holds the
  full 32-bit value, and we read the truncated short back from `iir[][0]`. The
  object is self-consistent; the state words overflowing a short is a fact
  about the object with no wrong behaviour attached to it.
* **Finding F227** — `getMPrecvdBits` calls `txrxdmainit` twice inside the V.90
  branch. The finding rules it out itself: it is idempotent, so this is wasted
  work rather than a defect, recorded so a reconstruction that tidied it would
  still be caught.
* **Finding F425** — `moh_recvd` compared signed. Proved equivalent over every
  input; both readings reach the same block.
* **Finding F1119** — `V34DisconnectThreshTable`'s unsigned `cmp $0x7 / jbe`
  looks like a signed/unsigned slip and is the **correct** bound: it rejects
  negative and above-7 alike and defaults to index 3.
* **Finding F819** — the V.92 `FRNDINT` ceiling has no guard on the sign, and
  both forms agree on all 256 reachable values.
* **Finding F216** — `V34SetupModulator`'s V.90 arm. This is the *retraction* of
  a dead-branch claim (`D31`), not a defect: the arm is live.
* **Finding F122** — the AGC integrator seeded from a stale return register.
  Retracted as `D34`; refuted by `xor %eax,%eax` at 0x5ac13.
* **Findings F92, F95, F229, F650, F631, F874, F821, F865, F875, F837, F1110, F1101** —
  each is an inconsistency, an asymmetry or a redundancy in the original with
  no wrong behaviour shown: a zero sample treated as positive by one test and
  negative by another with no consequence measured; an equaliser precision
  split the finding calls "most likely deliberate"; one CRC register shared by
  two packers that nothing reads between packs; one field read at two
  signednesses that agree over the only values it holds; a GCC partially-dead
  store; a block memset twice; a field nothing reads; the Numerical-Recipes
  one-based array convention; a banner asymmetry invisible at debug level 0; a
  dead first write of six flags; a deliberate drain.
* **Finding F620** — `c1959`'s poles are written high-then-low where the other
  seven go low-then-high. An original transcription slip and harmless: the pair
  is summed, not ordered, and the midpoint is still exactly 1959.
* **Finding F1108, item 5** — both window lengths scaled by the linear
  equaliser's length. Measured to be what the blob does (using `dfeLength`
  breaks 2,064 checks); that it is *wrong* rather than intended is not
  established.
* **Findings F722, F730, F750, F418, F352** — branches the object emits that nothing
  can reach, all inlined "already there" guards the optimiser could not see
  through. Compiler-emitted dead edges, not authored defects.
* **Finding F428's `(cap * 7) >> 14`** — looks like a divide by 2340 where 2400
  was meant. It is a strength-reduced divide that agrees with `/2400` over
  every rate index the object uses.
* **Finding F80** — 60000 in a signed 16-bit timeout field. Works reliably
  because both comparisons are unsigned.
* **Finding F61** — the initial message value 18 is a never-matches sentinel,
  only compared and never used as an index.
* **Finding F289** — "the per-sample transmit route is not a function of the
  object" was retracted by finding F319 as `D60`. The sensitivity is real but it
  is the *placement* of the five pointed-to blocks, not the route; D137 has it
  under `D61`. Do not file 289 as a separate defect.
* **Findings F299, F263, F318** — stores nothing can observe: `nofBits = 0` before
  the packer writes it, `bits[crcAt] = 0` re-written by the preceding section's
  framing zero, `is_short = 0` masked by `v34modeminit`'s unconditional clear.
  All carried as equivalences, and the third measured to flip to caught when
  that clear is deleted.
* **Finding F282** — the unmasked `sar %cl` on a sign-extended `vect_idx`. The
  blob and the reconstruction agree at every swept value including -1 (shift
  30) and 100, and nothing shows a caller producing an out-of-range index.
* **Finding F305** — cases 4 and 7 gate on `!= 0x10` where the counter advances
  by 2, which would step over the constant from an odd start. Every writer in
  range advances by two and nothing produces an odd value, so no defect is
  shown — but this is the thread to pull if a `+= 1` writer of +0x3a4 appears.
* **Finding F317** — "a configuration large enough to overflow prints the
  truncated number". The field is 16-bit and the object prints what it stored,
  so the `cwtl` is consistent rather than wrong.
* **Findings F270 and F314** — `V34DisconnectThreshTable`'s out-of-range fallback
  to entry 3 and the unsigned `jbe` that also rejects negatives are deliberate
  and consistent across two independent call sites.

### It is a defect but it is not the object's

* **The reconstruction's own, since fixed** — findings F69, F73, F130, F151, F168,
  F170, F183, F185, F203, F204, F219, F546, F549, F573, F591, F593, F613, F748, F750, F721,
  F781's headline, 879's divide, 1021, 1107, 1112, 356a. Each was caught by the
  differential test or by codegen comparison; the object was right.
* **Tooling** — `dis.py`, `refcheck.py`, `offcheck`, `extcheck`, `compare.py`,
  `closure.py`, `mutate.py`, `reanchor.py`, `coverage.py`, `debugaudit.py`,
  `vparse.py`, `cppstruct.py`, `decompile.sh`, and objdump's FDIVP/FDIVRP swap.
  Findings F39, F43, F46, F49, F134, F245, F355, F370, F432, F541–545, 555–557, 570–572,
  618, 619, 637–639, 670, 690, 700, 705, 818, 860, 862, 872, 907, 940, 941,
  985, 1004, 1065, 1103, 1114 among others.
* **Test fixtures and mutation bookkeeping** — findings F241, F242's seeding,
  341, 343, 349, 362, 364, 366, 374, 404, 416, 427, 429, 434, 445, 592, 713,
  714, 715, 718, 723, 724, 729, 735, 743, 744, 746, 753, 788, 920, 987, 1000,
  1064. A surviving mutation is a statement about the suite.
* **Third-party** — finding F40's note that SpanDSP 0.0.6 ships the Bell 103
  presets swapped.
* **Two sweeper inferences that the record does not make, and one it
  contradicts** — a reading of finding F19 in which the ANSam phase reversal is
  a no-op half the time (the record states the hop is 180° and this depends on
  whether a full phasor cycle spans 0x8000 or 0x10000; the record's reading is
  not overturned by an inference); a join of finding F1146's fork `IODELAY` 48
  with finding F1022's threshold of 86, which finding F1146 does not draw; and a
  reading of finding F424's XMITMP self-pointer as an object bug, where the
  finding presents it as a reconstruction ambiguity. All three are recorded as
  *not established* rather than filed.


---

# Appendix B — reachability of D70 onwards

*Task #98. Appendix A asks whether each claim is measured; this asks whether
anyone can hit it. Both are needed and neither implies the other.*

**FIRES TODAY — 14.** These change what a user sees, on a call
nobody has to construct.

> D73 D74 D76 D77 D78 D79 D80 D81 D82 D83 D84 D85 D86 D87

**NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES — 26.**
The object is safe only because of what its callers happen to pass, and in
most cases nothing says so anywhere the caller can see. Host-side validation
is the usual answer.

> D70 D71 D88 D89 D90 D91 D92 D93 D94 D95 D96 D97 D98 D99 D100 D101 D102 D103 D104 D105 D106 D107 D108 D109 D110 D111

**LATENT — 31.** The mechanism is in the object; no path to it is
known. Documentation only until one is found. Several are one reconstructed
caller away from being reclassified, and `PPSEG`'s unit mix (D112) is the one
to watch: it is latent at every symbol rate except 2400, and essentially every
real connection negotiates something else.

> D72 D112 D113 D114 D115 D116 D117 D118 D119 D120 D121 D122 D123 D124 D125 D126 D127 D128 D129 D130 D131 D132 D133 D134 D135 D136 D137 D138 D139 D140 D141

**CANNOT FIRE — 21.** A gate or an arm no input reaches. None
misbehaves; each is a place where the author wrote something that was never
delivered, and that is why they are recorded rather than deleted.

> D75 D142 D143 D144 D145 D146 D147 D148 D149 D150 D151 D152 D153 D154 D155 D156 D157 D158 D159 D160 D161

**The grade is a claim and can be wrong in one direction cheaply.** Moving an
entry from LATENT to FIRES TODAY needs one named caller. Moving it the other
way needs a proof of unreachability, which is why CANNOT FIRE is the smallest
group and the only one where the compiler was used as a witness (gcov marking
a body NOT EXECUTABLE settles D142 and D143).

---

# Appendix C — would fixing any of this improve connect reliability or rate?

*The owner's question, answered per entry rather than in general. Two bench
measurements are the thing to explain, and both are recent and real:*

* *Five IDENTICAL V.34 calls at one setting: **4 of 5 connected**, at 14400,
  14400, 4800, none, 14400.*
* *The far end reports **33600/ARQ on every successful call** while our side
  reports 14400 or worse. Consistent across five calls and two modems.*

## The short answer

**Most of this register cannot affect a V.34 call, and saying so is most of the
value.** Of 156 entries, the great majority are in Bell 103, V.21, V.22, V.23,
the call-progress and dialler code, the tooling-adjacent margins, or in arms
nothing reaches. Fixing them would change nothing a user sees.

**The candidates that survive the filter are all in the RECEIVE path**, and
that is not a coincidence worth glossing over: an asymmetric rate — them 33600,
us 14400 — points at our receiver, because that is the direction whose quality
*we* report and *they* adapt to. Every entry in the ranked list below sits
there.

**Before anything else, the most likely explanation is not a bug at all.** V.34
negotiates the two directions INDEPENDENTLY. A link that genuinely runs 33600
one way and 14400 the other is a correct report from both ends, not a
contradiction, and asymmetric impairment is the normal state of a
hybrid-plus-VoIP path. Rule that in or out first — it costs one call with the
two directions' rates read from the same end — because if the link really is
asymmetric then nothing below applies and the effort belongs in the transport.

## The one that is definitely NOT it

**D4 is out, and this is now proved rather than assumed.** It is the register's
only defect with a measured user-visible effect (zero reciprocal → AGC drives
the block to silence → receiver never regains lock, 46 of 700 blocks), so it
attracts attention. Its full caller closure, counted from the blob's
relocations: `FPM_div` ← `FPM_AGC_agc`, `FPM_atan`, `V32FP_status`;
`FPM_AGC_agc` ← seventeen functions across Bell 103, V.21, V.22, V.23, V.17,
V.27, V.29, V.32; `FPM_atan` ← `FPM_FSE_receive`, `FPM_SRE_recover`,
`V22_FSE_receive`, `V22_SRE_recover`, and those four are called only by
`DemodDataV17/V27/V29/V32`. **Nothing in V.34, V.90 or V.92 reaches it.** D4 is
serious for the legacy pumps and irrelevant to this problem.

## Would fixing it change observable behaviour at all?

Three groups, and only the third is worth any effort.

**Group 1 — no observable change, by construction. Fixing buys robustness and
nothing else.** D1 is the worked example: `FPM_sqrt` reads one past its table
and gets the *right answer*, because the neighbouring table starts with 32768
which is exactly `sqrt(1.0)` in Q15. The 21 CANNOT-FIRE entries are here too,
as are the entries recorded as equivalent mutations — a dead store is by
definition not observable. Roughly 30 entries.

**Group 2 — changes behaviour, on a path this call never takes.** Everything
Bell 103, V.21, V.22, V.23, dialler, call progress, cadence, V.8-only,
K56flex, V.90/V.92-only. Includes several genuinely bad defects (D4, D6, D11,
D13, D22, D80, D81, D82) that will matter the day somebody uses those pumps and
matter not at all today. Roughly 100 entries.

**Group 3 — changes behaviour in the V.34 path.** What follows.

## Ranked: could it touch CONNECT SUCCESS or RATE?

### F1. D77 — V.34 does not connect below `IODELAY` 86. CONNECT. Measured.

The only entry with a **measured** connect failure in V.34. Arm 47
`TX_PHASE2_ANS` reads an all-ones `fsk.sr` from a carrierless slicer as
"repeated INFO0" while the far end is correctly silent, and **loses the race by
one block — about 22 samples in 200**. 85 fails, 86 connects.

Why it stays top of the list even though D74 already set the default to 120: a
one-block race is a *cliff*, and the margin from 120 down to 86 is consumed by
anything that makes the effective delay vary. A SIP/RTP path with a jitter
buffer is exactly that. **4-of-5 connects is the shape a marginal race
produces**, and this is the only such race in the record.

*Fix:* already host-side (`SLMODEMD_IODELAY`). The action is to measure, not to
code — see the experiment below.

### F2. D72 — the echo canceller may overrun its history at HIGH `IODELAY`. SUSPECTED.

The buffer is sized in the constructor from the delay at that moment;
`setEchoDelay` then raises the delay at runtime from `MDMPRM_IODELAY` **without
reallocating**, and `resetEchoHistory` zeroes the new, larger length. V.32
measurably works at 108 and 180 and fails at 240 and 300.

**Read 1 and 2 together: the usable band is bounded below by a connect race and
above by a suspected heap overrun.** If the transport pushes you up to escape
D77 you may walk into D72. That pincer, not either entry alone, is the most
actionable thing in this appendix.

### F3. D137 / D61 — the object's answer depends on where its memory is. UNPROVEN CAUSE.

**The only recorded mechanism in the object that can give different results
from identical inputs.** Byte-identical objects placed differently produced
different answers, and `v34handshakinit` left two fields holding
`ref_Convolve32` on one side and `ref_Convolve32 + 0x40` on the other — *the
same table, sixty-four bytes in, from identical inputs and identical code*.
Ruled out: bring-up, stack residue, x87 residue, absolute address, alignment,
object placement, object-to-block distance, neighbourhood contents within
32 KB. What is NOT established is which byte it reads.

If it holds outside the fixture, then which filter block the receiver selects
depends on where `malloc` put things, and that varies between processes. **That
is "five identical calls, three different outcomes" exactly.** It is also the
weakest-evidenced item here, and finding F324 records that once both sides are
brought up congruently the two pointers agree — so this may be a fixture
artefact and not a live nondeterminism. Do not quote it as established.

### F4. D112 — `PPSEG` adds symbols-at-baud to a count of 4-sample ticks. RATE, and worse the faster you go.

`0x68154` sums `q`, a count of symbols at the **negotiated** baud, with
`filtdelay`, a count of 4-sample ticks. **Dimensionally correct only at 2400
baud.** At 3429 the term should be `filtdelay * 3429 / 2400`; at `IODELAY` 216
that is 127 where the object uses 89 — **an error of 38 symbols in an
echo-adaptation start delay.**

Why this is the leading hypothesis for the *asymmetric rate*: the echo canceller
cancels our own transmit leaking into our own receive. Start its adaptation at
the wrong moment and **our receive** SNR suffers while our transmit is
untouched — so the far end sees us at 33600 and we report 14400. And the error
scales with symbol rate, so it is worst exactly where 33600 lives (3429 baud)
and vanishes at 2400.

**It predicts a discriminating experiment** (below), which is why it is ranked
above entries with better evidence.

### F5. D29, D30, D32 — a family that starts from uninitialised memory. VARIANCE.

* **D29** — `V34TimingFiltersInit` zeroes eighty *shorts* over a region holding
  a 40-short history followed by 40 *ints*, so **the upper twenty entries of
  the timing prefilter keep whatever was there**. Fires on every setup. The
  prefilter convolves 40 taps against a state whose upper half was never
  initialised; output is wrong for about 20 symbols. Consequence explicitly
  unmeasured.
* **D30** — `cursor` is seeded from the OLD `dline` field a few instructions
  before `dline` is written. On a first initialisation that field has never
  been written, so the cursor comes from uninitialised memory.
* **D32** — `V34ModulatorProcess` seeds its delay-line shift from a stale
  register.

Individually each is "unmeasured". Together they are three places where V.34
**acquisition starts from heap garbage**, and heap garbage differs between
calls. That is a mechanism for run-to-run variance that needs no unknown cause,
unlike item 3 — and D29 lands in *timing recovery during acquisition*, which is
precisely what decides whether you connect and at what rate.

**This is the cheapest hypothesis to test and I would test it first.**

### F6. D84 — the predictor rounds the real axis the wrong way, every symbol. RATE.

`receiver`'s complex predictor forms the real accumulator as
`b.hist_i - (0x2000 + a.hist_q)`, applying the rounding constant with the wrong
sign on the real axis only — half an LSB, on every symbol, at both call sites
(precoding and the adapting predictor). A systematic, *axis-asymmetric* bias in
the receive predictor costs SNR margin, and at 33600's constellation density
margin is what buys the top rate. Receive-only, so it fits the asymmetry.
Reproduced; the consequence is not separable from the rest of the chain without
a full-path measurement nobody has made.

### F7. D114 — the slicer's squared distance truncates to 16 bits. RATE, speculative.

A constellation point far enough from the target wraps to a small distance and
can win, producing a symbol error. Most likely during acquisition, before the
equaliser converges, which is when errors are most expensive. The metric exists
**twice** — inlined in `decoderv34` as well as in `decision` — so a fix to one
would not reach the other. Unmeasured against a real V.34 constellation.

### F8. The rest of group 3, and why they are lower

* **D26, D27** — echo canceller delay-line handling. Both argued dormant with
  numbers: D26's window is `taps` samples of provably zero output, D27's single
  wrap covers the whole domain in which the function returns anything
  meaningful (`lag <= 1513`). Fixing changes nothing.
* **D111** — the receive-rate completion. The five-way at 0x62f9c is the only
  writer of the rate and its floor, and it has **no default arm**: two
  uninitialised stack slots then drive about 1,600 bytes of rate-ladder
  arithmetic into the capability record. Catastrophic for rate *if* an
  `rx_baud` outside the five V.34 symbol rates ever arrives — and nothing shows
  one does. Worth an assertion precisely because the failure mode is "a
  plausible wrong rate" rather than a crash.
* **D87** — the restart path's inlined `SetINFO0dBits` drops the
  `v90_receiver` guard the real function has. Answering side, after a CRC
  failure in `DET_INFO`. Affects handshake robustness on a retry path.
* **D59** — the per-sample dispatch does not terminate on an unhandled state:
  it spins. Demonstrated (`V34HS_HANG=1` exits 3, and the harness arms
  `SIGALRM` because of it). This is a **hang**, not a bad rate — the candidate
  for the one call in five that produced nothing, *if* an unhandled state is
  reachable, which nothing shows.
* **D44** — `t3`'s tail stays at -1 for ring sizes 15, 17 and 18, which
  `MMaxTable` produces often. Settled harmless on transmit; open on receive,
  where `shellDemapper` indexes `t3[d1 + d2]` with nothing bounding the sum.
  Re-open when `demapFrame` lands.

## Three experiments, in the order I would run them

None of the above is measured on a live call; the register's evidence is
fixtures and disassembly. These are what turn it into an answer, and none needs
new hardware.

1. **Poison the heap.** D29/D30/D32 say acquisition reads uninitialised memory.
   Run the same call twenty times with the allocator's fill set to a different
   constant per group (`MALLOC_PERTURB_` is enough to start). **If the rate
   distribution moves with the fill pattern, the variance is in that family**
   and it is the answer. If it does not move, that family is out and item 3
   moves up. This is one afternoon and it is decisive either way.
2. **Sweep `IODELAY`.** Five calls each at 86, 100, 120, 150, 180, 240. D77
   predicts failures clustering at the bottom; D72 predicts failures appearing
   again at the top. A U-shaped connect rate confirms the pincer and hands you
   the setting; a flat one rules both out.
3. **Force 2400 baud.** D112 is exactly correct at 2400 and wrong in proportion
   to baud above it. If the rate ceiling drops (as it must) but **the
   call-to-call variance collapses**, that is strong evidence for D112 and
   therefore for the asymmetry. If the variance survives at 2400, D112 is not
   the cause.

## What I could not determine

* **Whether any entry here actually fires on the bench.** Nothing in this
  register has been observed on a live call; the strongest evidence in it is a
  differential fixture, which by construction compares us against the blob and
  is blind to whether the blob is doing the right thing.
* **The cause of the placement dependence (item 3).** Ruled out eight
  hypotheses; the byte it reads is still unnamed.
* **Whether the reported asymmetry is a defect at all.** V.34's two directions
  negotiate independently and this may simply be a correct report of an
  asymmetric line. Nothing in the object can settle that; one call with both
  directions read from the same end can.
* **What the far end's "33600/ARQ" actually names** — its transmit rate, the
  negotiated maximum, or the achieved receive rate. Three different claims, and
  which one it is decides whether there is an asymmetry to explain.

## D200 ⚠ `sessionTermination`'s "EVALUATION DISABLED" notice ends in a bare `\n` where its four siblings in the same function end in `\r\n`

*Batch of 2026-08-11, from `V90Demodulator::sessionTermination` (blob 0x1ab30). **Reachability: FIRES** whenever `TIMING_HISTORY_EVALUATION_ENABLED` is zero and the call reached the data state. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1274.** The other four strings this function passes to `edprintf` -- .rodata.str1.4 +0x4754, +0x47c0, +0x4814, +0x4868 and +0x48a0 -- all end `\r\n`; the one at +0x48ec ends `\n`. `edprintf` encodes its argument byte for byte, so the two produce a different character count on the diagnostic channel, and whether the manufacturer's decoder cares is not something this tree can measure. Reproduced rather than tidied.

## D220 ⚠ `V90Demapper::V90Demapper` stores both `sysdep_malloc` results without checking either, and its own destructor is the only code that ever tests them

*Batch of 2026-08-11, from `V90Demapper::V90Demapper` (blob 0x30640/0x30710). **Reachability: FIRES** on allocation failure only. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1303.** `mov %eax,0x1c(%esi)` and `mov %eax,0x20(%esi)` follow their calls with no `test`, so a failed allocation leaves the object holding NULL where `count_24` says there are `levels` elements; `~V90Demapper`'s two null tests then read as guards against a state the constructor is not supposed to be able to produce, and they are the only ones anywhere. With `levels == 0` the same two calls ask for `levels * 4` and `levels` bytes -- two zero-byte blocks taken and freed for nothing, which `t_v90demapctor.cpp` sweeps and both sides do identically. Reproduced rather than repaired.
## D225 ⚠ `V92EchoCanceller`'s constructor reads `echoDelay` and `echoLength` before anything has written them, and folds the difference into a value it then discards

*Batch of 2026-08-11, from `V92EchoCanceller::V92EchoCanceller` (blob 0x110a0 / 0x111e0). **Reachability: FIRES** on every construction. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1314.** The constructor's third instruction group is `setEchoDelay(params->V92_ECHO_INITIAL_DELAY)` inlined -- `mov 0x38(%esi),%ecx` at +0x35 loads `echoDelay` out of storage `sysdep_malloc` has just returned, and `add %edx,0x2c(%esi)` folds `delay0 - garbage` into `echoLength`, which is equally uninitialised. The result is DEAD: the tail call to `reset()` rebuilds `echoLength` from `filterLength`, `echoDelay` and the parameter block, and the history's allocated length at +0x1c is built from +0x18 and +0x38 and never from +0x2c. Reproduced rather than tidied, and reproducing it depends on `-fno-lifetime-dse` (finding F1224). Not D72: that entry is about the buffer this constructor sizes, not about what it reads before sizing it.

## D226 🐛 `V92EchoCanceller`'s constructor divides by its second argument and does not guard it

*Batch of 2026-08-11, from `V92EchoCanceller::V92EchoCanceller` (blob 0x110a0 / 0x111e0), +0x98. **Reachability: CANNOT FIRE** on the shipped path -- finding F1188 threads the argument back to `VPCMXF_Create`'s `(int)trunc(arg4 * 8.0 + 0.5)` and reads it as 40. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1188 already names this** as "a divide-by-zero, a different defect, not an overrun"; it is registered here so that the constructor's own entry exists. `div %edi` takes the block length straight from the argument, so a caller passing zero traps before the second allocation. Reproduced: the reconstruction divides in the same place and adds no check.

## D227 ⚠ the echo canceller's construction notice says "constraction"

*Batch of 2026-08-11, from `V92EchoCanceller::V92EchoCanceller` (blob 0x110a0), .rodata.str1.4+0x30ac. **Reachability: FIRES** whenever `dsplibs_debug_level` is above 1. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1310.** The string is `"V92EchoCanceller: constraction\r\n"`. It is the original's typo, it goes through `dsplibs_debug_printf` unencoded, and it is reproduced character for character -- `v92ec`'s "the construction notice's typo is corrected" mutation exists to make sure a later reader cannot quietly fix it.

## D228 🐛 a zero `V92_ECHO_FILTER_LENGTH` makes the constructor's `filterLength - 1` wrap, and at a zero initial delay the history allocation wraps with it

*Batch of 2026-08-11, from `V92EchoCanceller::V92EchoCanceller` (blob 0x110a0), +0x62 and +0x8f. **Reachability: CANNOT FIRE** at the shipped `V92_ECHO_FILTER_LENGTH` of 180 (finding F1188); it needs a parameter file, which slmodemd never supplies (finding F879). Status: `unmeasured`. Fix class: none proposed.*

**Finding F1312.** `lea -0x1(%eax),%ecx` stores `filterLength - 1` at +0x18 as an unsigned word, and the history length adds `echoDelay` to it. At `filterLength == 0` that term is 0xffffffff; an initial delay of 1 or more brings the sum back into range and the object allocates a small buffer, which the differential test drives on both sides. An initial delay of 0 leaves 0xffffffff, and the `sysdep_malloc` four instructions later is asked for 16 GB with no check on the result. Reproduced, unguarded, and not driven.

## D229 🐛 a negative `V92_ECHO_FILTER_LENGTH` becomes a four-billion tap count and an unchecked allocation

*Batch of 2026-08-11, from `V92EchoCanceller::V92EchoCanceller` (blob 0x110a0), +0x49..+0x85. **Reachability: CANNOT FIRE** at the shipped 180, for D228's reason. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1312.** The field is signed -- the object rounds it with `test/js/add $0x3/and $0xfffffffc`, which is `x / 4 * 4` on an `int` -- and the rounded value is then stored into an UNSIGNED `filterLength` and shifted left by two to size `echoCoeff`. Any negative parameter therefore asks `sysdep_malloc` for about 16 GB, and the result is used without a null test by the `reset()` this constructor tail-calls. Reproduced; the arm is the reason the signed rounding cannot be told apart from the `& ~3` that D72 and finding F1188 write, since no test that reaches it survives.
---

## D210 🐛 💤 `~V92Transmitter` nulls one of the six pointers it releases and leaves the other five dangling

*V.92 modulator batch. **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: documentation only unless a caller is found that destroys twice.*

**Finding F1281.** `movl $0x0,0x4c(%esi)` at .text+0x53aa6 follows the precoder's release and nothing follows the other five, so a second destruction frees +0x08, +0x48, +0x58, +0x50 and +0x54 again and skips +0x4c. The asymmetry is the object's; both halves are reproduced. D181 is the same shape one level down, and no path in the object reaches either -- hence 💤.

---

## D211 🐛 💤 `~V92BitsToSymbol`, `~V92Phase4Modulator` and `~V92Modulator` null nothing at all, so a second destruction double-frees fourteen buffers between them

*V.92 modulator batch. **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: documentation only.*

**Findings F1281, F1288.** Two pointers in the bit-to-symbol stage, one in the phase 4 modulator and eleven in the modulator, every one left holding a freed address; the modulator's own member scrambler is the fifteenth, through `Scrambler`'s destructor, which its header already records. The three fixtures drive every null combination of those pointers and assert `harness_alloc.free_null` at zero, which is what says the guards exist; nothing drives a second destruction, because a double free is what it would be measuring.

---

## D212 🐛 `V92Transmitter`, `V92BitsToSymbol`, `V92Phase4Modulator` and `V92Modulator` make twenty allocations between them and check none of them

*V.92 modulator batch. **Reachability: UNMEASURED.** Status: OBSERVED, not driven. Fix class: needs a decision -- the allocator never fails in the harness.*

**Findings F1280, F1285.** Six, two, one and eleven `sysdep_malloc` calls, and in twelve of the twenty the very next instruction is a constructor call on the returned pointer -- `movl $0x60; call sysdep_malloc; call V92Transmitter::V92Transmitter` at .text+0x4deee is the shape. The same family as D171, D175 and D180, and the same reasoning: a null return faults at the sub-object's first store rather than being deferred. The five raw buffers are worse only in that nothing writes through them until a member this tree has not written runs.

---

## D213 ⚠ `V92BitsToSymbol`'s constructor initialises +0x10, +0x18 and +0x1c and skips +0x14, which `nofBitsForNextTime` multiplies by

*V.92 modulator batch. **Reachability: UNMEASURED** -- it needs a call to `nofBitsForNextTime` or `setSymbolsBlockSize` before `reset`, and no caller has been read. Status: OBSERVED, not driven. Fix class: none proposed.*

**Finding F1287.** `reset(V92MappingParams *)` at .text+0x4e070 fills +0x14 from the mapping parameters' first word and the constructor at +0x4ded0 does not, so between construction and the first `reset` the field holds whatever `sysdep_malloc` left. `nofBitsForNextTime` (+0x4e0c0) and `setSymbolsBlockSize` (+0x4e130) both `imul` by it and return the product. The same shape as D164 and as finding F1248's hole in `V92ModulusEncoder`.

---

## D214 ⚠ 💤 `V92Modulator`'s constructor and the `reset` it inlines write every word of the object except +0x24 and +0x3c

*V.92 modulator batch. **Reachability: UNMEASURED** -- no member that reads either word has been written. Status: OBSERVED, not driven. Fix class: none proposed.*

**Findings F1283, F1287.** Twenty-eight of the object's thirty-one declared fields are written between the constructor and the inlined `reset`; the other three are these two words and the two bytes of alignment at +0x0e, which no constructor would write. +0x24 and +0x3c sit between named fields on both sides rather than at the end where an alignment hole would be. Whether anything reads them before some other member fills them is a question the sixteen unwritten members hold the answer to -- hence 💤 rather than 🐛.

## D230 🐛 the constructor's illegal-`modemSide` arm leaves the modulator pointer uninitialised, and the destructor then destroys it

*Batch of 2026-08-11, from `V92Modem::V92Modem` (blob 0x13d30 / 0x13ec0), +0xf4 and +0x13a. **Reachability: CANNOT FIRE** on the shipped path -- the only caller, `VPcmFloModem`'s constructor at .text+0xfac0, computes the argument as `dec %ebp; sete %dl; movzbl %dl,%esi`, which is 0 or 1 and nothing else. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1323.** The digital arm writes `movl $0x0,(%esi)` and the analog arm writes the allocation; the third arm prints "V92Modem Constructor: Illegal modemSide" and returns, storing nothing. `~V92Modem` then reads +0x000, finds whatever the enclosing storage held, and calls `_ZN12V92ModulatorD1Ev` on it followed by `sysdep_free`. Reproduced exactly, and `t_v92modem.cpp` asserts the word is still the fixture's seed before nulling it to make the destructor safe.

## D231 🐛 `~V92Modem` nulls one of the five pointers it releases and leaves the other four dangling

*Batch of 2026-08-11, from `V92Modem::~V92Modem` (blob 0x13a80 / 0x13990), +0x44..+0x7f. **Reachability: FIRES** on every destruction. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1322**, and the same family as D210 for `~V92Transmitter`. `movl $0x0,0x8(%esi)` writes back over `phase2Info` and there is no store anywhere near the other four -- `mappingParams`, `modulator`, `cp` and `parameters` all keep the addresses they were freed at. Harmless as shipped, because the object is an embedded member of `VPcmFloModem` that is destroyed once and never reused, but a second destruction would double-free four blocks and destroy three freed objects. Reproduced; `-fno-lifetime-dse` is what keeps our single store from being optimised away (finding F1272).

## D232 🐛 `V92Modem`'s constructor uses five `sysdep_malloc` results with no null test

*Batch of 2026-08-11, from `V92Modem::V92Modem` (blob 0x13d30 / 0x13ec0), +0x67, +0x8a, +0xb4, +0xc6 and +0x141. **Reachability: CANNOT FIRE** unless `sysdep_malloc` returns NULL, which slmodemd's wrapper does only on a failed `malloc`. Status: `unmeasured`. Fix class: none proposed.*

Each allocation is followed immediately by a constructor call or, for the 0xb4 parameter block, by `V92createConstellations` -- and in every case by a store into `*this` -- with no `test` between them, so a failed allocation is dereferenced at once. The same shape as D177's family and as finding F1303's reading of `~V90Demapper`'s guards. Reproduced; the null tests that would have to precede these five are not there.

## D221 ⚠ `V90Phase3Demodulator`'s constructor zeroes +0x3cc and then calls `reset`, which zeroes it again

*Batch of 2026-08-11, from `V90Phase3Demodulator::V90Phase3Demodulator` (blob 0x212c0/0x21430). **Reachability: FIRES** on every construction. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1305.** `mov %ecx,0x3cc(%ebx)` with `%ecx` zero sits between the two member constructor calls, and the constructor's last act is `reset(...)`, which has `movl $0x0,0x3cc(%ebx)` of its own. The first store is dead in every execution. It is reproduced because the instruction is in the object and because its POSITION -- between two member constructions, which is what makes it a mem-initializer rather than a body statement -- is the evidence for where the field is declared. The cost of the redundancy is one store per construction and there is one construction per call.

## D222 ⚠ `~V90Phase3Demodulator` frees two pointers and nulls neither, so a second destruction double-frees

*Batch of 2026-08-11, from `V90Phase3Demodulator::~V90Phase3Demodulator` (blob 0x20cb0/0x20c00). **Reachability: latent** -- nothing in the reconstructed graph destroys one twice. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1306.** Both arms are `test`/`jne` then destroy-and-free, with no store back to +0x3f0 or +0x428 afterwards, so the guards protect against a pointer the constructor never wrote rather than against re-entry. `~V90Demodulator` has the same shape across thirteen slots and `V90Modulator`'s destructor across five, so it is the object's house style and not a local slip. Reproduced rather than repaired; `t_v90rxctor.cpp` asserts the object is byte-identical after the destructor runs, which is what pins the absence of the stores.

## D223 ⚠ `~V90Demodulator` releases thirteen slots and nulls none of them, so a second destruction double-frees all thirteen

*Batch of 2026-08-11, from `V90Demodulator::~V90Demodulator` (blob 0x1ad70/0x1b010). **Reachability: latent** -- nothing in the reconstructed graph destroys one twice. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1309.** Each of the eight destroy-and-free arms and each of the five bare frees is followed by no store back to the slot, so the thirteen pointers are still there when the function returns and every one of them is stale. `V90Modulator`'s destructor, `V90BitsToSymbol`'s and `~V90Phase3Demodulator` (D222) all have the same shape, so it is the object's house style rather than a local slip. `t_v90demctor.cpp` asserts the object is byte-identical after the destructor except at +0x094, which is what pins the absence of the stores.

## D224 ⚠ Destroying a `V90Demodulator` prints four diagnostics and writes back a persisted modem parameter

*Batch of 2026-08-11, from `V90Demodulator::~V90Demodulator` (blob 0x1ad70/0x1b010) via `sessionTermination` (0x1ab30). **Reachability: FIRES** on every destruction. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1309.** The destructor's first act is an unconditional `sessionTermination()`, which is not a teardown helper: it emits four `edprintf` diagnostics and stores into `params->modemParams->clockDeviation`. So tearing a session down has a side effect on state that outlives the object, and it happens whether or not the caller wanted a session terminated -- a `delete` issued during error recovery writes the same parameter a clean shutdown does. Reproduced rather than repaired; D200 records a separate defect in one of the four strings.

## D235 🐛 `V90Modem`'s constructor leaves BOTH the modulator and the demodulator pointers uninitialised on an illegal `side`, and the destructor then destroys and frees whatever it finds

*Batch of 2026-08-11, from `V90Modem::V90Modem` (blob 0x194e0 / 0x19740), +0xfe onwards. **Reachability: CANNOT FIRE** on the shipped path -- the only caller, `VPcmFloModem`'s constructor, passes its own `side` argument through unchanged, and `VPCMXF_Create` computes that as `sete %al`, which is 0 or 1 and nothing else. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1332.** The same shape as D230 for `V92Modem`, one class up and twice as bad: `V92Modem`'s third arm leaves ONE word uninitialised, and this one leaves TWO. The switch is `test`/`je` for 0, `dec`/`je` for 1, then a fall-through that prints "V90Modem Constructor: Illegal modemSide" and returns; +0x00 and +0x04 keep whatever the storage held. `~V90Modem` then tests each, calls `_ZN12V90ModulatorD1Ev` or `_ZN14V90DemodulatorD1Ev` on it, and frees it. Reproduced exactly. `test/unit/t_v90modemctor.cpp` drives `side = 2` and asserts our allocator counters EQUAL the blob's rather than asserting they are zero, which is the only assertion that is true of the object.

## D236 🐛 `VPCMXF_Create` constructs into its allocation before it tests it for NULL

*Batch of 2026-08-11, from `VPCMXF_Create` (blob 0xfcf0), +0xaf and +0xe2. **Reachability: CANNOT FIRE** unless `sysdep_malloc` returns NULL, which slmodemd's wrapper does only on a failed `malloc`. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1333.** `call sysdep_malloc` at 0xfd9f, `call _ZN12VPcmFloModemC1E...` at 0xfdcd with nothing between them, and `test %ebx,%ebx` at 0xfdd2 -- so on a failed allocation the 651-byte constructor runs over a null pointer and the process is gone before the "new VPcmFloModem() failed." message it would have printed. The guard is real code and it is unreachable in the only circumstance it was written for. Reproduced where it is rather than moved to where it would work: moving it is a different function, and the same family as D232 for `V92Modem`'s five allocations.

## D237 ⚠ `~VPcmFloModem` exists in the blob as two global symbols and in our object as none

*Batch of 2026-08-11, from `VPcmFloModem::~VPcmFloModem` (blob 0xd0a0 D1, 0xd030 D2, 0x61 = 97 bytes each). **Reachability: n/a** -- this is a symbol-table difference, not a behavioural one. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1334.** The destructor is IMPLICITLY DECLARED on both sides -- it is six member destructor calls in reverse declaration order and nothing else, which is exactly what GCC generates and exactly what 0xd0a0 contains. An implicit destructor is implicitly inline, and our build has one call site for it, `VPCMXF_Delete`, into which GCC inlines it and then emits no out-of-line copy. The blob has both `D1` and `D2` as ordinary global `T` symbols.

**AND NOTHING IN THE BLOB CALLS EITHER OF THEM.** `objdump -dr` over the whole 1.2 MB finds ZERO `R_386_PC32` relocations against `_ZN12VPcmFloModemD1Ev` or `D2Ev`; the blob's own `VPCMXF_Delete` at 0xf6c0 inlines the six calls exactly as ours does. So the blob's two symbols are DEAD CODE -- GCC 3.4.2 emitted an out-of-line copy of an inline function nothing referenced, and modern GCC does not -- and `tools/closure.py dp_vpcm_init --missing` reporting 0 symbols and 0 bytes is CORRECT rather than a measurement artefact, because the pair is in no call graph to be missing from. Neither `debugaudit.py --missing` nor `coverage.py` lists them either. It is filed as a deviation because the SYMBOL TABLES differ and a count taken symbol-by-symbol will see it; the behavioural consequence is nil, and 194 of the blob's own bytes are unreachable in the blob. `test/unit/t_vpcmctor.cpp` drives the blob's `D1` directly by symbol, so the code at 0xd0a0 is differentially tested against ours even though ours lives inside `VPCMXF_Delete`.

Neither way of forcing the symbols out is right: an out-of-line definition cannot be inlined into `VPCMXF_Delete` and would turn its six calls into one, and an in-header one comes out weak and in a comdat group where the blob's are global.

## D255 ⚠ `setConnectionType`'s else arm writes three fields where the if arm writes four

*Batch of 2026-08-11, from `V90AutoDigitalImpDetector::setConnectionType` (blob 0x40290, 97 bytes), +0x0f onwards. **Reachability: unmeasured** -- no caller of this method is written yet. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1362.** `reset` and `setConnectionType` install the same two sets of four values on the same test, and they do not agree about the fourth. `reset` writes +0xa978 on both arms -- 1 when the connection type is 2 and 0 otherwise. `setConnectionType` writes it only on the first arm: its else path is three stores, `0xa97c`, `0xa97a`, `0xa980`, and there is no fourth store anywhere in the ninety-seven bytes. So calling it with a type other than 2 leaves +0xa978 at whatever the last type-2 call put there, and the object's state after `setConnectionType(0)` depends on its history where after `reset(_, _, 0)` it does not. Reproduced exactly; the mutation that ADDS the missing store is caught by `t_v90adid`, which is what makes the asymmetry a measurement rather than a reading.

## D256 🐛 `addReceivedSampleToStorage` has no bound on the store index

*Batch of 2026-08-11, from `V90AutoDigitalImpDetector::addReceivedSampleToStorage` (blob 0x41ff0, 149 bytes). **Reachability: REACHABLE FROM A CONFORMANT PEER -- upgraded 2026-08-24 from "unmeasured".** Status: CONFIRMED. Fix class: a bound; NOT YET WRITTEN.*

**THE UPGRADE.** The original note said no caller was written, so how many
samples a phase is offered was not established. The SPEC establishes it without
needing the caller: V.90 §9.3.2.10 gives the analogue modem **5000 ms** to stop
the DIL, which is about **6,667 samples per phase** at 8 kHz against a row of
**2,110**. The overrun begins around **1.6 s**, well inside the window the
Recommendation permits, with the peer REQUIRED to keep sending until told to
stop. So this fires on a conformant exchange that merely takes its time, not on
a malformed one.

**The 2,111th sample writes into the next phase's row**, and a phase offered
enough of them walks off the end of the 43,440-byte object.*

**Finding F1363.** The method stores at `sampleStore[phase][int_9100[phase]]` and then increments `int_9100[phase]`, and there is no comparison against 0x83e -- or against anything else -- in the whole method. The row is 2,110 shorts; the 2,111th sample offered to a phase writes into the next phase's row, and a phase offered enough of them walks off the end of the 43,440-byte object entirely. Reproduced without a check, because adding one would be a different function. `t_v90adid` keeps the index inside the row deliberately and says so at the call site: a test that let it run away would be scribbling over its own memory rather than measuring the object's.

## D257 ⚠ `clearCamulativeAltVal` takes two arguments and reads one

*Batch of 2026-08-11, from `V90AutoDigitalImpDetector::clearCamulativeAltVal` (blob 0x40270, 31 bytes). **Reachability: unmeasured.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1364.** The mangling is `Ess` -- two shorts -- and the second is never loaded: the thirty-one bytes touch `0x8(%esp)` and `0x4(%esp)` and nothing else. Its pair `clearCamulativeVal` uses both, as a (phase, code) index, so the shape of the alternate half is what explains it: the alternate accumulators are per phase and have no code dimension for a second argument to select. The parameter is declared and left unnamed in the reconstruction, and the mutation that starts using it is caught -- which is only possible because the test sweeps the second argument independently of the first.

## D258 ⚠ `adjustUinfoToPhaseOffset` wraps by testing for 6, not by a modulus

*Batch of 2026-08-11, from `V90AutoDigitalImpDetector::adjustUinfoToPhaseOffset` (blob 0x44940, 162 bytes), +0x54. **Reachability: unmeasured.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1365.** The walk is `cmp $0x6,%dx; setne %bl; neg %ebx; and %edx,%ebx` -- next = (phase + 1 == 6) ? 0 : phase + 1 -- so it folds back only from exactly 5. An offset of 6 reads `linMapp[6..11][ucode]`, which is inside the object but is `linMappAlt` and `prevLinMapp` rather than the mapping table, and a negative offset reads in front of the object. The write-back loop is unconditionally 0..5, so the damage is one-way. Reproduced as the object has it; `t_v90adid` sweeps the offset over 0..6 and asserts both the wrapping and the non-wrapping case were reached, and stops there because an offset of -1 would have the test reading memory it does not own.

## D259 🐛 The code histogram's index is unmasked, and `short_8b00[5][128]` IS `int_9100[0]`

*Batch of 2026-08-11, from `V90AutoDigitalImpDetector::addReceivedSampleToStorage` (blob 0x41ff0), +0x74. **Reachability: unmeasured** -- it needs a phase of 5 and a received code of 128 or more, and what codes reach this method is the caller's business. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1366.** The histogram increment is `movzwl 0x8b00(%edi,%ecx,2)` with `ecx = phase * 128 + code`, and `code` is the full `unsigned char` argument. The array is 6 x 128 shorts ending at +0x9100, which is `int_9100[0]` -- the sample-store index of phase 0. So a phase of 5 with a code of 128 increments the low half of phase 0's store index instead of a histogram bin, and codes 128..139 reach all six of those indices. The effect compounds: the corrupted index is what the NEXT sample stores at, and D256 says nothing bounds it. Both sides do it identically and `t_v90adid` compares them doing it, in `run_accumulate`, where the whole byte is swept one call at a time. The forty-block sequence masks the code to seven bits instead, and says why at the line that does it -- an index of 65,777 leaves the object, and a test that follows it there measures nothing.
## D250 🐛 `MTD7_COEF_9600`'s numerator puts the notch's zeros at 1328 Hz while its poles stay at 1477

*Batch of 2026-08-11, from `MTD7_COEF_9600` (blob .data 0x0078da, 10 bytes), element 3. **Reachability: FIRES, and it is MEASURED** -- at 9600 Hz eleven of the sixteen DTMF pairs come back with the wrong high-group tone, and every one of the eleven is 1477 Hz being chosen when absent or missed when present. At 8000 Hz all sixteen decode. Finding F1416. Status: **FIXED behind `DSPLIB_REPRODUCE_BUGS`, 2026-08-24**. Fix class: a corrected constant.*

**THE FIX, AND WHY THIS ENTRY EARNED ONE WHERE THE OTHER THIRTEEN OF ITS FAMILY
DID NOT.** `docs/deviation-triage.md`'s discriminator for the *defect,
reachable, no fix warranted* class is whether the CORRECT value can be derived
independently of the wrong one. Here it can, four ways over: the bank's own
design rule `b1 = -round(2 cos(w0) * 2^14)` gives -18613 at 9600 Hz for 1477
Hz; the fifteen sibling tables all follow that rule; this table's own `a1` read
back through `a1 = round(1.8 cos(w0) * 2^14)` gives the same w0; and the 8000
Hz twin scaled to this rate agrees. D302 passed that test and D451 failed it;
this one passes it four times, which is why it is the single fix the triage
recommended out of 232 bug-marked entries.

`src/service/dtmf_mtd_coeffs.c` carries the constant behind the define, in the
`src/dsp/fpm_div.c` shape. Both of finding F6810's checks hold: the reproduce
build's object is unchanged, and the default build's DIFFERS -- which is the
half that catches a fix that is not actually live. `make phase` exit 0, 241
passed / 0 failed.

**What is NOT claimed.** Nothing drives the fixed build, so no test asserts
that the corrected coefficient decodes all sixteen pairs at 9600 Hz; what is
asserted is the object's behaviour with the object's value, exactly as before.
The fix's benefit rests on finding F1413's derivation and 1416's measurement of
the defect, not on an experiment with the fix in place. Say so before quoting
it as a repair.*

**Finding F1413.** Every one of the sixteen tables is a notch with `b1 = -round(2 cos(w0) * 2^14)` matching its own `a1 = round(1.8 cos(w0) * 2^14)` to within 0.1 Hz. This one does not: `a1 = 16751` is 1477.04 Hz and `b1 = -21143` is 1328.45 Hz, where the design gives -18613. A biquad whose zeros and poles are 150 Hz apart is not a notch at either frequency, so the 1477 Hz section of the 9600 Hz bank has a materially different response from its 8000 Hz twin. Reproduced byte for byte; `test/unit/t_dtmfrx.c` compares the table against the object's own `ref_MTD7_COEF_9600` AND asserts the count of mis-decoded pairs at each rate, so the consequence is held to as well as the bytes. No mechanism is proposed -- the value is not a bit flip of -18613, not a digit transposition of it, and and not the right coefficient for 1477 Hz at any rate this library uses (7200 gives -9114, 8000 gives -13085, 9600 gives -18613).

## D251 ⚠ `reset_dtmf` clears seven of the receiver's eight filter states and leaves the low group's pre-notch alone

*Batch of 2026-08-11, from `reset_dtmf` (blob 0x090a90), +0x67..+0x9c against `DTMF_MTD_detect` (0x0927b0) +0x140. **Reachability: FIRES** on every reset, but its effect is bounded -- the section is a 0.9-radius biquad, so whatever is left decays by 40 dB in about 40 samples. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1414.** `reset_dtmf` zeroes eight shorts at +0x354, +0x358, +0x35c and +0x360 -- four biquad states -- and then the eight tone states at +0x368..+0x387. `DTMF_MTD_detect` uses +0x360 as the HIGH group's pre-notch and **+0x364 as the LOW group's**, and +0x364 is in neither range. So a receiver that is reset while the low pre-notch is ringing carries that ringing into the next detection. The four cleared states are also one more than the two the code uses, which is the other half of the same off-by-one: the loop looks written for a bank that has since moved. Not reconstructed here -- `reset_dtmf` is outside this closure -- and recorded so that whoever writes it reproduces the range rather than "fixing" it.

## D252 🐛 `dtmf_modem` copies its caller's whole block into a 198-entry stack array with no bound check

*Batch of 2026-08-11, from `dtmf_modem` (blob 0x0911e0), +0x26. **Reachability: CANNOT FIRE** on the shipped path -- the only caller, `cid_progress`, hands it a block whose length is set by the datapump's frame size. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1410.** The copy loop's bound is the `count` argument and nothing else; the destination is 0x18c bytes of frame. A caller passing more than 198 samples writes over the return address. The object's own `hold` buffer at +0x262 is 100 shorts, so a block above 200 would also overrun that on the realignment path, one field short of `last_digit`. Reproduced as it is, with the limit stated in `include/dsplib/dtmf_rx.h` rather than enforced, because enforcing it would change what the function does for inputs the object accepts.

## D253 ⚠ `band_pass` builds two five-coefficient filter sections on every call and reads neither

*Batch of 2026-08-11, from `band_pass` (blob 0x090d30), +0x64..+0xa9. **Reachability: FIRES** on every call -- ten stores, no reads. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1410.** Four sections are initialised into the frame: a 244 Hz high-pass at each of the two rates (+0x30, +0x40) and the 870 Hz band-pass at each (+0x50, +0x60). Only the band-pass pair is ever loaded -- the rate test selects between +0x50 and +0x60 and nothing else takes the address of, or reads, +0x30 or +0x40. GCC 3.4.2 emitted the stores anyway. The reconstruction declares all four, so the source says what the original said; a modern compiler deletes the two dead ones again, which changes nothing observable and is why this is `⚠` and not `🐛`. What it suggests is that the high-pass was once in the chain and was removed without removing its table, which would make the DC blocker three statements further down its replacement.

## D254 🐛 `dtmf_modem`'s sensitivity-1 path reports a block counter where a keypad code belongs

*Batch of 2026-08-11, from `dtmf_modem` (blob 0x0911e0), +0x4c6. **Reachability: FIRES** when `sens == 1` and no digit string has been started (`ndigits == -1`). Status: `unmeasured`. Fix class: none proposed.*

**Finding F1410.** With `sens == 1` and `ndigits == -1`, any tone-bank result other than 13 is DISCARDED and replaced by `stable + 20`, where `stable` is how many consecutive blocks agreed -- so the value that goes on to be compared against `last_digit`, and to be written into the digit string if it is 9 or less, is a count and not a code. 20 and up is outside the keypad range, so the immediate effect is that nothing is ever accepted while this holds; the path only leaves itself when the bank returns exactly 13 ('D'), which then resets the machine. It reads like a diagnostic or a bring-up hook left in. Reproduced exactly, and `test/unit/t_dtmfrx.c` drives `sens == 1` from `ndigits == -1` so the arm is compared rather than merely written.
## D260 🐛 `V92PreFilter::process` runs a filter whose tap count rounded to zero, and the filter then walks off its history

*Batch of 2026-08-11, from `V92PreFilter::process` (blob 0x574b0) and `V92PreFilter::setCoefficients` (blob 0x57450). **Reachability: unmeasured** -- it needs `setCoefficients` called with a count of 1, 2 or 3, and nothing this tree has reconstructed does. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1371.** `setCoefficients` stores the CALLER's counts at +0x0c and +0x10 and hands the same counts to the two filters, which round them DOWN to a multiple of four -- so a count of three leaves +0x0c non-zero and the filter behind it with no taps at all. `process` gates on +0x0c and +0x10 and not on the filters' own counts, so it calls a filter that has none, and `FloatFIR::process`'s carry-tail loop (blob 0x46c20: `dec %edx; jne`, entered with the tap count already decremented) then decrements from zero and copies backwards without a bound. The blob's loop and ours are the same loop, so this is the object's behaviour and not the reconstruction's; `test/unit/t_v92precoder.cpp` says in as many words why its sweep contains no count of 1, 2 or 3, because driving one is a segmentation fault on both sides rather than a comparison.

## D261 🐛 `V92Precoder::process` reads two locals the search may never have written, and feeds them to both filters

*Batch of 2026-08-11, from `V92Precoder::process` (blob 0x56f50), frame slots +0x18 and +0x1c. **Reachability: unmeasured** -- it needs a symbol whose candidate interval is empty or whose every candidate squares above 1e12, and what the parameter block actually holds is not modelled. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1374.** The search writes `0x18(%esp)` and `0x1c(%esp)` only from inside its accept arm, and the code after the loop reads both unconditionally: `flds 0x18(%esp)` into `FloatFIR::process` for +0x70, `flds 0x1c(%esp)` into the other for +0x74, and again for `outf[i]`. Two ways to reach that with neither written: `lo > hi`, which a negative modulus produces, and every candidate's square exceeding the initial 1e12. On the first of the four symbols the values are whatever the frame held; on a later one they are the previous symbol's, and `out[i]` is left holding whatever the caller put there. Reproduced exactly, with the `-Wmaybe-uninitialized` the honest spelling produces suppressed at the function and explained there. `t_v92precoder.cpp` drives the case where symbol 0 has run first, which is the only one where the two sides hold the same values and the comparison means anything; the first-symbol case is undrivable by construction, since it would compare two fixtures' stacks.

## D262 🐛 `V92ConvolutionEncoder::process` stores and returns two registers nothing on that path wrote

*Batch of 2026-08-11, from `V92ConvolutionEncoder::process` (blob 0x54f40), +0x35. **Reachability: unmeasured** -- it needs `mode` outside 0, 1 and 2, and what supplies `mode` is a runtime value nothing here has modelled; see the caller survey below. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1375.** `process` switches on `mode` against 0, 1 and 2 to narrow `inverseMap`'s coset label to an index, and the two table lookups, the store to `state` and the `return` all sit AFTER the switch. There is no `default:` arm, so a fourth mode reaches them with the index never written -- and the object is explicit about what that costs:

    54f75:  89 7b 04    mov %edi,0x4(%ebx)
    54f78:  89 f0       mov %esi,%eax

two callee-saved registers that nothing on that path assigned, stored into `state` and returned. There is no load from either table on that arm at all: with the index undefined the loads are undefined too, and GCC 3.4.2 simply dropped them, which is why the arm is four bytes rather than twenty. The next call then indexes `nextState` with whatever `state` now holds.

Reproduced exactly, by leaving the index uninitialised; the `-Wmaybe-uninitialized` that produces is suppressed at the function and explained there, the same trade as D261 one class over. Initialising it, or adding a `default:`, would be a different function.

**THE CALLER SURVEY IS THE BLOB'S, NOT THE RECONSTRUCTION'S, AND IT IS COMPLETE.** Within the class only `reset` writes +0x00 -- `makeStateTtransitionTable` and `process` read it and nothing else touches it -- so `mode` can only be whatever `reset` is passed. `objdump -dr` over the whole 1.2 MB finds EXACTLY ONE `R_386_PC32` against `_ZN21V92ConvolutionEncoder5resetEi`, at 0x53db3 inside `V92Transmitter::reset(V92MappingParams *)`, and no resolved same-TU call to 0x54d60 either -- which is the half that matters, because a relocation's presence only says the symbol is global while its ABSENCE is what would have hidden a caller in the same translation unit (findings F306 and F333). `process` is the same shape: one relocation, from `V92Transmitter::process` at 0x5464c, and no other call site. `makeStateTtransitionTable` likewise has exactly one, from `reset`.

And what that one caller passes is NOT a constant:

    53da6:  8b 56 10    mov 0x10(%esi),%edx     the mode
    53da9:  89 54 24 04 mov %edx,0x4(%esp)
    53dad:  8b 47 54    mov 0x54(%edi),%eax     this->convolutionEncoder
    53db3:  e8 ..       call V92ConvolutionEncoder::reset(int)

`+0x10` of the `V92MappingParams` block the caller was handed. Whether that field can hold anything but 0, 1 or 2 is not modelled here and `V92Transmitter::reset` is not reconstructed in this tree, so the class is `unmeasured` and not `CANNOT FIRE`. The three arms the switches DO name are 16-, 32- and 64-state trellis codes, which is a plausible complete set for the field -- but plausible is not measured, and the earlier draft of this entry said `CANNOT FIRE` on exactly that reasoning before the relocation scan was run.

**It is deliberately not driven.** Both sides would be reading two different pieces of stack, so any difference the fixture reported would be one the fixture created -- the same reason `t_v92precoder.cpp` leaves D261's first-symbol case alone. `t_v92convmapper.cpp` says so where it drives `process`, and drives modes 0, 1 and 2 only. The unnamed modes ARE driven through `makeStateTtransitionTable` and `reset`, where the missing arm is well-defined behaviour -- nothing is built -- and the assertion that a seeded 0x2008 comes back byte for byte on both sides is a real check.
## D263 ⚠ `V92ModulusEncoder::reset` stores the first half-product to +0x08 and then overwrites it on both paths

*Batch of 2026-08-11, from `V92ModulusEncoder::reset` (blob 0x550f0, 0x8a5 = 2,213 bytes), +0x3d8 and +0x401. **Reachability: FIRES** on every call -- the store is unconditional -- but is unobservable, because both arms of the `if` that follows write the same two words before anything can read them. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1376.** `reset` computes the product of the first six moduli, stores it to +0x08 as a 64-bit pair at 0x55418/0x55445, and then stores over it at 0x5567a (the `n == 8` arm, with zero) or at 0x5593c (the other arm, with `u >> (62 - n)`). It is a dead store, and it is in the object because GCC 3.4.2 does not eliminate a dead store through a pointer -- `this` may alias anything, so the store is live as far as the compiler can prove. It is reproduced rather than dropped, for the usual reason: the goal is an object that behaves identically, and "identically" includes the write a debugger or a concurrent reader would see. No test can distinguish the two, so no mutation is offered for it either; a mutation that deletes the store would be `equivalent` and unprovable by instruction text, since the blob's own text contains it.

## D264 ⚠ `progress`'s twelfth digit is a bare quotient, bounded by nothing, while the same function advertises `m11 - 1` as its range

*Batch of 2026-08-11, from `V92ModulusEncoder::progress` (blob 0x559a0, 0x11f9 = 4,601 bytes), 0x565b3 against 0x55aa1. **Reachability: FIRES** whenever the bit count admits a value at or above the product -- the parameter block carries the count and the twelve moduli as independent fields, so nothing in the object ties them together. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1379.** Case 0 extracts eleven digits with `out[k] = v % m[k]` and then stores the leftover quotient: `out[11] = (v - out[10]) / m10`, with no reduction modulo the twelfth modulus and no reference to +0x44 anywhere in the case -- it is the ONE field of the object that case 0 never reads. Cases 1 and 2 of the same function, which exist to report each digit's range, do read it: case 2 writes `out[11] = m11 - 1`. So a caller that trusts case 2's answer and feeds case 0 more bits than the product holds gets a twelfth digit past the range it was told to expect, and the eleven below it are all correctly reduced. Reproduced exactly; `test/unit/t_moduluscoder.cpp` drives bit counts both under and over the product, and the mutation that reduces `out[11]` modulo the twelfth modulus is caught, so the difference is measured rather than assumed.
## D270 ❌ RETRACTED -- the Jd accessors read a payload-contiguous bit layout and the packers write a framed one, and that is the two DIRECTIONS rather than a defect

*Message and echo batch of 2026-08-11, from `V90Jd::getRatesMask` (blob 0x1e8b0), `getConstelationSize` (0x1e900), `getMaxLookahead` (0x1e920) and their `V92Jd` counterparts at 0x11eb0, 0x11f00, 0x11f20, plus `V92Jd::getJdPhase` (0x11e60). **RETRACTED the same day it was opened**, by the unpackers landing: `unPackData` and both V.92 unpackers strip the framing and fill `bits[0..47]` flat, which is precisely the layout the accessors read (finding F1395). The entry is kept because a reference in `V90Jd.cpp` and both headers points at it, and because the reasoning is worth keeping: the accessors were written before anything was known to WRITE what they read, and the honest entry at that moment was this one. Status: EXPLAINED, both layouts driven against the blob. Fix class: none; there is nothing wrong.*

**Findings F1390 and F1395.** The constructors and the packers put the rate mask at `bits[18..33]` and `bits[35..46]`, the constellation pair at `bits[47..48]` and the lookahead pair at `bits[49..50]`, around a 17-byte group frame; the accessors read `bits[0..27]`, `bits[28..29]` and `bits[30..31]`, with no frame at all, and `getJdPhase` reads `phaseBits[0..15]` where the constructor writes the Q16 phase at `phaseBits[18..33]`.  The two layouts differ by the framing and by 18 or 19 positions, so a Jd object cannot decode the message it just packed -- **and it never has to.** `unPackData` fills the flat one from the wire and the accessors read it; the packs fill the framed one and `getBitVector` hands it out.  The mapping is framed `18 + p` for payload 0..15, `35 + (p - 16)` for 16..31 and `52 + (p - 32)` for 32..47, identical in all three unpackers.  Both halves are reproduced exactly as the object has them, each is driven against the blob, and the ROUND TRIP -- pack, feed back one bit at a time, read out through the accessors -- is driven end to end in `t_v90jd.cpp` and `t_v92jd.cpp`.

## D271 ⚠ 💤 `V92Jd::getConstelationSize` reads `phaseBits[29..30]` where `V90Jd`'s otherwise identical accessor reads `bits[28..29]`

*Message and echo batch of 2026-08-11, from `V92Jd::getConstelationSize` (blob 0x11f00), +0x08 and +0x12. **Reachability: unmeasured** -- no caller has been read. Status: CONFIRMED. Fix class: none proposed.*

**Findings F1391, F1395 and F1396.** Three of `V92Jd`'s four accessors are `V90Jd`'s instruction for instruction and read `bits`; this one reads +0x67 and +0x68, which in `V92Jd`'s map is the SECOND vector, and at one index higher than `V90Jd`'s reads in the first.  +0x67 is +0x1f plus the 0x48 that `phaseBits` displaces everything after it by.

**THE INDEX IS NOW EXPLAINED AND THE VECTOR IS NOT, so this entry narrows rather than closing.**  All four accessors read the layout the unpackers fill, and in the PHASE message payload 28 is the constant tag byte `unPackJdPhaseData` checks (`cmpb $0x1,0x66(%ebx)` at 0x128bc), so the constellation pair sits one position later there -- 29..30 -- than in the data message.  What no reading accounts for is the vector: this accessor takes the pair out of `phaseBits` while `getRatesMask` and `getMaxLookahead` take theirs out of `bits`, so a `V92Jd` that has received a DATA message answers `getConstelationSize` from the phase vector.  Reproduced; the fixture paints the two vectors with different values so that reading the right index in the wrong vector diverges.

## D272 🐛 `V92EchoCanceller::process` decides whether to filter at all by comparing the OUTPUT buffer's first sample with 177.0f

*Message and echo batch, from `V92EchoCanceller::process` (blob 0x11870), +0x00. **Reachability: unmeasured** -- it depends on what the caller leaves in the output buffer, and no caller has been read. Status: CONFIRMED. Fix class: none proposed; reproduced as measured.*

**Finding F1398.** `flds (%edx); fcomps <.rodata.cst4+0x8c = 177.0f>` is the FIRST thing the function does, before it looks at `count`, so a call with `count == 0` still dereferences the second buffer. When the comparison holds, the block is copied through unfiltered, the read cursor is stepped one per sample as though it had been filtered, and none of the state machine runs -- no counter, no `setState`. The constant is referenced from that one instruction and from nowhere else in the 1.2 MB object (`relocscan.py --at .rodata.cst4:0x8c`), so nothing in the blob says what it means. The comparison is spelled `out[0] == 177.0f` because the object branches on ZF alone and FCOM sets C3 for equal AND for unordered: a NaN in `out[0]` takes the copy path, and under the object's own `-mno-ieee-fp` that is exactly what C's `==` compiles to. It read `!(out[0] < 177.0f || out[0] > 177.0f)` until finding F2300 -- two compares where the object has one, and correct only for the `-mieee-fp` the period tier used to be built with. `t_v90leaves.cpp` drives both the exact match and the NaN; the NaN case is the modern build's one declared divergence here, `tools/gccdiverge.json` and finding F2304.

## D273 🐛 `updateEchoHistory`'s compaction loop is bottom-tested with a count that underflows below two taps

*Message and echo batch, from `V92EchoCanceller::updateEchoHistory` (blob 0x11770). **Reachability: CANNOT FIRE** at the shipped `V92_ECHO_FILTER_LENGTH` of 180. Status: CONFIRMED. Fix class: none proposed.*

**Finding F1398.** `lea -0x1(%esi),%edx` then `dec %edx; jne` with no guard, so the loop always runs at least once and a `filterLength` below 2 makes the count 2**32 - 1, copying four billion words downward over everything below the buffer. The same shape as D72 one field along, and the same verdict for the same reason: the length is a parameter-block constant.

## D274 🐛 the COUNT_DELAY cursor advance is one conditional subtraction, not a modulo

*Message and echo batch, from `V92EchoCanceller::process` (blob 0x11870), the state-1 arm. **Reachability: CANNOT FIRE** at any block length the modem uses. Status: CONFIRMED. Fix class: none proposed.*

**Finding F1398.** `cmp %eax,%edx; jae; sub %eax,%edx` subtracts the wrap length ONCE, so a block longer than `historyAlloc - word_18` leaves the read cursor past the end of the buffer and the object does not loop back. Reproduced; a modulo would be a different function.
## D265 🐛 `V90MP::infoToBits` pads over the CRC it has just written, for every type-zero message

*Batch of 2026-08-11, from `V90MP::infoToBits` (blob 0x1f990), +0x4c0 arm, 0x200ce and 0x200d7. **Reachability: EVERY type-zero message with a non-zero group size**, which is all of them -- the sequence length is 0x56 rounded up, so it always exceeds 0x45 and the loop always runs. Status: `unmeasured` -- no caller of `infoToBits` is reconstructed yet, so what a peer does with a zeroed CRC is not known. Fix class: none proposed; reproduced exactly.*

**Finding F1386.** The two arms end alike -- copy the sixteen CRC bits into `bits`, zero the bit after them, pad to the sequence length -- and the type-one arm's constants are right (CRC at 0xab..0xba, `movb $0x0,0xd7(%ebx)` is bits[0xbb], the loop starts at 0xbc). The type-zero arm puts the CRC at 0x45..0x54 and then writes `movb $0x0,0x61(%ebx)`, which is bits[0x45], and starts its pad loop at 0x45 as well: both constants name the FIRST CRC bit instead of the one after the last, so the CRC goes out zeroed from the front. One slip explains both -- an index advanced through the copy and then not advanced past it. `t_v90mp.cpp` asserts the destruction rather than working around it, and `test/mutations/v90mp.json` carries the CORRECTED form (0x55/0x56) as a mutation that must be caught, so the day the test stops driving a type-zero message the suite says so.

## D266 🐛 `V90MP::bitsToInfo` inverts bits[0x70] on a CRC failure and leaves it inverted

*Batch of 2026-08-11, from `V90MP::bitsToInfo` (blob 0x20190), +0x5be, `cmpb $0x0,0x8c(%ebx); sete 0x8c(%ebx)`. **Reachability: any long message (+0x119 above 0x6f) whose CRC does not check.** Status: `unmeasured` -- how often the far end gets that one bit wrong is a property of the line, not of the object. Fix class: none proposed; reproduced exactly.*

**Finding F1386.** On a mismatch the object inverts one fixed bit of the received message, recomputes the whole CRC and compares again, reporting `recieved MP with modified good CRC` if that rescued it and `recieved MP with bad CRC` if it did not. bits[0x70] is inside `h2Imag`. Two things follow and both are reproduced: the repair is a GUESS, so a message that passes the second check is not necessarily the message that arrived; and the flip is unconditional on failure, so even when it does not help the bit stays inverted and the next thing to read `bits` sees the altered vector. A short message never gets the repair, because 0x55 is not above 0x6f -- `t_v90mp.cpp` drives that arm too.

## D267 🐛 `V90ConnectionEvaluator::evaluatePhase4`'s delayed-retrain give-up prints a `%d` with no argument behind it

*Batch of 2026-08-12, from `V90ConnectionEvaluator::evaluatePhase4` (blob 0x3fad0), 0x3ffcf. **Reachability: every delayed retrain that takes the counter past `MAX_NOF_V90_RETRAINS`** -- both +0x78 and +0x7c non-zero and the count already at the limit. Status: `unmeasured` -- the number printed is an uninitialised stack slot, so what it says cannot be predicted and is not the same on our build as on the blob's. Fix class: none proposed; reproduced exactly.*

**Finding F1388.** The string at `.rodata.str1.4+0xab34` is "V90ConnectionEvaluator (phase4): initiating fall back to V34 due to %d V90 retrains (last one delayed)", and the call site pushes only the format pointer: nothing is stored to `0x4(%esp)`. Its sibling forty-four bytes later at 0x3fffb stores `nofV90Retrains` there for a string carrying the same conversion, so this is one line written without its argument and not a different convention. `edprintf` formats with `vsnprintf`, which reads the outgoing-argument slot as an `int` and prints it.

**THE RECONSTRUCTION MATCHES THE OBJECT AND THE DIAGNOSTIC STILL DIFFERS**, which is why this is here rather than only in the findings. `src/pump/v90/V90ConnectionEvaluator.cpp` calls `edprintf` with the format alone, exactly as the object does, so the two sides run the same code -- but each reads its own frame, and the frames are not the same because the compilers are not. `test/unit/t_v90conneval.cpp` therefore compares object state, verdict and line count on that one path and not the transcript text; every other diagnostic in both evaluators is compared in full. The attempt to make the slot deterministic (drive phase 4's retrain arm first in the same call, since it passes `nofV90Retrains` in that very slot) was made and measured: the transcripts still differ, and differ only in that line.

Repairing it would mean passing `nofV90Retrains`, which is what the sibling line does and almost certainly what was meant. It is not repaired: the printed value reaches the manufacturer's decoder and nothing else, and behaving differently from the blob is the one thing the reconstruction may not do.

## D268 ⚠ Two float comparisons in the connection evaluator take a different branch from the blob's for an UNORDERED operand, and the fixtures steer around them

*Batch of 2026-08-12, from `V90ConnectionEvaluator::evaluateConnection` (blob 0x3e6d0), 0x3e933, and `::evaluatePhase4` (blob 0x3fad0), 0x3fae5. **Reachability: a NaN in `word_70` (the running average PDSNR) or in `evaluatePhase4`'s ratio argument, and nothing else.** Status: `unmeasured` -- see below for exactly what was and was not measured. Fix class: none proposed.*

**Findings F1388 and F1389.** Both sites are a float compare whose complement the object takes WITHOUT consulting the parity flag. 0x3e933 is `fcomps 0xa0(%edi); jae`, the complement of `avePdsnr < threshUp`; 0x3fae5 is `flds 0x438(%ecx); fcomp %st(1); jae`, with the PARAMETER as the left operand -- forced, because the argument was already on the stack and `fcoms 0x438(%ecx)` would have been one instruction shorter. `jae` is FALSE when the compare is unordered, so the object runs the arm for a NaN where C says it must not.

**WHAT IS RECORDED HERE IS AN UNTESTED INPUT, NOT A KNOWN DISAGREEMENT.** Under `make phase` -- GCC 13 -- our source took a different branch from the blob's for a NaN at these two sites, so `test/unit/t_v90conneval.cpp` keeps NaN away from them and feeds it to the other four float comparisons in the two functions, which are `ja`/`jbe` and agree. That is a fixture avoiding an input because the two sides disagreed on it, which is one step from widening a tolerance, and it belongs in this file rather than only in a source comment.

**WHAT WAS MEASURED SINCE.** `objdump -d` over our PERIOD-compiled `V90ConnectionEvaluator.o` (GCC 3.4.2, `build/period/src_pump_v90_V90ConnectionEvaluator.o`) finds no `jp`, `jnp` or `setp` anywhere in `evaluateConnection` -- neither does the GCC 13 object -- so neither of our builds tests for unordered explicitly, and any difference is in the compare DIRECTION and the condition code rather than in an added parity check. `make period` passes `t_v90conneval` at 155/157, but that says nothing about this: the fixture does not drive the input.

**WHAT WAS NOT MEASURED, and it is the whole question.** Whether the divergence survives the period build. Both sides are GCC 3.4.2 output there, and the blob's own compiler emitted the `jae`, so it is likely that our period build emits it too and the two agree -- but likely is not measured, and the cheap experiment is to feed a NaN at those two sites under `make period T=t_v90conneval` and see. Whoever does it should record the answer here: if they agree, this entry becomes a note about `make phase` alone and the fixture can stop steering; if they do not, it is a real deviation and the source is wrong.

## D280 🐛 `unitePhasesInfoOfUref`'s convergence checksum adds six copies of one entry

*Batch of 2026-08-11, from `V90AutoDigitalImpDetector::unitePhasesInfoOfUref` (blob 0x40cb0, 748 bytes), +0x260. **Reachability: FIRES ON EVERY CALL** -- it is the loop test, not an error path. What it cannot do is make the loop wrong: the checksum is only ever compared against its own previous value. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1421.** The method repeats its grouping until `linMapp` stops changing, and the "has it changed" test is a six-iteration sum. The sum's addend does not vary:

    40f14: shl    $0x7,%esi              ; esi = 5, the outer loop's terminal value
    40f1b: lea    (%esi,%ecx,1),%edi     ; edi = 5*128 + at
    40f1e: movzwl 0x0(%ebp,%edi,2),%ecx  ; HOISTED CLEAN OUT OF THE LOOP
    40f30: lea    0x1(%edx),%esi         ; the loop: six adds of the same ecx
           ...
    40f42: cmp    0x18(%esp),%bx

A load can only be hoisted out of a loop if its address is loop-invariant, so this is not a reading of the disassembly that could be wrong: the index is the OUTER loop's variable, left at 5 when it terminated, where the source plainly meant the inner one. The checksum is therefore `6 * linMapp[5][at]` truncated to a `short`, not the sum over the six phases.

It does not make the method loop for ever or terminate early in any way that has been observed: phase 5 is grouped like any other, so its entry stops changing when the grouping settles. It is recorded because a checksum that reads one sixth of what it is checking is a defect whether or not it happens to be sufficient, and because a reader who assumes the obvious sum will not understand the object's iteration count.

## D281 🐛 `unitePhasesInfoOfUref` reads an uninitialised local when every phase is flagged

*Batch of 2026-08-11, same function, +0x2a1 (`flds 0xc(%esp)`) and the NaN at `.rodata.cst4+0x328`. **Reachability: needs `short_2800[0..4]` ALL nonzero**, which no written caller produces today -- `updateUref` passes the flags through untouched from whatever the study path set. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1420.** The method picks the largest group of unflagged phases and hands that group's mapping entry and variance to every flagged phase. Both are held in locals: the variance in an x87 register seeded with the constant at `.rodata.cst4+0x328`, which is 0x7fc00000 -- a quiet NaN -- and the entry in a stack slot at `0xc(%esp)` **seeded with nothing at all**.

The grouping loop runs `i` over 0..4 and skips any phase that is flagged, so if all five are flagged it never executes, the two locals are never written, and the final loop stores a NaN and four bytes of stack residue into `linMapp` and `float_9d48` for all six phases. Reproduced as it is -- `bestValue` is declared and deliberately not initialised, because giving it a value would be inventing behaviour rather than reproducing it, and the blob's behaviour on that path is not a function of its inputs.

**The test cannot compare that path and says so at the line that avoids it.** Two builds have two stack frames, so the residue differs between the sides for reasons that have nothing to do with the reconstruction; `t_v90adid` therefore always leaves at least one of phases 0..4 clear. Every other arm of the method is driven and compared.

## D282 🐛 `getAltVarThresh` divides by a count that can be zero

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::getAltVarThresh` (blob 0x40650, 581 bytes), +0x7c (`de fa`, which prints `fdivrp` and IS `FDIVP` -- finding F245). **Reachability: needs no entry strictly below the average of the six**, which SIX EQUAL VALUES give -- and six equal zeros is what `reset` leaves at +0x9d48 until a study has run. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1424.** The method averages the six per-phase variances, averages again over only the entries strictly below that average, and scales the second average by its caller's factor. The second average is `below / count` with `count` a `short` counted in the same loop that selects the entries, and there is nothing between the loop and the division -- no test, no default. If every entry equals the average, nothing is selected, `count` is zero, `below` is zero, and the division is 0.0f/0, which the x87 answers with the real indefinite 0xffc00000. That NaN survives the multiplication by the factor, survives the floor at `altMinVarThresh` -- an ordered `>` is false against a NaN -- and is what the method returns.

The floor is what makes it look defended and is not: `if (altMinVarThresh > thresh)` fires only when the comparison is ordered, so the one value the floor exists to prevent is the one value it lets through.

Reproduced as it is, and `t_v90adid` carries a row of six equal entries and a row of six zeros precisely to reach it. The return value is compared as a BIT PATTERN rather than with `==`, because `==` is false for a NaN on both sides and would have passed for ever on exactly this arm.

## D283 🐛 `uniteLinMappInfoOfUnsuspectedPhases` never resets its running group size

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspectedPhases` (blob 0x41550, 601 bytes), +0x4a. **Reachability: FIRES whenever more than one group forms**, which needs three unsuspected phases with two of them out of each other's tolerance. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1426.** The method groups the unsuspected phases, keeps the largest group, and pools that group's accumulators. The size counter is set to 1 exactly once, at 0x4159a, BEFORE the scan that uses it:

    41586: mov    $0x1,%ebp
    4159a: mov    %ebp,0x24(%esp)        ; count = 1, outside the loop
    415a8: ...                           ; the scan over i starts here
    41772: mov    %eax,0x24(%esp)        ; the only other write: count + 1

`unitePhasesInfoOfUref`, which is the same shape, sets its own counter to 1 inside the loop beside `group[i] = i`. Here it is hoisted out, and a store of a constant cannot be hoisted out of a loop that also increments it -- so this is the source and not the optimiser. The consequence is that the second group starts counting from wherever the first finished: "the largest group" is really "the last group that merged anything", and a first group of three followed by a second group of two selects the second.

`t_v90adid` separates the two spellings with a directed block -- two groups of two, phases 0-1 and 2-3, with different means. Reset-per-group makes both size 2 and keeps the first; the object's running counter makes the second size 3 and keeps IT, and the two write different means into every unsuspected phase.

It also makes one of the method's other bounds untestable, which is recorded as a proven-equivalent mutation rather than as an uncaught one: because `count` never decreases, `best` always equals `count` by the end of an iteration, so letting the sixth phase lead a group can never beat the running best and the change cannot be observed. That argument is finding F1426's, above.

## D284 🐛 `uniteLinMappInfoOfUnsuspectedPhases` reads an uninitialised group number

*Batch of 2026-08-12, same function, +0xad (`mov 0x1c(%esp),%ebp`). **Reachability: needs `byte_280c[0..4]` ALL nonzero**, and unlike D281's condition this one has a producer -- `porcessFirstStudy` sets all six on a mapping smooth enough that no phase collects nine rough neighbours. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1427.** The scan writes `bestGroup` at 0x4178c and nothing else does; the pooling loop at 0x415fd reads it. The scan skips a phase that is suspected or already grouped, so if all five of phases 0..4 are suspected it never executes and the stack slot at `0x1c(%esp)` is read having never been written. The pooling then keeps whatever phases happen to match four bytes of stack residue, and the mean it writes into every unsuspected phase is a function of the frame rather than of the object.

It is the same shape as D281 and reproduced the same way -- `bestGroup` is declared and deliberately left uninitialised, because giving it a value would invent behaviour the blob does not have. **The test cannot compare that path**, for D281's reason: two builds have two stack frames. `t_v90adid` always leaves phase `trial % 5` unsuspected in the sweep, and the forty-block sequence forces `byte_280c` explicitly before every call rather than letting `porcessFirstStudy`'s output arrange it -- which is exactly the state that would reach this.

The difference from D281 is that this one is reachable from inside the class. D281 needs a caller to flag every phase at +0x2800; this needs `porcessFirstStudy` to find every phase smooth, which is what a clean line produces.

## D285 ⚠ The scan for the unsuspected phase stops at five, so the arm that handles "there is none" is dead

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::porcessSecondStudy` (blob 0x41cb0, 831 bytes), +0x45 (`cmp $0x4,%bx ; jle 41cd9`), and from the character-identical loop in `setQcLinearMapping` (blob 0x44700) at +0x10c. **Reachability: the arm at +0x247 can never be entered** -- proved by the loop's own bound, not sampled. Status: `measured`. Fix class: none proposed.*

**Finding F1429.** Both callers of `updateAltRbsPhaseInDil` open by finding the first RBS phase with a clear `byte_280c`, and both spell it the same way: store zero, test `byte_280c[0]`, then increment-store-test-bound until either the byte is zero or the index is above 4.

    41cd9: movzwl 0xa968(%esi),%ebx
    41ce0: inc    %ebx
    41ce4: cmpb   $0x0,0x280c(%ecx,%esi,1)
    41cec: mov    %bx,0xa968(%esi)         ; the member is the loop variable
    41cf3: je     41cfb                    ; a clear phase: stop
    41cf5: cmp    $0x4,%bx
    41cf9: jle    41cd9                    ; otherwise keep going while <= 4

So the largest value the loop can leave in +0xa968 is 5, reached when all six bytes are nonzero -- and 5 is also a perfectly good phase number. The very next instructions read the field back and branch on it being ABOVE five:

    41cfb: movzwl 0xa968(%esi),%eax
    41d02: cmp    $0x5,%ax
    41d0b: jg     41ef7                    ; "no unsuspected phase": clear it and skip

`41ef7` pops the sentinel off the FPU stack, stores zero into +0xa968 and into the stack slot the report prints, and jumps past the whole study. It is 18 bytes of code that no input can reach, and what it is FOR is legible: the author meant the scan to run to 6 and to spell "none found" as 6, which is what `for (i = 0; i <= 5 && byte_280c[i]; i++)` would have left. The bound is one short of that, so the "none found" case and the "phase 5" case are the same value and the phase-5 reading wins.

Both spellings are reproduced: the scan with its `<= 4`, and the guard with its `> 5`. A modern `-O2` may prove the arm dead and delete it, which costs nothing at tier 1 -- the behaviour is identical either way, and `t_v90adid` asserts the scan's answer directly, sweeping all six stopping points and the all-nonzero row that lands on 5.

## D286 🐛 `porcessSecondStudy` initialises its second-nearest distance once for the whole call

*Batch of 2026-08-12, same function, +0x00 (`flds 0x358` -- the FIRST instruction, before the register saves) against +0xc4 (`fld %st(3)`, once per (code, phase)). **Reachability: FIRES on the second repair decision of every call** -- the carried value is what all 701 remaining (code, phase) pairs are judged against. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1431.** For each of 117 codes and each of the six phases, the method measures the distance from the phase's own mapping entry to three neighbouring entries of the reference phase's, keeps the nearest and the second nearest, and repairs the entry only if the nearest is unambiguous: exact, or beaten by a factor of more than two.

The nearest distance is initialised per (code, phase) -- `fld %st(3)` copies the constant 32,256 into a fresh slot at the head of the phase loop. The second-nearest is not. It is loaded once, from `.rodata.cst4+0x358`, before the function has even pushed its registers, and the reg-stack allocator then REUSES that slot for the running value -- `fstp %st(6)` at 0x41e00 writes over it on the first update. An x87 slot can only be overwritten like that if the value in it is dead, so the load is a one-time initialisation and not a per-iteration one; there is no second `flds` anywhere in the 831 bytes.

The consequence is that "the runner-up" is not this code's runner-up but the smallest runner-up seen since the method was entered, and it only ever decreases. The ratio test therefore gets monotonically harder as the call proceeds: an entry that would be repaired on its own merits at code 100 is refused because code 3 happened to have two near-equal candidates.

The sentinel makes the first decision work at all. `.rodata.cst4+0x358` is 0x7fc00000, a NaN, and the update is guarded by `jae` -- which an unordered compare does not take -- so the first distance that fails to improve the nearest becomes the second-nearest whatever its size. Written as `!(d >= second)` rather than `d < second` for exactly that reason.

`t_v90adid` separates the two spellings with a constructed witness, because a sweep cannot: three random distances are all 32,256 or more about once in a hundred million. Phase 0 is left unsuspected and given -31746 across its row against 32767 in every other phase's, so all three distances are 64,513 -- above the nearest's sentinel, and 2.000031 times it, which is over the threshold by three parts in a hundred thousand. With the NaN the entry is repaired; with any ordered sentinel, or with 32,257 in place of 32,256, it is not. A second witness primes the second-nearest to 499 at code 0 and then asserts that code 3 -- whose three distances are 65,535 -- is NOT repaired, which is the carry itself: a per-iteration reset repairs it.

## D287 🐛 The DIL repair path indexes the sample store and the mapping table without bounds

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::updateAltRbsPhaseInDil` (blob 0x41840, 1124 bytes), +0x363 (`add %edx,0x60(%esp)`), and from `porcessSecondStudy` (blob 0x41cb0) +0x106 (`movswl 0x3c(%esp),%ebp`). **Reachability: the first needs a histogram that disagrees with what was stored, the second FIRES at codes 0 and 1 on every call.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1430.** Two unbounded indices in the same cluster, and neither is D256's.

THE SAMPLE CURSOR. `updateAltRbsPhaseInDil` treats each phase's sample store as "the samples for the codes in scan order, end to end": it starts a cursor at zero and, for each of the 115 codes it visits, quantises `short_8b00[phase][code]` samples starting at the cursor and then advances the cursor by that count. Nothing compares the cursor against the row's 0x83e entries, and nothing compares it against `int_9100[phase]`, which is what `addReceivedSampleToStorage` actually filled. The two agree only because both are driven by the same call: one increments the histogram bin, the other appends to the row. Anything that puts a count in `short_8b00` without a matching sample -- and the class has no other writer, so this is a claim about callers -- walks the cursor past the row, into the next phase's samples and eventually out of the object. The test clamps the seeded histogram to sixteen per code for that reason, which is 1,840 against 2,110; the forty-block sequence needs no clamp at all, because six samples a phase for forty blocks is 240.

THE NEIGHBOUR WINDOW. `porcessSecondStudy` chooses between the window [code, code+2] and the window [code-2, code] by which side of the reference the phase's own entry falls, and it runs `code` from 0. At codes 0 and 1 the second window indexes `linMapp[unSuspectedPhase][-2]` and `[-1]`, which for an unsuspected phase of 0 -- the commonest value, and what a clear `byte_280c[0]` gives -- is four bytes IN FRONT of the object. Above, the window reaches `linMapp[phase][118]` at the top code of 116, which is inside the row. The read is never a write, so nothing is corrupted; what the object gets is whatever precedes it in memory, and the repair decision for two codes out of 117 turns on it.

`t_v90adid` puts a sixteen-byte guard in FRONT of the fixture, seeded alike on both sides and compared like the one behind it. Without it the two sides read two different pieces of unrelated memory and the comparison measures the linker's layout; with it, codes 0 and 1 are tested for real rather than steered around by forcing the unsuspected phase away from zero, which would have left the commonest path untested.

## D288 🐛 `determineMaxUcode`'s report loop never ends at an argument of 255, and its scan window reads outside the row

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::determineMaxUcode` (blob 0x441f0, 1189 bytes), +0x1aa (`movzbl %bl,%ecx ; cmp %edi,%ecx ; jle`) and +0x25b (`lea 0x9d48(%ebp,%eax,4),%eax` followed by `subl $0x4,0x14(%esp)`). **Reachability: the first needs an argument of 255 or more, the second FIRES on every call whose scan reaches the bottom of the reference phase's row.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1438.** Two indices the method forms out of a `short` argument and never bounds.

THE REPORT LOOP CANNOT END. It prints `linearMappingVar[unSuspectedPhase][k]` for `k` from `short_a97a + 1` up to the argument, and `k` is an `unsigned char` zero-extended for a SIGNED compare against the argument as an `int`. At an argument of 255 the counter reaches 255, wraps to 0, and 0 is still at or under the bound -- so the loop runs for ever, printing about thirty characters a pass through `edprintf`. The object has no other terminating condition and neither does this. The reconstruction reproduces it; `t_v90adid` keeps every argument it offers at 254 or below and says so at the table.

THE SCAN WINDOW STRADDLES THE ROW. The five-entry window is `linearMappingVar[unSuspectedPhase][ci]` down to `[ci - 4]`, with `ci` an `unsigned char` walking down from the argument -- and the object forms each address by adding `unSuspectedPhase * 128 + ci` to the array base, so an index of 4 or less reaches BELOW the reference phase's row and an index above 127 reaches above it. Below phase 0 that is `uint_9d30`, four to twenty bytes in front of the array; above phase 5 it is past the object entirely, which is why the test picks the phase from the argument rather than sweeping the two against each other -- 793 is the last entry of `float_9d48` whose four bytes are still inside the 0xa9b0 the object occupies, and phase 5 plus a code of 154 is the first one out.

Neither is a write, so nothing inside the object is corrupted by either; the second decides which code the method answers with, so it is behaviour and not merely a read.

======================================================================

## D289 🐛 `determineMaxUcode`'s backwards walk has no floor, and the entry that stops it is forced by hand

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::determineMaxUcode` (blob 0x441f0), +0x3d1 (`dec %dl ; movzbl %dl,%eax ; cmpb $0x0,0xd00(%ecx,%eax,1) ; je`) and +0x374 (`mov %bl,0xd00(%ecx,%edx,1)` with %bl = 1). **Reachability: the walk FIRES whenever an unflagged phase's own maximum code is marked unusable; the non-termination needs all 256 bytes of the window clear.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1438.** The last loop gives each unflagged phase the highest usable code at or below `byte_a954`, and it finds it by decrementing a BYTE until `byte_0d00[phase][d]` is nonzero. There is no floor and no counter: the index wraps from 0 to 255 and keeps going, so the walk covers all 256 values and then repeats them for ever if none of them is set.

WHAT STOPS IT IS A STORE THE FILL MAKES ON PURPOSE. Immediately before, the fill sets `byte_0d00[phase][ucode]` to 1 for EVERY phase and whatever the variance test said about that code -- including for the flagged phases, whose 128 codes the fill skips entirely. That byte is one of the 256 the walk visits, so the walk always terminates in practice. It is the only guarantee there is, and it is a coincidence of the two loops being adjacent rather than a bound: change the reference code between them and the method hangs.

THE WINDOW STAYS INSIDE THE OBJECT. `byte_0d00[phase][d]` with `d` up to 255 is `+0xd00 + phase * 128 + d`, which at phase 5 reaches +0x107f -- inside `float_1000`, whose first 128 bytes the walk therefore reads as flags, and whose first bytes the forced store writes when `ucode` is 128 or more. Nothing leaves the object, so the test exercises the whole of it rather than bounding it.

======================================================================

## D290 🐛 `findPadGain` reads its chosen code uninitialised, and hangs outright for five values of `byte_a954`

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::findPadGain` (blob 0x43620, 2879 bytes), +0x44 (`mov %dl,0xa1(%esp)`, the only write to that slot) and +0x38 (`sub $0x5,%eax ; mov %eax,%ebx` against `movzbl %dl,%eax ; cmp %ebx,%eax ; jg`). **Reachability: the uninitialised read needs five variances of 1e8 or more with no NaN among them; the hang needs `byte_a954` in 3..7.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1439.** Both come out of the same five-entry scan at the top of the method.

THE CHOSEN CODE IS A STACK SLOT NOTHING NECESSARILY WRITES. The running minimum starts at the constant 1e8 and the slot at 0xa1(%esp) is written only when an entry beats it. Five entries of 1e8 or more leave the slot holding whatever the frame held, and that value is then clamped into [0x50, 0x5f] and used as the base code for the entire search -- so the pad gain the method reports is a function of the caller's stack and not of the object. An unordered compare TAKES, so a single NaN among the five is enough to write the slot; it is five ordered non-improvements that reach it. The local is left uninitialised here for D281's reason, and `t_v90adid` plants one small entry in every window it offers so that no trial can read it: two static objects have two different frames, and a trial that read the slot would be comparing the linker's layout rather than the reconstruction.

THE LOOP BOUND CAN BE NEGATIVE. The scan starts at `(unsigned char)(byte_a954 - 3)` and runs while the counter, zero-extended, is greater than `(int)start - 5` under a SIGNED compare. A `byte_a954` of 0, 1 or 2 wraps the start up to 253..255 and terminates normally; one of 3 to 7 puts the start at 0..4, the bound below zero, and a zero-extended byte can never fall under it -- so the loop decrements for ever, reading `float_9d48[unSuspectedPhase][d]` at every one of the 256 indices as it goes, which at phase 5 is past the end of the object. The reconstruction reproduces it and the test forces the field into [8, 0x9c].

======================================================================

## D291 🐛 `studyUrefHandler` prints a float through `%d`, twice, and the field and the report get two different roundings of it

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::studyUrefHandler` (blob 0x42140, 5335 bytes), +0xb1f (`fsts 0xa964(%ebx)`) with +0x12fe (`fstpl 0x4(%esp)` under "first update : trn1Sigma = %d"), and +0xdaf with +0x12e9 for the second copy. **Reachability: FIRES on every call that completes state 3 or state 5 with the debug level above 1.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1443.** The report is the object's own and so is the mismatch.

======================================================================

## D292 🐛 The initial-variance loop divides by a count it never tests, where every other mean in the class tests it

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::studyUrefHandler` (blob 0x42140), +0x120 (`mov 0x1c00(%ebx,%ecx,4),%ebp` straight into `push`/`fildll` with no `test`), against +0x320 in the same function (`mov 0x1c00(%ebx,%ecx,4),%edi ; test %edi,%edi ; je`). **Reachability: FIRES whenever a phase has no samples for the reference code at the end of state 0.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1442.** One loop out of five in this method omits the guard the other four have.

======================================================================

## D293 🐛 Two local arrays are filled by six loops and read by nothing

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::studyUrefHandler` (blob 0x42140), +0x40c, +0x43c, +0x6dc, +0x70c, +0x9ec and +0xa1c -- six `mov %rX,0x50(%esp,%ecx,2)` stores into one twelve-byte slot, and not one read of it anywhere in the 5,335 bytes. **Reachability: FIRES at the end of states 1, 2 and 3; state 5's otherwise identical tail does not have it.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1444.** Dead stores the compiler kept because the locals are arrays.

======================================================================

## D294 🐛 The study's states are numbered out of the order it runs them in, and one of its six durations is read by nobody

*Batch of 2026-08-12, from the jump table at `.rodata+0xd70` and `V90AutoDigitalImpDetector::studyUrefHandler` (blob 0x42140) +0x7cc (`mov $0x4,%esi ; mov %esi,0xa984(%ebx)` in the arm the table's entry 2 points at) with +0x1243 (`mov $0x3,%eax` in the arm entry 4 points at); and `int_a9a0` at +0xa9a0, copied in by `resetStudyUrefHandler` at 0x40913 and 0x40a94 and named in no displacement of any of the class's thirty-two members. **Reachability: the ordering FIRES on every study; the dead duration is never read at all.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1441.** The chain is measured from the arms and not from the numbers.

======================================================================

## D295 ⚠ Every floating-point compare in this class is signalling in the object and quiet in ours

*Batch of 2026-08-12, from `V90AutoDigitalImpDetector::determineMaxUcode` (blob 0x441f0) and `findPadGain` (blob 0x43620); `fcomp %st(1)` at 0x4446a and 0x44657, `fcomps` at 0x442a4, 0x442c0, 0x44347, 0x444eb. **Reachability: FIRES ON EVERY NaN** -- and NaNs are routine here, not exotic: `float_9d48` is a seeded variance and one 32-bit pattern in 128 is a NaN, and `findPadGain` forms gains from `ulaw2linear(0xff)`, which is 0. **Observability: NONE** -- nothing in the object, the library or the tests reads the x87 status word or unmasks the exception. Status: `unmeasured`. Fix class: none proposed.*

**Finding F1447.** The object compares with the `fcom` family, which raises the invalid-operation exception on a QUIET NaN. Every comparison this tree emits for the same source is `fucom`/`fucomp`/`fucompp`, which raises it only for a SIGNALLING NaN. The two agree on every branch taken -- that is what the differential tier checks and it passes over seeded NaNs at all three of `determineMaxUcode`'s float tests -- and they disagree on the exception flags left behind.

It is not a spelling that can be chosen away. It was first supposed that a relational operator would give `fcom` where `==` gives `fucom`, which would have made the D281-era respelling of the NaN test a fix for this as well; disassembling our own period-built object shows `fucomp` for `<` and `>` too, under **both** GCC 3.4.2 and GCC 13. GCC simply does not emit `fcom` from C comparisons. Reproducing it would take inline assembly at six sites for a difference no caller can observe, which is a worse trade than recording it.

======================================================================

## D296 🐛 The retrain detector stores the three accumulators it is about to clear

*Batch of 2026-08-12, from `VPcmV34Progress` (blob 0xb3c0) +0x30b, +0x311 and +0x314 -- `mov %eax,0x14(%ebp)`, `mov %edx,0x18(%ebp)` and `movl $0x40,0x1c(%ebp)` into the object's +0xac30, +0xac34 and +0xac38 -- against +0x349, +0x350 and +0x357, which write zero into all three unconditionally eight instructions later on every path out of the block. **Reachability: FIRES once every 64 samples of every call whose progress code is 3 to 6, on the arm where the input/output energy ratio counts a block.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1464.** The block that decides a notch has been detected saves the two energies and the block counter, increments the signal count, prints them, and then falls into the same three-word clear the *undetected* path uses. Nothing reads any of the three between the store and the clear, and the compiler kept them because they are member stores through a live pointer.

The reconstruction writes them, because the object does and because a differential test cannot tell a dead store from a live one -- only the same three writes in the same order keeps `make similarity` honest about what the source contained.

======================================================================

## D297 🐛 Two of the transmit loops have no iteration bound, and neither callee is obliged to make progress

*Batch of 2026-08-12, from `VPcmV34Progress` (blob 0xb3c0) +0x7bc-+0x7fd (the K56flex arm) and +0x2b4-+0x8e7 (the V.90 arm) -- `cmp %si,(%edx) ; jge` around calls to `modulatevector`, `v34handshak`, `v90RateReneg` and `v90RateRenegSilence`, with the loop's only exit the transmit queue reaching `f2aa0` and nothing counting the turns. **Reachability: hangs only if a callee returns without enqueueing, which no test in this tree has produced.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1465.** The V.34 arm beside them is bounded -- it runs exactly `nin & ~3` times and calls the same two functions once each -- so the shape is not the file's habit; it is these two arms. `f2aa0` is set to the block length immediately above the loop, so the ordinary case terminates in one or two turns.

Written as the object has it. A bound here would be a behavioural difference on an input the differential tier cannot produce, and the arms are the V.90 and K56flex ones, which no test in this tree drives at all.

======================================================================

## D298 🐛 `V22_MRF_filter`'s startup window is not the window a wrapping buffer would give

*Batch of 2026-08-12, from `V22_MRF_filter` (blob 0x8d160) +0xf2 — the branch taken when `widx < history_len`, which convolves `history[0 .. 29]` against `coeff[phase * 30 .. +29]`, where the branch at +0xab convolves `history[widx - 30 .. widx - 1]` against the same coefficients. **Reachability: FIRES on the first thirteen outputs of every stream, because `widx` climbs by two or three per output and the branch is taken until it reaches 30.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1543.** The buffer is 60 entries and only the first 30 are zeroed, so the clamped branch is reading exactly the zeroed startup region — it cannot read uninitialised memory. What it gets wrong is *where in the window the new samples sit*. With `widx` at 2, the sliding form would want `history[-28 .. 1]`; the object gives `history[0 .. 29]`, so the two newest samples are weighted by the OLDEST two taps of the phase rather than the newest two. The transient is thirteen outputs long and then the branch is never taken again for the life of the state.

`FPM_MRF_filter` does NOT do this — its buffer is genuinely circular, hlen entries with a two-part walk that wraps, so its startup window is the correct one with zeros in the tail. The clamp is specific to the V.22 copy, and is a consequence of its buffer being a 60-entry sliding one rather than a 30-entry ring. Reproduced exactly; a differential test that fed a stream and compared only the steady state would not see it, which is why `t_v22_mrf.c` compares from the first sample.

## D299 🐛 `V22_PPS_filter`'s startup branch indexes the coefficients at `phase`, where its steady-state branch indexes them at `phase * history_len`

*Batch of 2026-08-12, from `V22_PPS_filter` (blob 0x8d3c0) +0x23e — `add %edi,%edi ; lea (%edi,%ecx,1),%esi` with `edi` holding `phase`, so the base is `coeff + phase` — against +0x138, where the same base is `imul %esi,%eax ; add %eax,%eax ; lea (%eax,%ebx,1),%esi` with `esi` holding `history_len`, so `coeff + phase * 3`. Both then read three consecutive entries. **Reachability: FIRES on roughly the first forty outputs of every stream. `widx` advances once per SYMBOL, not once per output, so it takes three symbols — about 40 samples at 13.33 per symbol — to reach 3 and leave the branch.** Status: `unmeasured`. Fix class: none proposed.*

**Finding F1545.** The two branches differ in the history base for a good reason — the window has not filled — but the coefficient base is not part of that. Init lays the array out as `[p * 3 + t]`, so phase `p`'s three taps are at `coeff[3p]`, `coeff[3p + 1]` and `coeff[3p + 2]`. Reading them at `coeff[p .. p + 2]` lands on phase `p / 3` and, whenever `p` is not a multiple of 3, straddles into the next phase — three coefficients that do not belong to one phase and, except at `p = 0`, do not belong to the phase being emitted either.

`v22_mrf`, whose startup branch is otherwise the same shape, uses `phase * history_len` in BOTH branches — see D298, which is the other half of this pair. So the two files disagree, and this one is the odd one out.

How much of that transient it actually changes is measured rather than reasoned: the earliest outputs run against an all-zero history and are zero whatever the coefficients are. Reproduced exactly; `t_v22_pps.c` compares from the first output and a mutation that "corrects" the base to `phase * history_len` fails 26 checks in each of its three drive patterns, so the difference is measured and not supposed.

## D302 🐛 `FSE_decision_16pt` reads its magnitude table thousands of entries past the end

**Renumbered twice: D298 -> D300 -> D302.** It was allocated D298 on the `v32-datapump` branch while `v22-datapump` independently allocated the same number to `V22_MRF_filter`; the V.32 side moved to D300 at that merge. A later batch of V.22 work had meanwhile allocated D300 to `FPM_atan`, so it moved again. The V.22 entry stayed put both times because its own number is the product of a careful by-line renumber that its findings describe, and moving it would have falsified that account. Citations written before either merge may still say D298 or D300 -- it was allocated D298 on the `v32-datapump` branch while `v22-datapump` independently allocated the same number to `V22_MRF_filter`; the collision surfaced at the merge and the V.32 side moved. Citations written before that merge may still say D298 -- `refcheck.py` cannot catch those, because they still resolve, just to the wrong entry.

*Batch of 2026-08-12, from `FSE_decision_16pt` (blob 0x80e90, 490 bytes) +0x13b (`sar $1,%edi`) feeding +0x13d (`movzwl DECv32_MAG9600-0x2(%edi,%edi,1),%eax`). **Reachability: FIRES on every call** — `FSEv32_decision` (`.data` 0x74cc) is three function pointers and `_16pt` is slot 1; `SetRxModeV32` installs it on one arm of its seven-way jump table (0x819d0). **Observability: `*mag`, and through it the equaliser's whole error term.** Status: CONFIRMED, and the shift that belongs there is established four ways. **Fix class: DELIBERATE FIX, behind `DSPLIB_REPRODUCE_BUGS`** — updated 2026-08-17, findings F3800 and F3801.*

**Taxonomy: deliberate fix, with two rings that no setting can reproduce.** The entry is a *deliberate fix* under the register's own list: the original is wrong, the reconstruction corrects it, and `DSPLIB_REPRODUCE_BUGS` restores the object so bit-exactness stays provable. **The tension worth stating is the outer and mid rings.** `Added hardening`'s MECHANISM applies to them — "a differential test cannot compare against a fault" — but its PRECONDITION, "an input no caller produces", applies to neither: both rings are ordinary constellation points that any 9600 bit/s symbol reaches. That is exactly what makes this a live defect rather than a curiosity, and it is why the reproduce-bugs arm returns a stated value for them rather than a measured one.

**Finding F1603.** `DECv32_MAG9600` has three entries and is indexed `[n - 1]` off `(short)(|I| + |Q|)` for the decided point, whose only three values are 8192, 16384 and 24576. `FSE_decision_16Tpt` divides that by 8192 first (`sar $0xd`) and lands on 0, 1 and 2. `_16pt` divides by 2 and lands on 4095, 8191 and 12287 — 8190, 16382 and 24574 bytes past a six-byte table. `.data` is 0x9594 bytes, so only the first of the three is even inside the section; `.data+0x94f6` holds 0 and the other two are past the end of it.

The two functions are otherwise the same shape over the same table, which is what makes this a slip in `_16pt` rather than a misreading of either.

**WHAT THE OBJECT RETURNS**, measured in the linked daemon `slmodemd/slmodemd`, where `DECv32_MAG9600` is at 0x110878:

| point | \|I\|+\|Q\| | index | lands in | value | correct |
|---|--:|--:|---|--:|--:|
| inner | 8192 | 4095 | `.data` | **0** | 5792 |
| mid | 16384 | 8191 | `.bss` | **0** (zero at load) | 12953 |
| outer | 24576 | 12287 | past the last section | fault / arbitrary | 17378 |

**WHY 13 IS THE FIX, four independent lines.** (1) `>> 13` is the only shift that covers the table exactly: the three sums map to 1, 2, 3 and so to entries 0, 1, 2. (2) The table is right and complete — { 5792, 12953, 17378 } are the constellation's three L2 magnitudes to the unit (4096·√2 = 5792.6, √(12288² + 4096²) = 12952.99, 12288·√2 = 17377.9), so it is an L1 → L2 conversion. (3) `_16Tpt` computes the same quantity off the same two tables and shifts it `sar $0xd`. (4) `_4pt` and `_trn` hard-code `*mag = 0x3299` = 12953 = `DECv32_MAG9600[1]`, corroborating the table and its scaling from a third place.

**THE IMPACT, and it is shared with D451.** `*mag` reaches `src/dsp/fpm_fse.c:385` as `err_i = ((ph.cos * mag) >> 14) - i_val` and its quadrature twin, and that error drives the LMS tap update — so a zero magnitude tells the equaliser the decided point is the ORIGIN and it adapts on the whole received vector instead of on the decision error. That is D302's half. **It is not the whole failure of this function**: D451 is a second, independent defect in the same slicer whose unscaled metric collapses the decision itself, so `*angle` and the returned bits are wrong too and the carrier loop and the data path are affected as well as the equaliser. Neither defect alone explains the non-trellis 9600 bit/s path; both are needed.

**THE REPRODUCE-BUGS ARM IS A FLAT ZERO, and one third of that is measured.** Zero is what the daemon returns for the inner and mid rings. It is a stated CHOICE for the outer one, where there is nothing to reproduce. No out-of-range subscript is written at any setting: it would be undefined behaviour in a tree that has to stay 64-bit clean, and it would read OUR bytes rather than the object's.

**AND ONLY THE INNER RING IS DIFFERENTIALLY COMPARABLE — narrower than the table above.** The differential tier links the blob as `dsplibs_ref.o`, so the reads are at `ref_DECv32_MAG9600 + 8190/16382/24574`, i.e. blob `.data+0x94f6`, `+0xb4f6` and `+0xd4f6`. Only the first is inside the blob's own section, 158 bytes short of its end, and only that byte travels with the blob into any link. The other two leave the section, which is 1603's argument bounded to two rings instead of three. `t_v32fse.c` compares `*mag` on the inner ring — 89 of 1,466 symbols — excludes the other two, asserts both exclusions non-zero, and POISONS the pages it has to provide so that widening the gate fails loudly rather than passing against an anonymous zero page. Finding F3800 has the measurement and the four mutations, of which M1 fails exactly 89 checks.

**1603's verdict is superseded and its analysis is not.** Every number in it was re-derived and confirmed; what changed is the conclusion that the whole function had to stay out, when only one store did.

======================================================================

## D301 🐛 `FSE_decision_4pt` weights the in-phase error twice the quadrature one, so it is not a nearest-point decision

**Renumbered from D299.** It was allocated D299 on the `v32-datapump` branch while `v22-datapump` independently allocated the same number to `V22_PPS_filter`; the collision surfaced at the merge and the V.32 side moved. Citations written before that merge may still say D299 -- `refcheck.py` cannot catch those, because they still resolve, just to the wrong entry.

*Batch of 2026-08-12, from `FSE_decision_4pt` (blob 0x81080, 468 bytes) +0xf4 and +0xf7 — `sar $0xf,%edx` on the in-phase squared error against `sar $0x10,%eax` on the quadrature one, summed and compared. **Reachability: FIRES on every call.** Status: `measured`. Fix class: none proposed.*

**Finding F1607.** The two squared terms are scaled one place apart, so the metric is `(i-I)**2 / 2 + (q-Q)**2 / 4` and the decision regions are ellipses rather than circles. Every other slicer in the family shifts both terms alike — `_16Tpt` uses 16 and 16, `_64pt` 13 and 13 — which is what makes this one the odd one out rather than a family convention.

It changes answers, not just margins. Evaluating the object's own expression at a received `(2000, 2000)` gives 2749, 6797, 3249, 3297 and picks point 0, where the symmetric metric gives 2182, 3682, 3182, 1682 and picks point 3. At the origin it gives 2816, 4864, 2816, 4864 where a symmetric metric ties all four, because the four points are equidistant from the origin.

Reproduced as written, and covered by `t_v32fse`'s four-point passes — the differential test cannot tell a deliberate weighting from a slip, which is why this is recorded here rather than argued in a comment.

======================================================================
======================================================================

## D65 🐛 `FPM_log10` reads one element past its table, and it is NOT the lucky one

**Module** `src/dsp/fpm_log10.c` · original `FPM_log10`, `.text 0x0a8d20`,
table at `.rodata 0x00c3a0`

**Defect in the original, reproduced deliberately.** The mantissa is
normalised into `[0x4000, 0xffff]` and then indexed with

    idx = ((norm + 64) >> 7) - 128

For a Q15 caller -- mantissa `0x0000..0x7fff`, which is the domain D4's
analysis established for this layer -- `norm` lands in `[0x4000, 0x7fff]` and
`idx` runs **0..128**. The table has 128 entries, `0..127`. So mantissas of
`0x7fc0..0x7fff`, **64 of the 32768 Q15 values**, read one element past the
end during ordinary operation.

**`FPM_sqrt` has exactly this defect and escapes it.** What follows ITS table
is a 32768, which is precisely the value `sqrt_table[192]` should hold, so the
overrun returns the right answer by coincidence (D4's entry). This one is not
so lucky. What follows this table at `.rodata:0xc4a0` is `FPM_PPS_CFG`, whose
first short is **10**, where the correct entry -- `log10(1.0)` in Q15 -- is
**0**. Shifted down by three that is 1 count in Q12, so the result is one low
count high over that range rather than catastrophically wrong, which is
presumably why nobody noticed.

**Reachability: UNMEASURED.** The 64 mantissas are reachable *if* a caller
presents them; no caller of `FPM_log10` is reconstructed yet, so whether the
V.32 datapump ever normalises into `0x7fc0..0x7fff` is not established.

**REPRODUCING IT NEEDED A 129TH TABLE ENTRY, AND THAT IS THE INTERESTING
PART.** The reconstruction's table has 129 entries and the last is **10** --
not a logarithm, but a transcription of the neighbouring object's first
short. Without it, our overrun reads whatever our own linker placed after the
array, and when this file was first written that happened to be a value in
8..15, which agrees after the `>> 3`. **The test passed for that reason and
nobody would have known.** It would have started failing the next time
anything was added to the translation unit, and the failure would have looked
like a defect in the logarithm.

`FPM_sqrt` sets the precedent by adding a 193rd entry (D4). The difference is
that its extra entry is the mathematically correct one and this one is a
transcription of the defect, so the generator explicitly does not produce it:
`FPM_log10_table_derived()` returns 128 and the test asserts entry 128
separately, against 10, noting that the generator would have said 0.

**ABOVE Q15 THERE IS NO REPRODUCTION AND NONE IS CLAIMED.** A mantissa of
`0x8000..0xffff` needs no normalising and indexes up to 384, so the original
reads up to 512 bytes past its table. The unit test deliberately does not
compare that range: two builds disagreeing about memory neither of them owns
is not a defect in either. An earlier version of the test did compare it and
failed, which is how the luck above came to light.

Reproduced as-is over the Q15 domain: `test/unit/t_fpm_log10.c` drives all
32,767 non-zero Q15 mantissas against the blob, and the 64 that overrun again
by name.

======================================================================

## D66 ⚠ `FPM_log10`'s exponent coefficient is 1228 where log10(2) is 1233

**Module** `src/dsp/fpm_log10.c` · original `FPM_log10`, `.text 0x0a8dbe`

**UNMEASURED.** The result is `(table[idx] >> 3) - e * 1228`, in Q12. The
table half is Q15 log10 shifted down by three, which is Q12 exactly -- the
derivation reproduces all 128 entries with no slack. The exponent half should
therefore be `e * log10(2) * 4096 = e * 1233.2`, and the object multiplies by
**1228** (`imul $0x4cc`): 0.29980 against 0.30103, 0.4% low.

It is the only constant in the function that does not follow from the table,
and the error is proportional to the exponent rather than bounded -- 5 counts
in Q12 per octave of normalisation, about 0.0012 of a decade, or 0.024 dB per
octave read as a power ratio. A signal normalised through ten octaves is half
a count of Q12 short of where the table says it should be.

Whether that matters depends on what reads the result, and nothing that does
is reconstructed. Recorded because the two halves of one function disagreeing
about the same constant is the shape of a transcription error, not of a
deliberate approximation -- but "deliberate approximation" is not ruled out
either, and neither reading is established.

Reproduced exactly.

## D300 🐛 `FPM_atan` reflects the fourth quadrant through 0x7fff, where the other three are exact

*V.22 SRE batch, from `FPM_atan` (blob 0x0a6a50) +0x0fa. **Reachability: FIRES**, on any vector with `y < 0`, `x > 0` and `|x| > |y|` -- a full 45-degree wedge, and `t_fpm_atan` visits it hundreds of thousands of times. Status: CONFIRMED. Fix class: none proposed; reproduced as measured.*

**Finding F1586.** A full turn is 0x8000, so the reflection of an angle `t` about the positive x axis is `0x8000 - t`. The object computes `mov $0x7fff,%ebx; sub %ecx,%ebx` and uses that. The other three reflections in the same function -- `0x4000 - t`, `0x2000 - t` and `0x6000 - t` -- are all exact, which is what makes this one an error rather than a convention: a vector just below the positive x axis reports 0x7fff where the correct answer is 0 (or 0x8000, the same angle). The size of the error is one count everywhere in the wedge, so it is a constant bias of about 0.011 degrees and not a discontinuity. `V22_SRE_recover`, the only caller in the object, folds the result at 0x4000 immediately afterwards, so the bias reaches its timing loop as a one-count offset in `err_avg` and is swamped by the smoother's own truncation.

## D307 🐛 `reset_dtmf` clears sixteen of the twenty bytes of the digit string

*Renumbered: this was numbered **298** in this register until 2026-08-12. V.22 and V.32 had each independently allocated 298 and 299, and master took 300 and 301, so this session's five entries moved up to 302-306. Nothing else about the entry changed.  **And renumbered once more at the merge into master:** 302 was already taken there by `FSE_decision_16pt`, itself moved D298 -> D300 -> D302, so this entry is now **D307**.  Its four companions D303-D306 were free and did not move.*

*Batch of 2026-08-12, from `reset_dtmf` (blob 0x90a90) +0xdc-+0xef and the
same loop inlined into `create_cid_dtmf` (0x90bb0) +0xfc-+0x10f -- `mov
%cl,0x340(%eax,%ebx,1)` with `inc %eax ; cwtl ; cmp $0xf,%ax ; jle`, so the
index runs 0..15 over an array whose length is 20. **Reachability: every
reset and every construction, unconditionally.** **Observability: needs a
string of seventeen or more digits that is then read without a terminator** --
`dtmf_modem` writes `digits[ndigits]` for each digit and only writes the
terminating zero when the string ends with 'C', so `digits[16..19]` is stale
until a seventeenth digit overwrites it. Status: `unmeasured`. Fix class:
none proposed.*

**Finding F1701.** The array is twenty bytes and the object knows it: the next
field starts at +0x354, `dtmf_modem` stores at `digits[ndigits]` without a
bound and gives up when `ndigits` reaches 20, so all twenty are writable and
the last four are reachable. Only the clearing loop stops at sixteen.

The consequence needs a Caller ID string of seventeen digits or more that
never terminates -- an unterminated string of sixteen or fewer still ends in a
byte the loop cleared -- and then a consumer that reads it as a C string.
Neither half is produced by anything in this tree, which is why the status is
`unmeasured` rather than a defect with a measured effect.

Reproduced as found: `src/service/dtmf_rx.c` clears `digits[0..15]`, and
`test/unit/t_dtmfrx.c` stamps a known pattern into `digits[16..19]` before
every reset and asserts it comes through. Without that assertion the loop
bound would be pinned only while a random seed happened to leave those four
bytes non-zero.

======================================================================

## D303 🐛 `create_cid_dtmf` writes through `sysdep_malloc`'s result without testing it

*Renumbered: this was numbered **299** in this register until 2026-08-12. V.22 and V.32 had each independently allocated 298 and 299, and master took 300 and 301, so this session's five entries moved up to 302-306. Nothing else about the entry changed.*

*Batch of 2026-08-12, from `create_cid_dtmf` (blob 0x90bb0) +0x15f -- `movl
$0x38c,(%esp) ; call sysdep_malloc ; mov %eax,%ebx ; jmp +0x10`, and +0x10 is
`mov %cx,0x33c(%ebx)`, the store of the sample rate. The function's only
`test` is of its PARAMETER, at +0x8, and the allocating branch rejoins after
it. **Reachability: only when the allocation fails.** **Observability:
immediate -- a write to offset 0x33c of a null pointer.** Status:
`unmeasured`; the harness has no allocation-failure injection, so the path
cannot be driven differentially at all. Fix class: none proposed.*

**Finding F1702.** The same shape as D5's family, D171, D175, D180 and D220,
and the same reasoning: there is nothing to reproduce, because the original's
behaviour on a failed allocation is a fault and a differential test cannot
compare against one. Unlike the three constructors in D62, ours does not add
the check either -- D62's entries exist because hardening that the original
lacks is itself a deviation, and this reconstruction had no reason to acquire
a fourth.

======================================================================

## D304 🐛 `CID_FSD_demodulate` tests its sample count *before* decrementing it, so a negative count runs 65535 times

*Renumbered: this was numbered **300** in this register until 2026-08-12. V.22 and V.32 had each independently allocated 298 and 299, and master took 300 and 301, so this session's five entries moved up to 302-306. Nothing else about the entry changed.*

*Batch of 2026-08-12, from `CID_FSD_demodulate` (blob 0x92280) +0x9..+0x1b and
the loop bottom at +0x25f..+0x26b:*

```
   92289:  movswl 0x38(%esp),%eax     ; count, as a SHORT
   92292:  dec    %eax
   92293:  movswl %ax,%edx            ; the counter is a short too
   92299:  inc    %ax
   9229b:  je     924f1               ; exit only if it WAS zero
```

*so the loop is `while (count-- != 0)`, not `while (count > 0)`.
**Reachability: any caller passing a negative count.** **Observability: 65535
iterations, each reading one sample past the caller's array and each able to
write a bit past the caller's bit buffer.** This batch did not trace where
`cid_modem`'s count comes from, so it is not claimed to be unreachable in
practice -- only that nothing here produces it. Status: `unmeasured`. Fix
class: host-side, by not passing one.*

The same idiom appears at both ends of the loop and the counter is truncated
to a short on every pass, so a count of -1 runs 65535 further iterations
before the value comes back to zero. It is reproduced as `while (count-- !=
0)` in `src/service/cid_fsd.c` with the count declared `short`, and the test
does not drive it: both sides would agree while scribbling over the harness.

======================================================================

## D305 🐛 `CID_FSD_demodulate`'s "first 128 samples" counter is a short that keeps counting, and re-arms when it wraps

*Renumbered: this was numbered **301** in this register until 2026-08-12. V.22 and V.32 had each independently allocated 298 and 299, and master took 300 and 301, so this session's five entries moved up to 302-306. Nothing else about the entry changed.*

*Batch of 2026-08-12, from `CID_FSD_demodulate` (blob 0x92280) +0x18c..+0x1ad:*

```
   9240c:  movzwl 0x60(%ecx),%edi     ; high_count
   92413:  cmp    $0x7f,%di
   92417:  mov    %dx,0x60(%ecx)      ; stored INCREMENTED either way
   9241b:  jg     925bd               ; > 127: skip the accumulation
   92426:  add    $0xff80,%dx
   9242d:  je     92665               ; == 128: derive high_level
```

***Reachability: 32768 samples above the slicing threshold in one uninterrupted
receiver session -- about 4.5 s at 7200 Hz if every sample qualified.**
**Observability: `high_sum` starts accumulating again from whatever it holds,
and 128 samples later `high_level` -- the top end of the slicing range -- is
replaced by a number derived from a total that was never cleared.** Status:
`unmeasured`. Fix class: none proposed.*

The counter is incremented on every above-threshold sample for ever, and the
guard that stops the accumulation is `<= 127` on the SIGNED 16-bit value. So
after 32768 such samples it is negative, the guard passes again, and
`high_sum += y` resumes on top of the total from the first pass; 128 samples
after that, `high_level` is set to `(high_sum + 64) >> 7` of a sum of 256-odd
values divided by 128.

A Caller ID message is about 1.5 s and is preceded by a channel seizure, so
reaching 32768 qualifying samples means a receiver left running on a live
line rather than one demodulating a message. Reproduced as found;
`t_cid_fsd.c` asserts the reference derives `high_level` at all, which is what
would silently stop happening if the counter were widened.

======================================================================

## D306 🐛 `CID_MTD_detect`'s energy accumulators wrap after 257 full-scale samples

*Renumbered: this was numbered **302** in this register until 2026-08-12. V.22 and V.32 had each independently allocated 298 and 299, and master took 300 and 301, so this session's five entries moved up to 302-306. Nothing else about the entry changed.*

*Batch of 2026-08-12, from `CID_MTD_detect` (blob 0x926a0) +0x8f..+0xd0 --
`imul` then `add $0x20` then `sar $0x6` into a 32-bit accumulator, twice, with
the comparison at +0xdf..+0xf5 done with `seta`, so both accumulators are
unsigned. **Reachability: 257 samples at full scale, 1030 at half, in one
call.** **Observability: the gate compares two wrapped totals, so a loud block
can read as quiet and be rejected.** Status: `unmeasured` -- the wrap is
arithmetic and certain, the effect on the answer depends on the block. Fix
class: none proposed.*

`(x*x + 32) >> 6` is 16777216 for a sample of -32768, so 256 of them are
exactly 2^32 and the accumulator is back at zero. Both totals wrap the same
way, so the ratio test survives in the common case where they wrap together;
what does not survive is the `> 150` energy gate, which can pass or fail on
the residue rather than on the energy.

`cid_modem` builds its block in a stack array inside a 0x1dc-byte frame, which
bounds it well below 257 samples, so this is out of contract for the only
caller in the object. `t_cid_mtd.c` drives 256 and 300 full-scale samples so
both sides are compared either side of the wrap.

## D308 ⚠ `SMCv32_PMAP16` is read signed by three functions and unsigned by two, and one declaration cannot match both

*Renumbered at the merge into master: this was **D305** on `v32-datapump`, where it was written before `cid-dtmf` took D302-D306.  Nothing else about the entry changed.*

Bit-exact; the difference is in the mnemonic, not the value.

The object loads this four-entry phase map from five places:

| site | load | function |
|------|------|----------|
| 0x7f9ea | `movzwl` | `SMCv32_encoder_dif` |
| 0x7fa6b | `movzwl` | `SMCv32_encoder_dif`, second path |
| 0x81044 | `movswl` | `FSE_decision_16pt` |
| 0x8121d | `movswl` | `FSE_decision_4pt` |
| 0x814a9 | `movswl` | `FSE_decision_AB` |

One object, two signednesses, which is what two translation units with
independent `extern` declarations look like -- and is almost certainly how
the original was written, one header per pump stage.

WE DO NOT REPRODUCE THAT, because two `extern` declarations of one object
with incompatible types is undefined behaviour: C99 6.2.7p2 requires all
declarations of the same object to have compatible type, `short` and
`unsigned short` are not compatible, and no diagnostic is required across
translation units.  `tools/onedef.py` would not catch it either -- it tracks
type definitions, not object declarations -- so it would pass review looking
checked when nothing had checked it.  The tree has somewhere to record a
mnemonic difference and nowhere to make undefined behaviour safe.

So it is declared once, `const short` in v32dec.h, and the two encoder loads
in `SMCv32_encoder_dif` come out `movswl` where the object has `movzwl`.
Signed is the majority reading, three sites against two, so this is the
choice that leaves the fewest sites differing; it is also what master already
had before this branch merged.

NOTHING OBSERVABLE CHANGES.  The table is `{ 4, 0, 8, 12 }` -- every value
positive, so sign-extension and zero-extension agree on every entry, and the
differential tier is silent by construction.  This is a codegen-tier entry
and `compare.py` is where it will show.

`SMCv32_PMAP_ABS16` is NOT this case and is `unsigned short`: one consumer,
`movzwl`, nothing contradicting it.  An earlier revision of this branch
flipped it to `short` for symmetry with its neighbour, which had finding
F613's rule backwards -- that a difference is unobservable is why the codegen
evidence is worth having, not a reason to set it aside.

======================================================================

## D320 🐛 `GenericToneDetector::process` withdraws the answer and puts it straight back when `blocks1` is zero

*Batch of 2026-08-15, from `_ZN19GenericToneDetector7processEf` (blob 0x10350)
0x103fb..0x10420 against 0x10453..0x1047d, and the same shape in
`_ZN19GenericToneDetector7processEPfj` (0x10490) at 0x105e4 against 0x10572.
**Reachability: `samples1 == 0` at construction, which rounds up to
`blocks1 == 0`.** **Observability: the answer at +0x38 reads 1 after a block
that missed, on one arm and not on the other.** Status: `unmeasured` -- no
caller in the object has been traced for a zero `samples1`. Fix class: none
proposed; reproduced as found.*

A block that MISSES advances `count_30`, and `count_30` reaching `blocks2`
clears `count_2c` and the answer. On the arm the threshold passed the object
then falls through to `if (count_2c >= blocks1) detected = 1`, and the
comparison is unsigned, so a `blocks1` of zero makes it true of the zero that
withdrawal has just written and the answer goes back up in the same block. The
below-threshold arm jumps to the per-block cleanup at 0x10420 without loading
`blocks1` at all and leaves it down. The array overload's weak arm does the
same at 0x105e4.

So with `blocks1 == 0` the object's answer depends on WHICH KIND of miss the
block was, which nothing else about the class suggests it should. `t_gtonedet`
drives it -- tag 206 constructs with `samples1 == 0` and requires the blob's
answer to be down after a below-threshold block -- so the asymmetry is
reproduced and pinned rather than only noticed.

## D-V92DEC-1 🐛 `getV92Decision` returns an uninitialised register on states 6, 18 and out of range

`unmeasured.` The 34-entry jump table sends states 6 and 18 straight to the
epilogue, as does the `ja` for anything above 0x21, and the epilogue is
`mov %edi,%eax` over an `%edi` no arm on those paths has written. The caller's
register value is returned as the PCM decision. Reproduced rather than
repaired -- a chosen value would be behaviour the object does not have -- and
`t_v92dec.cpp` compares object state but not the return value on those three.
Finding F2101. Whether any caller can reach a phase 3 state of 6, 18 or above
33 is not measured here.

**AND THE SIBLING HAS IT TOO — see D321.** `getV90Decision` returns an
uninitialised `%edi` on states 7, 8, 0x12 and everything above 0x21. The two
functions were reconstructed in separate sessions with no sight of each other
and each found the same defect in its own half, on a different set of states.
That is the same author writing the same shape twice, which makes it much
likelier to be a habit than a slip, and it is why neither test asserts the
return value on its own dead states.

## D-V92DEC-2 🐛 `getV92Decision` tests three `int`-returning callees at less than 32 bits

`unmeasured.` `V90AutoDigitalImpDetector::isAltRbs` and
`::isThereAnyAltRbsPhase` are declared `int` and the object tests their result
with `test %ax,%ax`; `V92Jd::unPackJdData` and `::unPackJdPhaseData` are
declared `int` and are tested with `test %al,%al`. So a return value whose low
half or low byte is zero while the rest is not reads as false here and as true
anywhere the full word is tested. The reconstruction spells the narrowing the
object performs. The likeliest explanation is that the header the author
compiled this file against declared those four narrower than the header the
callees' own translation units used; whether any of the four can return such a
value is not measured.
## D321 🐛 `getV90Decision` returns an uninitialised register on four of its states

*Batch of 2026-08-16, from `_ZN20V90Phase3Demodulator14getV90DecisionEf` (blob
0x23830) at 0x238b0..0x238b7 and the dispatch at 0x238a2. **Reachability:
`state` 7, 8, 0x12 or any value above 0x21 when the method is called.**
**Observability: the `short` the method returns, and through it whatever
`getDecision` hands its caller.** Status: `unmeasured` -- no caller has been
traced for whether those four states can be current when a sample arrives. Fix
class: none proposed; reproduced as found.*

The decision is a local held in `%edi`, a callee-saved register, and every
state that has a decision to give writes it. The default block reached by
states 7, 8, 0x12 and everything above 0x21 does `movl $0x0,0x30(%ebx)` and
falls straight into `mov %edi,%eax`, so what comes back is whatever the caller
had in that register. It is not a missing `return`: the epilogue is shared
with the thirty other states and the register simply has no reaching
definition on this path.

`t_v90p3ddec.cpp` therefore compares the OBJECT on those states and does not
assert the return value, because the two sides agree there only by accident of
being called from the same place.

**AND THE SIBLING HAS IT TOO — see D-V92DEC-1**, which is the same defect in
`getV92Decision` on states 6, 18 and out of range. Found independently, in a
different session, from a different disassembly.

## D322 🐛 State 2 calls `unPackReset` through the Jd pointer state 3 checks for null

*Batch of 2026-08-16, from `_ZN20V90Phase3Demodulator14getV90DecisionEf` (blob
0x23830) at 0x24111 against 0x24052..0x2531d. **Reachability: `state == 2` and
`word_2c == 0x30` and `word_410 != 0` with a null `jd`.** **Observability: a
null dereference.** Status: `unmeasured` -- whether the machine can reach state
2 with a null Jd has not been traced. Fix class: none proposed; reproduced as
found.*

Both states enter the TRN1d data-directed state and both call
`jd->unPackReset()` on the way. State 3 loads `0x20(%ebx)` first and, finding
it null, sets state 0x19 and prints "ERROR: Null JdDetector" instead. State 2
loads the same pointer and calls straight through it. `t_v90p3ddec.cpp` drives
the guarded arm with a null Jd; the unguarded one is left alone because
exercising it is a crash and not a comparison.

## Read against findings F1204-1209: the register and the bench are the same investigation

*Salvaged from `review/nextsteps-2026-08-11`, which was deleted as a stray;
this was the only part of it not superseded. Written 2026-08-11 -- the counts
below are that sitting's and have not been re-measured.*

Everything above was written **before** findings F1204-1209, and ranks the
register against exactly the bench problem those findings have been chasing.
Nobody had read the two together. Doing so changes what the next bench sitting
should do.

**The register is a register of the blob's defects, and the bench is measuring
the blob.** The reconstruction is unfinished, so `slmodemd` on the bench links
`dsplibs.o`. This appendix is therefore not adjacent evidence — it is a list of
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

Item 5 above is D29/D30/D32, *"three places where V.34 acquisition starts
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
   item 3 above (D137/D61 — placement dependence) moves up. Decisive
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

### And one caveat of this appendix's own is now weaker

This appendix opens by warning that the asymmetry may not be a defect at all —
V.34 negotiates the two directions independently, and *"if the link really is
asymmetric then nothing below applies"*. 1208 weakens that: a channel that
merely differed direction-to-direction would not hold **our transmit at exactly
33600 on 22 of 22 calls** while our receive is trimodal at 4800–26400 with echo
exonerated. Something one-sided and discrete is in our receiver. The appendix's
own ranking is the right list to work down.

## D323 🐛 `linearEquFadeEdges` indexes DOWN from `linearEquLength` with no bound of its own

*Batch of 2026-08-16, from `_ZN12V90Equalizer18linearEquFadeEdgesEv` (blob
0x36970) at 0x36a5e..0x36a92. **Reachability: `dfeWindowHalf > linearEquLength`,
which `reset` and `setLinearEquEdgesFadingParams` both permit.**
**Observability: reads and writes before `linearEquCoefs[0]`.** Status:
`unmeasured` -- whether any parameter block produces it has not been traced.
Fix class: none proposed; reproduced as found.*

The right-hand taper is `linearEquCoefs[linearEquLength - j - 1] *=
dfeWindow[j]` for `j < dfeWindowHalf` -- `mov %esi,%eax; sub %edx,%eax; fmuls
-0x4(%ecx,%eax,4)`, unsigned throughout. Both halves are clamped
INDEPENDENTLY to a ratio of 0.5 of `linearEquLength`, so `linearEquWindowHalf
+ dfeWindowHalf` can reach `linearEquLength` but not exceed it *when both come
from the clamp*; nothing enforces that on the fields themselves, and
`linearEquLength == 0` with any non-zero `dfeWindowHalf` makes the first index
0xffffffff. The overlapping case is reachable through the clamp and is tested
(`t_v90equ.cpp`, `run_fadeedges`, `hi_i == 4`): the middle is tapered twice and
the two sides agree. The underflowing case is not tested, for the reason D322
gives -- exercising it is a crash and not a comparison.

## D324 🐛 `calcMeanErrorStatistics`'s early exit returns an uninitialised stack slot

*Batch of 2026-08-16, from `_ZN12V90Equalizer23calcMeanErrorStatisticsEv` (blob
0x388c0) at 0x388df and 0x38d34. **Reachability: `meanErrorCount == 0 &&
meanErrorFull == 0`, which is exactly the state
`resetMeanErrorEnergyDiagnostics` leaves the object in.** **Observability: the
returned float is whatever the frame held.** Status: `unmeasured` -- whether
any caller uses the return value on that path has not been traced; `process`
is the only caller and is not yet written. Fix class: none proposed;
reproduced as found.*

The function has ONE return point, `flds 0x20(%esp); ret`, and 0x20(%esp) is
written only by the `Std<float>` call that the early exit jumps over:

    388d7:  8b 86 a0 00 00 00   mov  0xa0(%esi),%eax
    388df:  0f 84 4f 04 00 00   je   38d34          <- straight to the return
    ...
    3891e:  d9 5c 24 20         fstps 0x20(%esp)    <- the only write
    ...
    38d34:  d9 44 24 20         flds  0x20(%esp)
    38d38:  ...                 ret

Transcribed as the uninitialised local it is, which is why `V90Equalizer.cpp`
declares `float std;` with no initialiser and GCC's warning on that line is a
true statement about the original. The reconstruction cannot be bit-exact here
and no test can make it so -- the two sides read two different frames -- so
`t_v90equ.cpp`'s `run_calcmeanerror` compares the object, the transcript and
the store guard on that path and skips only the value. Every other path
compares the returned bits exactly.

## D325 🐛 `enterPhase4` reads `coefs[0]` of a zero-length filter, and leaves it out of both sums

*Batch of 2026-08-16, from `_ZN12V90Equalizer11enterPhase4Ev` (blob 0x36b20) at
0x36c83 and 0x36fb0. **Reachability: `linearEquLength == 0` or `dfeLength == 0`
for the over-read; ALWAYS for the omitted first term.** **Observability: a
four-byte read past a zero-length allocation, and a printed "coefs sum" that
excludes the first tap.** Status: `unmeasured` -- the constructor rounds both
lengths down to a multiple of four and nothing bounds them below, so zero is
constructible but has not been traced to a caller. Fix class: none proposed;
reproduced as found.*

    36c83:  d9 02           flds (%edx)          ; linearEquCoefs[0]
    ...
    36c98:  83 f8 01        cmp  $0x1,%eax       ; linearEquLength
    36ca9:  0f 86 91 ..     jbe  36d40           ; and only NOW is the loop guarded

The first coefficient is loaded before the guard, so a zero-length filter reads
it anyway; and the loop that accumulates the two sums starts at index 1, so
`coefs[0]` seeds `maxLeCoefValue` and `minLeCoefValue` and appears in neither
"coefs sum" nor "abs coefs sum". Both are the object's, and the second is
reproduced in `summarise_coefs`, whose loop also starts at 1. `t_v90equ.cpp`'s
`run_enterphase4` sweeps a zero length -- the arena's arrays are real memory, so
the over-read is inside the fixture -- and asserts the sums against a
hand-computed total that also skips the first tap.

## D326 🐛 An empty filter prints a minimum magnitude of 65536, which no `short` can hold

*Batch of 2026-08-16, from `_ZN12V90Equalizer21convertEqualizerToMmxEv` (blob
0x37330) at 0x374a5 and 0x37774. **Reachability: `linearEquLength == 0` or
`dfeLength == 0` when the equaliser is converted.** **Observability: "short
high LE coeffs min value = 65536" in the diagnostic transcript.** Status:
`unmeasured` -- the running minimum is seeded with 0x10000, one past the
largest magnitude a 16-bit half can produce, and nothing re-seeds it from the
first entry, so a filter with no taps prints the sentinel. `t_v90equ.cpp`'s
`run_converttommx` sweeps a zero length and asserts the 65536 in plain text.
Fix class: none proposed; reproduced as the object has it.*

## D327 🐛 The two renormalisations of the same step size can disagree by a whole exponent

*Batch of 2026-08-16, from `_ZN12V90Equalizer21convertEqualizerToMmxEv` (blob
0x37330) at 0x373d8..0x373e2 against `_ZN12V90Equalizer16setLinearEquBetaEf`
(blob 0x36490) at 0x36564. **Reachability: any `maxLeCoefValue` and
`linearEquBeta` whose reciprocal rounds, which is every pair that is not a
power of two.** **Observability: `linearEquMmxShift` and `linearEquMmxBeta`,
and through them every fixed-point coefficient update.** Status: `unmeasured`
-- `convertEqualizerToMmx` computes `(1.0/(beta*2**24)) * maxLeCoefValue` and
the setter computes `maxLeCoefValue / (beta*2**24)`; the two differ in the last
bit, the quotient is fed to a truncated base-2 logarithm, and one ulp there is
one step in the exponent and a factor of two in the step size. Finding F2148.
Fix class: none proposed; both are reproduced as the object has them.*

## D328 🐛 The magnitude of -32768 is -32768, so a printed minimum magnitude can be negative

*Batch of 2026-08-16, from `_ZN12V90Equalizer21convertEqualizerToMmxEv` (blob
0x37330) at 0x377f1..0x377f6. **Reachability: an `array_18` or `array_44` entry
that converts to exactly -32768, which every value at or below -32768.0f does
through `fistps`.** **Observability: "Min LE History = -32768" where the number
is documented as a magnitude.** Status: `unmeasured` -- the object takes the
absolute value as an int and then truncates it back to 16 bits with `cwtl`, so
0x8000 wraps. Fix class: none proposed; reproduced as the object has it.*

## D329 🐛 The conversion writes the ALIGNED fixed-point arrays and every clear in the class clears the RAW ones

*Batch of 2026-08-16, from `_ZN12V90Equalizer21convertEqualizerToMmxEv` (blob
0x37330) at 0x37455..0x3748c and 0x377a8..0x377ed against
`_ZN12V90Equalizer5resetEj` (blob 0x36e10) and
`_ZN12V90Equalizer18zeroLinearEquCoefsEv` (blob 0x36850). **Reachability:
whenever the constructor's `(align8(p) - p) / 2` comes out non-zero.**
**Observability: the first `skew` entries of each array are cleared and never
written, and the last `skew` of the `+ 8` headroom are written and never
cleared.** Status: `unmeasured` -- glibc's i386 malloc returns 8-byte-aligned
blocks, so every observed skew is zero; the mutation set carries the same
observation from the constructor's side. Fix class: none proposed; reproduced
as the object has it.*

## D342 🐛 `reconstructInitialConditions` searches a constellation row with no bound

*Renumbered before merge: this was **D323**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner28reconstructInitialConditionsEP16V90MappingParamsPh`
(blob 0x4b6d0) at 0x4b6f0..0x4b70d. **Reachability: a `ucode[k]` byte that does
not appear anywhere in `constellation[k][]`.** **Observability: the search runs
past the row into the next one and, in the limit, off the end of the object.**
Status: `unmeasured` -- who fills `ucode` has not been traced. Fix class: none
proposed; reproduced as found.*

`while (constellation[k][d] != ucode[k]) d++;` is a bare `jne` with no compare
against the row length, against 128, or against anything else. `d` is an
`unsigned char`, so the worst case wraps at 256 rather than running for ever,
but 256 bytes past a 128-byte row is already the next constellation's data.
`t_v90cdesign.cpp` always plants the ucode value inside its row: both sides
would walk off identically and a matching crash is not a comparison.

## D343 🐛 `calcMtoMatchKtarget` loops about 2^32 times when the target is below log2(m)

*Renumbered before merge: this was **D324**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner19calcMtoMatchKtargetEff`
(blob 0x47be0) at 0x47c3c..0x47c53. **Reachability: `kTarget < log2(m)`.**
**Observability: a hang of seconds to minutes, then a nonsense result.**
Status: `unmeasured` -- nothing in the object calls this function at all (see
D345), so no caller's range is known. Fix class: none proposed; reproduced as
found.*

`(kTarget - log2(m)) / 6` is truncated toward zero into an `unsigned int`, so a
negative value becomes something near 2^32, and the doubling loop that follows
counts that value down one at a time. The conversion back to a float for the
fractional part reads the same low dword as unsigned again. The test keeps
`kTarget` above `log2(m)`: both sides hang identically and a timeout says
nothing about the reconstruction.

## D344 🐛 `constelBuild` and `findNextUcodeToAdd` index a 128-entry row with a byte

*Renumbered before merge: this was **D325**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner12constelBuildEss`
(blob 0x47b40) and `_ZN24V90ConstellationDesigner18findNextUcodeToAddEPhhPA128_sS2_PsPA128_h`
(blob 0x4b580). **Reachability: `constellation[which][0]` above 127, or a start
index above 127 in the second function's zero-dmin walk.** **Observability: the
read lands in the following row.** Status: `unmeasured`. Fix class: none
proposed; reproduced as found.*

Both index flat -- `(which << 7) + i` with no masking and no bound on `i`
beyond a byte -- so `i` reaching 128 is the next row's entry 0 rather than an
error. `findNextUcodeToAdd`'s second walk stops only when the index goes
NEGATIVE as a `signed char`, which is 0x80, so it reads element 128 by
construction whenever it runs to its bound. The test allocates seven rows
where six are used, so that both sides read the same defined bytes.

## D345 ⚠ Eleven of `V90ConstellationDesigner`'s members have no caller in the object

*Renumbered before merge: this was **D326**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from a sweep of every `R_386_PC32` relocation in
`.text`. **Reachability: none from inside the object.** **Observability:
none.** Status: verified -- the sweep is exhaustive over the section's
relocations. Fix class: documentation only.*

`pow6`, `calcK`, `realK`, `maxK`, `calcMtoMatchKtarget`, `findMinValueIndex`,
`findConstelMaxValueIndex`, `constelBuild`, `spectralDesign`,
`reconstructInitialConditions` and `findNextUcodeToAdd` are reached by nothing
in `dsplibs.o`. They survive because a non-static member function has external
linkage. Recorded because it bounds what can ever be learned about them: no
call site types an argument, fixes a return, or constrains an input range, so
every deviation above is `unmeasured` for a reason that will not change.

## D346 ⚠ `maxK` and `realK` add a fudge before truncating, and the two fudges differ

*Renumbered before merge: this was **D327**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner4maxKEP16V90MappingParams`
(blob 0x47a10, `fadds` 1e-6f) and `_ZN24V90ConstellationDesigner5realKEP16V90MappingParams`
(blob 0x4ab10, `fadds` 1e-9f). **Reachability: every non-zero product.**
**Observability: a K one larger than the exact logarithm gives, for a product
within a part in 10^6 below a power of two.** Status: `unmeasured` -- whether
any real constellation product lands in that window has not been computed. Fix
class: none proposed; reproduced as found.*

`maxK` truncates toward zero, so the 1e-6f is what stops a K of exactly 12
arriving as 11.9999995 and truncating to 11. `realK` returns a float and
truncates nothing, so its 1e-9f changes only the last place -- the same
correction applied where it cannot matter. Both are reproduced as written.

## D347 ⚠ GCC 13 quietens a signalling NaN where the blob's compiler copies bits

*Renumbered before merge: this was **D328**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner14spectralDesignEj28V90SpecialSpectralConditions`
(blob 0x47950) against `build/src/pump/v90/V90ConstellationDesigner.o`.
**Reachability: a signalling NaN in one of the eight `V90Parameters` spectral
shaper floats.** **Observability: bit 22 of the copied word comes out set;
`V90MappingParams+0x636` reads 0xef where the blob leaves 0xaf.** Status:
verified for the modern build, absent from `make period`. Fix class:
documentation only -- the source is right and neither build is patched.*

The object copies each shaper float with `mov`, which is what GCC 3.4.2 emits
for a float assignment. GCC 13 with `-mfpmath=387` emits `flds`/`fstps`, and
an x87 load quietens a signalling NaN. Spelling the copy as a `memcpy` would
make both builds agree and would be papering over a compiler divergence in
`src/`, so the assignment stands and `t_v90cdesign.cpp` excludes exactly that
one encoding from its fill -- the eight fields are read by
`Vparser_read_float` from a configuration file and cannot hold one.

## D348 🐛 `maxK` returns one less than the exponent from 2^22 upwards

*Renumbered before merge: this was **D329**, which the `V90Equalizer` batch had independently taken and landed on master first. Nothing else about this entry changed.*

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner4maxKEP16V90MappingParams`
(blob 0x47a10) at 0x47a94..0x47ac6. **Reachability: a constellation-size
product of 2^22 or more.** **Observability: a K one below the true base-2
logarithm.** Status: verified over 2^1..2^48 by `t_v90cdesign.cpp`, both sides
agreeing at every exponent. Fix class: none proposed; reproduced as found.*

The divisor `log10(2)` reaches memory as a float before the divide while the
dividend keeps the register's full precision, so the quotient carries a
relative error of a few times 1e-8. The `fadds` correction that follows is
1e-6f, an absolute quantity, and truncation is toward zero — so the correction
covers the error up to K = 21 and stops covering it at 22. `realK` returns a
float and truncates nothing, so its own 1e-9f never matters (D346). Finding
F2194.

## D330 🐛 `determineDminForRrn` reads `prevNofUcodes` and `prevDmin` uninitialised when its rate-down search does not run once

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner19determineDminForRrnEj`
(blob 0x47cc0) at 0x4815b and 0x48280, against the stores at 0x48060 and
0x4807f which are inside the loop. **Reachability: `(unsigned char)pParams->m[phase] < maxM`
while `pParams->m[phase] > maxM` — that is, a constellation size above 255,
since the count is truncated to a byte (D332) and the outer test is not.**
**Observability: `rrnDownDmin` set from a stack slot the function never wrote.**
Status: unmeasured; nothing in the object calls this member (D345), so no
caller's range is known. The differential test EXCLUDES it — two sides reading
two different stack frames disagree for a reason that is not the
reconstruction. Fix class: none proposed; reproduced as found.*

The loop is entered only when the byte-wide `nofUcodes` is at least `maxM`, and
its body is what writes `prevNofUcodes` and `prevDmin`; the three arms after it
read both unconditionally. Finding F2160.

## D331 🐛 `determineDminForRrn` divides one by a constellation count that nothing stops being zero

**THE HANG CLAIM IS REFUTED, per `docs/deviation-triage.md` family 8, and
should be struck.** Both truncations are 64-bit `fistpll` (0x47e13, 0x47e4e)
whose readers take the LOW dword, which for the x87 integer indefinite is
ZERO -- and both doubling loops are guarded by `test %eax,%eax; je` at
0x47e28/0x47e2a and 0x47e65/0x47e67. **Neither loop runs.** The divide is real
and gives +infinity (x87 float, not a trap), which propagates as a nonsense
`dmin`; there is no 2^31 iteration and no hang.

*Batch of 2026-08-16, same function, at 0x47d54..0x47d5a. **Reachability: no
`pParams->m[k]` with a zero byte at `constelTable + 0x280c + k`.**
**Observability: `1.0f / 0` is an infinity, every scaled size is a NaN, the
product is a NaN, and the two truncations that follow yield 0x80000000 — after
which each of the two doubling loops runs about 2^31 times.** Status:
unmeasured; the search's guard byte is written by nothing reconstructed. The
test plants a zero in one of the six every trial. Fix class: none proposed.*

The same shape as D343 and reached twice per call, because the 2^((kTarget -
log2 m)/6) computation is spelled out in each half rather than called. Finding
F2160.

## D332 ⚠ `determineDminForRrn` truncates a 32-bit constellation size to a byte and then compares it against the untruncated one

*Batch of 2026-08-16, same function, at 0x47ffd (`mov %al,%bl`) and 0x48600,
against the 32-bit `cmp` at 0x47ff3 and 0x485ef. **Reachability: any
`pParams->m[phase]` above 255.** **Observability: the search loop's starting
count is the low byte, so a size of 256 starts at 0.** Status: unmeasured.
Fix class: none proposed; reproduced as found, and it is what makes D330
reachable.*

`nofUcodes` is a byte because `constelBuild` returns one, and the object seeds
it with a straight `mov %al,%bl` off a `V90MappingParams::constellationSize`
that is four bytes wide everywhere else. Finding F2160.

## D333 🐛 `setConstellationToNoise` stages accepted indices into a 128-byte frame local with nothing bounding the count

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner23setConstellationToNoiseEfPA128_sS1_PsPhPA128_h`
(blob 0x48b70) at 0x4907d and 0x493f8 (`mov %cl,0xc0(%esp,%edx,1)`), against
the frame at 0x48b80 (`sub $0x14c,%esp`) which leaves 0x8c bytes above 0xc0.
**Reachability: `arg5[k] - params->unnamed_360` of 128 or more with most
entries accepted.** **Observability: the store walks past the local into the
rest of the frame and then past the frame into the caller's.** Status:
unmeasured; nothing in the object calls this member (D345), so no caller's
range is known. Fix class: none proposed; reproduced as found.*

The loop runs `for (i = params->unnamed_360; i <= arg5[k]; i++)` with `arg5`
an `unsigned char *`, so the span can be 256 while the staging array holds
128. `t_v90cdnoise.cpp` caps every trial's span at 120: two sides smashing two
different frames disagree for a reason that is not the reconstruction, and a
crash is not a comparison.

## D334 ⚠ `setConstellationToNoise` reduces a 32-bit constellation size to sixteen bits for the report's maximum

*Batch of 2026-08-16, same function, at 0x492d0 (`movzwl 0x604(%edi),%esi`)
and 0x492eb (`movzwl %ax,%esi`), against the 32-bit unsigned `cmp` at 0x492e7.
**Reachability: a `constellationSize[k]` of 0x10000 or more.**
**Observability: the report's `maxM` comes out as the low sixteen bits, so a
size of 65536 makes it zero and no ucode line is printed at all.** Status:
verified UNREACHABLE FROM THIS MEMBER -- it writes every `constellationSize[k]`
itself, from a count the staging array bounds at 128 (D333), a few
instructions earlier. Fix class: none proposed; reproduced as found.*

Recorded because the truncation is real and another writer of
`V90MappingParams::constellationSize` could reach it; this member cannot, so
the test asserts it rather than driving it.

## D335 🐛 `setConstellationToNoise` prints a zero threshold with a minus sign

*Batch of 2026-08-16, same function, at 0x48c79, 0x48e18, 0x48ec8 and 0x48f7b
(`fldz` then `fcomps` then `sbb %ecx,%ecx; and $0xfffffffe,%ecx; add
$0x2d,%ecx`). **Reachability: a `noiseEnergy` or a `pdSnrThresh*` of exactly
zero.** **Observability: the diagnostic reads "= -0.00".** Status: verified
over the sweep in `t_v90cdnoise.cpp`, which drives a `noiseEnergy` of exactly
0.0f and both signs of all three thresholds. Fix class: none proposed;
reproduced as found, and it is a diagnostic only.*

The carry after `fldz; fcomps v` is set when `0.0 < v`, so the sign character
is '+' only for a strictly positive value and '-' for zero as well as for
negative. Four sites, one shape. Finding F2175 records why the codegen tier
cannot reproduce the branchless select at either setting of `-mieee-fp`.

## D336 🐛 `setConstellationToNoise_forceRate` doubles about 2^32 times when its bit count goes negative

*Batch of 2026-08-16, from `_ZN24V90ConstellationDesigner33setConstellationToNoise_forceRateEfPA128_sS1_PsPhS3_PA128_h`
(blob 0x499b0) at 0x49a47..0x49a50. **Reachability: `(short)(RATE_FORCE *
0.00075 + 0.5) + mappingParams->shaperSR - 6` below zero, which needs only a
small forced rate and a `shaperSR` under 6.** **Observability: a hang of
minutes, then a nonsense target.** Status: unmeasured; nothing in the object
calls this member (D345). Fix class: none proposed; reproduced as found.*

`cmp $0x1,%eax; je` is an EQUALITY test, not `jle`, so the guard that skips the
loop only catches `n == 1`; the `dec/dec/jne` that follows counts a negative
`n` down through zero and round the whole 32-bit range. The same shape as
D343. `t_v90cdnoise.cpp` solves for the `n` it wants and never lets it go
below zero.

## D337 🐛 `setConstellationToNoise_forceRate`'s refinement loop has no bound

*Batch of 2026-08-16, same function, at 0x49b95..0x49bfc. **Reachability: a
bit count large enough that `2^n` overflows to an infinity, or merely large --
`n` of 60 needs about 10^18 increments.** **Observability: the function never
returns.** Status: unmeasured. Fix class: none proposed.*

The product of the six counts is walked up to `2^n` ONE INCREMENT AT A TIME,
and nothing caps the number of turns. A second reachability: if any count is
zero the product is zero for ever, which happens when `size` comes out below 4
and a `dmin` phase takes `size * 0.25f`. The test holds `n + 2a + b` in
[12, 45], which is what keeps `size` at 4 or more, and never lets a phase carry
both flags.

## D338 🐛 `setConstellationToNoise_forceRate`'s extend loop never terminates when no reachable ucode exists

*Batch of 2026-08-16, same function, at 0x4a5ab..0x4a5b4. **Reachability: no
`j` in [`constellation[k][0]`+1, 0x74] with `ucode[k][c0] + phaseDmin[k] <
ucode[k][j]`, while the phase still wants more ucodes.** **Observability: the
function never returns.** Status: unmeasured. Fix class: none proposed.*

The search falls out of its `for` and straight back into the `while` test with
nothing changed -- `constellation[k][0]` is only rewritten on the insert path.
The give-up at `constellation[k][0] > 0x73` does not cover it, because an
insert sets that byte to zero (D339) rather than advancing it. The test uses
rising ramps whose slope is large against the largest `phaseDmin` the sweep
produces.

## D339 🐛 `setConstellationToNoise_forceRate` inserts zero where the ucode it just found belongs

*Batch of 2026-08-16, same function, at 0x4a8ae (`mov %cl,0x4(%ebp,%esi,1)`)
against the search at 0x4a580..0x4a59f whose result is in %edx and the shift
loop at 0x4a87a..0x4a8a9 which overwrites %edx and leaves %ecx at zero.
**Reachability: any phase that reaches the extend arm.** **Observability:
`constellation[k][0]` becomes 0 rather than the index the search found, and
0x4a8ca then re-reads that byte from memory so `codecConstellation[k][0]`
encodes `ucode[k][0]` too.** Status: verified over the sweep in
`t_v90cdnoise.cpp`, which asserts the zero on every trial where the arm ran.
Fix class: none proposed; reproduced as found, and `src/` carries a comment
saying not to "fix" it.*

The store takes the shift loop's exhausted counter, which is zero on every
path into it including the `count == 0` one that jumps straight there. Finding
F2182.

## D340 🐛 `setConstellationToNoise_forceRate` compares its walk's lower bound UNSIGNED

*Batch of 2026-08-16, same function, at 0x49fdc and 0x4a2d1
(`cmp %ebx,0x360(%ecx); ja`). **Reachability: `params->unnamed_360` of 0, or a
`topUcode[k]` of 0.** **Observability: `i` reaches -1, the unsigned compare
never stops the walk, and the row is read backwards off its front.** Status:
verified -- this one was found by the test failing rather than by reading, on
trials where the fixture set the start to zero. Fix class: none proposed.*

`i` is `topUcode[k] - 1` and the bound is an `int` from the parameter block,
and the comparison is unsigned, so a start of zero can never be greater than
`i` and the loop's only exit is the `ucode < phaseDmin * 0.5f` break. The test
keeps both the start and the top at 1 or more: two sides reading two different
heaps disagree for a reason that is not the reconstruction.

## D341 ⚠ `setConstellationToNoise_forceRate` leaves a one-ucode phase's constellation unwritten

*Batch of 2026-08-16, same function, at 0x49e93 (`cmp $0x1,%di; je`).
**Reachability: `nofUcodeInPhase[k] == 1`, which needs `n + 2a + b` to be
exactly 12.** **Observability: `constellationSize[k]` is set to 1 and
`constellation[k][0]` keeps whatever the caller left there, which the report
then prints and indexes the ucode table with.** Status: verified over the
sweep. Fix class: none proposed; reproduced as found.*

The whole build loop is skipped, count stays at its initial 1, and the trim
and extend arms both see `nof == count` and do nothing. Finding F2186 records
why reaching this at all took the sweep to be re-parameterised.

## D349 🐛 `calcModulusParameters` shifts a 64-bit one by an unbounded count

*Batch of 2026-08-16, from `_ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams`
(blob 0x3dd80) at 0x3dd96..0x3ddb0. **Reachability: any
`mappingParams->shaperSR + mappingParams->word_0` outside [6, 69] -- the count
is that sum less six, formed with no test of any kind.**
**Observability: the i386 sequence is `shld %cl,%ebx,%esi ; shl %cl,%ebx ;
test $0x20,%cl`, which masks the count to six bits, so a count of 64 produces
1 rather than 0 and a count of 70 produces 64; and above 62 the one lands in
or past the sign bit of a SIGNED `long long`, after which every `__divdi3`
below it divides a negative.** Status: unmeasured -- nothing reconstructed
calls this member, so no caller's range is known.  The differential test holds
the count in 0..62; a shift outside that is undefined in the source language
and the two compilers are entitled to differ for reasons that are not the
reconstruction's.  Fix class: none proposed; reproduced as found.*

`V90MappingParams::shaperSR` is written by
`V90ConstellationDesigner::spectralDesign` out of the parameter block and is
`int`; `word_0` is unsigned.  Nothing between the two writes and this read
bounds either.  Finding F3050.

## D350 ⚠ `getPowerIndexForPower` walks its whole ladder for a NaN

*Batch of 2026-08-16, from `_ZN21V90ConstellationPower21getPowerIndexForPowerEf`
(blob 0x3e320) at 0x3e338 and 0x3e352 (`fcomp %st(1) ; fnstsw %ax ; sahf`).
**Reachability: a NaN argument.** **Observability: an unordered compare leaves
C0 set, so `jae` is not taken and `setb` yields 1 at every step -- the walk
runs all 34 turns and returns 0, the same answer an enormous power gives.**
Status: unmeasured; the compare is a bare ordered `fcom` with no parity test,
which is what `-mno-ieee-fp` emits for every comparison in the object (finding
F1990), so a NaN case would be measuring the two builds' float-compare flags
rather than the reconstruction.  The differential test drives both infinities
and stops there.  Fix class: none proposed; reproduced as found.*

Recorded rather than driven because the return is a saturating index and not a
failure code -- 0 is a legal answer, so nothing downstream can tell the NaN
apart.  Finding F3051.

## D351 🐛 `adjustConstellationsToNewK` writes one past a constellation row when it has exactly 128 points

`V90ConstellationDesigner::adjustConstellationsToNewK` refuses to add a point
when the row would exceed 128 -- `cmp $0x80,%eax; ja` at 0x4c726, so 128 is
ACCEPTED -- and the shift that follows then runs

```c
for (j = mappingParams->constellationSize[minIndex]; j != 0; j--) {
	mappingParams->codecConstellation[minIndex][j] = ...[j - 1];
	mappingParams->constellation[minIndex][j] = ...[j - 1];
}
```

with `j` reaching 128, which is one past the row.  `V90MappingParams` tiles
exactly, so the write is not off the end of the object and lands on a
NEIGHBOUR:

    constellation[k][128]        is  constellation[k + 1][0]        (k < 5)
    constellation[5][128]        is  codecConstellation[0][0]
    codecConstellation[k][128]   is  codecConstellation[k + 1][0]   (k < 5)
    codecConstellation[5][128]   is  the low byte of constellationSize[0]

**Reachability: a row that reaches 128 points, which the add pass produces
whenever the constellations start large enough that the product cannot cross
the next power of two before one row fills.** **Observability: the next
member to read that neighbour reads a value the design never chose, and the
`constellationSize[0]` case turns a length into an arbitrary byte.** Status:
measured -- reproduced under the fixture of `test/unit/t_v90cdadjust.cpp` with
constellation lengths above 60, and read off the core dump the follow-on
non-termination (D352) produced.  Fix class: bound the shift at 127, or
refuse at 128 rather than above it.  Reproduced as found; the differential
test keeps every length at 45 or below so that the pass cannot reach 128, and
the test's header says so.

## D352 🐛 `reconstructInitialConditions` decrements a length more times than the row has points, and the next round never terminates

D342 records that the search in
`V90ConstellationDesigner::reconstructInitialConditions` has no bound.  This
is what happens when it overshoots.  The member is

```c
while (p->constellation[k][drop] != target)
	drop++;
while (drop != 0) {
	unsigned int n = p->constellationSize[k];
	unsigned char i;
	for (i = 0; i < n; i++) { ... }
	p->constellationSize[k]--;
	drop--;
}
```

so `drop` larger than `constellationSize[k]` decrements an UNSIGNED length
past zero.  The round after that has `n == 0xFFFFFFFF` and an `unsigned char`
index, `i < n` is true for every one of the 256 values `i` can take, and the
loop runs for ever.

**Reachability: any caller whose saved first byte is no longer within the
first `constellationSize[k]` entries of the row -- which D351 produces
directly, by overwriting `constellation[k + 1][0]` with a value from row
`k`.** **Observability: total, the modem stops.** Status: measured -- the
combination hangs, and `#0 reconstructInitialConditions ... n = 4294967295,
k = 5` is off the core.  Fix class: bound `drop` by the length, or widen `i`.
Reproduced as found.

## D353 ⚠ `process` shifts the digital rate mask by a negative count

`V90ConstellationDesigner::process` tests the provider's rate mask with

```c
if (((rateMask >> (mappingParams->word_0 - 21)) & 1) == 0)
```

-- `sub $0x15,%ecx; sar %cl,%eax` at 0x4ce54 -- and the test is made BEFORE
the `word_0 <= 20` check that would have ruled the value out.  A `word_0`
below 21 therefore shifts by a negative count, which is undefined in C; the
i386 masks the count to five bits and both sides do the same thing, so the
differential test cannot see it.

**Reachability: any design that lands under the minimum, which is the arm
whose own diagnostic is "Connection design ERROR, D choosen is smaller than
minimum".** **Observability: the banner is printed or not printed on the
strength of an arbitrary bit; nothing else depends on it.** Status:
unmeasured on the wire.  Fix class: test the floor first.  Reproduced as
found, and the differential test drives `word_0` below 21.

## D354 ⚠ `adjustConstellationsToNewK` leaves its removed-point count naming a point it has put back nowhere

The removal pass ends by restoring the last point it took out, and the whole
restore -- INCLUDING the `removed--` -- is skipped when the row is down to a
single point:

```c
if (mappingParams->constellationSize[maxIndex] != 1) {
	...restore...
	removed--;
}
edprintf("... nof points removed %d \r\n", removed);
```

`cmp $0x1,%eax; je` at 0x4c4ce jumps straight to the diagnostic.  So on that
path the count is one larger than the number of points actually gone, and the
constellation keeps a point the member's own accounting says it returned.

**Reachability: a removal pass that empties a row down to one point, which
needs a constellation the design has already cut hard.** **Observability: the
count is a diagnostic only; nothing reads it back.** Status: unmeasured.  Fix
class: decrement outside the guard, or guard only the shift.  Reproduced as
found.

## D355 🐛 `constellationDesign` and `process` drop an argument on the arm that does not force the rate

Both members pass seven-argument and six-argument forms of the same design
call, and on the six-argument arm the argument that goes into
`setConstellationToNoise`'s `unsigned char *` slot is the SIXTH they were
handed and not the fifth -- which is never passed at all.  In
`constellationDesign` the fifth is never stored to the outgoing frame (%ecx
holds it from 0x54(%esp) at 0x4cad3 and is written nowhere); in `process` the
ninth is not (0x84(%esp) is loaded only on the forced arm, at 0x4cf13).

**Reachability: `FORCE_RATE_ENABLE` clear, which is the ordinary case.**
**Observability: `setConstellationToNoise` reads that argument as the
per-phase top ucode and bounds its staging loop with it, so the wrong one
changes which points enter every constellation.** Status: unmeasured against
the standard -- what the two arrays hold in a live session is not known here.
Fix class: pass the fifth.  Reproduced as found; `test/unit/t_v90cdadjust.cpp`
gives the two arrays different per-phase spans so that the reading is
measured rather than assumed, and the mutation set carries the swap.

## D356 🐛 `constellationDesign` divides by zero when the design it just made leaves a phase empty

`setConstellationToNoise` sets `constellationSize[k]` to the number of ucodes
its staging loop accepted, and that number is ZERO whenever no index in
`[unnamed_360, lastUcode[k]]` clears the phase's threshold -- there is no
floor on it and D341 records the sibling case where a phase is left unwritten
altogether.  `constellationDesign` then calls `adjustConstellationsPower` if
`ENABLE_DIGITAL_POWER_REDUCTION` is set, whose first act is

    power->getPower(mappingParams, 1, word_2c)

and whose `calcModulusParameters` computes `remaining[i] %
constellationSize[i]` through `__moddi3`.  A zero divisor there is a divide
error and the process takes SIGFPE.  `process` reaches the same call the same
way.

**Reachability: any design that leaves one of the six phases with no points,
which needs only a threshold above every ucode in that phase's span.**
**Observability: total, and immediate -- the modem dies rather than
misbehaving.** Status: measured, in the sense that it was REPRODUCED: the
first composed sweep in `test/unit/t_v90cdadjust.cpp` took SIGFPE with
`constellationSize = {0, 8, 0, 18, 1, 22}`, and `coredumpctl debug` put the
top frame in `__moddi3` under `calcModulusParameters` under
`adjustConstellationsPower` under `constellationDesign`.  What is NOT measured
is whether a live session can produce an empty phase; nothing here bounds the
ucode tables a real detector fills.  Fix class: floor the length at one, or
skip the power pass on an empty phase.  Reproduced as found; the test's
composed groups use a fixture that cannot produce an empty phase and its
header says why.
## D370 🐛 `FSE_decision_128pt`'s outer ambiguous cell returns the FARTHER of its two candidates

*Batch of 2026-08-16, from `FSE_decision_128pt` (blob .text 0x804e0) at
0x806cf-0x806db: `xor %eax,%eax ; cmp %cx,%bx ; setge %al ; dec %eax ; and
$0xfffffff4,%eax ; add $0x1a,%eax`, where `%bx` holds the squared distance to
point 26 and `%cx` the one to point 14.  `setge` therefore selects point 26
exactly when 26 is the FARTHER of the two, and point 14 otherwise.*
**Reachability: any received symbol whose rotated coordinates satisfy
`ri > 14481` and `rq >= 14481` -- 603,901,812 of the 2^32 (I, Q) pairs, and
the cell is entered on ordinary 14400 bit/s traffic.**
**Observability: the two distances differ at 603,873,842 of those
603,901,812 points, and at every one of them the object returns the point a
nearest-neighbour decision would reject.  Witness I = -32767, Q = -12286.**
*Status: 🐛 defect in the original, reproduced as written.  Fix class: none
proposed -- the slicer's own decision feeds the equaliser and the carrier
loop, not the bit stream, so the cost is a worse error term in a cell that a
Viterbi decoder is about to overrule anyway.*

The second ambiguous cell, twenty-five instructions further on at 0x807d1, is
the same shape with `setle` and picks the NEARER of ITS two candidates.  So
this is not one tie-breaking convention applied twice: the two arms of the
same construct disagree about which direction the comparison runs, which is
what makes the first one a slip rather than a choice.  Finding F3218.

## D371 🐛 `FSE_decision_128pt` carries point 26's I coordinate as a literal one greater than the table's

*Batch of 2026-08-16, from `FSE_decision_128pt` at 0x806bc, `mov
$0x3e3a,%eax` -- 15930 -- used as the I coordinate of constellation point 26,
where `DECv32_ANA_IMAP128[26]` (.rodata 0x75c0 + 0x34) is 15929.*
**Reachability: the same cell as D370.**
**Observability: the one-LSB difference survives the `>> 13` and flips the
decision between points 14 and 26 at 48,490 of the cell's 603,901,812 points.
Witness I = -32767, Q = -3841.**
*Status: 🐛 defect in the original, reproduced as written -- `src/` carries
`0x3e3a` and not the table entry.  Fix class: none proposed.*

**THE OTHER THREE LITERALS ARE RIGHT, WHICH IS WHAT MAKES THIS ONE A FINDING
AND NOT A COMPILER ARTEFACT.**  The two tie-breaks name four I coordinates as
immediates -- 0x2799 twice, 0x32e9 and 0x3e3a -- and the first three equal
`DECv32_ANA_IMAP128[14]`, `[15]` and `[24]` exactly.  A constant fold out of
the const table would have produced 15929 here too; a hand-written constant
would not have to.  Finding F3218.

## D372 🐛 `FSE_decision_128pt`'s region tree puts the same rq boundary in two different places

**NOT A DEFECT, per `docs/deviation-triage.md` family 9.** The
inconsistency between the arms is real and the defect is not: **0x16a0 = 5792
is EXACTLY the midpoint of the Q rows 4344 and 7240, and 0x2d41 = 11585 is
EXACTLY the midpoint of 10137 and 13033**, so a symbol on the line is
genuinely equidistant and both cells offer the same two I values. Enumerated
over the 21,182 `ri` values at `rq = 5792, ri > 11585`: the index differs at
all 21,182, the Euclidean distance is EQUAL at 20,136, the object is strictly
NEARER at 1,046, and strictly farther at **zero**. The `_64pt` cut at 8192 is
likewise the exact midpoint of 4096 and 12288. Suggest 🐛 -> ⚠, and rewrite
"the two never agree on the point" -- true of the index, misleading about
distance.

*Batch of 2026-08-16, from `FSE_decision_128pt`'s region tree.  Two arms cut
the rotated Q axis one way -- 0x80750 and 0x80801 both encode `cmp $0x2d40,%bx
; jg` and 0x8075d and 0x80811 both encode `cmp $0x169f,%bx`, so the bands are
`rq >= 0x2d41` and `rq >= 0x16a0` -- and the third cuts it the other way, with
`cmp $0x2d41,%bx ; jle` at 0x80644 and `cmp $0x16a1,%bx ; setl` at 0x808b2, so
its bands are `rq <= 0x2d41` and `rq <= 0x16a0`.  The two nominal boundaries
are 5792 and 11585 and each is claimed by the band above it in the first two
arms and by the band below it in the third.*
**Reachability: 82,597 (I, Q) pairs rotate to `rq` exactly 5792 with
`ri > 11585`, which is the third arm.**
**Observability: at every one of those 82,597 the object searches the cell at
base 0x1c and a single uniform convention would search base 0x18, and the two
never agree on the point.  Witness I = -32768, Q = -24576.**
*Status: 🐛 inconsistency in the original, reproduced arm by arm.  Fix class:
none proposed.*

The same one-apart pattern is in `FSE_decision_64pt`, where it is between the
AXES rather than between arms: the I bands are `i > 0x2000` and `i <= 0xe000`
while the Q bands are `q > 0x1fff` and `q >= 0xe000`, so a symbol at exactly
(8192, 8192) is inner in I and outer in Q.  Recorded here rather than as a
fourth entry because it is one construct read twice, and because `_64pt` runs
on the symbol as received, where a test can put a value on the line directly.
Finding F3217.
## D380 ⚠ Object +0x08 has two of the author's own names, `ptc` and "Max Block Length"


**This was numbered D351 when it was written.** It was renumbered to D380 at merge time: `v90-designer-methods` claimed D351-D356 concurrently, both branches having surveyed when the maximum was D350. Nothing outside this file, `v34pcmif.c`, `v34pcmif.h` and its finding ever referred to it as D351.
*Batch of 2026-08-16, from `VPcmV34SetMaxBlockLength` (blob 0x6500) at 0x6512
and its string at `.rodata.str1.4+0x980`, against `initdigital`'s string for
the same offset. **Reachability: every caller of either function -- this is a
naming disagreement in the object, not a code path.** **Observability: none at
runtime. `struct v34_object` +0x08 is one field with one value however it is
spelled; what is observable is only that a reader of one string will not find
the other.** Status: unmeasured, and not measurable -- there is no behaviour
here to drive. Fix class: none proposed; the field keeps `ptc`.*

`initdigital` prints +0x08 as "PTC" in "for tx data rate - %d, PTC - %d,
setting nofTxBits to %d" and computes `nof_tx_bits` from it, so `ptc` has a
READER behind it. `VPcmV34SetMaxBlockLength` stores its argument there and
reports "VPcmV34Main: Max Block Length modified to %d", and has only a writer.
The tree's rule is that a printed label names a field; applied twice to one
offset it gives two answers, so the tie is broken on which name has a use
behind it rather than on which string was read most recently.

Recorded rather than silently resolved because a future reader who finds the
"Max Block Length" string and greps for a field of that name will conclude the
tree missed it.  Finding F3304.
## D360 🐛 `V22_FSE_init` zeroes the first 49 history entries twice

*Batch of 2026-08-16, from `V22_FSE_init` (blob 0x08cd00) at 0x8cdd9 and
0x8cdf0.  **Reachability: every call.**  **Observability: none -- both loops
write zero to the same 49 entries, and the second then carries on to 97.**
Status: verified bit-exact; the second loop's bound, 0x61, is what the test
asserts by leaving a marker above index 48 and requiring it to be cleared.
Fix class: none proposed; reproduced as found.*

The coefficient loop clears `hist[i]` for i in 0..48 as a side effect of
walking the 49 taps, and the loop after it clears `hist[i]` for i in 0..97.
The first clear is entirely redundant -- `hist` is 98 shorts and the second
loop covers all of it.  Recorded because the redundancy is the kind of thing a
reader corrects without noticing, and correcting it would be a source change
with no test able to see it.  Finding F3500.

## D361 ⚠ `FSEv22_decision24` falls back on index 0, which is outside its own search window

*Batch of 2026-08-16, from `FSEv22_decision24` (blob 0x0884a0) at 0x88541 and
0x88545 (`mov %edx,0x8(%esp)` with `%edx` zero, then `shl $0xc,%ecx`).
**Reachability: a symbol more than 8192 from both of the two candidates the
sign and amplitude tests selected -- roughly, any point whose Q coordinate is
further than one constellation spacing outside the outer ring.**
**Observability: the returned symbol, the reported angle and the reported ring
are all those of constellation index 0 rather than of the nearer candidate, so
a badly off point in any quadrant decodes as if it were in the third.**
Status: unmeasured against a real receiver -- nothing reconstructed drives this
slicer from live samples yet.  The differential test drives it over the whole
16-bit range on both axes and counts the trials that take this path, requiring
that count to be non-zero.  Fix class: none proposed; reproduced as found.*

The initial best distance is `thresh[0] << 12`, the same 8192 the amplitude
test uses, rather than 0x7fff.  `FSEv22_decision12` uses 0x7fff and has no
equivalent.  Finding F3503.

## D362 ⚠ `FSEv22_decision12` accumulates its squared distance in sixteen bits

*Batch of 2026-08-16, from `FSEv22_decision12` (blob 0x088680) at 0x886de
onwards (`imul %edx,%edx ; imul %eax,%eax ; sar $0x10 ; sar $0x10 ; add ;
movswl %dx,%eax`).  **Reachability: any point far enough from a candidate that
the axis error exceeds about 23,170, since the two shifted squares then sum
past 32767.**  **Observability: the sum wraps negative and that candidate wins,
so the point decodes as the one it is FURTHEST from.**  Status: unmeasured
against a real receiver; the differential test counts the trials on which a
full-precision search would choose differently and requires that count to be
non-zero.  Fix class: none proposed; reproduced as found.*

Each axis error is truncated to a short before squaring, the 32-bit square is
shifted down sixteen, and only then are the two added -- and the sum is
truncated to a short again before the comparison.  Finding F3501.

## D363 🐛 `Detect_v22` passes `FPM_AGC_agc` a fourth argument it does not have

**NOT A DEFECT, per `docs/deviation-triage.md` family 9.** `FPM_AGC_agc`
(0xa6750) has exactly one return, `0xa6894: c3 ret` -- **a bare `ret`, not
`ret $imm`** -- so the call is cdecl and the caller cleans up. The extra push
is popped by the caller's own stack adjustment and the callee never reads the
slot. The observation is real evidence that the author's DECLARATION of
`FPM_AGC_agc` had four parameters, which is worth recording; it is not a defect
in behaviour. Suggest 🐛 -> ⚠, alongside D366, which is the same shape at four
sites and is already ⚠.

*Batch of 2026-08-16, from `Detect_v22` (blob 0x08c1c0).  **Reachability: every
call.**  **Observability: none -- the call is cdecl, the caller cleans up, and
the callee never reads the slot.**  Status: verified bit-exact; reproduced as a
three-argument call, exactly as `src/pump/v23/bwchdem.c` already does at the
same callee.  Fix class: none proposed.*

The object pushes a constant 1 as a fourth argument.  `FPM_AGC_agc` takes
three.  Unlike `bwchdem.c`'s site this caller also discards the return value,
so no `agc.signal` read-back is needed to stay faithful.  Finding F3507 for what
this function does.

## D364 ✅ `ModDataV22` narrows `V22_PPS_filter`'s `short` to `unsigned short`

*Batch of 2026-08-16, from `ModDataV22` (blob 0x08e310) at 0x8e369
(`movzwl %ax,%eax` immediately before the return).  **Reachability: every call
whose pulse shaper returns a negative count, which nothing reconstructed
produces.**  **Observability: the sign.**  Status: verified bit-exact over the
domain the differential test drives, which includes a sample count with bit 15
set.  Fix class: none proposed; the truncation is the CALLER's and lives in
`v22data.c`, and `v22_pps.h`'s `short` return is unchanged.*

Recorded so that the disagreement between the two declarations reads as
deliberate rather than as one of them being wrong.

## D365 ✅ `V22FP_TX_CLOCK` and `V22FP_PPS` are two names for one address

*Batch of 2026-08-16.  **Reachability: not a behavioural difference at all.**
**Observability: none.**  Status: verified -- `fp + 0x78` is written by
`TxClockSync` under the first name and is the base `V22FP_create` hands
`V22_PPS_init` under the second.  Fix class: both names are kept and
cross-referenced until `V22FP_create` lands and the object gets a real type,
at which point both become one struct member.*

Recorded here rather than silently unified because a reader meeting the two
constants would otherwise have to rediscover that they collide.  Finding F3505.

## D366 ✅ `V22FP_delete` drops a second argument that no callee reads

*Batch of 2026-08-16, from `V22FP_delete` (blob 0x088330).  **Reachability:
every call.**  **Observability: none -- cdecl, the caller cleans up, and none
of the four callees touches anything but its first argument.**  Status:
verified bit-exact.  Fix class: none proposed; reproduced as four
one-argument calls.*

The object pushes a literal 1 as a second argument to `V22_PPS_free`,
`V22_MRF_free`, `V22_SRE_free` and `V22_FSE_free`, four times with a fresh
`mov $0x1` each -- so the author declared all four with two parameters.  This
tree reconstructed all four from their own bodies, where the second parameter
is dead, and one of the four headers belongs to another effort.  The same
shape as D363, at four sites instead of one.

## D390 ⚠ Two of `V90CP`'s counts travel wider than the arrays they index

**This was written five numbers lower**, and moved up before it left its
branch: a sibling claimed the number immediately above D380 while this batch
was running, and five free numbers is not a gap when six branches are open at
once.  Nothing outside this file, `include/dsplib/V90CP.h` and
`test/unit/t_v90cpinfo.cpp` ever referred to it by the old number, and it is
spelled out here rather than cited so that the survey's own tool does not read
a retired number as a live reference.

*Batch of 2026-08-16, from `V90CP::infoToBits` (blob 0x52230) and
`V90CP::evaluateInfo` (0x519f0). **Reachability: any peer that sends a large
count, and any local caller that sets one.** **Observability: a read or a
write past the end of the array, identical on both sides -- so it is not a
DIFFERENCE and no differential test can fail on it.** Status: unmeasured. Fix
class: none proposed; reproduced exactly, and `t_v90cpinfo` bounds its own
seeds instead.*

`nof_58[k]` is carried in nine bits, so up to 511, and `short_58[k]` holds
384.  `nof_buf[k]` is carried in eight bits, so up to 255, and `buf[k]` is a
0x200-byte allocation holding 128 four-byte entries.  Both loops run to the
count with no clamp, in both directions: `infoToBits` READS past the end and
`evaluateInfo` WRITES past it.

**THE ATTRIBUTION ABOVE WAS WRONG AND IS CORRECTED HERE; THE DEVIATION IS
NOT.**  This used to say that `bitsToInfo`'s
"*** error CP bit , not enouch memory in the buffer ***" was the guard that
caught the second one, one layer out.  `bitsToInfo` has now been read and it
is not: all five of that string's referrers guard `word_cac`, the index into
`bits`, against 0x2edf, and NOTHING anywhere guards either count against the
array it indexes.  See finding F4361, and D520 for the five arms that carry
that check and the five that do not.

Recorded rather than clamped because clamping would be a behaviour change that
no test could justify, and because a later reader who seeds a count of 384 into
all four lists will watch both sides walk off the end of a 12,000-byte bit
vector together and need to know that is the object and not the
reconstruction.
## D381 ⚠ Two special spectral conditions at once: the log says both, the field says the later one

*Batch of 2026-08-16, from `V90SpectralVerifier::checkSpecialSpectralConditions`
(blob 0x45f10).  **Reachability: any line that trips more than one of the
three conditions.**  **Observability: yes -- the diagnostic stream and
`+0x28` disagree.**  Status: verified bit-exact; reproduced deliberately and
driven by the `both_isdn_and_pbx` and `all_three` cases in
`test/unit/t_v90specialcond.cpp`.  Fix class: none proposed -- an `else`
chain would change which condition is reported.*

The three tests are sequential `if`s.  `movl $0x2,0x28(%edi)` at 0x46588 and
`movl $0x3,0x28(%edi)` at 0x46573 store without testing what is already in
+0x28, so a line that trips both the German ISDN NT1 box test and the German
PBX test prints

    V90SpectralVerifier: German ISDN NT1 box conditions detected!
    V90SpectralVerifier: German PBX conditions detected!

and leaves 2 in +0x28.  `V90Equalizer` reads that field at three sites and
compares it against 2, so the ISDN detection is silently discarded by the one
consumer this tree has written.  Whether the three conditions are meant to be
mutually exclusive in practice is not something the object states; what it
states is that nothing enforces it.  The `three tests are an else chain`
mutation in `test/mutations/v90specialcond.json` is what holds this reading.
## D400 ⚠ `FPM_SRE_init`'s reuse test guards THREE buffers with the size of a fourth

*Renumbered at commit time from the number this batch first gave it, which
three live branches had each taken independently while the work was in
progress; the block below four hundred was exhausted by `master` and by the
sibling agent writing `FPM_FSE_receive`. Re-surveying at COMMIT time rather
than at claim time is what docs/plan.md asks for, and this is why. The old
number appears in one commit message on `fpm-shared-dsp`, which cannot be
rewritten.*

*Batch of 2026-08-16, from `FPM_SRE_init` (blob 0x0aa7c0).  **Reachability: a
re-init (`fresh` zero) whose configuration raises `rms_len` without raising
`coeffs`.  No such call is reconstructed, so unmeasured.**  **Observability: a
heap overrun of `2 * (new rms_len - old rms_len)` bytes in init's own clear
loop, which the differential tier cannot see because both sides overrun
identically.**  Status: unmeasured.  Fix class: none proposed; reproduced.*

The reuse path is

    if (!fresh && sre->cfg.coeffs < cfg->coeffs) { free x4; fresh = 1; }

so the decision to keep the four existing buffers is made on `coeffs` alone.
Three of the four are sized on `coeffs` or on `taps`, which is `coeffs / 10`,
and that is sound. **`rms_buf` is sized on `cfg.rms_len`, which the test does
not look at.** A re-init that raises `rms_len` alone therefore keeps a buffer
that is now too small, and the clear loop immediately below runs to the NEW
`rms_len`.

`FPM_PPS_init` has the same shape and does NOT have the bug -- its test is
`state->taps < cfg.coeffs / cfg.phases`, which is the size of the buffers it
guards. So this is a slip in one of two sibling functions rather than a
convention.

**CONFIRMED AT THE BATCH OF 2026-08-17, WHICH WROTE `FPM_PPS_init`, AND THE**
**VERDICT IS UNCHANGED.** The claim above was a reading made while writing
`FPM_PPS_filter`, and this entry is amended rather than rewritten because
nothing in it turned out to be wrong. Two things are now firmer than they were.

*The disassembly settles it outright.* `cltd; idiv %esi` at 0x0a9928 leaves the
quotient in `si`; `cmp %si,0x2e(%ebx)` at 0x0a9932 tests `state->taps` against
that register, and `lea (%esi,%esi,1)` at 0x0a9994 and `add %esi,%esi` at
0x0a99a6 size both buffers from the same one. The quantity tested and the
quantity allocated are one value, and this block has two buffers where SRE has
four -- so there is no third size for the test to miss.

*And it is measured rather than read.* `t_fpm_pps.c`'s reuse block now drives a
re-init that raises `cfg.coeffs` from 120 to 125 with `cfg.phases` at ten: the
quotient stays at twelve, so the buffers must be KEPT, and the allocator must
record zero frees and zero allocations. An SRE-shaped guard reallocates on that
input, and the mutation that rewrites `FPM_PPS_init`'s test into that shape is
in `test/mutations/fpmpps.json` and is caught by that pass alone. So the
sentence "this is a slip rather than a convention" is now something a test
fails on. Finding F3661.

The SRE half of this entry is UNCHANGED and still unmeasured: `FPM_SRE_free`
was written in the same batch, which adds a release path but no caller, and
nothing in it raises `rms_len`.

Unmeasured because no caller of `FPM_SRE_init` is reconstructed: the built-in
`FPM_SRE_CFG` is a template with null table pointers and whoever patches and
passes it is not written yet. Whether any real configuration ever raises
`rms_len` on a re-init is exactly what that caller would settle.
## D391 ✅ `FPM_FSE_receive` saturates a NEGATIVE smoothed error to 0x7fff

*Batch of 2026-08-16, from `FPM_FSE_receive` (blob 0x0a7e00) at 0x0a83f8
(`cmp $0x7fff,%eax` with `jbe`).  **Reachability: any symbol whose decision
error is large enough that the sum of its two squares, shifted down eleven and
cast to `short`, comes out negative -- which the differential suite reaches on
64 of 64 swept magnitudes above about 12000.**  **Observability: `state->mse`,
which is a compared field, and through it the LMS gate.**  Status: verified
bit-exact; the reconstruction reproduces the unsigned test.  Fix class: none
proposed.*

The smoothed error is formed as a signed `int` and tested against 0x7fff as an
UNSIGNED one, so the clamp catches both ends of the range and sends both to the
maximum.  A modem whose equaliser has just diverged therefore records the
largest possible error rather than a negative one, which is arguably what was
wanted; but the same test also means `state->mse` is non-negative for ever, and
the `mse > 0` gate below it can only fail on an exact zero.  Reproduced rather
than corrected, and the consequence for the LMS gate is finding F3586.

## D392 ✅ `FPM_phasor` reads its quadrant sign tables four entries early for a phase of 0x8000 or more

**CLOSED, ON BOTH HALVES.  Sine and cosine are verified bit-exact against the
blob over all 65536 phases against fourteen increments, under GCC 3.4.2 and
GCC 13, in `t_fpm_phasor` and `t_fpm_phasor_demod`'s sweeps; `t_fpm_fse_recv`
compares `out_i` AND `out_q` at all 65536 positions of each derotation sweep,
16384 of them out of the designed domain.**

*Batch of 2026-08-16, from `FPM_phasor` (blob 0x0a9300) at 0x0a9367 and
0x0a9392 (`movswl 0x0(%esi,%esi,1)` against `FPM_cos_sign` and `FPM_sin_sign`,
with no mask on the quadrant).  **Reachability: any caller that sets `phase`
to 0x8000 or above.  `FPM_FSE_receive`'s derotation does, for a quarter of its
angle range, because its reduction is a pair of tests and not a loop;
`FPM_phasor_dp` (0x0a93e0, unreconstructed) is a third user of the same two
tables.**  **Observability: both outputs, and every output a caller derives
from them.**  Status as of 2026-08-17: VERIFIED, both halves, both compilers.
Fix class: reproduced as found -- the unmasked index is kept, and what it
reads is now a CONSTANT IN OUR SOURCE rather than whatever our link puts
below each table.*

The object indexes its two quadrant sign tables with an unmasked quadrant, so
a phase read as a negative short reads four entries before each table.  Our
reconstruction always reproduced the unmasked index; the question was always
what it would find there.

**THE ANSWER IS NOW A VALUE, AND THAT IS THE WHOLE ENTRY.**  The eight
out-of-range words are constants, measured from the blob:

    COEF_DC       0x0081d0 (10 B)  [-12971, 12917, 28620, -25834, 12917]
    pad           0x0081da ( 2 B)  [0]
    FPM_sin_sign  0x0081dc ( 8 B)  [16384, 16384, -16384, -16384]
    FPM_cos_sign  0x0081e4 ( 8 B)  [16384, -16384, -16384, 16384]

    FPM_sin_sign[-4 .. -1] = [28620, -25834, 12917, 0]
    FPM_cos_sign[-4 .. -1] = [16384, 16384, -16384, -16384]

`src/dsp/fpm_phasor.c` carries them as four LEADING entries of
`FPM_cos_sign_ext` and `FPM_sin_sign_ext`, and the phasor indexes
`ext[FPM_PHASOR_SIGN_BELOW + quad]`.  `quad` is exactly -4 .. 3 -- phase in
-32768 .. 32767, `idx = phase >> 5` in -1024 .. 1023, `quad = idx >> 8` -- so
every index the function can form is 0 .. 7 inside ONE array object.  Defined
C, no layout assumption, and the same value for every one of the 65536 phases.
`FPM_sin_sign` and `FPM_cos_sign` keep the object's names, the object's four
words each and the object's `.data` binding, and nothing indexes them.

**THIS IS D4'S FIX, RUN BACKWARDS.**  `FPM_div` indexes a 128-entry table with
0 .. 128 and `src/dsp/fpm_div.c` reproduces the overrun by giving OUR table a
129th entry holding the neighbour's first word: the adjacency became a value
and the layout dependence vanished.  Same move, other end of the array.

**WHAT IT REPLACED, AND WHY THAT HAD TO GO.**  The previous batch closed the
cosine by ARRANGING MEMORY -- `FPM_sin_sign` and `FPM_cos_sign` made `.data`
globals and declared cosine first, so that both compilers' reverse `.data`
emission order put them adjacent in the object's order, with `COEF_DC` moved
into `fpm_mtd.c` so its tail landed before `FPM_sin_sign`.  Every step of that
is right about the OBJECT.  None of it makes reading before an array anything
other than undefined behaviour in OURS, and the owner ruled it out: an
implementation must not depend on the ordering of objects in memory.  Note
that this applied to the COSINE half too, which the entry then called closed:
same-translation-unit declaration order is a convention of the two compilers
we use, not a guarantee.

**WHAT THE CLOSE IS WORTH, AND WHERE IT IS NOT.**  Two bounds, so the sweep is
not read as larger than it is:

- **quadrant -1 (phase 0xE000 .. 0xFFFF) separates NOTHING on the sine.**  The
  object's word there is the two bytes of `.data` padding at 0x081da and is
  zero, so both sides return zero for all 8192 of those phases and no input
  can tell "the word is right" from "the word is zero".  It agreed before any
  of this work for an unrelated reason -- `fpm_cos_table[256]` was there under
  the old `static const` layout and is also zero (finding F3623) -- and that
  was two layouts coinciding rather than coverage.  `t_fpm_phasor` counts the
  separating trials per quadrant, prints them, and asserts that one at 0 with
  the derivation.  What DOES check that word is the neighbourhood block, which
  compares it against `dsplibs_ref.o`'s own byte, and the `fpmphasor`
  mutation that makes it non-zero, which both tiers catch;
- **`FPM_phasor_dp` at 0x0a93e0 is still not reconstructed.**  It references
  both tables (0x0a944b, 0x0a9472) and must index the EXTENDED arrays with the
  same bias when it is written, not the four-entry exported ones.

**THE `--coverage` OBJECTION IS GONE.**  Finding F3624 recorded that
`--coverage` appends `__gcov_.FPM_MTD_create/delete/detect` to `fpm_mtd.c`'s
`.data` at offsets 0x0c, 0x24 and 0x3c, immediately after `COEF_DC` at 0, so
in the instrumented tree `tools/debugcov.py` builds, the sine's window read
gcov metadata -- which is why the adjacency could not be asserted.  With no
adjacency to assert, the objection does not apply.  The general rule 3624
states is unchanged and is worth keeping: **a claim about what lies before or
after a symbol is assertable WITHIN a translation unit and is not assertable
ACROSS one.**  The right response to it is to stop making the claim.

**`COEF_DC` STAYS IN `fpm_mtd.c`.**  Its attribution rests on evidence that is
independent of the phasor entirely: its one reference in 1.2 MB is at .text
0x0a921a inside `FPM_MTD_detect`, `nm` marks it `D` and not `R`, and the
two-byte pad at 0x081da is itself proof of a translation-unit boundary
(findings F3620-3622).  Only the DEPENDENCE was removed, not the attribution.

**The mutation sets.**  `test/mutations/fpmphasor.json` is 7 of 7 caught: the
quadrant masked, the phase read unsigned in each of the two functions, a wrong
cosine window, a wrong sine window, a non-zero pad, and a drift between the
exported `FPM_cos_sign` and the extended copy of it -- that last one catchable
only by the neighbourhood block, which is what ties the two copies together.
`test/mutations/fpmmtdlayout.json` used to be the register of the open half at
0 of 2 caught, deliberately; it is REPURPOSED rather than retired, with the
same source, the same binary and the same two mutations, both now
`equivalent`.  Their SURVIVAL is the positive statement of the fix, and
`mutate.py` fails the run if either is ever caught -- which would mean the
phasor had re-acquired a dependence on another translation unit's layout.
`test/mutations/fpmmtd.json` is new and keeps `COEF_DC`'s VALUE adjudicated
where it belongs, against `t_fpm_mtd`.

Findings F3588 for the original derivation, 3620-3622 for the translation-unit
attribution and the emission order, 3623 for what each compiler showed before
any fix, 3624 for the instrumented build, and 3700-3703 for this one.

## D393 🐛 `FPM_iir_filt_block` writes back a register it never assigned when `sections` is zero

*Batch of 2026-08-26, from `FPM_iir_filt_block` (blob 0x0a8b10, 287 B).*

The function is `FPM_iir_filt` over a block: GCC inlines the call at `-O3`
within the translation unit, so the object holds the single-sample engine's
body verbatim inside a two-deep loop nest and shows no `call`.

The inner loop is guarded. `sections - 1` is hoisted out of the outer loop into
`(%esp)` at 0x0a8b4a, and each outer iteration tests it:

    a8b66:  cmpw   $0xffff,(%esp)          ; (short)(sections-1) == -1 ?
    a8c02:  jne    a8b74                   ; enter the inner loop only if not

The write-back at the bottom of the outer loop is:

    a8c19:  mov    %dx,-0x2(%edi)          ; samples[i] = %dx

`%edx` is assigned **only** at 0x0a8bf5, inside the inner loop. Nothing in the
prologue or the outer loop writes it, so on the `sections == 0` path the store
commits whatever the caller happened to leave in `%edx`. The correct value
there is the untouched input sample, which the object still holds in `%ebx`.

**Reachability: none in the object.** `service.py` puts the symbol in the "no
entry point reaches it" class, and no relocation anywhere in `dsplibs.o` names
it — it is a `T` symbol with no caller. A `sections` of zero is therefore not
reachable *a fortiori*, and would be an odd thing to ask a cascade for in any
case.

**Observability: the whole block.** Every sample would be overwritten with the
same stale register, since `%edx` is not touched between outer iterations
either.

**Not reproduced, and not behind `DSPLIB_REPRODUCE_BUGS`** — there is nothing
to reproduce. The object's value on that path is a property of its caller's
register state, not of its own code, so there is no behaviour a differential
test could compare against. `src/dsp/fpm_iir.c` writes the sample back
unchanged, which is what the source plainly said before the compiler lost it.

**And `t_fpm_iir` deliberately does not drive `sections == 0`.** Comparing two
arbitrary choices would be a check that passes or fails on register weather.
The zero-`count` case IS driven, and the compared domain is `sections` 1..3
against the three real coefficient sets plus the unstable pair, at seven
fragment sizes.

Contrast `FPM_iir_filt_II` directly below it in the same TU, whose zero-section
case is well defined and IS tested (`iir_II zero sections`): its accumulator is
the sample itself and the compiler kept it in one register throughout.

Finding F8161.

## D394 ⚠ `FPM_TONE_find_rev`'s reversal history is a fixed 80 words while its index runs modulo `2 * cfg.f1e`

*Batch of 2026-08-26, from `FPM_TONE_find_rev` (blob 0x0ab170). **Unmeasured**.*

`rev_hist` occupies +0x52..+0xf0 — exactly 80 words, fixed by
`FPM_TONE_create`'s second clear loop (finding F8169) and by the same bounds in
the object. `rev_idx` advances modulo `2 * cfg.f1e`, so any `cfg.f1e` above 40
walks the write straight through `rev_idx` (+0xf2), `rev_block` (+0xf4),
`rev_acc` (+0xf8) and `iir_self` (+0xfc) — that is, through the biquad's
coefficient pointer and its state.

**Reachability: no configuration in the object sets `f1e` above 40.**
`FPM_TONE_CFG`'s value is 40 and `2 * 40 == 80` exactly fills the region.

**Reproduced, and not a defect in the reconstruction**: the object reserves the
same fixed 80 words and indexes them the same way, so ours is faithful. Recorded
because the bound is a *coincidence between a configuration constant and an
array size*, with nothing in either the object or this tree tying them together
— the kind of pairing that breaks silently the first time someone writes a
second configuration, which is exactly what an 8 kHz retarget would do.

Finding F8169.

## D395 ⚠ `FPM_TONE_find_rev`'s age counter is a `short` advanced once per sample and reset only on a report

*Batch of 2026-08-26, from `FPM_TONE_find_rev` (blob 0x0ab170). **Unmeasured**.*

`rev_age` (+0x4c) increments once per input sample and is cleared only when a
reversal is reported. A stream that never reports overflows it after 32,768
samples — **4.1 seconds at 8 kHz** — after which the `rev_age > 160` gate fails
for another ~32,768 samples and every reversal in that window is silently
dropped.

**Reachability: any call sequence that runs the detector for more than about
four seconds without a report.** Whether `RxHdxPhsReversal` (the one caller,
finding F8170) can hold it that long has NOT been established — that needs
V.32's receive machine, which is unwritten.

**Observability: the return value**, which is `rev_age >> 3` and would come back
negative, and then the missed reports.

**Reproduced.** `t_fpm_tone`'s silence stream runs 1,600 samples, so this is
**unreached by the tests rather than disproved by them** — the distinction
matters, and it is why this entry says `unmeasured` rather than
`out-of-contract`.

Finding F8171.

## D410 ⚠ `V92setParamsInfoFromCPUnPck` stores through all ten of the block's array pointers without testing one of them

**This entry was written with a number in the three-eighties and moved to 410
before it was committed**, so nothing has ever referred to it by the old one.
The survey that produced the old number swept every BRANCH and stopped at the
maximum it found; four sibling WORKING TREES had already claimed higher numbers
in commits they had not made yet, the highest of them four hundred. A branch
sweep is not a survey while other agents are live -- the same lesson the
findings numbering learnt one batch earlier, where the branches topped out
twenty-one numbers below an uncommitted worktree.

*Batch of 2026-08-16, from `V92setParamsInfoFromCPUnPck` (blob 0x012f00).
**Reachability: every call that has any non-zero length or size.**
**Observability: none through the block -- a null slot is a store to address
0 and the process is gone.** Status: verified bit-exact against the blob over
all forty-five level-0 cases. Fix class: none proposed; reproduced.*

The four coefficient arrays at +0x5c..+0x68 and the six constellations at
+0x84..+0x98 are allocated once, by `V92createFilterCoefficients` and
`V92createConstellations`, and D171 records that neither of those two checks
a `sysdep_malloc` return. This function is the reader on the other side of
that: it loads each pointer and stores through it under a length taken from
the CP, with no null test anywhere -- `mov 0x5c(%esi),%ecx` at .text+0x131b9
and `fstps (%ecx,%edx,4)` two instructions later is the whole of it.

So a failed allocation at construction is a null in the block, and the first
CP with a non-zero `lz1` writes a float to address 0. The two behaviours
compose into a crash that neither function alone would show, which is why it
is recorded here rather than only at D171.

What the function DOES check is the other half of the same question, and
checks it carefully: every length is clamped before it is used --
`lz1`..`lp2` to 0x148 and the six `LC` to 0x80, eight separate conditional
stores -- and both ceilings are exactly what the allocations behind them
hold. The bound that could overrun a buffer is enforced; the pointer that
could be null is not.
## D385 ✅ `V90Demapper::hardDecision` reads `sign` uninitialised on its refusal arm

*Batch of 2026-08-16, from `V90Demapper::hardDecision` (blob 0x31050).
**Reachability: only when `sampleCount >= sampleCapacity`, which is the arm
that prints "Hard decision input buffers are full" and decides nothing.**
**Observability: none -- the value is multiplied by `level`, which is zero on
that arm and on no other.**  Status: verified bit-exact with `sign`
initialised.  Fix class: initialised to 1 at its declaration; one instruction,
no behaviour.*

The object assigns 0x20(%esp) in both arms of the sign test and nowhere else,
then loads it at 0x31229 -- on a path that reaches the load without having
taken either arm -- and multiplies it by a register it has just zeroed. So the
original source declares the variable without an initialiser and the C++ rules
make that path undefined. Reproducing the undefinedness would put a genuine
uninitialised read into `src/`, where a later compiler is entitled to delete
the multiply; initialising costs one `mov` on a diagnostic path and makes the
function total. The differential suite drives the arm on every trial mode and
both sides return zero.

## D386 ✅ `V90SignBitsExtractor::process` switches on an unset action when its state is neither 0 nor 1

*Batch of 2026-08-16, from `V90SignBitsExtractor::process` (blob 0x31ab0).
**Reachability: `reset`'s second argument is stored into `state` unfiltered, so
any caller that seeds it above 1 reaches it -- `reset` is not yet written and
no caller is.**  **Observability: total, if it is ever reached -- the switch
selects an inversion pattern from whatever `%edi` held on entry.**  Status: our
version verified bit-exact over states 0 and 1; states above 1 cannot be
compared because the blob has no defined answer.  Fix class: `action` is
initialised to `V90SBE_PASS_ALL`, the arm a zero register would have selected.*

The object tests `state == 0` and then `state == 1` and falls out of both with
its action register never written (0x31abf..0x31acb, then 0x31b6a reading
`%edi`). This is a live gap and not a dead one: nothing in the class clamps
`reset`'s argument. It is recorded rather than silently repaired because the
choice of `V90SBE_PASS_ALL` is OURS -- the object has no answer to reproduce --
and because the batch that writes `reset` should check whether any caller
passes a third value, in which case this becomes a real behavioural difference
rather than a formal one.

## D430 -- the transmit ring cursor is truncated to a short before the wrap test  `unmeasured`

`TxNoCarrierV17`, `TxNoCarrierV29` and `TxNoCarrierV32` all compute the next
write index as `lea 0x1(%r),%eax` followed by `cwtl` or `movswl %ax`, and only
then compare it against the ring length. A cursor seeded at 32767 therefore
wraps to -32768 rather than to 0, and the following store lands 64 KB below the
ring's buffer. Not reachable through the constructors that have been read --
`V17TX_create` sets the length to 50 and `V32FP_recreate` takes it from
`V32_SYMBOL_LEN` -- so this is a property of the arithmetic and not a live
fault. Recorded because the truncation is what distinguishes the object's
expression from `widx + 1 < len`, and `t_v17data.c` seeds the corner to hold
the reconstruction to it.

## D431 -- the V.32 symbol ring is declared as two different structs  `unmeasured`

`ModDataV32` and `TxNoCarrierV32` hand `fp + 0xb0` to an `SMCv32_encoder_*` as
`struct v32_symout *` and to `FPM_PPS_filter` as `struct fpm_smc_ring *`. The
two are the same layout under two tags -- finding F3646 has the field-by-field
table -- so `src/pump/v32/v32data.c` carries a cast at each of the three sites.
Not repaired here: unifying them is a TYPE change, `docs/plan.md` §3 forbids
one from inside a batch with other work in flight, and phase 6 collects the 27
punned sites into a batch of their own. This is the 28th and it is not
provably wrong in the way those are -- both readings are correct about the
bytes -- so it is a modelling duplication rather than a defect.

## D451 🐛 `FSE_decision_16pt`'s squared-error metric is never scaled, so it wraps and eight of the sixteen points cannot be decided

*Batch of 2026-08-17, from `FSE_decision_16pt` (blob 0x80e90) +0xe8..+0xf2 —
`imul %edx,%edx; mov %ebp,%eax; imul %ebp,%eax; add %edx,%eax; cwtl`, with no
shift between the products and the truncation. **Reachability: FIRES on every
call**, on the same arm of `SetRxModeV32` that reaches D302. **Observability:
the decided point, and so `*angle` into the carrier loop and the four
differentially encoded bits into the data path — not only the equaliser's
error term.** Status: CONFIRMED, and the reachable set is a proof over the
whole input domain rather than a sample. Fix class: none proposed; reproduced
as written. Finding F3801.*

Every sibling scales both squared terms before summing them — `_16Tpt` `>> 16`
on each, `_4pt` 15 and 16 (which is D301, its own defect), `_64pt` and
`_128pt` 13 and 13. This one sums them raw and keeps the low sixteen bits, so
the metric is `(short)(di² + dq²)` and wraps for any error above about 181
counts per axis.

**AT AN EXACT CONSTELLATION POINT EVERY CANDIDATE SCORES ZERO.** Both
coordinates of all sixteen points are ±4096 or ±12288, so every difference is
a multiple of 8192 and every squared difference a multiple of 2²⁶ — zero in
the low sixteen bits. `min` starts at 0x7fff, point 0 takes it, and `d < min`
is false for the other fifteen. A noiseless symbol is therefore decided as
point 0 whichever of the sixteen it actually is.

**AND ONLY EIGHT POINTS ARE DECIDABLE AT ALL.** With `I[k] = 4096a`, `Q[k] =
4096b`, `(i - I[k])² + (q - Q[k])² ≡ i² + q² - 8192(ai + bq) (mod 2¹⁶)`, and
`8192x mod 2¹⁶` depends only on `x mod 8` — so the metric takes at most eight
distinct values over the sixteen points, ties go to the lower index, and only
the first index carrying each value can win. Over all sixty-four residue pairs
that union is exactly {0, 1, 2, 3, 4, 5, 8, 10}. Points 6, 9 and 12 are
unreachable, which leaves point 3 as the only inner-ring point the slicer can
name.

**NOT FIXED, and that is the difference from D302.** D302 has a right answer
four independent lines agree on. This has none: the shift the author meant is
not recoverable from the object, since the family uses 16, 15/16 and 13 in
different members, and picking one would be inventing a constant to make a
defect look like an implementation. Reproduced exactly, and `t_v32fse.c`'s
mutation M2 — the metric given `_16Tpt`'s `>> 16` — fails 3,915 checks, so the
reproduction is measured rather than assumed.
## D500 ✅ `V92BitsToSymbol::process` diagnoses BUFFER_OVERFLOW after the write has already happened

Both overloads that take a `bits` argument hand the transmitter
`symbols + symbolsDone` and let it append. Only afterwards do they add the
produced count on and compare the total against `nSymbols`, the size the
staging buffer was allocated for:

    4e403  mov    0x10(%ebx),%edx        ; symbolsDone
    4e406  add    %edx,%eax              ; + what the transmitter produced
    4e408  cmp    0xc(%ebx),%eax         ; against nSymbols
    4e40b  mov    %eax,0x10(%ebx)
    4e40e  jbe    ...                    ; over -> BUFFER_OVERFLOW

By the time the comparison is true the transmitter has written past the end of
the buffer. The clamp that follows -- `symbolsDone = symbolsBlockSize` -- only
tidies the count; nothing is undone and no caller is told which bytes are
gone. Reproduced.

`t_v92btosproc.cpp` reaches the arm without corrupting its own heap by
allocating the staging buffer large and writing `nSymbols` DOWN afterwards:
the object's comparison is against the field, so the arm is entered for the
right reason while every store lands in real storage. A fixture that made the
allocation as small as the field would be comparing two corrupted heaps.

## D501 ✅ `V92Transmitter::process` never bounds its bit buffer

`bitBuffer` is `sysdep_malloc(0x50)` and `process` stores into it at
`bitsBuffered`, which rises one per input bit and falls by `K` when a frame
comes out. Nothing anywhere compares either against 0x50. `K` reaches this
class from the parameter block through `V92Transmitter::reset`, and the
unpacker builds it as `2 * (drn + 17)` with no clamp that this tree has found,
so a `drn` above 23 gives a `K` above 80 and the buffer is walked off on the
first frame. `V92ModulusEncoder::progress` then reads `bytes[i]` for `i` down
from `K - 1` and follows it out. Reproduced; the fixture stops at `K = 80`,
which is the last value that fits.

## D502 ✅ the drain overload never tells its caller how many symbols it delivered

`V92BitsToSymbol::process(unsigned int &nbits, short *out)` writes
`symbolsBlockSize` shorts on its normal path and only `symbolsDone` of them on
its BUFFER_UNDERFLOW one, and the only number it hands back through `nbits` is
what `nofBitsForNextTime` wants NEXT time -- not the count just written. A
caller that ignores the status code cannot distinguish a full block from a
partial one, and the rest of `out` keeps whatever it held. Reproduced. Not a
fault the object ever hits with its own caller, which checks the status.

## D503 ✅ `V92CP::evaluateCRC` verifies a message whose CRC is wrong by 256

The sixteen absolute differences between the computed register and the received
one are summed into a single BYTE -- `add %al,0x13(%esp)` at .text+0x4ebc3 --
and the verdict is whether that byte is zero. Sixteen stages differing by 16
sum to 256, which is zero in a byte, so the message verifies. So does any other
combination summing to a multiple of 256.

Unreachable through the protocol, where every entry of `bits` is 0 or 1 and the
largest possible sum is 16. It is reachable through the class, which never
checks: `bitsToInfo` writes `bits` and nothing bounds what it writes.
Reproduced, and `t_v92cpcrc.cpp` drives it on purpose because it is also the
only trial that pins the accumulator's width.

## D504 ✅ `V92CP::calcCRC` runs off the address space for a length below seventeen

The loop bound is `word_910 - 17` in unsigned arithmetic and the guard is
`cmp %ebp,%esi; jae` with `%esi` holding 18. A `word_910` of sixteen or less
wraps the subtraction to about four billion, the guard passes, and the loop
walks `bits[i]` upward until it faults. There is no check anywhere in
.text+0x4e5f0.

Reproduced, and NOT driven: a fixture that reached it would crash on both
sides rather than compare them. `t_v92cpcrc.cpp`'s grid starts at seventeen,
which is the first length that behaves -- and it behaves by computing a bound
of zero and returning without clocking anything.
## D520 -- five of `bitsToInfo`'s ten store sites bound the cursor and five do not  `unmeasured`

*Batch of 2026-08-17, from `V90CP::bitsToInfo` (blob 0x52d20).  **Reachability:
any state entered with `word_cac` at or above 0x2ee0, which the state machine
cannot reach on its own but a caller or a previous message can leave behind.**
**Observability: identical on both sides, so it is not a DIFFERENCE -- what is
observable is that the two halves of the member behave differently from each
other.**  Status: verified bit-exact; driven by
`test/unit/t_v90cpb2i.cpp`'s `run_cp_b2i_guard`.  Fix class: none proposed;
adding the missing five would be a behaviour change no test could justify.*

Ten arms store the arriving bit into `bits[word_cac]`.  Five of them --
states 3, 8, 10, 11 and 12, which are the ones whose length the MESSAGE
carries and which can therefore run long -- open with

    cmp    $0x2edf,%eax
    ja     <print "not enouch memory in the buffer" and store nothing>

and five -- states 2, 4, 5, 6 and 7, whose lengths are all fixed constants in
the code -- store unconditionally.  So the author bounded exactly the arms
whose length a peer controls, which is a defensible reading of the risk and
is not a uniform check; a cursor left above 0x2edf on entry to state 5 writes
`crc[0]` and one left far above it writes past the object.

The test drives a guarded arm at 0x2edf, where it must store, and at 0x2ee0,
where it must not -- and 0x2ee0 IS `crc[0]`, so "it did not store" is read
off the blob's own object rather than restated from our source -- then drives
an unguarded arm at 0x2ee0 and watches both sides write `crc[0]` together.
The sweep stops there rather than going further out for D390's reason: past
the object there is nothing to compare and a faulting test reports nothing.

## D521 -- `bitsToInfo` stalls for ever on a counted block of zero entries  `unmeasured`

*Batch of 2026-08-17, from `V90CP::bitsToInfo` (blob 0x52d20).
**Reachability: any peer that sets +0x08 or +0x0c and sends all-zero counts,
which is legal on the wire.**  **Observability: identical on both sides.**
Status: verified bit-exact; driven by `test/unit/t_v90cpb2i.cpp`'s
`run_cp_b2i_corners`.  Fix class: none proposed.*

States 8 and 11 advance on `word_cb0 == alpha` and `word_cb0 == beta`, the two
block lengths computed as seventeen times the sum of the counts.  Both count
FIRST and test afterwards --

    word_cb0++;
    if (word_cb0 == alpha) { ... }

-- so a length of zero is never matched: `word_cb0` starts at 0, becomes 1 on
the first bit of the block and climbs from there.  The transmitter is
consistent about it, `infoToBits` emitting no entries at all for a count of
zero, so a well-formed message with +0x08 set and four zero counts leaves the
receiver sitting in state 8 for the rest of the sequence and for every
sequence after it, until a caller resets the object.  There is no timeout in
the member.

The same argument applies to `beta` and state 11.  It is not reachable by
accident from a peer that has anything to say -- a block flag set with
nothing in the block is the degenerate case -- which is presumably why it was
never hit.
## D470 🐛 `V90TRN2Designer::maxK` returns one bit short for every exact power of two above 2^21

**Where:** `src/pump/v90/V90TRN2Designer.cpp`, `maxK`; blob 0x3ca30.

**What the original does:** computes `log10(product) / (float)log10(2) + 1e-6`
and truncates towards zero.  The dividend stays in the coprocessor at extended
precision and the divisor is ROUNDED TO A FLOAT, which makes it 4.757e-8 too
large; the quotient for an exact power of two therefore lands `k * 4.757e-8`
below `k`, and the 1e-6 guard only covers that up to `k = 21.02`.  2^22 comes
back as 21, 2^36 as 35, 2^42 as 41.

**Impact:** `V90TRN2Design` uses the result as the frame's bit count --
`mappingParams->word_0 = maxK - shaperSR + 6`, read back by
`V90ConstellationPower` as `1LL << (shaperSR + word_0 - 6)` -- so a TRN2
constellation set whose lengths multiply to an exact power of two above 2^21
is designed one bit smaller than it could be.  It is a lost bit, not a wrong
answer: everything downstream is consistent with the smaller count.

**Status:** reproduced, not fixed, and not behind `DSPLIB_REPRODUCE_BUGS`.
The fix would be `1.0f / (float)log10(2)`'s error running the other way, or
simply `fyl2x` against `fld1`, and either changes the designed constellation
for real calls -- so this is a defect in the original that the reconstruction
is required to keep.  Finding F4400 has the measurement and the table.

## D471 🐛 `V90TRN2Designer::setTrn2DummyConstel` has no bound against the 128-byte row

**Where:** `src/pump/v90/V90TRN2Designer.cpp`, `setTrn2DummyConstel`; blob
0x3cb00.

**What the original does:** `for (i = 0; i < params->nofUcodesInTrn2; i++)`
over `constellation[k][i]` and `codecConstellation[k][i]`, both of which are
128 bytes per row.  Nothing tests the count against 128, and nothing tests it
against the row's own length -- the count comes from `V90Parameters` +0x078,
which `setToDefault` seeds to 8 and `setNofUcodesInTrn2` can replace with
+0x080.

**Impact:** a count above 128 writes into the next constellation's row, and
above 768 past the end of the second table.  No caller in the object produces
one: the two writers of +0x078 are `setToDefault` (8) and `setNofUcodesInTrn2`
(the +0x080 default, 4).

**Status:** reproduced, not fixed.  Out-of-contract for every input the object
itself can produce, so there is nothing for a differential test to compare
against beyond 128; `test/unit/t_v90trn2design.cpp` drives 0, 1, 78, 79, 80,
127 and 128 and deliberately stops there.

## D472 ⚠ `V90TRN2Design`'s eighth argument is never read

**Where:** `src/pump/v90/V90TRN2Designer.cpp`, `V90TRN2Design`; blob 0x3cb60.

**What the original does:** the mangling gives twelve parameters and the
eighth is a `short`.  There is no reference to `+0x120` anywhere in the 3,767
bytes.  The one caller, `V90Demodulator::exitPhase3`, computes it --
`movswl 0xa968(%edx),%ecx` -- and passes it.

**Impact:** none.  It is recorded because a reader who sees the caller do work
for it will look for the use, and because the reconstruction has to keep the
parameter to keep the mangled name.

**Status:** reproduced -- the parameter is declared and unused.
`test/unit/t_v90trn2design.cpp` runs the same design twice with two different
values in that slot and asserts the mapping block is identical, so "never
read" is measured rather than read off a listing.

## D560 ✅ `V92Phase4Modulator::generateRu` and `::generateRuNot` return an uninitialised local

Both switch on `(symbolCount - 1) % 6` over cases 0-2 and 3-5 with no default
arm and no return after the switch. The quotient can never exceed five, so the
third path is unreachable through any input -- but GCC emits it, and it returns
whatever the register the symbol lives in happened to hold: `ja 16ffb` at
.text+0x16fdf, landing on the shared `mov %ebx,%eax` before either arm wrote
%ebx.

Reproduced as `short sym; switch (...) {...} return sym;`, which is what
produces that code. A `default:` arm would add an instruction the blob does not
have. Not reachable by any trial, so `t_v92p4gen.cpp` cannot drive it and does
not try; finding F4704.

## D561 ✅ `bits[-1]` at `bitsPerSymbol == 0` writes into `prevBit`

**CLOSED AND DRIVEN.  The write is the OBJECT'S, our access is defined C now,
and the input and the four store-order mutations it settles are back in
`t_v92p4sym` and green on both compilers.**

*Batch of 2026-08-17, on branch `v92-fold-oob`.  Fix class: reproduced as
found -- the value and the aliasing are kept and the undefined access is
removed, which is D392's disposition and D4's before it.  **Reachability:
`bitsPerSymbol` is written at exactly one site among the class's own
thirty-six symbols, `reset`'s `add $0x2,%dl; mov %dl,0x43(%esi)` at
.text+0x19065, and the add is EIGHT BITS WIDE -- a second argument of 254
gives zero.  The constructor never writes the field at all.**
**Observability: `prevBit`, the block, and every symbol the mapper derives
from them.**  Findings F5400, F5401, F5402.*

`generateCPu`, `generateSUVu`, `generateE2u` and `generateTRN2u` fold the
carried differential bit into `bits[bitsPerSymbol - 1]`. With `bitsPerSymbol`
zero that subscript is -1, which is `V92Phase4Modulator+0x7b` -- the top byte
of the `unsigned int prevBit` at +0x78. Inside the object, which is why it
neither faults nor is caught by a checking allocator.

**IT IS THE OBJECT'S OWN ARITHMETIC, AND THE OVERLAP IS LOAD-BEARING.** All
four sites LOAD and STORE through `0x7b(%count,%this,1)` with the count
zero-extended from +0x43 -- `movzbl 0x7b(%edx,%ebx,1)` then
`mov %al,0x7b(%edx,%ebx,1)` at .text+0x17c3a/+0x17c44, and the same pair at
+0x17c91/+0x17c9b, +0x17dc0/+0x17dc7 and +0x17ef0/+0x17ef7 -- and the blob has
no guard. So the byte the fold reads is `prevBit >> 24`, and which of the two
overlapping stores lands last decides what +0x7b holds. The object commits to
both orders: byte then `prevBit` in `generateCPu` and `generateSUVu`, `prevBit`
then byte in `generateE2u` and `generateTRN2u`.

**WHAT CHANGED IS OUR ACCESS, NOT THE VALUE.** `prevBit` and the block are one
array object in `V92Phase4Modulator.h`:

```c
	union {
		unsigned int prevBit;
		unsigned char bitsExt[V92P4M_BITS_BELOW + V92P4M_BITS_LEN];
	};
```

`bitsExt[V92P4M_BITS_BELOW + i]` is the object's `bits[i]` and
`bitsExt[V92P4M_BITS_BELOW - 1]` is its `bits[-1]`, so every index the fold can
form is inside one array and the behaviour is defined. **The order is the
source's in BOTH directions, measured rather than assumed**: unmutated, GCC
3.4.2 emits the blob's order at all four sites; with the `generateTRN2u` order
mutation in `src/`, it emits the mutation's, and `make period` goes 221 passed
/ 1 failed against 222 / 0. `compare.py` does not
move -- 410 identical (the same SET), 78 same size, 606 different size, 401355
bytes, before and after, on GCC 3.4.2 exact -- because a constant bias on an
index into a member array rides in the addressing mode. Finding F3701's
argument, one class along.

**WHAT IT REPLACED.** The entry used to read *reproduced, and NOT DRIVEN*:
`t_v92p4sym.cpp` had the input in its grid and took it out, because while the
subscript was out of bounds the two stores' ORDER belonged to the compiler --
GCC 13 kept our order, GCC 3.4.2 did not, and `make period` failed 240 checks
on unmutated source. Four store-order mutations were withdrawn with it. That
was right about the TRIAL (finding F4705) and it was not the fix; the owner's
ruling is that an implementation must not depend on the ordering of objects in
memory, and this was the same ruling with two members of one class rather than
two objects in one section. **A trial withdrawn because our code was undefined
comes back once it is defined**, or the fix is invisible to the suite that
motivated it: the grid starts at zero again, `saw_bps_zero_folding` proves the
fold is reached on the arm that folds, and the four mutations are back and
caught -- by the same 80 and 160 checks that used to fail on unmutated source.
## D600 ⚠ Both phase 4 decision members return an uninitialised local on five arms

**Where:** `src/pump/v90/V90Phase4Demodulator.cpp`, `getV90Decision` and
`getV92Decision`; blob 0x25ea0 and 0x26ac0.

*RENUMBERED before merge.  `v92cp-infotobits` took the number this entry was
first written under, in the same window, and landed on master first; nothing
outside this batch had referenced it.*

**IT IS THE PHASE 4 INSTANCE OF D321**, which records the same shape one class
along in `V90Phase3Demodulator::getV90Decision`.  Two entries and not one
because a deviation names a SITE: these are different functions in a different
translation unit, the arms are five rather than one default block, and the
reachability argument is different -- phase 3's four states are unmeasured and
phase 4's four are states the receiver passes through.  Read D321 first; what
follows is only what differs.

**What the original does:** both build the returned decision in `%edi` and both
save and restore `%edi` around the body, but neither writes it on every path.
`getV90Decision` leaves it untouched on states 4 and 0x10 and on the
out-of-range `ja` at 0x25ec8; `getV92Decision` on states 5 and 6 and on its own
`ja` at 0x26ae8.  All five reach

    25fa0:  89 f8            mov %edi,%eax
    25fa2:  8b 5c 24 40      mov 0x40(%esp),%ebx
    ...
    25fb1:  c3               ret

so what comes back is whatever the CALLER left in `%edi`.  There is no
initialiser anywhere in either function and no path that could have been
eliminated as dead: the jump table sends states 4 and 0x10 to the same block
as the range check.

**Impact:** none that reaches a modem.  `V90Demodulator` calls `getDecision`
only in states whose arm assigns the value, and the four inert states are ones
the receiver passes through rather than sits in.  A caller that used the answer
in one of the five would get stack litter.

**Status:** reproduced.  `short decision;` with no initialiser is what puts it
in front of the compiler, and GCC 3.4.2 emits the object's shape from it; GCC
13 warns `-Wmaybe-uninitialized` at both `return` statements, which is correct
and is the point.  `test/unit/t_v90p4ddec.cpp` compares the OBJECT and the
transcript on all five arms and the return value on none of them -- an
assertion there would be comparing two pieces of stack litter and would pass or
fail for reasons unrelated to this reconstruction.  Initialising it would be a
behaviour change on exactly the paths the object leaves open, so it is not
done.

## D570 ✅ `word_10c` bounds two six-group blocks and nothing bounds `word_10c`

`V92CP::infoToBits` sends `word_10c` groups of eight 16-bit words out of
`short_42` and as many again out of `short_a2`.  Both blocks hold SIX groups --
they abut at +0x0a2 and end at +0x102 -- and the loop trusts the field:
`movzwl 0x10c(%edi),%ebp` at .text+0x4f29f, then `cmp 0x4(%esp),%ebp; ja` with
no clamp anywhere in the 1,916 bytes.  A value above six walks off the end of
`short_a2` into `word_104`, `suv`, `word_10c` itself and beyond, and at 118 or
more the cursor runs past `bits` as well.

`setV92CPpckFromParamsInfo`, which is what fills the field, counts up to six
and no further -- `cmpl $0x5,0x14(%esp); jbe` at .text+0x33a13 -- so nothing in
the object produces an out-of-range value.  That is a property of the writer
and not a check in the reader.

Reproduced as the object has it, and NOT DRIVEN.  `t_v92info.cpp` clamps its
grid to 0..6 and says so at the top: past six the subscript is out of bounds in
OUR source, and a trial there would be the compiler adjudicating our undefined
behaviour rather than the object adjudicating our reading -- D561's argument,
one class up.  Finding F4750.

**AND D561 HAS SINCE BEEN CLOSED THE OTHER WAY, WHICH IS THE STANDING WORK
HERE.**  The clamp is a statement about OUR source and not about the object, so
it expires the moment our access is made defined: D561 gave `V92Phase4Modulator`
one array object spanning `prevBit` and the block, and its withdrawn trial and
mutations came straight back.  The same move is available here -- the walk runs
off `short_a2` into `word_104`, `suv` and `word_10c`, all members of the same
class -- and until someone makes it, this grid stops at six for the reason
above rather than because the object does.  Findings F5400 and F5402.

## D571 ✅ `byte_128 == 0` divides by zero in the padded-length round-up

The padded length is `(cursor / (12 * byte_128) + 1) * 12 * byte_128`, and the
object's `div %ecx` at .text+0x4efdd has no guard: a zero at +0x128 raises #DE.
`infoToBits` itself stores 1 there when +0x001 is zero, and
`V92Phase4Modulator` writes it at five sites, so the field is expected to be
set before the message is packed -- but nothing checks, and the constructor
does not initialise it.

Reproduced, and kept out of the grid: `t_v92info.cpp` uses 1..6.  A trial at
zero would trap identically on both sides, which measures the CPU rather than
the reading.  Finding F4750.

## D700 ✅ `generateSymbol` divides by four different fields and guards none of them, and that is how D571 is reached

Twelve unguarded unsigned divisions in one function, in four groups:

  - `symbolCount % word_1b0` in the arms for states 5, 6, 8, 9, 11 and 13, and
    `symbolCount == word_1b0` in 10 and 12 which is not a division at all.
    `word_1b0` is zero out of the constructor and out of `reset`, neither of
    which writes it; only the arms that call `getBitVector` ever set it.
  - `(symbolCount - 24) % patternLength` in state 1, and
    `pattern[(symbolCount - 25) % patternLength]` inside `generateCPt`.
    `patternLength` is likewise unwritten by the constructor.
  - `patternLength / bitsPerSymbol` in states 4, 5, 13, 23, 24 and 27.
  - `patternLength / cp->bitsPerSymbol` in states 6, 8 and 12.

The object's own instructions are `divl 0x1b0(%esi)`, `divl 0x1ac(%esi)` and
`div %edi` with nothing testing the divisor anywhere in the 4,055 bytes.

**AND IT ANSWERS D571'S OPEN QUESTION.**  D571 records that `V92CP::infoToBits`
raises #DE when `V92CP::bitsPerSymbol` is zero and leaves reachability to
whoever writes the callers.  It is reachable from here: ten arms call
`infoToBits`, three of them (states 4, 24 and 27) set `cp->bitsPerSymbol` from
this class's own `bitsPerSymbol` immediately before -- which is zero out of the
constructor -- and the other seven do not set it at all.  `reset` stores 1
there, so the object's own lifecycle keeps it non-zero, and that is a property
of the writer rather than a check in the reader, exactly as D570 says of
`word_10c`.

Reproduced with no guard added, and NOT DRIVEN at zero: `t_v92p4sym.cpp`'s
`gs_setup` keeps `word_1b0` non-zero, `patternLength` in 1..7 and both
`bitsPerSymbol` fields in 1..16.  A trial at zero raises #DE identically on
both sides, which measures the CPU rather than the reading -- D571's argument.
Finding F4820.

## D701 ✅ Five `generateSymbol` arms return an uninitialised local when nothing is staged

`V92BitsToSymbol::process(unsigned int &, short *)` copies `symbolsBlockSize`
symbols into `out` when at least that many are staged and `symbolsDone` of them
when fewer are -- which at zero is none.  `out` is the address of ONE `short`
on the caller's frame, and the states 16, 17, 26, 27 and 28 arms never
initialise it: the object's `lea 0x3a(%esp)` and its seven siblings point at
slots nothing has stored to.  `V92BitsToSymbol::reset` leaves `symbolsDone` at
zero, so the very first symbol after a reset returns frame residue.

The same shape as D560, one level up, and it is the object's: there is no
initialisation and no test of the return value at any of the five sites.
Reproduced by writing the arms as the calls they are.

**NOT DRIVEN, and it took 635 failing checks to notice.**  With `symbolsDone`
left at zero the fixture disagreed on every trial of those arms -- `got 0,
reference 1`, with the object, the CP, the chain, the scrambler and the mapper
all agreeing -- because our frame is not the blob's and the residue therefore
is not the same residue.  `t_v92p4sym.cpp` now stages symbols so the copy
always happens.  This is why the per-member tests of `generateRm` and
`generateB1u` never showed it: ours and the blob's are separate functions with
the same frame, so both read the same residue and agreed for the wrong reason.
Finding F4821.
## D660 ⚠ `generateRi` and `generateRiNot` read an uninitialised `short` on an unreachable path

**Where:** `src/pump/v90/V90Phase4Modulator.cpp`, `generateRi` and
`generateRiNot`; blob 0x2c6b0 and 0x2c710.

**What the original does:** both compute `(symbolCount - 1) % 6` and dispatch
on it with a two-range decision tree, and the tree's default edge falls into
the tail that returns the value:

    2c6d7:  83 f8 02      cmp   $0x2,%eax
    2c6da:  76 1b         jbe   2c6f7            ; 0..2, sym = codeLevel
    2c6dc:  83 f8 05      cmp   $0x5,%eax
    2c6df:  77 1a         ja    2c6fb            ; -> the tail, %ebx unset
    ...
    2c6f7:  0f bf 59 3c   movswl 0x3c(%ecx),%ebx
    2c6fb:  89 d8         mov   %ebx,%eax        ; the tail

so on the `ja` what comes back is whatever the CALLER left in `%ebx`.
`generateRiNot` has the same shape with `%ecx` and one more `neg`.

**Impact:** none, and this one is arithmetically impossible rather than merely
unreached.  `x % 6` on an unsigned dividend is at most 5, so the `ja` is never
taken for any value of `symbolCount` including 0 and 0xffffffff -- 0 gives
0xffffffff % 6 == 3, which is inside the range.  It is the switch's default
edge, kept because GCC 3.4.2 emits the comparison rather than proving the
modulus's range.

**Status:** reproduced, and D561 does not bite: no input can drive the
reconstruction into it either, so `test/unit/t_v90p4mtab.cpp` sweeps
`symbolCount` over both periods and past zero with nothing excluded from the
grid.  `short sym;` with no initialiser and a `switch` with no `default` is
what puts it in front of the compiler.  Adding a `default:` would add an
instruction the object does not have and would change behaviour on exactly the
path the object leaves open, so it is not done.

## D661 ⚠ Both phase 4 data pumps pass an uninitialised `unsigned int` by reference and read it back

**Where:** `src/pump/v90/V90Phase4Modulator.cpp`,
`generateDataSymbolBeforeFPE` and `generateDataSymbolBeforeRRN`; blob 0x2d770
and 0x2d7d0.

**What the original does:** each reserves two stack slots, hands their
addresses to `V90BitsToSymbol::process(unsigned int &nofBits, short
*outSymbols)`, and reads the first back:

    2d7dc:  8d 4c 24 10   lea    0x10(%esp),%ecx      ; &nofBits
    2d7e4:  89 4c 24 04   mov    %ecx,0x4(%esp)
    2d78e:  e8 ..         call   V90BitsToSymbol::process
    2d793:  8b 44 24 10   mov    0x10(%esp),%eax      ; read straight back
    2d797:  85 c0         test   %eax,%eax

Nothing writes `0x10(%esp)` before the call in either function, and
`process` writes through the reference only on the arm where
`symbolsBlockSize` is non-zero -- with it zero the callee prints
"SIZE_NOT_SET", returns status 1 and leaves the slot alone.  So there is a
reachable configuration in which the `test` reads whatever was on the stack.

Note that the STATUS is not what either pump looks at: `%eax` holds it on
return and both discard it in favour of the reference.  A reconstruction that
read the return value instead would agree on this configuration and disagree
on the others; the mutation "the status is read rather than the demand" is
what holds that.

**Impact:** none that reaches a modem.  `symbolsBlockSize` is zero only
between construction and the first `setSymbolsBlockSize`, and a pump asked for
a symbol before the converter has been told its block size is already out of
contract -- the object's own answer to it is a diagnostic line.

**Status:** reproduced.  `unsigned int nofBits;` with no initialiser is what
puts it in front of the compiler, and GCC 3.4.2 emits the object's shape from
it.  Unlike D660 this one IS reachable, so `test/unit/t_v90p4mgen.cpp` keeps
it out of the grid rather than running it and declining to assert: a trial
that reaches undefined behaviour in the reconstruction is not a differential
trial (D561).  The two configurations that are swept -- `(symbolsBlockSize 1,
symbolsDone 2)` and `(2, 1)` -- both write the reference and both write the
symbol, so the object, the transcript and the return value are all asserted on
every trial.  Initialising it would be a behaviour change on exactly the path
the object leaves open, so it is not done.
## D670 ✅ Four `V90PreFilter` arms form a coefficient pointer outside its bank

**Where:** `src/pump/v90/V90PreFilter.cpp`, `getFilterPointer` and both
`setFilter` overloads; blob 0x44ff0, 0x45090 and 0x44a30.

`getFilterPointer` clamps the row to the bank's last -- 30 for the 20-tap
banks, 50 for the 40-tap one -- on every arm EXCEPT the one it takes when no
reference loop is selected, where `je 0x45040` at 0x45002 jumps past the
`cmp $0x1e` the other arms fall through.  Neither `setFilter(PreFilterCoefType,
unsigned)` arm clamps at all.  So `bank1(80)` and `bank3(0)` are both
reachable, and the second is a pointer 800 floats BEFORE its array.

This is the same deviation the file already carries for `selectFilter`'s ISDN
and PBX arms, which take the row straight out of the registry: the file comment
says "THE INDEX IS NOT ALWAYS CLAMPED, AND THAT IS THE OBJECT'S DOING ...
Reproduced literally", and `bank1`/`bank2`/`bank3` say "`row` is deliberately
not range-checked".  These three members are where the unclamped arms come
from -- `selectFilter`'s automatic arm is `getFilterLength` and
`getFilterPointer` inlined.

**Reproduced, and DRIVEN.**  `t_v90prefilter.cpp`'s `run_filteraccessors`,
`run_setfilter_gain` and `run_setfilter_type` sweep gains 0..80 across every
`coefType`, which is the same grid `run_selectfilter_synthetic` has been
driving through the same three helpers since batch 3.  The pointer is
arithmetic and is never dereferenced: `FloatFIR::setCoefficients` stores it,
compares nothing through it and reads nothing through it, and the test resolves
it by ADDRESS against each side's own bank bases rather than by loading from
it.  Excluding it instead would leave the object's two unclamped paths with no
trial at all, and the clamp mutations ("bank 3's clamp is bank 1's", "bank 2 is
not clamped", "getFilterPointer clamps on the no-reference-loop arm too") are
exactly the defects that would then be invisible.

## D671 ✅ `getV90Capability` reads `loops[-1]` when the search selects nothing

**Where:** `src/pump/v90/V90PreFilter.cpp`, `getV90Capability`; blob 0x45a10.

The last line is `dataBase[codecType].loops[refLoop].capability`, and the only
thing that has bounded `refLoop` by then is `autoSelection`, which leaves it
untouched when no table entry beats its 1e10f starting distance.  The object
computes `%ecx * 17` at 0x45a5c on a negative `%ecx` and loads through it; ours
would subscript out of bounds.  It is reachable only when the search matched
nothing AND the registry's +0x500 is not 6, because either alone returns 1
before the load.

**Reproduced, and NOT DRIVEN** -- and unlike D670 this one is a LOAD and not
arithmetic.  Every trial in `run_getv90capability` calls `set_measurement`
first, so the search always lands on a record and `refLoop >= 0` is asserted
per trial rather than assumed.  A trial that arranged the empty case would be
measuring our undefined behaviour and not the object's, which is D561's ruling.
The same hazard is already inside `autoSelection`, whose closing `edprintf`
prints `loops[refLoop].name` unguarded.
## D680 ⚠ `V90Demapper::reset`'s doubled arm runs past the constellation row, and the trial that would reach it is not driven

`reset` and `resetNoSpectral` both lay TWO levels down per code when the
detector's `short_2800[i]` is set, so the cursor runs to `2 * n - 1` while the
row holds 128 entries. With `n` above 64 the object writes past the row: for
`i < 5` into the next row, and for `i = 5` past `constellation` entirely and
into `constellationSize` behind it.

It is the object's own arithmetic and there is no bound anywhere near it. One
`shl $0x7` and an `add` form `i * 128 + k`, scaled by two —
`mov %bx,0x30(%ebp,%eax,2)` at 0x30924 — and `V90Demapper.h` records the same
flat indexing for `updateConstelation` and `linearMappingStudy`.

**Reproduced in shape, and NOT DRIVEN.** `t_v90demap.cpp`'s row lengths stop at
63, so `2 * n - 1` stays inside the row on every trial, in `run_reset` exactly
as in `run_resetns`. Writing `constellation[i][k]` with `k >= 128` is out of
bounds for the declared array whatever the object does with the same address,
and a trial that reaches it is measuring undefined behaviour in the
reconstruction rather than comparing two implementations — D561's rule, and
the same reason that entry took its input out of the grid.

**WHAT THAT COSTS IS ONE ENCODING CLAIM, AND IT IS NAMED HERE RATHER THAN
TESTED.** The object loads `mapp->constellationSize[i]`, SPILLS it to
`0x18(%esp)`, then stores it to `constellationSize[i]`, and every loop compare
reads the spill (0x308e8..0x30955). This reconstruction keeps that as a local,
because a re-read of the member would differ exactly when the sixth row's
overrun rewrote it — which is the input above. So the local is settled by the
instruction and by nothing in the differential tier, and the mutation that
swaps it is registered `equivalent` with this entry as its reason.

**Not corrected**: clamping the cursor would make the reconstruction disagree
with the blob for any caller that does produce a row above 64, which is the one
thing it may not do.

## D710 🐛 `VPcmV34GetVisualDiagnostics` selectors 3 and 4 write `points[0]` whatever `maxCount` says

**Where:** `src/pump/v34/v34diag.cpp`, `VPcmV34GetVisualDiagnostics`, the
`VDIAG_RESAMPLER_PHASE` and `VDIAG_RESAMPLER_OFFSET` arms.  Blob 0x73d0 and
0x7412.

**What:** every other arm of this function clamps its point count against the
caller's `maxCount`.  These two never read it at all.  Both write eight bytes
at `points[0]` -- the real half from the receiver on the V.34 arm, the
imaginary half always -- and return 1, so

```
    VPcmV34GetVisualDiagnostics(obj, 3, points, 0)
```

writes one point into an array the caller has just said has room for none, and
tells the caller it wrote one.

**How it is known:** `0x3c(%esp)` holds `maxCount` and is loaded in seven of
the nine arms.  In these two the only reference to it is the argument slot of
the K56flex stub at 0x75be and 0x75e5, whose result is then discarded; the
stores at 0x73e9, 0x742c and 0x73f0 are unconditional and no compare against
`0x3c(%esp)` exists anywhere between the jump-table entry and the return.

**Consequence:** an application that sizes its buffer from a run-time count
and passes zero -- which selector 7 and any selector above 8 make a reasonable
thing to do, since both legitimately return nothing -- gets one point of
overrun.  Eight bytes, on the caller's own storage.

**Reproduced, not fixed.**  The bound is the caller's and the object ignores
it; clamping here would make the reconstruction disagree with the blob for
every caller that passes zero, which is the one thing it may not do.  Not
behind `DSPLIB_REPRODUCE_BUGS`: the right fix is in the API contract rather
than in this function, since a caller that passes a buffer of one is served
correctly and the fault only exists at zero.

**Driven:** `t_v34diag.cpp`'s `run_visual` sweeps `maxCount == 0` over all ten
selectors and compares the point array PAST the caller's bound against the
seed on both sides -- `sawOverrun` requires the overrun to have been observed,
so the deviation is a checked claim and not a comment.
## D800 ✅ `V92Modulator::resamplerPhaseOffset` is read by `mkResampledSignal` and written by no member of the class

`+0x24` is loaded three times, all of them inside `mkResampledSignal` and all of
them single-precision:

    14ae6:  d8 43 24   fadds  0x24(%ebx)    the phase step itself
    14b75:  d9 43 24   flds   0x24(%ebx)    the diagnostic's whole and fractional
    14bdb:  d8 5b 24   fcomps 0x24(%ebx)    the diagnostic's sign

The whole class was swept for a store to it -- .text+0x14050 to +0x156e0, which
is all eighteen symbols including both constructor and both destructor variants
-- and there is none.  `progress`'s four `0x24(%esp)` are its own frame and not
the object.  So a modulator that reaches `resamplerPhaseChange == 2` before
something OUTSIDE the class has filled the word advances the resampler's phase
by whatever `sysdep_malloc` left there, and `Resampler::setNormalizedPhase`
takes anything outside `[0, 1)` to zero, so the observable effect ranges from a
small timing error to the phase being reset.

It is the second half of the pair `V92Modulator.h` has recorded since the
constructor batch, and the halves have separated: `+0x3c` turned out to be
written after all, by `progress`, so only `+0x24` is left.

Reproduced with no initialisation added, and DRIVEN: `t_v92modstate.cpp` seeds
the word identically on both sides, never zeroes it, and sweeps the `OFFSET` arm
over five values including a negative one and one past 1.0.  The two sides agree
BECAUSE nobody wrote it, and a reconstruction that helpfully cleared it fails.
Finding F5900.

## D801 ⚠ `mkResampledSignal` subtracts the split point from the block length without testing it

The second of the two resample calls is given
`blockRemaining - resamplerPhaseChangeAt` as its sample count:

    14a36:  mov 0x3c(%ebx),%eax
    14a39:  mov 0x8(%ebx),%ecx
    14a3c:  sub %eax,%ecx

-- an unsigned subtraction with nothing between it and the call.  A split point
past the end of the block therefore asks `Resampler::resample` for about four
billion input samples out of a buffer of `blockSize + 10` floats.  Its input
pointer, `resampleIn + resamplerPhaseChangeAt`, is already past the end by then.

`progress` is the only writer of the pair AND IT IS NOW WRITTEN, which settles
the reachability the other way: it stores `resamplerPhaseChangeAt = i` from
inside `for (i = 0; i < blockRemaining; i++)`, so the split point it stages is
always strictly less than the block length that was current when it was staged.
Nothing in `mkResampledSignal`'s own 662 bytes bounds it, and nothing has to as
long as the pair is only ever set from there -- but `blockRemaining` is
RECOMPUTED by every call of `progress`, so a change staged in one block and
applied against a shorter later one is still not excluded by anything either
function does.  Finding F7540.

Reproduced with no guard added, and NOT DRIVEN: `t_v92modstate.cpp` keeps the
split point in `0 .. blockRemaining`.  A trial past it walks off both sides'
buffers at once and would measure the allocator rather than the reading, which
is D571's argument.  Finding F5900.

## D802 ⚠ `mkResampledSignal`'s wrapped join computes its trip count as `n2 - 1` unsigned, and does not test for zero

The second `Resampler::resample` reports how many samples it produced through a
reference argument.  When the resampler's phase wrapped past 1.0 the join drops
that segment's first sample, so the copy runs `n2 - 1` times:

    14a79:  8b 7c 24 28   mov 0x28(%esp),%edi     n2
    14a7d:  4f            dec %edi
    14a7e:  83 ff 00      cmp $0x0,%edi
    14a81:  76 2d         jbe 14ab0

`jbe` is CF or ZF, so at `n2 == 0` the decrement leaves 0xffffffff, neither flag
is set, and the loop is ENTERED -- four billion four-byte copies out of
`resampleTail` and into `resampleOut`, both of which hold `nSamples + 10`
floats.  The reported length is `n1 + n2 - 1`, which underflows the same way.

`n2` is zero when the second segment is given no input and the resampler has
fewer than two samples pending, which `Resampler::resample`'s `avail > 1` guard
makes reachable: `resamplerPhaseChangeAt == blockRemaining` is enough.  So this
is the same shape as D801 one call later and from a different instruction.

Reproduced with no guard added -- the C is `for (i = 0; i < n2 - 1; i++)` over
an `unsigned int`, which is the object -- and NOT DRIVEN: `t_v92modstate.cpp`
gives the second segment at least eight input samples on every split trial and
asserts that it produced at least one output.  A trial at zero walks off both
sides' buffers at once, which measures the allocator rather than the reading
(D571's argument).  Finding F5900.
## D850 ⚠ The float DFE history shift is unguarded, and runs away at `dfeLength` 0 or 1

*`V90Equalizer::process`, blob 0x39830. Reproduced as a bounded input
requirement, not as code.*

The decision-feedback history is shifted up by one before the new sample is
pushed, and the object writes it as a counted loop over `dfeLength - 1`
iterations with **no entry guard**:

    39840:  lea (%esi,%ebp,4),%edx     ; &array_44[dfeLength]
    39843:  lea -0x8(%edx),%ebx        ; src = &array_44[dfeLength - 2]
    39846:  lea -0x4(%edx),%ecx        ; dst = &array_44[dfeLength - 1]
    39849:  lea -0x1(%ebp),%edx        ; count = dfeLength - 1
    39850:  mov (%ebx),%eax ; sub $4,%ebx ; mov %eax,(%ecx) ; sub $4,%ecx
    3985a:  dec %edx ; jne 39850

At `dfeLength == 1` the count starts at zero, the body runs once reading
`array_44[-1]`, and the counter wraps to 0xffffffff; at `dfeLength == 0` it
starts at -1. Either way the loop copies four billion floats downward through
the heap. **The FIXED-POINT twin of the same shift, at 0x39421 over
`array_12cAligned`, DOES have the guard** -- `cmp $0x1,%ebp; je` -- so the two
arms disagree at exactly `dfeLength == 1`, which is what says the guard is
something GCC placed rather than something the source spells differently at
the two sites.

**Our source carries the guarded shape at both sites**, because the two cannot
both be written and the unguarded one has no defined behaviour to reproduce:
an object that walks off a heap allocation for 2^32 iterations does not
survive to be compared against. `dfeLength >= 2` is therefore a precondition
of the reconstruction, it is what `V90Equalizer::V90Equalizer` and
`V90Demodulator::reset` in fact pass, and the differential grid excludes 0 and
1 deliberately rather than by oversight.

**Not corrected**: nothing here is corrected, because at every value the object
survives the two shapes are the same loop.

## D851 ⚠ The fixed-point LMS shift is masked, and the object leaves it to the hardware

*`V90Equalizer::process`, blob 0x3937a and 0x39405. Fix class: defined
behaviour, at the cost of one instruction per loop.*

Both fixed-point coefficient loops step by `(coef * beta) >> shift`, where the
shift is `linearEquMmxShift` or `dfeMmxShift` -- each a truncated base-two
logarithm `setLinearEquBeta` and `setDfeBeta` compute from two fields the
caller controls, and each able to come out negative or above 31. The object
loads the count as a byte and shifts:

    39372:  0f b6 4c 24 64   movzbl 0x64(%esp),%ecx
    3937a:  d3 fa            sar    %cl,%edx

and `sar %cl` masks the count to five bits in hardware, so every value of the
field does something defined *on this processor*. C does not: `v >> n` is
undefined outside 0..31 whatever the target does.

Ours writes `v >> (n & 31)`, which reproduces the hardware exactly and costs
an `and $0x1f` the object does not have, at both sites. **The comment in the
source used to assert GCC folded the mask into the shift and that assertion
was wrong** -- `objdump` on the period-compiled object shows the `and` at
0x3ac9 and 0x3b55. It is recorded rather than removed because D561's
disposition stands: the reproduction keeps the value and the aliasing and
removes the undefined access, and a source that is undefined for inputs the
object survives is not a reproduction of it.

**Not corrected**: nothing to correct. At every count the object survives, the
two produce the same coefficient.
## D900 ⚠ The shipped library ignores `_tagModemParameters::paramFile` entirely

*Batch of 2026-08-17, from `Vparser_read_int` (blob 0x0b0990),
`Vparser_read_float` (0x0b09a0), `V90Parameters::loadParams` (0x027a00) and
`V92Parameters::loadParams` (0x0158e0).  **Reachability: every construction of
either parameter block with a non-null `paramFile`.**  **Observability: none
-- the two stubs write nothing, so both arms of the guard leave the object
identical, which `t_v90params` and `t_v90loadparams` measure on both sides.**
Status: verified bit-exact; reproduced as written.*

`_tagModemParameters` carries a `char *paramFile` at +0x78, both parameter
blocks' `init()` test it and call `loadParams` when it is set, and
`loadParams` then makes 295 (V.90) or 54 (V.92) calls naming every overridable
parameter in the class. **All 349 of them go to a three-byte stub.** Both
`Vparser_read_int` and `Vparser_read_float` are `xor %eax,%eax; ret`, and an
`awk` over `objdump -dr` of the whole of `.text` shows those two `loadParams`
members are their only callers anywhere.

So the released library accepts a parameter-file pointer, walks the entire
parameter list against it, and applies nothing. Whatever read the file was
removed before this object was linked -- the call sites, the names and the
target addresses all survived, which is why the field map is recoverable at
all (finding F860).

Not proposed as a fix and not fixable from the object: what the parser did
with the file is not in the object in any form, so writing one would be
invention with no oracle. Recorded because a reader who finds `paramFile` in
`modem_params.h` will otherwise assume it does something.

## D901 🐛 `loadParams` reads `BLL_TRN1_QC_SLOW_K2` into `SLOW_K1`, and +0x0f4 is the field it should have used

*Batch of 2026-08-17, from `V90Parameters::loadParams` (blob 0x027a00), calls
53 and 54 of 295.  **Reachability: any parameter file naming either
coefficient -- so, given D900, never in this build.**  **Observability: none
in this object, for D900's reason; in a build with a real parser it is a wrong
coefficient in the timing recovery's slow arm.**  Status: verified bit-exact;
reproduced exactly as the object has it, both calls to the same address.*

Finding F861 records four offsets that `loadParams` reads twice under two
names, and reads three of them as what they plainly are: a `GERMAN_PBX_`
override written after the name it overrides. **The fourth is not that shape
and is a defect in the original.**

```
    +0x0d8  BLL_TRN1_QC_INITIAL_K1 2e-4   +0x0dc  BLL_TRN1_QC_INITIAL_K2 0.0
    +0x0e0  BLL_TRN1_QC_FAST_K1    5e-4   +0x0e4  BLL_TRN1_QC_FAST_K2    7e-12
    +0x0e8  BLL_TRN1_QC_MEDIUM_K1  3e-4   +0x0ec  BLL_TRN1_QC_MEDIUM_K2  5e-12
    +0x0f0  BLL_TRN1_QC_SLOW_K1    1e-4   +0x0f4  unnamed_0f4            2e-12
                    ^ read TWICE, as _K1 and then as _K2
```

Four things converge and nothing dissents:

- three complete `K1`/`K2` pairs precede it and the fourth pair's `K2` slot is
  the only `unnamed_*` field anywhere in that run;
- `setToDefault` writes +0x0f4 with **2e-12f**, continuing the `K2` series
  7e-12, 5e-12, 2e-12 across FAST, MEDIUM and the slot in question. The
  `INITIAL` pair is quoted above but carries no weight either way: its `K2` is
  0.0f, which is a stage switched off rather than a term in the series, and its
  `K1` is 2e-4 rather than the largest, so the `K1` column is not monotone
  either. **The argument is the FAST/MEDIUM/SLOW triple and not a four-term
  progression**, and it is stated that way rather than made to look tidier;
- +0x0f4 is one of the nine `unnamed_*` slots finding F878 measured as a FLOAT
  declared `int` -- so it is a coefficient, not a count or a duration;
- +0x0f0 being read twice is otherwise unexplained, where the other three
  aliases explain themselves.

So the author wrote `Vparser_read_float(file, "BLL_TRN1_QC_SLOW_K2",
&BLL_TRN1_QC_SLOW_K1)` -- the name advanced and the address did not. The
consequences in a build with a real parser are that `BLL_TRN1_QC_SLOW_K2` in a
parameter file lands on `SLOW_K1`, silently discarding the value the file gave
`SLOW_K1` on the line before, and that the real slow `K2` at +0x0f4 cannot be
overridden at all.

**The field is NOT renamed and the alias is NOT unwound.** Both would be a
rule-3 usage inference sitting among 291 rule-1 measurements, which is exactly
what `V90Parameters.h`'s own header comment declines for the other fifty
`unnamed_*` slots, and the header's layout is frozen for the whole tree. The
reconstruction emits both calls against +0x0f0 because that is what the object
does; `t_v90loadparams` compares the two logs entry for entry and would fail
if it did not. What would settle the name outright is a reader of +0x0f4 --
`V90Jd` or whatever consumes the TRN1 quality-control betas -- naming it in a
diagnostic. That is CLAUDE.md's evidence rule 1, a format string that prints
the thing, and it is exactly how finding F3527 retired +0x074 and +0x078 in
this same class: `V90TRN2Design`'s own `%d` line named `maxUcode` and
`nofUcodesInTrn2`, and the slot beside them at +0x080 kept its offset name
because no string reached it. Nothing weaker should retire this one either.

## D920 🐛 `V92CP::evaluateInfo` reads the mask words back bit-reversed

`infoToBits` writes each sixteen-bit mask word LEAST significant bit first --
`*p++ = (unsigned char)(s & 1); s = (short)(s >> 1)` over `j = 15; j >= 0`, so
`bits[pos + 1]` carries bit 0 and `bits[pos + 16]` carries bit 15.
`evaluateInfo`'s `case 7` and `case 8` read the same sixteen positions with
`binaryTable[15 - i]` and `i` ascending, so `bits[pos + 1]` is given weight
2^15 and `bits[pos + 16]` weight 2^0. **A mask word does not survive a round
trip through the class**: `short_42[k][j]` comes back with its bits in the
opposite order from the one it went out in.

**Every other field DOES round trip**, which is what makes this an isolated
defect rather than a misreading of the object. `word_104`, `char_02`,
`word_28` and the five floats were all read out of `evaluateInfo` and checked
against `infoToBits` field for field, and all of them agree on the positions
and on the direction:

    word_104     bits[27 + i] weight 2^i, both                     agree
    char_02      bits[21 + i] weight 2^i, both                     agree
    word_28[k]   bits[103 + 4k + i] weight 2^i, both               agree
    flt_10       bits[52 + i] weight fltTable_2[15 - i], both      agree
    flt_14..20   bits[69 + i] weight fltTable_1[6 - i], both       agree
    masks        bits[pos + 1 + i] weight 2^i out, 2^(15-i) back   DISAGREE

**SETTLED: `evaluateInfo` IS THE WRONG SIDE.** Finding F6800 closed this against
ITU-T V.90 Table 14 and V.92 Table 23, which are identical here: "bit 137
corresponds to Ucode 0", with "Bit 0 is transmitted first" -- so the
first-transmitted bit of each sixteen-bit block is the LOWEST Ucode. That fixes
the WIRE only; what fixes the class's own storage is its consumer,
`getConstellationMask`, which fills these words as `mask[v >> 4] |= 1 << (v &
15)` -- bit `j` of word `k` is Ucode `16k + j`. Both agree, and both convict the
reader: `infoToBits` puts Ucode 0 on the first wire position, `evaluateInfo`
gives that position weight 2^15. The translation was verified faithful on both
sides first (.text+0x4f2f3 and +0x4f7f4), so this is the object's defect and not
ours.

**Reproduced, and not fixed**, per the rule above. `t_v92cpeval` drives it
directly with bit vectors whose two readings differ (`i & 1`, `(i >> 1) & 1`
and `i & 7` -- exactly the fills a symmetric round trip cannot tell apart) and
`t_v92cpb2i` drives it through a real message, so the behaviour is pinned
either way.

**Reachability unmeasured.** Whether a live V.92 session ever puts a non-zero
mask word on the wire depends on `setV92CPpckFromParamsInfo`, which fills the
two blocks and is not written yet. Finding F6603.

## D921 ⚠ The seven-position skip is an empty loop in the object

At the end of `evaluateInfo`'s `case 6` the object emits

    4f7bb:  48        dec  %eax          (%eax = 6 on entry)
    4f7bc:  79 fd     jns  4f7bb
    4f7be:  8d 6a 07  lea  0x7(%edx),%ebp

-- a seven-iteration countdown with nothing in it, beside a `lea` that adds
the seven in one instruction. That is what a source loop looks like after GCC
3.4.2 has strength-reduced the induction variable out of the body and declined
to delete the empty shell; the compiler has no pass that removes an empty
loop.

The reconstruction writes `word_124 += 7;`. The loop is unobservable -- it
touches no memory, and its only effect on `%eax` is dead -- so this is
"bit-exact, different structure", and an empty loop in the source would read
as a defect to every future reader. The cost is two instructions' worth of
`compare.py` in one function.

**What would overturn this** is evidence that the body was not empty in the
source: something the loop read and discarded. Nothing in the range suggests
one -- there is no load between the `dec` and the `jns` -- so it is recorded
as the compiler's and not as ours. Finding F6605.

## D922 ⚠ `evaluateInfo`'s mask arms keep the read cursor in a register; the object reloads it

The object reloads `word_124` from memory at the top of every one of the
sixteen inner iterations -- `mov 0x124(%edi),%eax` at 4f810 and 4f81e -- and
stores it straight back. Our source reads and writes the member and the
compiler keeps it in a register across the loop, because `short_42[k][j]` is a
`short` store and `word_124` is an `int` and type-based aliasing says the two
cannot overlap.

**They can, and `word_10c` is what makes them.** Nothing bounds it (D570), so
the destination walks forward out of its array:

    case 7   short_42[14][1] is +0x124, so word_10c >= 15 clobbers the cursor
    case 8   short_a2[8][1]  is +0x124, so word_10c >= 9  clobbers the cursor

At and above those the object's decoded mask value becomes the next read
position. One trial measured it landing on 22,899, which is 20 KB past an
object of 2,328 bytes; the reconstruction, holding the cursor in a register,
walks on undisturbed. **The two behaviours differ, and neither is defensible
as a behaviour** -- the object's reads unmapped memory and ours ignores a
store the object honours.

**Out-of-contract divergence, and the boundary is exactly the two numbers
above.** `word_10c` is set by `evaluateInfo`'s own `case 6` as one more than
the largest of six FOUR-BIT counts, so the largest value the class can give
itself is 16 -- which is already past `case 8`'s ceiling of 9 and one past
`case 7`'s of 15. So the boundary is NOT unreachable by construction and this
is a bound on the FIELD, not a proof about it: a `word_10c` above 8 has to
come either from a peer sending large counts in bits[103..127] or from
`setV92CPpckFromParamsInfo`, which is unwritten. **Reachability unmeasured**,
and the same for `case 7` above 14.

`t_v92cpeval` drives right up to the boundary on both arms -- fourteen groups
for `case 7`, eight for `case 8` -- so every aliasing target BELOW the cursor
is tested: the state word, both other cursors, `word_10c` itself and, for
`case 8`, `word_104` and `suv`. The object overwrites all of those while the
loop runs and carries on regardless, because its own loop bound and indices
are locals rather than fields, and that IS reproduced and IS green.

**What would close it** is forcing the reload in a way both compilers honour.
Nothing tried does so without either a `volatile` -- which changes codegen
everywhere the field is touched, including in `bitsToInfo` -- or a
type-punned access, which is undefined in its own right. The honest position
is that the reconstruction is exact up to the boundary and undefined past it,
which is where the object is too.

## D923 🐛 `V92CP::bitsToInfo` stores into `bits[word_11c]` at seven sites and guards none of them

Every one of the seven is a bare `mov %cl,0x129(%reg,%ebx,1)` with no compare
before it -- 0x4fa8a, 0x4fac8, 0x4fb30, 0x4fb58, 0x4fbc2, 0x4fc2a and
0x4fc9c -- and `word_11c` is advanced by one on every call in states 2 to 10
without an upper bound anywhere. `bits` is 2,000 entries; a stream that stays
in one collecting state past 2,000 positions writes through `crc`,
`vectorLen`, `msgLen` and `word_914` and then off the end of the 0x918-byte
allocation.

**THE SIBLING CLASS GUARDS AND SAYS SO.** `V90CP::bitsToInfo` carries
`cmp $0x2edf; ja` at five of its ten store sites and prints the author's own
`"\n *** error CP bit , not enouch memory in the buffer *** \n"` instead --
findings F4361 and D520, which record that five of ten are guarded and five are
not. So the author knew the failure mode, wrote a diagnostic for it, and
**none of it is in the V.92 class**: `V92CP` has no bound, no string, and no
arm to reach one from. The two classes were read from addresses 0x2180 apart
and this is the sharpest difference between them.

**Reachability: NOT from a conformant peer. This paragraph replaces an earlier
one that said the opposite** -- it claimed the bound was "reachable from the
wire and not only from a fault", which finding F6800 disproved.

ITU-T V.90 Table 14 and V.92 Table 23 both bound each of the six four-bit
constellation indices at bits 103:127 to **"an integer between 0 and 5"**.
`word_10c` is `max + 1`, so a legal peer yields at most 6, `gamma` and `delta`
at most 816 each, and a longest legal message of about 1,786 of the 2,000
entries -- confirmed independently by `t_v92cpb2i`, whose longest six-group
message measures 1,785. **No conforming exchange can reach the end of the
array.**

Reaching it needs an index of 6 to 15, which the recommendation forbids and
which nothing here rejects: `word_28[k]` is accumulated from four bits and fed
to `max` with no clamp anywhere upstream. So the exposure is to a MALFORMED OR
HOSTILE peer rather than a legal one -- still a real defect, and still the one
the sibling class guards against, but a materially weaker claim than the
original wording made.

**Reproduced, and not guarded.** Adding a bound would be a behavioural
difference on exactly the inputs that matter, and CLAUDE.md's rule is that a
defect recorded here is not quietly corrected in `src/`. `t_v92cpb2i` stays
inside the array on purpose -- its longest message is six groups in both
blocks, 1,785 of 2,000 -- and says so in its header. Finding F6600's batch.
## D780 ✅ `getSpectrumOfBin` indexes the spectrum with no bound at all

`V90SpectralVerifier::getSpectrumOfBin(unsigned long)` is fifteen bytes and
four instructions:

    45dc0  mov 0x4(%esp),%ecx      ; this
    45dc4  mov 0x8(%esp),%eax      ; bin
    45dc8  mov 0x1c(%ecx),%edx     ; this->spectrum
    45dcb  flds (%edx,%eax,4)
    45dce  ret

There is no compare, no clamp and no mask.  `spectrum` is
`sysdep_malloc(4 * (fftLength / 2))` and holds `fftLength / 2` floats, so any
argument at or above that reads past the allocation.

`getSpectrumOfNearestBin` is the same access with the index computed rather
than passed -- `spectrum[(unsigned)(freq / binWidth + 0.5f)]` at 0x45ec0 --
which puts the bound on the CALLER's frequency and on `binWidth`, neither of
which the function sees.  A frequency above `sampleFreq / 2` indexes past the
array, and a negative one converts to 0x80000000 through the object's
`fistpll`, which is undefined in C and reads the low dword as zero on this
target.

**Reproduced, and the reconstruction adds no guard.**  Adding one would make
the two disagree for exactly the callers that need reproducing, which is the
one thing this tree may not do.

**NOT DRIVEN OUT OF RANGE, deliberately.**  `t_v90specacc.cpp` computes every
quotient it drives and marks each row `indexable` or not; the negative and
huge rows go through `freqToLeftBin`, `freqToRightBin` and `freqToNearestBin`,
which return the number without subscripting anything, and never through the
two `getSpectrumOf*` entry points.  D561's rule: a trial that reaches
undefined behaviour in the reconstruction is not a differential trial, so the
out-of-range access is described here rather than executed.

**Not corrected**: the guard would be a behavioural difference, and the
callers inside the object -- `checkSpecialSpectralConditions`'s seven probes
-- are bounded by parameter values rather than by anything the class checks.

## D790 ✅ `setV92CPpckFromParamsInfo`'s mask loops write across `short_42` into `short_a2`, and the codec loop's last group overwrites the field bounding it

`getConstellationMask` clears eight words and then sets `mask[b >> 4]` with no
mask on the nibble, so a constellation byte of 0x80 or more addresses entries
8 to 15 of a buffer it was handed as eight.  `V90MappingParams.h` records that
for the out-of-line function and `t_v90cmask`'s `run_masks` drives it into a
24-entry buffer with eight guard entries.

`setV92CPpckFromParamsInfo` calls it -- inlined, but the arithmetic is the
same -- with a ROW of `V92CP::short_42` and then of `short_a2`, which are
eight words each and abut.  So the overflow is not into guard space:

| the write | where it lands |
|---|---|
| `short_42[i][8..15]`, i < 5 | `short_42[i + 1]` |
| `short_42[5][8..15]` | `short_a2[0]` |
| `short_a2[i][8..15]`, i < 5 | `short_a2[i + 1]` |
| `short_a2[5][8..15]` | `pad_102`, `word_104`, `suv`, **`word_10c`** |

The last row is the interesting one.  `word_10c` is what BOUNDS both loops and
the object re-reads it from memory on every iteration (`movzwl 0x10c(%ebp),%esi;
cmp; ja` at .text+0x33b28 and .text+0x33be7), so the codec loop's last group can
change its own trip count -- upwards as easily as downwards.  It also
overwrites `suv`, which the same function set from the record eighty
instructions earlier, and `word_104`, which `V92CP::setSUV` owns.

**REPRODUCED, NOT FIXED**, and it is reproduced by construction rather than by
choice: the source is `getConstellationMask(params, (int)i, cp->short_42[i])`,
which is what the object inlines, and the overflow is entirely inside the
callee.  Both sides do the same thing to the same bytes.

**AND IT IS NOT DRIVEN, WHICH IS THE OTHER HALF OF THE ENTRY.**  Writing past
a row of `short short[6][8]` is out of bounds in OUR source as well as the
object's, and D561's ruling is that a differential trial reaching undefined
behaviour in the reconstruction is not a differential trial.  So
`t_v90cmask`'s `run_pack` sweep uses only the two byte alphabets that cannot
produce a byte at or above 0x80 -- mode 2, masked below 0x80, and mode 3,
`((id + n) & 3) * 0x11` -- and the overflow is recorded here with the offsets
worked out rather than exercised.  `run_masks` still drives the same overflow
against the same callee, into a buffer sized for it, so the BEHAVIOUR is
covered where it can be covered safely; what is not covered is what it does to
a `V92CP`.

Closing it needs the mask region modelled as one addressable block -- the
shape `v92-fold-oob` gave D561's own site -- which is a `V92CP` layout change
and belongs to a batch that owns that header.  D570 is the same class of
finding on the same two blocks from the reading end.

## D791 ✅ `V90CPPacker`'s mask buffer is sixteen words where the object's stack slot is eight, and the object's overspill is inert

The same `getConstellationMask` overflow as D790, reached from the OTHER
caller, and this time the destination is a LOCAL rather than a field -- which
is what makes it closable instead of merely recorded.

`getConstellationMask` clears eight words and then sets `mask[b >> 4]` with no
mask on the nibble, so a constellation byte at or above 0x80 addresses entries
8 to 15 of a buffer it was handed as eight.  In the object, `V90CPPacker`'s
buffer is `%esp+0xf0` and the sixteen-entry CRC register is `%esp+0x100`
immediately above it, so entries 8..15 land exactly on `crc[0..3]`:

| the write | where it lands |
|---|---|
| `mask[0..7]` | the eight words the function reads back |
| `mask[8..15]` | `crc[0]`, `crc[1]`, `crc[2]`, `crc[3]` |

**AND IT IS INERT, WHICH IS MEASURED AND NOT ASSUMED.**  Both mask loops --
the constellation one at .text+0x3c1fc and the codec one at .text+0x3c2f8 --
finish before the CRC register is seeded at .text+0x3c404, which writes all
sixteen entries unconditionally.  Nothing reads `crc[]` in between, and
nothing reads `mask[8..15]` ever: the extraction loop runs `j` from 0 to 7.
So the object's own overspill cannot reach the message.

**OUR LOCAL IS `short mask[16]`.**  Same behaviour, because only the low eight
are read and the CRC is seeded afterwards on both sides; but the write is in
bounds in our source, which is what lets the case be DRIVEN rather than
avoided.  That is the whole difference from D790, whose destination is a row
of `V92CP::short_42` and cannot be widened without a layout change to a header
that batch does not own.  `t_v90cmask`'s `run_packer` therefore sweeps all
four byte alphabets, including the two that guarantee a byte at or above 0x80,
and counts that it reached one -- where `run_pack` beside it must exclude
them under D561.

Nothing is hidden by the wider local.  A reconstruction that read
`mask[8..15]` would be caught by the differential, because the object's
entries 8..15 hold whatever the previous group left in `crc[0..3]` and ours
hold whatever the previous group left in `mask[8..15]`; the two are different
values and the two sides would disagree the moment either was used.

## D930 🐛 `V90SpectralShaper` reads an uninitialised action, and runs a four-billion-iteration loop, on inputs its own tables and `reset` cannot produce

*Renumbered from D800 on merge.*  The `v90-spectral-reneg` branch measured the
deviation high-water when it was cut; `v92-modulator-tail` took D800 for
`V92Modulator::resamplerPhaseOffset` while that branch ran.  Both measurements
were right when made -- a number cannot be reserved across a branch's lifetime,
only claimed at merge.  See finding F6100.

Two undefined-behaviour sites in one class, both reproduced and neither driven.

**ONE -- the leading digit's switch has no default, and the local it writes is
read either way.**  `advanceTrellis` recovers the winning candidate's leading
decimal digit as `bestAction / pow10Table[shaperId]` and converts it to an
`ACTIONS` with a four-armed switch (0x32d39..0x32ed3).  Digits 1, 2, 3 and 4
store 0, 1, 2 and 3 into a stack slot; **anything else falls through to
0x32d4f with that slot never written**, and the two switches that follow read
it -- one to pick a polarity pattern for the outgoing frame, one to update the
trellis state.  `act = leading - 1` would have been a single `dec` and the
object has four separate constant stores, so the switch is the source's, and
a `default:` arm would be code the object does not contain.  Reproduced by
writing no default and no initialiser.

It is unreachable through the object's own data: finding F5853 regenerates all
128 entries of `actionLookupTable` from the digit rule and every digit in every
live entry is in 1..4, while the dead entries are zero and are never selected
because the candidate loop runs to 2^(shaperId+1).  It becomes reachable only
if `shaperId` exceeds 3, which walks off the table's eight rows first, or if
the table is corrupted.

**TWO -- `process` computes an unsigned bound as `blockLength - 1`.**
`lea -0x1(%ecx),%ebx; cmp $0x0,%ebx; ja` at 0x32fd8 copies the caller's sign
bits into `frameBits[1..blockLength-1]`.  With `blockLength` zero the
subtraction wraps to 0xffffffff and the loop runs to four billion, writing
through the object and everything after it.  `reset` can produce a zero
`blockLength`: it is `6 / shaperSR` with an explicit guard storing 0 when
`shaperSR` is zero (0x3286c), and `6 / shaperSR` is also 0 for any
`shaperSR` above 6.  So the value is reachable from the parameter block, and
only the caller's choice of `shaperSR` keeps it out.

**NOT DRIVEN, AND THAT IS D561'S RULE.**  Both sites are undefined in OUR
source as much as in the object's, so a trial reaching either is not a
differential trial. `t_v90spectrellis.cpp` and `t_v90shapeact.cpp` therefore
hold `shaperId <= 3` and `shaperSR` in {1, 2, 3, 6}, which gives
`blockLength` in {6, 3, 2, 1} and never zero, and the exclusion is stated in
both files' headers with the reason. `t_v90shapereset.cpp` DOES sweep
`shaperSR` at 0, 7, 12 and 0xffffffff, because `reset` itself is total and
storing a zero `blockLength` is the behaviour under test there; what no suite
does is call `process` afterwards.

Closing either needs a `default:` arm and a guard that the object does not
have, so neither is fixed. Anyone linking this library for real should bound
`shaperSR` to 1..6 at the parameter block, which is where the constraint
actually lives.

## D940 ⚠ The three phase 4 message sources return an uninitialised `short` on two of `V90BitsToSymbol::process`'s arms

**Where:** `src/pump/v90/V90Phase4Modulator.cpp`, `generateMP`, `generateCPd`
and `generateSUVd`; blob 0x2dbd0, 0x2e540 and 0x2e5f0, 167 bytes each.

**What the original does:** each reserves a two-byte stack slot, hands its
address to `V90BitsToSymbol::process(unsigned int &nofBits, short
*outSymbols)`, and returns what comes back sign-extended:

    2dbec:  8d 54 24 22   lea    0x22(%esp),%edx      ; &symbol
    2dbf4:  89 54 24 08   mov    %edx,0x8(%esp)
    2dc02:  e8 ..         call   V90BitsToSymbol::process(unsigned int&, short*)
    2dc07:  0f bf 44 24 22 movswl 0x22(%esp),%eax     ; read straight back

Nothing writes `0x22(%esp)` before the call in any of the three, and `process`
writes through that pointer only when it drains a whole block: with
`symbolsBlockSize` zero it reports SIZE_NOT_SET, and on the BUFFER_UNDERFLOW
arm it returns without touching `*outSymbols` (finding F7520 has the arm order,
and it is the object's own: `symbolsDone < symbolsBlockSize` is tested first).
So there are two reachable configurations in which the returned symbol is
whatever was on the stack.

**This is D661's shape on the OTHER parameter and D661 itself does not cover
it.** D661 is about the `unsigned int &nofBits` these same two callees read
back in `generateDataSymbolBeforeFPE`/`BeforeRRN`; this is about the `short
*outSymbols`, in three different members, and the arm that leaves it alone is
the underflow one rather than the size-not-set one.

**Reproduced rather than guarded.** The reconstruction leaves the local
uninitialised exactly as the object does; the differential test seeds the slot
identically on both sides so the trials still compare, which is why no test
fails on it.

**Status:** unmeasured. All three functions are callerless in the blob (zero
relocations of any kind name them), so no caller exists whose behaviour could
be affected, and whether the vendor's intended caller would have reached
either arm cannot be known from this object.

## D480 ⚠ `FSE_getdiag` throws away a scatter log that is filled exactly to capacity

Its `which == 0` arm guards the waiting count against `FPM_FSE_DIAG - 1` and
discards everything above it:

    a7d46: mov  0x4e0c(%esi),%edx      ; diag_n
    a7d4c: cmp  $0x1df,%edx            ; 479
    a7d52: jg   a7d97                  ; -> zero the count, return 0

`FPM_FSE_receive` fills `diag` to 480 entries -- its own guard is the same
`> FPM_FSE_DIAG - 1`, and it RESETS rather than wrapping -- so a count of
exactly 480 is reachable between two receive calls, and 480 points of
constellation display are dropped instead of delivered. The `which == 1` arm
has no capacity guard at all.

`unmeasured`: the log is a diagnostic display and nothing in the datapump reads
it back, so a dropped block costs a frame of a scatter plot and nothing else.
`test/unit/t_v32fpsub.c` drives 478, 479, 480 and 481 and holds the
reconstruction to the object at each.

## D481 ⚠ `V32FP_delete` passes a second argument to five deallocators that take one

Finding F8215 has the disassembly. Before each of `FPM_FSE_free`,
`FPM_SRE_free`, `FPM_ECC_free`, `FPM_MRF_free` and `FPM_PPS_free` the object
stores a literal 1 into the outgoing area's second slot; all five callees read
only the first. GCC does not emit dead stores there, so the author's
declarations for these five had two parameters and this tree's have one.

`src/pump/v32/v32fpctl.c` makes the one-argument call, so the reconstruction
emits five fewer instructions than the object at those sites. Invisible to the
differential tier -- the argument is never read -- and visible to the codegen
tier, where it is five `mov $1` / `mov %reg,0x4(%esp)` pairs. Repairing it means
changing five prototypes in five headers, which is a change to files outside
this batch.

`unmeasured` in the sense that nothing establishes what the 1 MEANT. It is not
`unmeasured` about whether it is there.

## D482 ⚠ `RxClampV32`'s cursor is a `short` tested against -1, so a negative block length writes 65,535 words

    825fc: movswl 0x9e(%ebx),%eax    ; the block length
    82603: dec    %eax
    ...
    82611: movswl %ax,%ecx
    82614: inc    %ax
    82616: jne    82606              ; body

The loop runs from `n - 1` down to 0 and stops when the 16-bit cursor reaches
-1. A length of 0 writes nothing, which is right; a NEGATIVE length starts below
-1, wraps through -32768 to 32767, and writes 65,535 words before it reaches the
sentinel -- 128 KB past whatever buffer the caller supplied.

Not reachable through anything read so far: the eleven `RxHdx*` states that call
it all pass the same +0x9e that `RxHdxNull` accumulates as a positive sample
count. Recorded because the truncation is what distinguishes the object's loop
from `for (i = 0; i < n; i++)`, and **it is MEASURED rather than asserted** --
`test/unit/t_v32fpctl.c` drives a length of -1 into a buffer sized for it and
compares all 65,535 words against the object's.

## D483 ⚠ `CalcTurnAroundDelay` narrows to sixteen bits before it clamps, so a large underflow comes back positive

    83b07: sub  %edx,%eax     ; budget - (three charges)
    83b09: cwtl               ; <- narrowed here
    83b0a: mov  %eax,%edx
    83b0c: not  %edx
    83b0e: sar  $0xf,%edx
    83b11: and  %edx,%eax     ; x < 0 ? 0 : x
    83b14: cwtl

The four fields are 16-bit and the subtraction is done at 32 bits, but `cwtl`
takes the low half BEFORE the branchless clamp. A budget that undershoots by
more than 32,768 therefore wraps to a positive number and is reported as
surplus turnaround time rather than as none.

`unmeasured`: the four fields are timing quantities in symbols and nothing read
so far puts a charge anywhere near 32,768. `test/unit/t_v32fpctl.c` sweeps the
budget and the charges across the sign boundary and holds the reconstruction to
the object either side of it.

## D490 -- `FPM_AGC_agc` is called with four arguments and defined with three  `unmeasured`

Every V.32 call site pushes a fourth outgoing slot holding the constant 1 --
`DemodDataV32` at 81ce6, `RxHdxTone` at 83937, `RxHdxNoSignal` at 83a37 -- and
`FPM_AGC_agc` (0xa6750, 566 bytes) reads only 0x50, 0x54 and 0x58 off its
frame, never 0x5c. The two declarations that must exist to produce this are a
four-parameter prototype in the caller's translation unit and a
three-parameter definition; the extra argument is pushed and ignored, so
nothing observable depends on it.

Not V.32's alone: `DemodDataV17`, `Detect_v22` and `v23FP_rx_progress` do the
same, and `src/pump/v23/bwchdem.c` and `src/pump/v22/v22data.c` already record
it at their own call sites. `src/pump/v32/v32demod.c` follows them and passes
three, which costs one instruction against the object and is the whole of the
deviation. Adding a fourth parameter to `fpm_agc.h` would move the code
generation of every other caller in the tree to fix one dead store, so it is
not done. See finding F8234.

## D491 -- `DemodDataV32` masks three of its four equaliser enables with the AGC's flag and leaves the fourth bare  `unmeasured`

Four `int`s in the datapump block are copied into named enables of the timing
recovery and the equaliser, and three of the four are ANDed with `agc.f18`
first:

    sre.adapt   = fp[0x04] & agc.f18       81d26
    fse.pll_on  = fp[0x08] & agc.f18       81d7f
    fse.tilt_on = fp[0x0c] & agc.f18       81e50   (mode 6, timing mode != 0)
    fse.lms_on  = fp[0x10] & agc.f18       81e58   (same arm)

but on the arm taken when `hdx->mode` is NOT 6, the object writes
`fse.tilt_on = 1` and `fse.lms_on = fp[0x10]` -- `mov 0x10(%edx),%ebp` at
81d8e followed by `mov %ebp,0x250(%edx)` at 81d9c, with no `and %ebx`. Every
sibling assignment in the function carries the mask and this one does not.

The asymmetry is codegen-visible, so it is reproduced rather than repaired:
`src/pump/v32/v32demod.c` writes the bare assignment with a comment naming this
entry, and `test/mutations/v32demod.json` carries the repair as a mutation
("D491 undone") which the differential test catches. Whether the author
intended the equaliser's coefficient adaptation to survive the AGC's gate
outside mode 6, or omitted the mask, cannot be told from this object.

**Status:** unmeasured. Reaching it needs `hdx->mode != 6`, a live carrier and
`agc.f18` clear at the same time; the differential test constructs exactly that
and both sides agree, but whether a running V.32 modem produces the
combination is a question about `V32FP_control` and the handshake, neither of
which is reconstructed.

## D492 -- `hdx->mode` reaches 6 and `V32NextState` has six slots, the last of them unrelocated  `unmeasured`

`V32NextState` (.data 0x76cc) is 24 bytes -- six pointers -- and the object
relocates five: `V32OrgNextState`, `V32AnsNextState`, `V32LocLoopNextState`
twice, and `V32RngInitNextState` at indices 0 to 4. Index 5 carries no
relocation. The table is indexed by `hdx + 0x76`, loaded `movswl` and used
directly: `call *0x0(,%edx,4)` at 7fd9b in `TxHdxTone` is one of several.

`V32FP_modem` (82735) writes **6** into that field, together with
`hdx->state = 0x22` (`V32_STATE_DONT_CARE`), when bit 0 of the instance's
status byte is set. Six is one past the last slot the table has, so a dispatch
taken with the mode at 6 would call through `.data + 0x76e4`, outside the
symbol.

This is the shape of D1 and D4 -- a table whose own index expression can reach
past its last entry -- and it is recorded on that resemblance and nothing more.
`DemodDataV32` only COMPARES the field against 6 and never indexes with it, so
the reconstruction is not affected; the dispatch sites belong to the `TxHdx*`
states, which are not written. **Whether mode 6 can be live at a dispatch is
not measured and is not claimed**: `blobfix.md`'s warning applies with force,
because this would be a missing DECISION rather than a missing value if it is
real, and index 5 being NULL suggests the table's tail is deliberately inert.

## D493 -- a V.32 transmit state can return a negative sample count and walk the output pointer backwards  `unmeasured`

`V32TxHdxModem` sign-extends the state's return with `cwtl` (7fd25) before both
`lea (%esi,%eax,1),%edx` and `lea (%ebx,%eax,2),%ebx`, so a state returning a
negative count subtracts from the running total and moves the output cursor
DOWN by twice that many bytes -- below the buffer the caller supplied. Nothing
bounds it and no store is guarded.

No state does: every `TxHdx*` exit read so far returns `hdx->sample_len`, which
`V32FP_control` and `V32FP_recreate` fill from `V32_SAMPLE_LEN`. So this is a
property of the arithmetic, not a live fault, and it is recorded for the same
reason D430 is -- the sign extension is what distinguishes the object's
expression from an unsigned one, and `test/unit/t_v32hdx.c` drives a state that
returns -20 and -50 over a guard region so the reconstruction is held to it.

**Status:** unmeasured. Establishing whether any real state can return a
negative count needs the twenty states, none of which is reconstructed.

## D404 ⚠ `RateToSeq` indexes `V32_RATE_SEQ` with its argument and does not bound it

```
   821e0:  0f bf 44 24 08          movswl 0x8(%esp),%eax
   821e5:  0f b7 84 00 00 00 00    movzwl 0x0(%eax,%eax,1),%eax  <== V32_RATE_SEQ
   821ed:  c3                      ret
```

Three instructions, no compare. The table is 14 bytes — seven shorts — and the
argument is a signed `short`, so any value outside 0..6 reads elsewhere in
`.data`. The neighbours are `V32_FINAL_RATE_SEQ` two bytes below and the
padding above, so a small overrun returns a plausible rate signal rather than
faulting.

The first argument is a modem instance and **is never read**, which is the
other half of why there is no guard: the function has nothing to validate
against.

**Reproduced.** `src/pump/v32/v32seq.c` has no bound either, and the
differential test deliberately does not sweep out of range — the two tables sit
at different addresses in the two objects, so an out-of-bounds read is not a
comparison of anything.

**Status:** unmeasured. Every in-object caller of `RateToSeq` is unwritten
(`V32OrgNextState` and its three siblings), so whether any of them can produce
an index outside 0..6 is a question for the pass that writes them.

## D401 ⚠ `InitGenSequence` divides by its width argument with no test for zero

```
   8368d:  31 d2                   xor    %edx,%edx
   8368f:  f7 f1                   div    %ecx
```

`%ecx` is the fourth argument zero-extended from 16 bits, and nothing between
the load and the divide compares it. A width of zero raises `#DE` and the
process dies.

The same argument then becomes the shift count of `mov $1,%eax; shl %cl,%eax`,
so a caller passing zero would in any case get a field mask of zero.

**Reproduced.** The reconstruction divides in the same place with the same lack
of a guard. `DSPLIB_REPRODUCE_BUGS` is not involved: nothing here is fixed, so
there is nothing to fence.

**Status:** unmeasured, and the reason is D404's. The callers are the unwritten
next-state functions.

## D402 ⚠ Two variable shifts in the generator can exceed the width of their type

`InitGenSequence` computes `(1 << width) - 1` and `GenSequence` computes
`pattern >> (index * width)`. Both are `shl`/`sar` with the count in `%cl`,
which x86 masks to five bits, so the object's behaviour for a count of 32 or
more is a shift by count mod 32. In C that shift is undefined.

The reconstruction writes the shift and does not mask it, so GCC emits the same
instruction and the same thing happens — but it is *undefined*, not *defined to
match*, which is why it is recorded rather than left implicit.

**Not reachable with the object's own configurations.** `index` is bounded by
`total / width - 1` and `width` by the field layout of a 16-bit pattern word, so
`index * width` stays under 16 for every call the handshake can make; the
literal 16-bit rate signals V32_RATE_SEQ holds are what those calls carry.

**Status:** unmeasured, for D404's reason.

## D403 ⚠ `DetSequence`'s bit counter is a `short` and its bit width is an `unsigned short`, so a width above 32767 never terminates

The inner loop counts `%esi`, sign-extended from 16 bits at every step —

```
   8384e:  8d 46 01                lea    0x1(%esi),%eax
   83851:  0f bf f0                movswl %ax,%esi
   83854:  3b ee                   cmp    %ebp,%esi
   83856:  7c a8                   jl     83800
```

— against `%ebp`, which is `movzwl 0x54(%edi)`, the detector's bit width **zero**
-extended. So the bound reaches 65535 and the counter wraps to -32768 at 32767:
the comparison is true again and the loop restarts, for ever. Nothing else in
the loop changes, so it is a true non-termination and not merely a long run.

**Reproduced**, `short bit` against `int nbits`, and it is not hypothetical:
this was found because a mutation that pointed the detector at the *generator's*
width field — which the test fixture filled with a pseudorandom value — hung and
was recorded `caught (hang)` rather than caught by a check. The fixture now pins
that field small so the mutation fails a check instead; the deviation is what is
left.

**Status:** unmeasured. `InitDetSequence` is the only writer of the field and
takes it from its caller, and every caller is unwritten. The widths the V.32
handshake actually uses are the number of bits per received word, which is
single figures.

## D405 ⚠ `GenerateAnsTone`'s two phases end on DIFFERENT comparisons

The tone phase:

```
   8684a:  8b 43 0c    mov  0xc(%ebx),%eax        ; elapsed
   8684d:  01 f0       add  %esi,%eax             ; + count
   8684f:  3b 43 10    cmp  0x10(%ebx),%eax       ; vs TONE_LEN
   86852:  7c 1f       jl   86873                 ; continue if BELOW
```

The silence phase, forty bytes earlier:

```
   8680b:  8b 43 0c    mov  0xc(%ebx),%eax        ; elapsed
   8680e:  01 f0       add  %esi,%eax             ; + count
   86810:  3b 43 14    cmp  0x14(%ebx),%eax       ; vs SILENCE_LEN
   86813:  7e 5e       jle  86873                 ; continue if AT OR BELOW
```

`jl` against `jle`. So a tone whose accumulated count lands exactly on
`TONE_LEN` has ENDED, and a silence whose accumulated count lands exactly on
`SILENCE_LEN` has NOT: the silence runs for one further block, and therefore
for `SILENCE_LEN + 1` samples or more where the tone runs for `TONE_LEN` or
more. With the two lengths set equal — which is the obvious configuration —
the silence is one block longer than the tone.

**Reproduced**, `>=` for the tone and `>` for the silence, and the differential
test lands a case exactly on each boundary so that the asymmetry is checked
rather than assumed. Mutating either comparison into the other's is caught.

**Status:** unmeasured, and unmeasurable from this object: `GenerateAnsTone`
has no caller anywhere in `dsplibs.o`, so no configuration of `TONE_LEN` and
`SILENCE_LEN` exists to say whether one block matters. Whether this is a
defect or a deliberate guard band cannot be decided here.

## D406 ⚠ `GenerateAnsTone` discards the overrun at every phase change

Both ending arms write a literal zero to the sample counter --
`movl $0x0,0xc(%ebx)` at 0x86815 and 0x86854 -- rather than the excess over
the phase's length. So a phase that ends on a block overshooting its length by
N samples starts the next phase at 0 and the N are lost; the cadence drifts
later by up to one block per phase change.

**Reproduced.** Carrying the overrun instead is one of the suite's mutations
and is caught, so the reconstruction is pinned to the object's behaviour and
not merely compatible with it.

**Status:** unmeasured, for D405's reason -- no caller exists. Note that with a
`count` that divides both lengths exactly, the overrun is always zero and the
deviation is inert; the differential test drives cadences where it is not
(150-sample phases in 40-sample blocks) as well as where it is.

## D950 ⚠ `FPM_phasor_dp` halves the sum of its two fractional fields, and nothing explains the halving

The double-precision phasor's advance is

    acc = ((phase + inc) << 15) + ((frac_phase + frac_inc) >> 1);

and the `>> 1` is the object's `sar $1,%ebx` at 0x0a949e -- a single arithmetic
right shift on the SUM, after the addition and before it joins the accumulator.

**Why it is a deviation entry rather than a comment.** The accumulator's two
halves are fifteen bits each: `phase` is stored as `acc >> 15` and
`frac_phase` as the remainder against `1 << 15`. So the fractional field's
natural weight is one part in 32768, and a fractional increment joining the
accumulator ought to join it unscaled. The halving means the effective
increment is `inc + frac_inc/2` rather than `inc + frac_inc`, i.e. the
fractional half runs at HALF the rate the split implies, and one unit of
`frac_inc` is 1/65536 of a phase unit rather than 1/32768. Nothing in the
object says why. There is no scaling constant, no compensating doubling
anywhere in the function, and no caller: `readelf -rW` finds no relocation
against `FPM_phasor_dp` in 1.2 MB (F8320), so there is no configuration to
read the intended units off.

Three readings fit and the object cannot separate them: the two fields are
Q16 and the shift converts them to the accumulator's Q15; the shift is a
guard against the sum overflowing a short, which it can (`0x7fff + 0x7fff`);
or it is a defect and the author meant `>> 0`. **Naming the fields for any of
them would be naming on inference, so they stay `frac_phase` and `frac_inc`,
which is what the arithmetic establishes and no more** -- CLAUDE.md's rule,
and F8168's own warning against reading a Q-format into them.

**Reproduced**, and pinned: `test/mutations/fpmphasordp.json` carries five
mutations of this one expression -- no halving, halved twice, only one operand
halved, a division instead of a shift, and the operands read unsigned -- and
`t_fpm_phasordp` catches all five.

**Status:** unmeasured, and unmeasurable from this object, for D405's reason:
the function has no caller, so no configuration exists that would say which
reading is the author's.

## D951 ⚠ All eight fax message reporters guard their table with its own entry count

`v17rx_message` and its seven siblings answer `*out = TABLE[code]` under a
guard of `(unsigned)code > N` -- and N is the table's ENTRY COUNT, not
count-minus-one, in every one of the eight (`cmp $0xa` over ten entries,
`cmp $0x7` over seven, `cmp $0x6` over six, `cmp $0x8` over eight, read off
0x09c240..0x09cad0). `code == N` therefore reads one pointer past the table
and hands the caller whatever `.data` the link put there. Reproduced
verbatim in `src/fax/class1tx.c`; `t_class1leaves` pins every in-range and
above-range code and SKIPS exactly the one-past code, because the byte the
two links placed after the table is not the same byte and no equivalence is
defined over it.

**Status:** unmeasured. Whether any caller can present `code == N` is a
question about FAXVMI's callers, which are not reconstructed.

## D952 ⚠ `FIFO_full_test` computes its occupancy fraction in a short that wraps at count 2

The test is `(count << 14) / size >= 0x399b` -- 90% in Q14 -- but the
numerator is truncated to a SHORT before the divide (`shl $0xe; cwtl` at
0x096dd0): a count of 1 gives 16384, a count of 2 gives -32768, and every
count above 1 therefore answers "not full" through a negative quotient for
any positive size. With a positive size the only input that can answer 1 is
count == 1 with size == 1. Reproduced in `src/fax/fifo.c`; `t_class1leaves`
sweeps counts against positive and negative sizes.

**Status:** unmeasured. What `count` means at +0x0c (bytes? frames?) is
`FIFO_create`'s to settle; if it counts something that can only be 0 or 1
the wrap is unreachable.

## D953 ⚠ `SGD_sequence_det` reports a stack local it never wrote when no alignment scores

The best-alignment index lives in a stack slot that is only written when a
trial distance beats the running best (initialised to 0xffff). When no
trial does -- `n == 0`, or every distance saturating to 0xffff -- and the
acceptance test still passes (0xffff reads as -1 through `movswl`, so any
non-negative threshold accepts it), the object stores `buf + base +
GARBAGE` into its status block and returns the garbage as the match index.
The reconstruction initialises the slot to 0 (`src/fax/sgd.c`), which is
observable only on exactly that degenerate path; `t_faxsgd` does not drive
it, because there is nothing defined to compare there.

**Status:** unmeasured, and unmeasurable against the object on the affected
path -- the value compared would be the blob's stack residue.

## D954 ⚠ `pack_next_bit` enters its byte collector with the bit position backdated, not cleared

The framer's seizure state (3) counts consecutive spaces and, on reaching
`f028 + 1` of them, enters the collector state (2) -- but it writes the
counter BACK TO f028 and that same field is the collector's bit position,
so the first byte after seizure collects only bits f028..7 and keeps zeros
below (the accumulator was cleared entering state 3, and is NOT cleared on
the 3->2 transition -- only the 1->2 transition clears it). With
`create_cid`'s seed of 2 the first byte holds six live bits. Reproduced in
`src/service/rxcid.c` and exercised by `t_cidleaves`' streams.

**Status:** unmeasured. Whether the first post-seizure byte ever reaches
`cid_get_strings` is `cid_modem`'s question, and it is not reconstructed.
