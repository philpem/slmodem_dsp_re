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
1200). `make blobfix BLOBFIX="--fix D1"` weakens `FPM_sqrt_table` and links a
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
figure in finding 40 was measured on a Bell 103 capture, and that made it easy
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
rather than safe. Finding 40 and the caller counts above are the evidence;
`tools/relocscan.py`-style reasoning applied to `readelf -rW` is how they were
obtained.

**Table derivation**, exact for all 128 entries:
`table[i] = trunc(2^30 / ((i + 0x80) * 0x100))` — truncated, like the sine
tables; rounding differs on 58 of them.

**Now fixable IN THE BLOB, opt-in** (task #99, `docs/blobfix.md`, finding
1201). "Reproduce, do not fix" above governs `src/` and is unchanged — the
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
make `FPM_AGC_agc`'s shift go negative (finding 29).

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
| `.data:0x7664` / `0x7660` (global) | 32604 | 2277 | 34881 | **1.064** ✗ |

The four wrong ones all carry α = 32604, which is `32768 - 164` — the value
that belongs with the β of the `0x77b8` pair. Someone copied that TU's α while
changing β. The intended values are plainly `32768 - β`: 31130 for β = 1638,
30491 for β = 2277.

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
(finding 35). `B103FP_create` clears that buffer in the branch that installs
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

This also explains what the parameters recovered in finding 44 select.
`GetDialToneCallProgressFilterIndex`, `GetBusyToneCallProgressFilterIndex`,
`GetRingbackToneCallProgressFilterIndex` and
`GetCongestionToneCallProgressFilterIndex` choose *which* design a given tone
is detected with, and `GetDialToneFilterSubindex` chooses which of the seven
variants within a `Filter_*` family. The tables are not leftovers; they are a
per-country filter bank, and the homologation data picks from it.

### The rule this earns

Twice in this phase a negative result from a hand-written `readelf`/`objdump`
query has been believed: once for relocations inside the toneiir configuration
(finding 46), once here. **A query that returns nothing must be shown to
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
land within 0.06% of nominal (finding 58) and the calling tone emits a
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

`FP_Pow` is exp() in Q14 (finding 9), so turning decibels into a linear ratio
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
output by 16 by truncating to 16 bits and then shifting. Finding 90 has the
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
fails rather than passes.

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
216.

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
122.  This is the second retracted deviation after D28, and both were filed
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
print the readable message instead of the encoded one.  Finding 175 is why
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
reconstructed. Compare finding 129, which is the same shape in
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
cumulative count would pass `INT_MAX` (finding 182) -- 69, 58 and 56
entries.  Everything above that keeps `preinitV34`'s fill of -1.

**What we do:** the same.

**Reachable?** The sizes are, and often: `MMaxTable` produces all three.
Whether `shellDemapper` then INDEXES into the -1 region is **unmeasured** --
it reads `t3[d1 + d2]` with nothing bounding the sum, which is finding 129.
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
`dftenergy` writes `shift`, and finding 212 established that the neighbouring
`energy` genuinely does go negative from a seeded accumulator, so "a negative
shift cannot happen" is not something this reconstruction can assert.

**What we do:** reproduce both widths. The sweep leaves one shift in
thirty-two wide open rather than narrowed towards zero, so negative ones occur
and the comparison is tested.

**Reachability:** reached in the sweep. Whether a real probe produces a
negative `shift` is not measured — that is a question about `dftenergy`'s
inputs. `unmeasured` — task #47.

**How it was found:** by the two different widening instructions on the two
sides of one comparison, which is the same thing finding 212 records for the
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
`test/harness/v34hsstep.c` arms a `SIGALRM` around every step. Finding 287.

## D60 ~~⚠ The per-sample transmit loop's result is not a function of the object~~ RETRACTED

`unmeasured` for what it actually reads.

Stepping one sample of the per-sample transmit loop from two objects holding
identical bytes at different addresses leaves them differing in the modulator
at +0x2078..+0x25d1, for three of the nineteen txstates that have a body --
and WHICH three changes when code that runs after the step is edited.

Ruled out by experiment: our bring-up versus the blob's, 64 KB of scrubbed
stack, a shared shaping buffer, past-the-end reads of the fixture's seed
tables, and a short `preemp0`. Finding 289 has the detail. Whatever is left
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
Findings 319 and 322.

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
images (finding 319). What that does not do is say what the old layout was
feeding the loop. The bounds measured while closing it (finding 322):

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
64 in-place edits would — finding 622's "Owed" paragraph. The mechanical half
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
- *Ambiguous alias or internal table* — finding 622's two blind spots. D6:
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
(finding 1066). An entry is measured when its own claim is driven, not when
its function is.

## One entry whose own expiry condition has now fired

**D35** says of the 2743-baud arm that "whether either can put 2743 there has
not been measured", and names its own trigger: "`unmeasured` — task #47, or
whenever `probeselect` lands". `probeselect` has landed. All three writers of
`+0xaa96` now exist in `src/pump/v34/v34hshak.c`, and none can write 2743 —
see finding 1063. The entry is no longer resting on the absence of a writer.


---

# Part II — the defect sweep of `docs/findings.md` (task #98)

Everything that was in the deleted `docs/fixlist.md`, folded in here and
given register numbers.

**What was swept.** Every line of `docs/findings.md`: 37,637 lines, 721
numbered finding headings over 717 distinct numbers — only 1, 2, 3 and 4
collide, and those four are sub-headings inside finding 39, so any citation of
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

**Finding 234** (`docs/findings.md:13472`). Both arms take the row straight
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
`docs/blobfix.md`, finding 1203). The three banks are `GLOBAL`, so the
override mechanism that repaired D1 and D4 would *reach* them — and that is
the trap, not the opportunity. This defect is a missing DECISION, not a
missing VALUE: there are no correct coefficients for row 100, so a longer bank
could only be filled with invention, and replacing one bank symbol also
destroys the adjacency finding 235 records the reconstruction's own tests
having to reason around (`preFilterCoefType1` ends at 0x15b0, `Type2` begins
at 0x15c0). An override here would change behaviour on exactly the input in
question, to a value we made up. Mechanism B — a clamp, patched into the two
sites — is the only object-side form that fits, and the host-side mitigation
named above is still better than either, because it also covers the
`getV90Capability` path `selectFilter` cannot reach.

---

## D71 🐛 a one-sample overrun in the receive path becomes a wild pointer within six more

*Task #98 sweep, from fix list §0.3. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED.*

**Finding 123** (`docs/findings.md:7157`). The receive buffer holds fourteen
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
and the bound are finding 1188.*

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
the two `VPcmFloModem` call sites (finding 1188).  `setEchoDelay` later changes
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
cancellers.** Finding 1205: over the bench's SIP path the reflection arrives at
205.62 ms, while `V34EchoFilter`'s delay line holds 172.5 ms (D27) -- so no
`lag` reaches it, and the sizing question above is a structure change rather
than a setting. Note the arithmetic on THIS page is `V92EchoCanceller`'s and
does not carry over: the V.34 datapump shares neither the structure nor the
`echo_delay = IODELAY + 60` mapping.

## D73 🐛 `DP_V32BIS` (132) never connects

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

*Task #98 sweep, from fix list §3. **Reachability: FIRES TODAY.** Status: CONFIRMED, FIXED.*

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
`docs/forkblob.md` and findings 1140-1146. We do not ship that blob and there
is nothing for us to fix; it is here so nobody re-derives it.

The authority is **finding 1145** (`docs/findings.md:37495`), which traces all
three predecessors of `0x75d80` by hand and names the address the edit should
have used: `0x75d8e`, not `0x75d86`. Nothing in the fork's link line includes
`.mod` today, so it affects nobody.

---

## D76 🐛 `FPM_div` reads past its table and drops the call

*Task #98 sweep, from fix list §5.1. **Reachability: FIRES TODAY.** Status: CONFIRMED. Already in this register as D4 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: opt-in extension.*

**Finding 40.** A level estimate of 4088 normalises to mantissa `0xff80`,
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

**Findings 960, 1022, 1026** (and 962). Arm 47 `TX_PHASE2_ANS` reads an
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

*Task #98 sweep, from fix list §5.3. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Findings 823, 824.** The host's rate window lands at runtime `+0x30`/`+0x34`,
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

*Task #98 sweep, from fix list §5.4. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: host-side.*

**Findings 49, 50.** The field is commented out of `slmodemd`'s
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

**Finding 60.** `cadence_create` converts `GetDialToneDetectionThreshold` from
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

**Finding 47.** The cadence detector's IIR cascade has about 42 dB of passband
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

**Finding 45.** All three of `CallingTone.c`'s constants encode 9600 Hz while
call progress runs at a fixed 8000 (finding 41): the phase step of 2219/16384
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

**Finding 52.** `AnalyseDialString` makes an unknown dial-string character
INVALID when `modifier_validation` is zero and TOLERABLE when it is set —
turning validation *on* makes the parser more forgiving. **Twenty of the fifty
shipped countries set it**, so those twenty accept dial strings they were
configured to reject.

*Fix class:* **documentation only** in the object; a host that cares can
withhold the flag.

---

## D84 🐛 `receiver`'s complex predictor rounds the real axis the wrong way

*Task #98 sweep, from fix list §5.9. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 140.** The imaginary accumulator starts at `+0x2000` and the real one
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

**Finding 622**, corroborated by **finding 30**. The Bell 103 and V.23 copies
of `AGC_DEF_ALPHA` carry `16384, 1638` where the alpha should be 31130, so the
slow pair sums to **34242 rather than 32768** — a DC gain of 1.045. The v21
copy has the correct shape. Finding 30 adds independent evidence that it is a
copy-paste slip: `FPM_TONE_detect` writes `31130 * x + 1638 * y` longhand for
the same smoother, and 31130 + 1638 is 32768 exactly. That heading names D6.

*Trigger:* live in the Bell 103 and V.23 receivers. *Fix class:* **opt-in
extension**; the register entry exists, the reachability is what this adds. No
automated guard is possible — `ref_AGC_DEF_ALPHA` is file-static in six
translation units, so a test naming it binds to whichever the linker picks.

---

## D86 🐛 the V.21 offer can never be withdrawn

*Task #98 sweep, from fix list §5.11. **Reachability: FIRES TODAY.** Status: CONFIRMED. Already in this register as D16 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Finding 75.** The test that should clear the V.21 bit reads the framing stop
bit as well, so it is never true and the JM carries V.21 whatever the far end
offered. Found by running a real negotiation against SpanDSP with a
deliberately narrowed offer.

*Fix class:* **documentation only** — reproducing it is what keeps the
handshake bit-exact, and no interop failure has been traced to it.

---

## D87 🐛 the restart's inlined `SetINFO0dBits` lost its guard

*Task #98 sweep, from fix list §5.12. **Reachability: FIRES TODAY.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 402.** Microstate 44's restart path contains a hand-inlined copy of
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

**Finding 129.** `t1` (+0xa48), `t2` (+0xb48) and `t3` (+0xc48) are 128 entries
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

**Findings 186, 306 and 736.** Two separate unclamped indexes in one function,
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

Finding 306 is the third witness and the one that names the cost: "a varied
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

**Finding 290.** With the diagnostics on, `v34handshakinit` announces each
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

**Finding 313.** The function has two switches on the same `requestedDp`
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

**Finding 444.** The default accept arm copies `nbytes = ceil(bits/8)` shorts
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

**Finding 227.** Two in one finding.

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

*Task #98 sweep, from fix list §6.5. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 73.** At 0x78070 the collector reads `word[fdbc]` and only *then*
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

**Finding 602.** The four-argument `design(nTaps, cutoff, type, gain)` allocates
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

**Finding 801.** `MDMPRM_DPRUNTIME` is stored at +0x28 and **dereferenced
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

**Finding 54.** The dial string is copied into a 100-byte field with an
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

**Finding 53.** With `break` zero, `elapsed < break` is never true, the line is
never interrupted, and the digit still counts down to completion on the make
timer alone — so the dialler reports success and dials nothing. Nothing in the
code guards against it, and `t_pulse` asserts it in that direction rather than
avoiding the case.

*Trigger:* a country table with break = 0. *Fix class:* **host-side** — validate
the country table. See also findings 56 and 59 in D118 for two more of the
same shape.

---

## D99 🐛 `getConstellationMask` clears eight entries and can write sixteen

*Task #98 sweep, from fix list §6.10. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding 829.** The shift is `mov %dl,%al; shr $0x4,%al` — 8-bit, with no
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

**Finding 828.** `getConstellationsIndex` uses the loop cursor as its equality
verdict, so a length of zero leaves `n = 0`, the test compares 0 against 0, and
the two constellations are declared duplicate without a byte being read.
`t_v90cmask`'s shape 2 asserts the blob returns 1 for exactly that.

*Trigger:* any caller with `constellationSize[k] == 0` for two or more entries.
*Fix class:* **documentation only.**

---

## D101 🐛 `V92deleteConstellations` and `V92deleteFilterCoefficients` leave ten dangling pointers

*Task #98 sweep, from fix list §6.12. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 831.** "**Neither delete writes anything.** There is not one store in
either function; the freed slots keep their stale pointers." Each free is
guarded by `if (p != 0)`, which the omission defeats: a second delete
double-frees and any later read is a use-after-free.

*Fix class:* **documentation only** — a helpful `= NULL` is exactly the
wrong-but-plausible change the rule forbids. Same shape as D56 (`~FloatIIR`)
and finding 55 below.

---

## D102 🐛 `CALLPROG_Delete` leaves three of five sub-object pointers dangling

*Task #98 sweep, from fix list §6.13. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 55.** Of the four sub-objects it frees, only `busy` and `f70` have
their pointers cleared; `dial`, `band` and `dtmf` are left dangling, so a
second `CALLPROG_Delete` on the same object frees them again. `call_delete`
calls it once — but `CALLPROG_Delete` is a global symbol and the asymmetry is
invisible from outside.

*Fix class:* **documentation only.**

---

## D103 🐛 the caller-supplied `FPM_TONE` path frees five pointers it never obtained

*Task #98 sweep, from fix list §6.14. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Already in this register as D5 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Finding 1067.** Measured on both sides: `create` with supplied state does
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

**Finding 1108.** `cursor = min(cursor, linearEquLength - 1)` is compiled with
`jae`, so a zero-length equaliser computes 0xffffffff and the clamp is a no-op:
the 1.0f tap lands at whatever cursor was asked for. Spelling the comparison
signed breaks **648 checks**, so the unsigned form is the object's beyond doubt;
the length-zero consequence is reasoned.

*Fix class:* **host-side** — the length is a parameter. No caller is named.

---

## D105 🐛 `FloatARMA` with zero denominator taps

*Task #98 sweep, from fix list §6.16. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED (store) / SUSPECTED (loop). Fix class: documentation only.*

**Finding 873.** With `nDen == 0` the constructor still executes
`m_a[0] = 0.0f` — 0x4731e and 0x47379 are both unconditional — writing four
bytes through a `sysdep_malloc(0)`. `t_floatarma` constructs that shape and
does not drive `process` on it, where the carry-tail loop's `dec %edx; jne`
count underflows: `FloatFIR`'s zero-tap hazard in a second class.

*Fix class:* **documentation only.**

---

## D106 🐛 `refLoopsType2` is terminated only by the linker's zero padding

*Task #98 sweep, from fix list §6.17. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: already handled.*

**Finding 234.** Five of the six loop arrays end with an all-zero record inside
the symbol. `refLoopsType2` is 2,244 bytes — exactly 33 records — and **not one
of them has the zero first byte the counting loops stop on**. What stops them
is the 28 bytes of `.data` alignment padding between the end of the symbol at
0x6124 and `refLoopsType1` at 0x6140, which happen to be zero.

This is finding 13's pattern in a second place: correct only because of what
the linker put next, and it breaks the moment the tables are regenerated or
reordered. Our copy carries a 34th all-zero record, and `t_v90pftab.cpp`
asserts the counted length is 33 on both sides.

*Fix class:* **already handled** in the reconstruction; **documentation only**
for the original.

---

## D107 🐛 the DIL segment search runs one past its row when `dilCount` is zero

*Task #98 sweep, from fix list §6.18. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 232.** `resetDILGenerator` finds which G.711 segment `dilLevel[0]`
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

**Finding 82.** `*nbits` is written only on the paths that return 0, so a
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

**Finding 81.** `v23FP_rx_progress` omits the `rx_state = 0` reset its Bell 103
twin performs on a failed detect, so the gate accumulates non-consecutive
detections however far apart they fall, and once the counter reaches 10 it
never returns. What compensates is the detector's 0.885 energy ratio — its
configuration, not the gate.

*Trigger:* any input producing two 1300 Hz-dominant blocks at any separation.
*Fix class:* **documentation only.**

---

## D110 🐛 `four1` and `realfft` validate nothing

*Task #98 sweep, from fix list §6.21. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 832.** No power-of-two check, no null check and no length check in
either body; the first loop runs on the raw argument. The compiler's
`-freciprocal-math` rewrite of `A/len` to `A*(1.0/len)` is exact **only**
because every length a caller passes is a power of two; on any other length the
two forms disagree and the twiddle seeds change as well. The only in-object
caller found is `Psd::process` with `m_length`, which nothing validates either.

*Fix class:* **documentation only.**

---

## D111 🐛 the receive-rate completion has no default arm

*Task #98 sweep, from fix list §6.22. **Reachability: NEEDS A CALLER OR CONFIGURATION NOTHING VALIDATES.** Status: CONFIRMED. Fix class: host-side.*

**Finding 428.** The five-way at 0x62f9c is the only writer of the rate and its
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

**`PPSEG` adds two different units together (finding 1043).** At 0x68154 a
count of symbols at the *negotiated* baud is summed with `filtdelay`, a count
of 4-sample ticks. The sum is dimensionally correct **only at 2400 baud**; at
3429 the term should be `filtdelay * 3429 / 2400`, which at `IODELAY` 216 is
127 where the object uses 89 — an error of 38 symbols in an echo-adaptation
start delay. Finding 1046 states the position: latent at every rate but 2400,
no fixture reaches `PPSEG` at another rate, must be reproduced and not
repaired. **Unobserved, not unreachable** — essentially every real connection
negotiates something other than 2400.

---

## D113 🐛 `updateAlpha` divides by zero

*Task #98 sweep, from fix list §7.3. **Reachability: LATENT.** Status: SUSPECTED. Already in this register as D33 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**`updateAlpha` divides by zero (finding 127, `D33`).** `(1 << (shift+21)) /
((energy + 0x8000) >> 16)` traps for `energy` in [-0x8000, -1]: the
normalisation loop stops immediately because bit 30 is already set in a
negative value, so no shift rescues it. The entry condition is that
`V34EchoEstimateDelayLineEnergy`'s sum of squares has already overflowed
negative — a second defect upstream.

---

## D114 🐛 The slicer's distance metric wraps at 16 bits

*Task #98 sweep, from fix list §7.4. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The slicer's distance metric wraps at 16 bits (findings 110 and 119).**
The squared distance is shifted logically and then truncated, so a
constellation point far enough from the target wraps to a small distance and
can win. Whether it fires with a real V.34 constellation is unmeasured. **The
metric exists twice** — inlined in `decoderv34` as well as in `decision` — so a
fix to one would not reach the other.

---

## D115 🐛 `detectRetrainReq` widens the two sides of its compare differently

*Task #98 sweep, from fix list §7.5. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`detectRetrainReq` widens the two sides of its compare differently
(finding 212).** All four sites are `movzwl` on the energy and `movswl` on the
threshold, so an energy short that has wrapped negative reads as ≥ 32768 and
clears any threshold, and a negative threshold rejects every energy. Reachable
in the object's own arithmetic — `0x04000000` in both accumulators is the
corner exactly — but "a sweep of all 32767 amplitudes of a 1200 Hz sine gave no
negative energy at all". The function also has no caller inside the object.

---

## D116 🐛 The low-level block counter is unsaturated and compared signed

*Task #98 sweep, from fix list §7.6. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The low-level block counter is unsaturated and compared signed (finding
360).** The counter at +0x0234 advances on every block below the level floor,
is cleared only by a block at or above it, and its test against 0x257f is
signed — so after 2^31 sub-floor blocks it wraps negative and the condition
stops being reported. 2^31 *calls*, not loop iterations.

---

## D117 🐛 `B103FP_create`'s 700-tick floor is an unsigned compare on a signed quotient

*Task #98 sweep, from fix list §7.7. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`B103FP_create`'s 700-tick floor is an unsigned compare on a signed
quotient (finding 36).** A negative `tone_timeout_ticks` divides to a negative
`t`, which as unsigned is enormous, passes the `jae`, and is stored as a
negative short — the clamp is meant to enforce a floor and does not. Not
reachable from `B103_CFG`, where 14000/20 is exactly 700 and the clamp is a
no-op.

---

## D118 🐛 Two country parameters hang the dialler

*Task #98 sweep, from fix list §7.8. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Two country parameters hang the dialler (findings 56 and 59).**
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

**Three leaf defects in V.8 (finding 62).** `v8_absfn(-32768)` returns
-32768, because negating it overflows and the result is narrowed back to a
short. `v8_copycoeff` counts with a short, so a count above 32767 never
terminates. `v8_crc` compares its bit argument sixteen bits at a time, so a
value that is non-zero overall but zero in its low half counts as a zero bit.
None is reachable from the signal path.

---

## D120 🐛 `Dual_TONE_detect`

*Task #98 sweep, from fix list §7.10. **Reachability: LATENT.** Status: SUSPECTED. Already in this register as D9 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**`Dual_TONE_detect` (finding 42, `D9`).** The energy floor
`Dual_TONE_create` installs is **1** — not 1000, not a fraction of full scale —
so the "no signal" branch is very nearly unreachable and a detector fed
anything at all reports 1 rather than 0. Separately, the sample index is
truncated to 16 bits every iteration by a `cwtl` inside the loop, so a block
longer than 32767 samples loops for ever. Nothing in the library passes one,
and nothing validates it either.

---

## D121 🐛 `vpcm_run`'s training timeout never clears its counter

*Task #98 sweep, from fix list §7.11. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`vpcm_run`'s training timeout never clears its counter (finding 982).**
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
(finding 189).** A step that would leave the bounds writes **nothing** rather
than clamping, so `rate_want` keeps what it held; the handshake is still torn
down and the attempt still counts, with the stale value as its target. A
negative `rate_want` is the "no target" encoding, so a stale value is read as a
real rate index.

---

## D123 🐛 The FSK delay line is aliased onto the echo canceller's coefficients

*Task #98 sweep, from fix list §7.13. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The FSK delay line is aliased onto the echo canceller's coefficients
(finding 100).** `fskdetect`'s 49-entry delay line and `echo0 + 0x0c` are the
same memory, proved from two independent address readings. `V34EchoCleanUp`
zeroes all 144 entries of `coeff_frac` and `V34EchoAdapt` writes every one of
them, so they cannot both be live. The reading that fits is deliberate reuse —
phase 2 carries the INFO messages, data mode adapts — but **nothing in the
object enforces the separation**, and a retrain that re-enters phase 2 with the
canceller live is the case to check.

---

## D124 🐛 The V.23 answer tone is in spec by frame quantisation

*Task #98 sweep, from fix list §7.14. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The V.23 answer tone is in spec by frame quantisation (finding 84).**
The silence is `rate/20` = 400 samples = 50 ms, against V.25's 75 ± 20. It
lands in spec only because the silence state uses `<=` where the tone state
uses `<`, and because `dp_wrapper` delivers 160-sample frames, so 400 samples
takes three frames to exceed: 480 samples, 60 ms. **A different frame size puts
it back out of spec.**

---

## D125 🐛 `V90Demodulator::reset` sizes the first block from the previous configuration

*Task #98 sweep, from fix list §7.15. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`V90Demodulator::reset` sizes the first block from the previous
configuration (finding 608).** `reset` at 0x1c038-0x1c04c calls `agc.reset()`
and only *then* writes `blockLen` from `params+0x64`; `reset` has already
copied the OLD `blockLen` into `count`, so the first block after the first
reset runs for the constructor's 500 samples rather than the configured length.
It self-corrects on later resets.

---

## D126 🐛 `hamming` and `blackman` divide by zero at one tap

*Task #98 sweep, from fix list §7.16. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`hamming` and `blackman` divide by zero at one tap (finding 246).** Both
compute `1 / (n - 1)`, which for `n == 1` is +infinity, and `0 * infinity` is
the x87 indefinite: both write **0xffc00000** into `w[0]`. There is no guard,
adding one breaks the match, and the test drives `n == 1` deliberately and
compares as bits. Any caller asking for a one-tap window gets a NaN.

---

## D127 🐛 `autoSelection` lets a NaN win the search

*Task #98 sweep, from fix list §7.17. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`autoSelection` lets a NaN win the search (finding 236).** The running
best is updated on `fcom %st(2); fnstsw; sahf; jae`, and `fcom` sets C0 for an
unordered result as well as for a less-than one, so a NaN distance takes the
update arm where C's `<` does not. **A NaN is reachable because the measurement
is whatever Phase 2 left in memory and the six differences are taken from it
unchecked.** Found by the test, not by reading — in the arm where the
measurement was left as seeded pseudorandom bytes.

---

## D128 🐛 `tone_detect`'s second section folds instead of scaling

*Task #98 sweep, from fix list §7.18. **Reachability: LATENT.** Status: SUSPECTED. Already in this register as D25 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**`tone_detect`'s second section folds instead of scaling (finding 90,
`D25`).** Section 2 truncates the accumulator to 16 bits and *then* shifts by
4, where section 1 shifts first; on loud input it wraps. "There is no reading
under which both are intended. One is a slip." Both orders behave identically
over the levels a correctly-AGC'd detector sees.

---

## D129 🐛 `modem_serrint`'s 60-tap FIR path feeds the adaptation at full width

*Task #98 sweep, from fix list §7.19. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`modem_serrint`'s 60-tap FIR path feeds the adaptation at full width
(finding 128).** The accumulator is used at 32 bits for the products and only
the queue store narrows it, so a loud enough sample drives both echo
cancellers' error terms and the leaky energy estimate with a value outside a
short. Reproduced. Requires receiver flag bit 11; the Hilbert branch is the one
the receiver normally takes.

---

## D130 🐛 Nothing initialises the history-ring index, and the store precedes the range test

*Task #98 sweep, from fix list §7.20. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Nothing initialises the history-ring index, and the store precedes the
range test (finding 781).** At 0x5d051 `hist_2f58[idx]` is written, and only at
0x5d04c/0x5d059 is `idx + 1` tested against 0x257 — unsigned. Nothing in the
object initialises `f2aa6`, and a whole call from a fixture that fills the
object with pseudorandom bytes drives it negative on the first sample.

---

## D131 🐛 `FPM_FSD_demodulate` discards input past its bit cap

*Task #98 sweep, from fix list §7.21. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`FPM_FSD_demodulate` discards input past its bit cap (finding 32).** The
loop stops once `max_bits + 1` bits have been written and throws the remaining
samples away rather than holding them over. Bell 103's ceiling is 8 bits from
64 samples and `DemodDataB103` feeds 48 at a time, so it never bites in normal
use — safe by the fragment size the one in-library caller happens to use.

---

## D132 🐛 `FPM_sqrt_dp`'s mantissa can be 17 bits

*Task #98 sweep, from fix list §7.22. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`FPM_sqrt_dp`'s mantissa can be 17 bits (finding 21).** After
normalisation `x >> 15` reaches 0x1ffff, which does not fit the
`unsigned short` it is stored in and wraps for `x >= 0x80000000`. The same
shape as `FPM_sqrt`'s one-past-the-end read. `FPM_rms` is the only caller found
and its accumulator is bounded by the /36 scaling, so it may be unreachable.

---

## D133 🐛 The AGC's block partition can exceed what `FPM_rms` is dimensioned for

*Task #98 sweep, from fix list §7.23. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The AGC's block partition can exceed what `FPM_rms` is dimensioned for
(findings 28 and 29).** The folded block reaches `block_len + block_len/2 - 1`
= 53 samples where `FPM_rms`'s 1/36 headroom is sized for 36, so at 53 samples
a block above about 82% of full scale sticks at 32703 and the AGC applies less
gain reduction than it should — wrong in the safe direction. Separately, a
`count` below `block_len/2` is passed through completely untouched: not gated,
not scaled, and for Bell 103 that is any call of 1..17 samples.

---

## D134 🐛 `initTxSequence` reads a fourth byte it does not own

*Task #98 sweep, from fix list §7.24. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`initTxSequence` reads a fourth byte it does not own (finding 66).** The
original tests `*(int *)cm & 0x80008` — a single 32-bit read across a
three-byte flag structure, correct only by accident of layout. The
reconstruction writes it as the two byte tests it plainly is.

---

## D135 🐛 The A-law boundary row's last entry can never be reached

*Task #98 sweep, from fix list §7.25. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The A-law boundary row's last entry can never be reached (finding 231,
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
(finding 73).** Two extension-matching loops that look identical: the first
keeps its "something has matched" flag in the field `febc` and clears it before
every marker word; the second keeps it in a stack local set up once before the
loop and **never cleared**. Once anything has matched, a later marker whose
first character is wrong is abandoned rather than scanned through. Reachable
from the wire; the finding does not claim the author thought it wrong.

---

## D137 🐛 The blob disagrees with itself on the receive path

*Task #98 sweep, from fix list §7.27. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**The blob disagrees with itself on the receive path (findings 736 and
754).** At two fixture fills, byte-identical inputs give different results on
**both sides** — the control cases fail too, so it is not the reconstruction.
`V34HS_PROBE` reports 0 object bytes and 0 padding bytes differing after setup
and side B re-run at its own address differs in 0 bytes, so each side is
deterministic and the inputs are identical. It survives `REFINIT`, `EQPTR`,
`SKEW`, `NOSCRUB` and `PADVARY`, is geometry-sensitive (moving side B out of
its arena halves it), and lands in four modelled receiver fields. **The cause
is not established; what is left is a read of memory outside the object.** Any
arm that calls `receiver` inherits it.

This is the same shape as `D61` and probably the same defect. **Findings 319,
322 and 324** measured it on the transmit side: byte-identical objects placed
differently in memory give different answers, and `v34handshakinit` left
+0x0a28 and +0x2608 holding `ref_Convolve32` on one side and
`ref_Convolve32 + 0x40` on the other — the same table, sixty-four bytes in,
from identical inputs and identical code. Congruent wrappers for the five
pointed-to blocks take it from 23 of 24 fills failing to 0 of 24. Ruled out:
our bring-up against the blob's, stack residue, x87 residue, absolute address,
alignment, object placement, object-to-block distance, and block neighbourhood
contents within 32 KB. **What is NOT established is which byte it reads**, and
finding 322 says so in terms — do not quote this as if the mechanism were
known. Finding 319 retracts `D60`, which had read the same evidence as a
fixture artefact.

---

## D138 🐛 Two idle-symbol emitters hardcode the scrambler polarity

*Task #98 sweep, from fix list §7.29. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**Two idle-symbol emitters hardcode the scrambler polarity (findings 280
and 303).** `k56FlexPhase34` case 4 hardcodes the generator as the literal 1
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

**`bits[7] &= 0xdf` takes the whole high byte with it (finding 312).**
`V34SetINFO1aBits`'s INFO1d arm sets the bit with `bits[7] |= 0x20` and clears
it with `bits[7] &= 0xdf` — and the clear is `and $0xdf` on a zero-extended
short, so it is not the set's inverse: it destroys bits 8..15. A test whose
message shorts never carry a high byte cannot tell the difference, and none
does. Finding 335 narrows it — the reader side takes the bit only through
`testb $0x20` on the low byte — but does not close the writer-side clobber.
Whether the protocol ever puts anything in that high byte is not established.

---

## D140 🐛 `SineWave::generate` advances the phase once more than it emits

*Task #98 sweep, from fix list §7.31. **Reachability: LATENT.** Status: SUSPECTED. Fix class: documentation only.*

**`SineWave::generate` advances the phase once more than it emits (finding
601).** The advance sits between the `cmp` and the `jb` and the wrap happens
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
116).** `txwritequeue` adds four to `count` per call; `txmit` open-codes the
same enqueue and adds **one per sample**. The two agree only while the
modulator returns exactly four samples per call, which is measured at 2400 baud
and not fixed for the other six rates `V34SetupModulator` handles.

---

---

## D142 🐛 💤 `probeselect` can never select pre-emphasis index 0 — the counter increments before the first test, so it exits at 6..10 and the `i == 5` arm is dead

*Task #98 sweep, from fix list §8.1. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings 218, 219 (`D53`).** Measured twice: a sweep asserting 6..10 returned and 0..5 never, and gcov marking the body NOT EXECUTABLE after 6,938 executions of the test.

---

## D143 🐛 💤 `preempindex` has the same shape in a second function, and can only return 6 or more

*Task #98 sweep, from fix list §8.2. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings 197, 620 (`D36`).** Confirmed by construction in finding 1061: the guard is unreachable at 5 and reachable at 6.

---

## D144 🐛 💤 `setInitialPhase`'s divide-by-zero guard tests a sum of two constant polynomial values that runs 7,632..11,772

*Task #98 sweep, from fix list §8.3. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 242.** Computed exhaustively; dead in the original too.

---

## D145 🐛 💤 microstate 58's third block is unreachable after its own second block, so the error-recovery reset there is dead

*Task #98 sweep, from fix list §8.4. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 431.** Reasoned from what the second block writes; recorded as an equivalence so it expires loudly.

---

## D146 🐛 💤 five of six `hs_setstate` guards in microstate 44 compare against a state the dispatch has already made impossible

*Task #98 sweep, from fix list §8.5. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 448.** Reasoned from what selects the arm.

---

## D147 🐛 💤 the `+0xaaa6 == 0` test at 0x62fd5 sits forty bytes after the literal 1 is stored there unconditionally

*Task #98 sweep, from fix list §8.6. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 428.** Two addresses, nothing in between; the suite records it equivalent.

---

## D148 🐛 💤 `loadModemParamsData`'s two absolute-value operations can never do anything — the quantity comes from an unsigned divide

*Task #98 sweep, from fix list §8.7. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 879.** Proved over the whole domain; both mutations recorded equivalent.

---

## D149 🐛 💤 `VPcmV34Create`'s K56flex arms are unreachable (one relocation, and the caller can pass only 0, 1 or 2), and `vpcm_create`'s failure path is dead because both of the callee's exits are `xor %eax,%eax`

*Task #98 sweep, from fix list §8.8. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 1119.** Read from the disassembly plus a relocation count.

---

## D150 🐛 💤 the INFO1a arm calls `detectorinit` twice with only the warm-up differing and nothing reading between, so the first call — and the warm-up of 10 it installs — is discarded

*Task #98 sweep, from fix list §8.9. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 442.** Reasoned from the callee writing nine fields and reading none.

---

## D151 🐛 💤 `setfinalrate` has no arm for receive rate codes 1, 6 and 7 and silently leaves the previous rate in place; its own comment says so

*Task #98 sweep, from fix list §8.10. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 442.** Read from the disassembly and the original's comment.

---

## D152 🐛 💤 `CALLPROG_DIALING` (code 3) is computed and then filtered out by a whitelist of 4, 12 and 14, so it is never reported

*Task #98 sweep, from fix list §8.11. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 60.** Read from the tail's message filter.

---

## D153 🐛 💤 `toneiir_create(state, NULL)` builds a filter whose numerator is all zeros, so the default configuration cannot pass a signal

*Task #98 sweep, from fix list §8.12. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 46.** From relocations at 0x61a0, 0x61a4, 0x61c8 and 0x6244.

---

## D154 🐛 💤 K56flex is shipped as names only — constructor, destructor, resets, phase-3 entry and `setMinMaxRates` are bare `ret`, and the demodulator returns the constant 5

*Task #98 sweep, from fix list §8.13. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings 1090, 381, 318.** All 24 symbols disassembled with sizes.

---

## D155 🐛 💤 and its callers treat it as live: `k56FlexPhase34`'s Ja and MP completion arms (78 and 57 bytes) can never execute, because both bit sources are three bytes of `xor %eax,%eax; ret`

*Task #98 sweep, from fix list §8.14. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 281.** Measured twice — the byte-level stub, and 55 lines added to coverage of which 48 are covered, the seven uncovered all in these two arms.

---

## D156 🐛 💤 the ANSam phase-reversal detector's window is wider than the spacing real ANSam produces, so its report has never fired

*Task #98 sweep, from fix list §8.15. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 169.** Observed never to fire in any test.

---

## D157 🐛 💤 `FloatFIR`'s one-at-a-time tail can never run — every writer of `taps` masks it to a multiple of four

*Task #98 sweep, from fix list §8.16. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 234.** Reasoned from the writers; corroborated by a surviving mutation.

---

## D158 🐛 💤 txstate 71 runs the scrambler, stores the quadrant, and then transmits `vect4[0]` — the load at 0x64257 has no index register where 86's at 0x63e40 scales by four

*Task #98 sweep, from fix list §8.17. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 340.** Read from the two addressing modes; the arm is differentially tested.

---

## D159 🐛 💤 fifty-seven of table 1's eighty-two dispatch entries point at the per-sample loop's own bottom, so a txstate with no arm never advances and the loop **spins forever**

*Task #98 sweep, from fix list §8.18. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Already in this register as D59 — that entry is the authority for the mechanism; this one adds reachability and the fix class. Fix class: documentation only.*

**Findings 287, 420.** Demonstrated, not argued: `V34HS_HANG=1 ./build/test/t_v34hsstep` drives txstate 6 `ANSAM` with the cursor below the limit and the run exits 3. The harness arms `SIGALRM` around every step because of it.

---

## D160 🐛 💤 `probeselect` has no 2743-baud arm, and `chkForceBaudRate` writes `allow[0]` and `allow[1]` that nothing reads — one standard rate the object can be configured for and can never select

*Task #98 sweep, from fix list §8.19. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Finding 218.** Two independent readings; `D35` is the same gap from the other side.

---

## D161 🐛 💤 `vpcm_create`'s max-rate clamp at 0x3b65 is 0xdac0 = 56000, the same number as slmodemd's `MODEM_MAX_RATE`, so it never fires under the shipped host

*Task #98 sweep, from fix list §8.20. **Reachability: CANNOT FIRE.** Status: CONFIRMED. Fix class: documentation only.*

**Findings 823, 824.** Driven: raising the host ceiling to 64000 makes the clamp fire.

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
rather than passes.  Finding 1223.
## D163 🐛 💤 `V90CP::printNofRecievedMpMpNot` prints `"V90MP: received %d MP, %d MPNot"` — the CP class's debug line names the other class

*V.90/V.92 message-parameter batch. **Reachability: unmeasured** — diagnostic only, and only above `dsplibs_debug_level > 1`. Status: CONFIRMED — the literal at `.rodata.str1.4+0xd6b0` is byte for byte `V90MP`'s at `+0x5a34`. Fix class: documentation only; reproduced, not corrected.*

**Finding 1239.**

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

**Finding 1240.** Asserted in test/unit/t_v90spectral.cpp against the seeded
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

**Finding 1241.** All six members were scanned for a load at +0x10 and none
has one; a seventh reader outside the class would have to reach it through a
`V90SdDetector *`, which has not been swept.

**RENUMBERED at the merge**, from `D163`, the number this batch committed on its own branch: three of the nine parallel construction-path batches picked the same
next-free number out of this file, and the two before it took D162 and D163.
CLAUDE.md's rule is that the entry says what it used to be called, because a
citation that still resolves but now points at the wrong entry is the one
thing `tools/refcheck.py` cannot catch.

---

---

# Part III — looked at and judged NOT a defect

This is part of the deliverable, not an offcut. A register that only
records hits cannot be audited, and four of the items below had already
been filed as defects somewhere before being read properly.

## A retraction that is still cited as live (D28)

**7.1 `D28` is RETRACTED and finding 180 still cites it as live.** Finding 180
says `V34EchoReportCoeff` "dumps a hardcoded 144 coefficients whatever `taps`
says (D28), which out of the shared 32-entry array runs 48 shorts past the end
of the struct". **Finding 98 retracted that**, and so does the register:
`V34InitializeImplementationSpecific` sets both cancellers' tap counts to 0x90
— 144 — at 0x71dc6 and 0x71e1e, so the dump is sized to the array exactly and
there is no over-read. `(144 / 6) * 6` is 144, so the scan's rounding-down is a
no-op too. **This is not a defect.** It is listed here because it is the one
place in the record where a retracted claim still reads as current, and because
it was one of this task's seven starting pointers.

## The rest, in two kinds

Two kinds, and they carry different information.

### It looked like a defect in the object and is not

* **Finding 297** — "Had `nofBits` been anywhere earlier the object would
  overrun its own buffer on a descriptor it can represent". It is not
  anywhere earlier: `bitVector` holds 2,700 entries and the largest descriptor
  the fields can describe is 2,654 bits. A counterfactual, and in fact the
  finding's point is that two independently derived offsets agree.
* **Finding 180 / `D28`** — see the retraction note above. Retracted by finding 98 and by the
  register; `taps` **is** 144, so there is no over-read.
* **Finding 230** — "the object is compared whole, and so is a guard past its
  end". Test methodology: the guard exists so that a store overrunning the
  object fails rather than passing silently.
* **Finding 394** — "the staged clear runs to eleven words rather than ten".
  A surviving *mutation* of the reconstruction, not the object; the eleventh
  word is +0xabc2, which the statement after the loop zeroes anyway.
* **Finding 423** — "the run at +0x25da wrong has +0x25d8 PAST the end". A
  deliberately chosen test seed to separate two guards in series.
* **Finding 139** — its own heading says `receiver` "found a bug in
  `V34TimingFilter`". Read in full, the bug is the **reconstruction's**: the
  object multiplies the register the store came out of, which still holds the
  full 32-bit value, and we read the truncated short back from `iir[][0]`. The
  object is self-consistent; the state words overflowing a short is a fact
  about the object with no wrong behaviour attached to it.
* **Finding 227** — `getMPrecvdBits` calls `txrxdmainit` twice inside the V.90
  branch. The finding rules it out itself: it is idempotent, so this is wasted
  work rather than a defect, recorded so a reconstruction that tidied it would
  still be caught.
* **Finding 425** — `moh_recvd` compared signed. Proved equivalent over every
  input; both readings reach the same block.
* **Finding 1119** — `V34DisconnectThreshTable`'s unsigned `cmp $0x7 / jbe`
  looks like a signed/unsigned slip and is the **correct** bound: it rejects
  negative and above-7 alike and defaults to index 3.
* **Finding 819** — the V.92 `FRNDINT` ceiling has no guard on the sign, and
  both forms agree on all 256 reachable values.
* **Finding 216** — `V34SetupModulator`'s V.90 arm. This is the *retraction* of
  a dead-branch claim (`D31`), not a defect: the arm is live.
* **Finding 122** — the AGC integrator seeded from a stale return register.
  Retracted as `D34`; refuted by `xor %eax,%eax` at 0x5ac13.
* **Findings 92, 95, 229, 650, 631, 874, 821, 865, 875, 837, 1110, 1101** —
  each is an inconsistency, an asymmetry or a redundancy in the original with
  no wrong behaviour shown: a zero sample treated as positive by one test and
  negative by another with no consequence measured; an equaliser precision
  split the finding calls "most likely deliberate"; one CRC register shared by
  two packers that nothing reads between packs; one field read at two
  signednesses that agree over the only values it holds; a GCC partially-dead
  store; a block memset twice; a field nothing reads; the Numerical-Recipes
  one-based array convention; a banner asymmetry invisible at debug level 0; a
  dead first write of six flags; a deliberate drain.
* **Finding 620** — `c1959`'s poles are written high-then-low where the other
  seven go low-then-high. An original transcription slip and harmless: the pair
  is summed, not ordered, and the midpoint is still exactly 1959.
* **Finding 1108, item 5** — both window lengths scaled by the linear
  equaliser's length. Measured to be what the blob does (using `dfeLength`
  breaks 2,064 checks); that it is *wrong* rather than intended is not
  established.
* **Findings 722, 730, 750, 418, 352** — branches the object emits that nothing
  can reach, all inlined "already there" guards the optimiser could not see
  through. Compiler-emitted dead edges, not authored defects.
* **Finding 428's `(cap * 7) >> 14`** — looks like a divide by 2340 where 2400
  was meant. It is a strength-reduced divide that agrees with `/2400` over
  every rate index the object uses.
* **Finding 80** — 60000 in a signed 16-bit timeout field. Works reliably
  because both comparisons are unsigned.
* **Finding 61** — the initial message value 18 is a never-matches sentinel,
  only compared and never used as an index.
* **Finding 289** — "the per-sample transmit route is not a function of the
  object" was retracted by finding 319 as `D60`. The sensitivity is real but it
  is the *placement* of the five pointed-to blocks, not the route; D137 has it
  under `D61`. Do not file 289 as a separate defect.
* **Findings 299, 263, 318** — stores nothing can observe: `nofBits = 0` before
  the packer writes it, `bits[crcAt] = 0` re-written by the preceding section's
  framing zero, `is_short = 0` masked by `v34modeminit`'s unconditional clear.
  All carried as equivalences, and the third measured to flip to caught when
  that clear is deleted.
* **Finding 282** — the unmasked `sar %cl` on a sign-extended `vect_idx`. The
  blob and the reconstruction agree at every swept value including -1 (shift
  30) and 100, and nothing shows a caller producing an out-of-range index.
* **Finding 305** — cases 4 and 7 gate on `!= 0x10` where the counter advances
  by 2, which would step over the constant from an odd start. Every writer in
  range advances by two and nothing produces an odd value, so no defect is
  shown — but this is the thread to pull if a `+= 1` writer of +0x3a4 appears.
* **Finding 317** — "a configuration large enough to overflow prints the
  truncated number". The field is 16-bit and the object prints what it stored,
  so the `cwtl` is consistent rather than wrong.
* **Findings 270 and 314** — `V34DisconnectThreshTable`'s out-of-range fallback
  to entry 3 and the unsigned `jbe` that also rejects negatives are deliberate
  and consistent across two independent call sites.

### It is a defect but it is not the object's

* **The reconstruction's own, since fixed** — findings 69, 73, 130, 151, 168,
  170, 183, 185, 203, 204, 219, 546, 549, 573, 591, 593, 613, 748, 750, 721,
  781's headline, 879's divide, 1021, 1107, 1112, 356a. Each was caught by the
  differential test or by codegen comparison; the object was right.
* **Tooling** — `dis.py`, `refcheck.py`, `offcheck`, `extcheck`, `compare.py`,
  `closure.py`, `mutate.py`, `reanchor.py`, `coverage.py`, `debugaudit.py`,
  `vparse.py`, `cppstruct.py`, `decompile.sh`, and objdump's FDIVP/FDIVRP swap.
  Findings 39, 43, 46, 49, 134, 245, 355, 370, 432, 541–545, 555–557, 570–572,
  618, 619, 637–639, 670, 690, 700, 705, 818, 860, 862, 872, 907, 940, 941,
  985, 1004, 1065, 1103, 1114 among others.
* **Test fixtures and mutation bookkeeping** — findings 241, 242's seeding,
  341, 343, 349, 362, 364, 366, 374, 404, 416, 427, 429, 434, 445, 592, 713,
  714, 715, 718, 723, 724, 729, 735, 743, 744, 746, 753, 788, 920, 987, 1000,
  1064. A surviving mutation is a statement about the suite.
* **Third-party** — finding 40's note that SpanDSP 0.0.6 ships the Bell 103
  presets swapped.
* **Two sweeper inferences that the record does not make, and one it
  contradicts** — a reading of finding 19 in which the ANSam phase reversal is
  a no-op half the time (the record states the hop is 180° and this depends on
  whether a full phasor cycle spans 0x8000 or 0x10000; the record's reading is
  not overturned by an inference); a join of finding 1146's fork `IODELAY` 48
  with finding 1022's threshold of 86, which finding 1146 does not draw; and a
  reading of finding 424's XMITMP self-pointer as an object bug, where the
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

### 1. D77 — V.34 does not connect below `IODELAY` 86. CONNECT. Measured.

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

### 2. D72 — the echo canceller may overrun its history at HIGH `IODELAY`. SUSPECTED.

The buffer is sized in the constructor from the delay at that moment;
`setEchoDelay` then raises the delay at runtime from `MDMPRM_IODELAY` **without
reallocating**, and `resetEchoHistory` zeroes the new, larger length. V.32
measurably works at 108 and 180 and fails at 240 and 300.

**Read 1 and 2 together: the usable band is bounded below by a connect race and
above by a suspected heap overrun.** If the transport pushes you up to escape
D77 you may walk into D72. That pincer, not either entry alone, is the most
actionable thing in this appendix.

### 3. D137 / D61 — the object's answer depends on where its memory is. UNPROVEN CAUSE.

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
weakest-evidenced item here, and finding 324 records that once both sides are
brought up congruently the two pointers agree — so this may be a fixture
artefact and not a live nondeterminism. Do not quote it as established.

### 4. D112 — `PPSEG` adds symbols-at-baud to a count of 4-sample ticks. RATE, and worse the faster you go.

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

### 5. D29, D30, D32 — a family that starts from uninitialised memory. VARIANCE.

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

### 6. D84 — the predictor rounds the real axis the wrong way, every symbol. RATE.

`receiver`'s complex predictor forms the real accumulator as
`b.hist_i - (0x2000 + a.hist_q)`, applying the rounding constant with the wrong
sign on the real axis only — half an LSB, on every symbol, at both call sites
(precoding and the adapting predictor). A systematic, *axis-asymmetric* bias in
the receive predictor costs SNR margin, and at 33600's constellation density
margin is what buys the top rate. Receive-only, so it fits the asymmetry.
Reproduced; the consequence is not separable from the rest of the chain without
a full-path measurement nobody has made.

### 7. D114 — the slicer's squared distance truncates to 16 bits. RATE, speculative.

A constellation point far enough from the target wraps to a small distance and
can win, producing a symbol error. Most likely during acquisition, before the
equaliser converges, which is when errors are most expensive. The metric exists
**twice** — inlined in `decoderv34` as well as in `decision` — so a fix to one
would not reach the other. Unmeasured against a real V.34 constellation.

### 8. The rest of group 3, and why they are lower

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
