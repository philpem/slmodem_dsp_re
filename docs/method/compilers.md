# Moving from GCC 13 to GCC 3.4.2: every variance, and which way it points

The object was built by **GCC 3.4.2** (`.comment`, 279 times over, finding
F606). This tree's differential tier — *the only thing that decides* — was
built by GCC 13. That is backwards, and this document is the record of
closing it: `make period` builds `src/`, `test/harness/` and `test/unit/` with
the period compiler, links them against the blob with binutils 2.15, and runs
the suite.

## Current priority and fixture-validity correction (2026-09-20)

Original source, binary and functional fidelity under **Gentoo GCC 3.4.2-r2**
comes first. Modern functional correctness remains secondary portability work;
making its dashboard green is not a reconstruction objective. The historical
variance record below is retained, not permission to change source to satisfy
a newer compiler or to migrate exemptions before the inputs are understood.

Primary differential fixtures must establish valid/reachable inputs at an
explicit boundary. Constructor/reset plus arbitrary internal-field writes is
not a lifecycle witness. Component-method reachability is narrower than a
public modem connection, and assumed negotiation inputs must be named.
Synthetic adversarial tests remain useful exploratory fidelity probes, with
their raw failures visible; an impossible fixture is not grounds to weaken
any gate. Add valid lifecycle coverage rather than treating a planted state
as a primary portability requirement.

The [P4D audit](../p4d-period-fixture-audit.md) corrects the reachability reading
of the historical negative-energy sentinel: initialized short-square energies
are nonnegative, but a real zero/zero measurement boundary still generates an
unordered arithmetic path. A keep-rate flag is not a runtime NaN witness.

**And until finding F2200 the period compiler was not 3.4.2 either.** It was
Debian sarge's `gcc 3.4.4 20050314 (prerelease)`, near enough to be used and
wrong enough to matter: 18 of 183 translation units come out different, and
the exact 3.4.2 — now bootstrapped from the GNU tarball by
`tools/toolchain/Dockerfile.exact` — matches the blob on six more functions
and no fewer. This tier did not see it: 183 passed / 0 failed on both, so
every variance recorded below stands as written. The codegen tier did.
Finding F2201 for the part that remains out of reach, which is Gentoo's patch
stack rather than the version.

## Why it was backwards, in one example

Where two compilers disagree, the disagreement has to be absorbed somewhere.
With only the modern build as a gate, the only place available was the
reconstruction's own source. `src/pump/v90/Resampler.cpp`:

```c
static float
round32(float v)
{
	volatile float r = v;		/* forces a store GCC 13 optimises away */
	return r;
}
```

The object rounds an x87 accumulator to `float` because GCC 3.4.2 ran out of
registers and spilled it. GCC 13 keeps 80 bits, the difference reaches the
output, and the `volatile` was added to force the store. Every gate then went
green — with the source drifted away from what the author wrote. Finding F1352
measured it: compiled by GCC 3.4.2 the *plain* source spills anyway, and
matches the object's `faddp %st,%st(2)` stack discipline **better** than the
shim does.

That is the failure mode. The shim is not the disease; the gate that made the
shim necessary is.

## The rule for reading a variance

**A rejection in `src/` is a finding. A rejection in `test/` is plumbing.**

The author's compiler was GCC 3.4.2, so anything in the reconstruction that
3.4.2 refuses is something the author cannot have written — fix it in `src/`
and record why. The tests were never in the object; fix them freely.

And a third category that is easy to miss: **our verification apparatus.**
The offset assertions pinning every struct to the object's map are ours, not
the author's. Where the period compiler lacks what they use, the shim belongs
in `tools/toolchain/period_compat.h` — *outside* the reconstruction — rather
than in the source being reconstructed.

| | shim goes | why |
|---|---|---|
| reconstruction (`src/`, `include/`) | **nowhere** — fix the source | it is the artefact being made faithful |
| apparatus (assertions, harness) | `period_compat.h` | never in the object; may use what it likes |
| tests (`test/`) | in the test | never in the object |

### And the same rule pointed the other way

This table had no row for a construct the **modern** compiler DEMANDS, and one
had accumulated inside the reconstruction: six identical copies of a C++14
sized `operator delete` under `src/pump/v90/` (finding F7816). Each was
correct — `#if`'d out under 3.4.2, carrying no claim about the object — and
each was still apparatus sitting in the source being reconstructed. The rule
is now symmetric.

| | shim goes |
|---|---|
| a construct GCC 3.4.2 lacks | `tools/toolchain/period_compat.h`, `-include`d by the period build |
| a construct GCC 13 demands | **a flag that withdraws the demand**, in `CXXFLAGS`; failing that, a `-include`d sibling of `period_compat.h` |
| either one, inside `src/` | nowhere — it is not the author's |

**Prefer the flag, and this is not a new idea — it is V8's.** `-fno-lifetime-dse`
is already in that list for exactly this reason: the feature postdates 3.4.2,
its absence is the period semantic, and the modern build asks for the absence.
`-fno-sized-deallocation` is the second instance, and finding F7900 is where
the pattern got written down. A flag deletes the need for a shim instead of
relocating it, which is what "the shims are gone" below is about.

**And the move was MEASURED, which is the transferable part.** Position in a
translation unit is a lever-3 carrier: consolidating the tree's *unsized*
`operator delete[]` into `sysdep.h` cost eight destructors their byte identity
(finding F7815), so "it is only plumbing" is not a licence to move plumbing.
The sized form was exempt *by construction* — the period compiler never
received its tokens — and that prediction was checked object by object rather
than assumed: all 200 period objects byte-identical across the move, `md5sum`
against `md5sum`, with `make phase` green either side.

---

## The variances

### V1 — `__builtin_offsetof` is GCC 4.0+ · 127 sites · APPARATUS

Every struct map is pinned by

```c
typedef char x_off_field[ ((int)__builtin_offsetof(T, field) == N) ? 1 : -1 ];
```

GCC 3.4.2 has no such builtin and parses `field` as an ordinary identifier,
so the error it gives names the *member* and not the builtin — misleading if
read quickly.

**Resolved in `period_compat.h`**, with the classic null-pointer expansion
`<stddef.h>` itself used before the builtin existed. Verified a constant
expression in this position on both a POD and a polymorphic class, where it
correctly accounts for the vptr's four bytes.

One site needs argument-count dispatch:
`__builtin_offsetof(Scrambler<unsigned char, int>, field)` — the preprocessor
counts the template's comma as a separator. Macro arguments are token
sequences, so `Ta`/`Tb` receive `Scrambler<unsigned char` and `int>` and
pasting `Ta, Tb` reassembles exactly what was written.

### V2 — opaque enums with a fixed base are C++11 · 9 sites · RECONSTRUCTION

`enum V90ComputationalMode : int;` is C++11. GCC 3.4.2 rejects it outright,
so it is not what the author wrote. C++98 has no way to declare an enum
without defining it, so these became definitions. What the spelling had to
preserve was **measured, not assumed**:

| property | result |
|---|---|
| mangling | identical — `_Z1f7ModeOld` either way; enums mangle by name |
| `sizeof` | 4 bytes for every spelling tried, both compilers |
| signedness of `: int` | an **empty** `enum E { }` matches it exactly |
| signedness of `: unsigned int` | needs a pin — see below |

The signedness is **FORCED**, not free. `V90ModemSide`'s base is `unsigned`
because the destructor's range test at 0x192c2 is `cmpl $0x1,0x49bc(%esi);
jbe` — an unsigned compare:

| C++98 spelling | GCC 13 | GCC 3.4.2 |
|---|---|---|
| `enum E { }` | `jle` | `jg` | 
| `enum E { P = -0x7fffffff - 1 }` | `jle` | `jg` |
| `enum E { P = 0xffffffffu }` | **`jbe`** | **`ja`** |

So an empty enum would have silently changed an emitted branch.

**An empty enum is wrong for a second reason**, and it is the one that
decided the spelling. Its value range is 0..0, so every cast this type exists
for becomes undefined: `V92Modem.h` depends on `(V92ModemSide)2` being well
defined to drive the "illegal modemSide" arm both functions carry, and
`__tHardwareCodecTypes__` takes a cast from a **runtime int**
(`_tagModemParameters::codecType` at +0x54). A single pin enumerator restores
the full range.

The `_BASE_PIN` enumerators are **ours**. The object names no enumerator —
the mangling carries the type's name and nothing about its contents — so a
named enumerator would be a guess in the record. The pin fixes the underlying
type and claims nothing else.

**One type, one home.** These were spelled in two headers each, which an
opaque *declaration* permits and a *definition* does not. Each now has one
home; `tools/onedef.py` is the gate that keeps it that way.
`include/dsplib/V90CodecType.h` exists because `V90ModemCtor.cpp` needs
`__tHardwareCodecTypes__` and must not have `V90PreFilter.h` (finding F1112).

### V3 — `__SIZEOF_POINTER__` is GCC 4.6+ · 81 guards, 78 files · **SILENT**

The dangerous one. The offset assertions are guarded:

```c
#if __SIZEOF_POINTER__ == 4
```

Under GCC 3.4.2 that predefine does not exist, so the guard reads `#if 0` and
**every assertion inside vanishes**. A translation unit compiles perfectly
clean with its structural checks deleted — which is strictly worse than
failing, because it looks like success.

`-D__SIZEOF_POINTER__=4` restores them. With them on, all 152 TUs still
compile, which is the first evidence the maps hold under the period compiler
at all.

*The general lesson: a feature test that is a `#if` on an undefined macro
fails OPEN. Prefer one that fails loudly.*

### V4 — GCC 3.4 defaults to `gnu89` · 1 file · PLUMBING

`test/harness/harness.c` declares a variable in a `for` initialiser. Nothing
in `src/` needed a newer dialect; `-std=gnu99` covers it.

### V5 — linkonce comdat naming · `tools/refrename.py` · APPARATUS BUG FOUND

`.gnu.linkonce.NAME` is name-based COMDAT: the linker keeps the first section
of a given name and discards the rest. GCC 3.4 puts every instantiated
template member and every vtable in one. `refrename.py` moves the blob's
copies out of that namespace so ours cannot displace them.

It only ever matched `.gnu.linkonce.**t**.` — the text ones. The blob also
carries four `.gnu.linkonce.**r**.*`, which for GCC 3.4 is where a **vtable**
lives, and those went straight through. Nothing noticed while the modern
build was the only consumer, because GCC 13 emits vtables into section groups
with different names and there was nothing to collide with. Compile our side
with the period compiler and the link fails:

```
ref__ZTV12V90Resampler: discarded in section
`.gnu.linkonce.r._ZTV12V90Resampler' from build/dsplibs_ref.o
```

Now generic over the kind letter, and moving each to the matching ordinary
section — a vtable into `.text.ref_*` would have been executable data. 79
text + 4 rodata = 83.

### V6 — binutils 2.15 program headers · LINK · `-static -Wl,-N`

A dynamic link fails with `Not enough room for program headers (allocated 8,
need 9)`; a plain static link fails the same way one segment lower. The blob
carries more sections than ld 2.15's default layout leaves segments for.

`-N` (OMAGIC) puts text and data in one writable, non-page-aligned segment,
so no extra program header is needed. It costs a writable text section in a
test binary. The alternative — a hand-written linker script with a `PHDRS`
block — buys the same result and one more thing to maintain.

### V7 — unrestricted unions are C++11 · 5 tests · PLUMBING · **CLOSED**

`union slot { T o; unsigned char raw[N]; slot(){} ~slot(){} }` is C++11: the
explicit empty pair is itself the C++11 workaround, and C++98 forbids a union
member whose type has a non-trivial constructor or destructor outright. GCC
3.4.2 said *"member with constructor not allowed in union"* for eight slots
across `t_scrambler`, `t_v90leaves`, `t_v90p3dreset`, `t_v90p3mod` and
`t_v92p3mod`.

**613 use sites, 8 declarations — only the declarations changed.** `o` became
a REFERENCE bound to the raw bytes, so every `x.o.member()` still reads as it
did:

```c
struct mod_slot {
	union {
		unsigned char raw[SLOT];
		double align_;		/* alignment only; trivial */
	};
	V92Phase3Modulator &o;

	mod_slot() : o(*(V92Phase3Modulator *)raw) {}
};
```

The `double` is not decoration. These tests compare object layouts byte for
byte, and a bare `unsigned char` array guarantees alignment 1. `t_v90leaves`
already used exactly this union for its `sd_slot` and `sv_slot`, so the idiom
is the file's own.

**AND IT HAS ONE SHARP EDGE, which cost a segfault before it was found.** A
union can be *cast onto* raw memory, because `s->o` is then a reinterpretation
of bytes that are already there. A struct with a reference member cannot: the
reference is a stored pointer that only a real constructor ever writes.
`t_v90p3dreset`'s `snap()` did

```c
struct p3d_slot *s = (struct p3d_slot *)dst;   /* dst is a raw buffer */
s->o.field = ...;                              /* dereferences nothing */
```

It compiled and died on the first store. The repair is also the clearer
spelling — `dst` holds a copy of the OBJECT, so it is cast to the object:
`V90Phase3Demodulator *s = (V90Phase3Demodulator *)dst;`. One site in five
files; grep for a cast to a slot pointer before applying this idiom
anywhere else.

### V8 — flags that must NOT cross over

`tools/toolchain/period.mk`'s flag set is derived from the object (findings
F606, F607, F612, F616); the TOP-LEVEL `Makefile`'s is not. Three of the
top-level Makefile's must not be passed to the period compiler:

| flag | why |
|---|---|
| `-fno-lifetime-dse` | postdates 3.4.2. Its **absence** is the period semantic, which is exactly why the modern build has to ask for it (finding F1272). |
| `-fno-sized-deallocation` | the same argument, C++14 rather than a GCC pass. Sized deallocation is what makes GCC 13 call `_ZdlPvj` for `delete p` on a class with a destructor; 3.4.2 has none to prefer, so asking GCC 13 not to prefer one makes both compilers resolve the delete-expression identically (finding F7900). |
| `-fno-pie` | no PIE to disable |
| `-fno-stack-protector` | the Gentoo `ssp`/`pie` patches were off in the object |

**Two of those four are one pattern and it is worth naming**, because it is
where a modern-side shim goes instead of into `src/`: *where GCC 13 has a
feature the period compiler never had, turn the feature off rather than write
source that appeases it.* Both of these were found the same way — a defect the
modern build could see and the period build could not.

### V9 — two tests that failed against the blob · **CLOSED, and it was NEITHER compiler**

| test | disagreement |
|---|---|
| `t_v90equ` | `block size` — got 132, reference 140 |
| `t_v92precoder` | `stored in the reference's allocation order` — got 0, reference 1 |

**It was the C LIBRARY, and the argument needs no disassembly.** Our
`V92Precoder::V92Precoder` allocates `fir1` then `fir2` in two sequenced
statements — no compiler may reorder them — and the blob is a fixed binary
whose behaviour cannot vary with what compiles the other half of the process.
The only term left that changes between the two builds is libc: the period
build links a 2005 **static** glibc.

Both allocators recycle LIFO, *identically* — 40 trials of
`a=malloc(0x14); b=malloc(0x14); free(a); free(b)`:

| libc | non-monotonic pairs |
|---|---|
| 2005 static glibc | **20 of 40** |
| modern glibc | **20 of 40** |

The destructor frees `fir1` then `fir2`, so the next construction gets
`fir2`'s chunk first and `fir1 < fir2` **inverts on alternate trials**.
Neither libc is monotonic; our side being in phase with the blob's was luck
that held under one build and not the other. `malloc_usable_size` is the same
mistake in a second costume — it reports the chunk served, and glibc hands
over a remainder too small to split, so two equal requests read 132 and 140.

**The checks meant to ask something real; the proxy was not.** "Did `fir1`
get the FIRST allocation" and "is our block the same size as the blob's" are
genuine properties of the reconstruction. The allocator is the only thing
that knows either, so it records both now:

```c
unsigned long harness_alloc_ordinal(const void *p);   /* 1, 2, 3, ... */
unsigned      harness_alloc_reqsize(const void *p);   /* bytes asked for */
```

**The durable lesson: a differential check must compare something both sides
COMPUTE, not something the environment hands them.** Of the three properties
of an allocation a test can see — address, usable size, ordinal — only the
last is a fact about the code under test. Finding F1353.

---

### V10 — a check only the period compiler can make · 1 test · APPARATUS

`test/unit/t_v90equ.cpp`'s `V90Equalizer: the coefficient sums are float`
group is compiled under `#if defined(__GNUC__) && __GNUC__ < 4` and nowhere
else. It is the only compiler-version conditional in `test/`, and it is
registered here because an unexplained one is exactly what V3 was.

`convertEqualizerToMmx` accumulates four coefficient sums in `float`, storing
each back to a four-byte slot every iteration (`fstps 0x4c(%esp)` inside the
loop). GCC 3.4.2 spills the same way; GCC 13 under `-fexcess-precision=fast`
holds the accumulator in an x87 register across the whole loop and rounds
once at the end. So under the modern build a `float` accumulator and a `long
double` one give the SAME answer, and the check would pass for the wrong
source and fail for the right one -- not a check that is unavailable, a check
that says the opposite of the truth.

`tools/gccdiverge.json` was tried first and taken back out. It is the
declared route for "modern GCC provably cannot", and two narrowing spellings
had already failed -- `(float)(fsum + c)` and `(float)((double)fsum +
(double)c)`, the second being the one that repaired `Resampler.cpp` -- so the
precondition was met. It fails on the tooling instead: `tools/mutate.py` does
not consult the register, builds with the modern compiler, and treats a binary
with any failing check as a dead shard, so one entry took the whole
24-mutation `v90equ` suite down. A guard that asks the question of the
compiler that can answer it costs nothing and breaks nothing.

`make period` passes the witness -- 65536.0f plus nineteen taps of 0.0005f,
which is `+65536.0000` in `float` and `+65536.0078` in extended -- and that is
what says the source is right. Finding F2153, and finding F2139 for the rule it
applies.

## Where it stands

| | |
|---|---|
| `src/` under GCC 3.4.2 | **152 of 152** compile (was 132 of 152) |
| period differential | **155 of 155 pass** |
| `make phase` | period/structural reconstruction gate |
| `make portability` | modern GCC, 64-bit, interop and coverage/debug-site gate |

The modern build stays as an explicit opt-in. It compiles in seconds against
minutes, it is the portability check, and `make check64` still proves the tree
is 64-bit clean. It is not the default gate and not the thing that decides.

## The shims are gone

Stage 2 of task #113, all of it gated on the period differential. Finding F1354.

| site | outcome |
|---|---|
| `Resampler.cpp` `round32`'s `volatile` | removed — **and so was the helper**; a `double` accumulator narrowed by explicit conversion satisfies both compilers |
| `fft.cpp` `volatile float tempr, tempi` | removed; GCC 3.4.2 spills from plain source |
| `fft.cpp` four `(double)` casts in `realfft` | removed; they had changed float arithmetic to double, a semantic alteration |
| nine files citing GCC 13 | all explanation, no shims |

`src/` and `include/` contain **no `volatile` outside a comment**. Every
remaining `(double)` is an integer conversion, a `sizeof`, a libm argument or
`DFTC.c`'s deliberate widening.

**What one removal cost the modern build**, and it is the only one:
`tools/gccdiverge.json` declares `t_fft`'s four checks, 47,955 of 476,100
words. `four1`'s butterfly narrowing is not expressible in standard C under
`-fexcess-precision=fast` — not by assignment, not by `(float)` cast, not by
a named `double` temporary. Only `volatile`, which speaks to GCC 13 alone.

**ONE CAME BACK, AND IT WAS NOT A `volatile`.** Finding F7816 needed a C++14
sized `operator delete` for the modern build and put it in six `.cpp` files.
Nobody read it as a shim, because it is `#if`'d out under 3.4.2 and therefore
cost the object nothing — and that is exactly what makes this class hard to
see. **A shim for the compiler that does NOT decide is invisible to every
measurement in this document**: the period differential cannot fail on it, the
codegen tier cannot see it, and `byteident` reads the same number either way.
Only reading the file finds it.

Removed by `-fno-sized-deallocation` rather than relocated, which is this
section's own rule applied to it: with the feature off, `delete p` reaches the
unsized `operator delete` those files already define. `src/` and `include/`
now contain no C++14 construct and no `__cplusplus` version test at all — one
grep, and it is a stronger invariant than "no `volatile`" because it is what
the shim was wearing. Finding F7900.

**The rule that made the difference**, and it fired on the very first removal:
*a shim the period compiler also needs is a fact about the object, not a thing
to delete*. Deleting `round32` outright failed the period gate at 592/4356 —
our `resample` is not unrolled where the object's is, so it never runs out of
registers. That sent the search toward a spelling that states the narrowing
instead of hoping for it.

## The modern LINK tier: flags, and a declared exception (issue #82)

Finding F11359.  The binding pass made seven symbols `static` to match the
reference's LOCAL records, and GCC 13+ then transforms or removes them, so a
differential fixture that names one cannot link in the modern tier.  This is a
DIFFERENT stage from the check divergences `tools/gccdiverge.json` declares:
the binary does not exist to run.

Measured on GCC 14.2.0-19, 13 of 376 fixtures failed to link.  Three shapes:

| shape | symbols | disposition |
|---|---|---|
| renamed | `AnalyseDialString` (Dialer.c), `bValidateEnergyValue` (Fdspkrnl.c) | `-fno-partial-inlining -fno-ipa-cp` on the host tree |
| eliminated static | `pGlobalFDSPObj`/`uCorrelationReportsNo` (Fdsp.c), `v34initialbauds` (VpcmFloModem.cpp), the V.22/V.32 tables | `-fno-toplevel-reorder`, PER-TU |
| template weak copy | Agc, DiffCoder, LowPassFIR, Queue, Scrambler, SineWave | no flag; declared |

`-fno-toplevel-reorder` is F11359's correction: it was not tried there, and it
keeps every eliminated static.  It is applied only to `Fdsp.o`, `V22.o`,
`V32.o` and `VpcmFloModem.o` because globally it also moves a `static`
function's convention from regparm(3) back to regparm(0), and three fixtures
call two of those functions directly.  `Makefile`'s `HOSTPORTFLAGS` is the one
source; `make period` builds its own tree with the period compiler and never
sees any of it.

The binding pass also changed those two functions' MODERN calling convention
(regparm(2) -> regparm(3), the object's GCC 3.4.2 capping a static at 2), and
`t_dialer`/`t_dialstring`/`t_fdspkrnl` declared the object's regparm(2) for our
copy.  The link fix exposed it as divergence and a segfault; the declarations
now follow the building compiler, exactly as `EchoCanceler` in `t_fdspkrnl.c`
already did.  `test/`, not `src/`.

The template copies are issue #77: only `-fno-inline` keeps them and that
changes every TU.  Their six fixtures are declared in `tools/linkdiverge.json`,
a sibling of `tools/gccdiverge.json` with the same discipline -- it names a
fixture and its symbols, excuses the fixture only when the linker's undefined
set is a subset of them and every error is an undefined reference, treats a
now-linking entry as STALE, is never consulted by `make period`, and does NOT
run the fixture (no stub binary).  `make linkexc` prints
`link exceptions: N declared, M excused, 0 stale (K binaries built)` and the
portability boundary refuses to print its OK line without that denominator,
like `COVCOUNTS`.

Before: 13 fixtures unlinked, 0 declared.  After: 7 link and pass (3 rename +
4 static), 6 declared link-exceptions, 0 unexcused link failures, 0 stale.
`make period J=1` 376 passed / 0 failed.  The modern test tier remains red on
GCC 14 for the x87 reasons of #30 -- a control rebuilt the failing TUs without
`HOSTPORTFLAGS` and reproduced the failures exactly -- which is not this tier.

**The modern x87 census and the register reconciliation are in
`docs/issue30-modern-x87.md` (finding F11363).**  After it: 347 of 376
fixtures green, 29 uncovered modern-only divergences (all green on the period
compiler), `tools/gccdiverge.json` at 6 entries / 14 checks with 0 stale and 0
uncovered, and the `sinc<float>` return-narrowing domain bounded -- no tested
flag reaches `fsin` + a single double pi load + binary32 return narrowing.
`make portability` remains red on those 29 and on a pre-existing
`mutation-snapshot` state (2 MISSING, 270 stale).

(2026-09-19)

## The modern tier is a portability check: functional, not byte-exact

The project owner's rule, and it is the rule that governs everything below:

- **`make period` (GCC 3.4.2-r2 Gentoo) is the reconstruction authority.**  It
  stays byte/value-EXACT against the blob and has **no allow-list**.  Nothing
  in this section widens it, and nothing may.
- **The modern tier (GCC 14, x32 -> x64) is a PORTABILITY check.**  It must
  produce a FUNCTIONALLY CORRECT result.  It is **not** required to reproduce
  the blob's exact code or its exact x87 values; crossing x32->x64 legitimately
  changes codegen and rounding, and a rounding-level difference there is the
  compiler and not the source.
- **The tolerance is modern-tier-only, documented, denominator-reporting, and
  must never excuse a period failure.**  A period failure is a hard failure
  whatever the modern tier says.

### The mechanism: `HARNESS_FLOAT_TOL`

`test/harness/harness.c` gains a RELATIVE float tolerance inside
`diff_eq_float_` only, compiled in **only** when the Makefile defines
`HARNESS_FLOAT_TOL` on the modern harness object:

```make
HARNESS_FLOAT_TOL := 1e-6
$(HARNESS_OBJ): CFLAGS += -DHARNESS_FLOAT_TOL=$(HARNESS_FLOAT_TOL)
```

| property | how |
|---|---|
| tier-gated | a **Makefile-provided define**, never a `__GNUC__` version test |
| period untouched | `period_inner.sh` compiles `test/harness/` from its own flag list and cannot see that line; with the macro undefined the tolerance code is absent and the compare is bit-for-bit |
| criterion | `|a-b| <= eps * max(|a|,|b|)`, RELATIVE -- never an absolute slack, so a difference near zero still fails |
| call-site budgets win | applied only where the call site passed neither a ULP nor an absolute budget; an explicit `diff_eq_float_ulp`/`diff_eq_float_abs` keeps its own bound |
| reaches no decision | it is in `diff_eq_float_` alone.  A transcript `strcmp`, a `diff_eq_int` decision/index and a raw `diff_eq_obj` byte compare are untouched and stay hard failures |
| denominator | `diff_float_tolerant` counts checks that passed ONLY via the tolerance; `diff_end` prints `(N within modern tolerance)` beside the check count, and the counter is reset per group |

`1e-6` is ~8 ULP at any binade.  The largest measured harness-reachable modern
divergence is **2 ULP** (`t_v90cdesign`, F11364); tighter would leave a
rounding-level difference failing, wider would start to hide a real error.

### The negative control, and why the rule needs one

A tolerance is a detector, and a detector that cannot fail is dead (F134,
F2400, F2401).  `test/safety/t_float_tol.c` (`make safety`) is the control; it
is **adaptive**, asking the LINKED harness (`harness_float_tol()`) which arm it
is in, so it is meaningful in either build:

| case | with the define | without (period-style) |
|---|---|---|
| exact | pass | pass |
| 1 ULP apart | **pass**, counted tolerance-only | **fail** (bit-for-bit) |
| ~84,000 ULP apart | fail | fail |
| `0.0` vs smallest subnormal | fail (relative, not absolute) | fail |

Measured: `PASS t_float_tol: 4 checks, 0 bad (linked harness tol=1e-06)` on the
modern harness object and `... tol=0` when the same source is linked against a
no-define harness object.  The beyond-eps case failing in BOTH builds is the
part that proves the tolerance is a tolerance and not an off switch.

### What it does and does not close

`t_v90cdesign` goes green -- its 71 checks are 1-2 ULP `diff_eq_float` compares,
and it prints `PASS ... 496 checks (71 within modern tolerance)`.

**It does not make the 29-fixture census green, and the measurement says it
cannot.**  Of the 29, only `t_v90cdesign` fails through `diff_eq_float`.  The
rest fail through forms a float tolerance must not touch:

- **decision-level** -- a bank index (`t_v90prefilter`: `-2` vs `3547`), a
  mapping design's ucodes (`t_v90trn2design`), a "decision" value
  (`t_v90p3ddec`: `-1480` vs `0`), a verdict and counts (`t_v90spectral`), a
  flag byte (`t_v90modprog`).  These are changed OUTCOMES, not rounding, and
  they stay hard failures;
- **non-float** -- `t_dspmath`'s `hamming`/`blackman` at n==1 produce a finite
  `0.08` where the blob produces the x87 indefinite NaN; `t_v34hshak` SIGSEGVs;
  `t_v27fax` is FAX;
- **raw byte / boolean comparisons the harness cannot type** -- `diff_eq_obj`
  over structs containing floats (`t_floatarma`, `t_gtonedet`, `t_v92dec`,
  `t_vpcmrunpcm`'s region words, ...), and `strcmp` of debug transcripts
  (`t_v90cdadjust`, `t_v90cdnoise`, `t_v90dataph`, `t_v90demod`, ...).

Those are not closable by a harness tolerance: `diff_eq_int` on `0`/`1` is a
decision, and making it float-tolerant would excuse every boolean in the suite.
`diff_eq_obj` has no field-type information at runtime.  The transcript
question was left **to the register** rather than parsed: a transcript encodes
decisions as well as formatted floats, and parse-and-compare would hide the
former to catch the latter.  F11364 records the full classification.

**The rule is therefore: the tolerance is correct and it is now in place, but
it closes the rounding-level `diff_eq_float` divergences only.  The remaining
modern reds need per-site work or the register, and the decision-level ones
must stay red.**  Do not read "the modern tier is functional" as "the modern
tier is green".

(2026-09-19)

### The mixed abs+rel budget, for values that pass through zero

The criterion above, `|a-b| <= eps*max(|a|,|b|)`, is the right shape for a
value away from zero and the WRONG shape for one that crosses it: the same
~3e-5 absolute sinc/FIR error is rel = 1e-6 on a coefficient near 1 and
rel = 1.585 on a resampled sample near 2e-5.  A relative budget wide enough to
cover the latter is an off switch for the former.

`harness_float_tol_fixture_mixed(atol, rtol)` supplies the standard allclose
criterion instead:

```
|a-b| <= atol + rtol * |b|          (b = the reference, the blob's value)
```

Same reach and same tier-gating as the pure-relative setter (`atol` 0 there,
so the two cannot be confused; the setters clear each other; the period build
returns 0 for both), same `diff_float_tolerant` denominator, and it still
reaches no `diff_eq_int`, no transcript `strcmp` and no unnamed byte.

Four fixtures whose float difference is the sinc/FIR design divergence use it;
each records its measurement beside the call and each keeps its hard failures:

| fixture | measured near-zero / functional | atol / rtol | residual |
|---|---|---|---|
| `t_v92modstate` | max \|diff\| 3.26e-5 (\|ref\|<1e-4); max rel 4.191e-2 | 1e-4 / 5e-2 | 4 queue checks, abs 0.353 |
| `t_resampler` | max \|diff\| 4.4e-6; max rel 8.274e-3 | 1e-5 / 1.5e-2 | 8 NaN-phase decisions |
| `t_v90demprog` | max \|diff\| 2.38e-7; max rel 2.607e-5 | 1e-6 / 5e-5 | 4 sample/status decisions |
| `t_floatarma` | func near-zero 3.5e-9; func max rel 2.3e-5; adversarial from 1.2e-4 | 1e-6 / 1e-4 | 734 adversarial checks |

The negative control is `test/safety/t_field_typed.c`, which drives every one
of those budgets: at each, a zero-reference difference under `atol` and a
normal value inside `rtol*|b|` pass modern-only and count tolerance-only, a
value beyond the floor and one beyond the relative half fail, a changed index
and flag still fail, and against a no-define harness object the setter is inert
so even the inside value fails.  Measured: `PASS t_field_typed: 48 checks, 0
bad (linked harness tol=1e-06)` and the same source `tol=0`.

The mixed form closes the near-zero group (87 checks in `t_v92modstate`) and
moves no fixture from red to green: the residual in the table is decision-level
or adversarial and stays red.  Finding F11367.

(2026-09-19)

