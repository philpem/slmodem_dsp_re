# Moving from GCC 13 to GCC 3.4.2: every variance, and which way it points

The object was built by **GCC 3.4.2** (`.comment`, 279 times over, finding
F606). This tree's differential tier — *the only thing that decides* — was
built by GCC 13. That is backwards, and this document is the record of
closing it: `make period` builds `src/`, `test/harness/` and `test/unit/` with
the period compiler, links them against the blob with binutils 2.15, and runs
the suite.

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

`tools/toolchain/build.sh`'s flag set is derived from the object (findings
F606, F607, F612, F616); the Makefile's is not. Three of the Makefile's must not
be passed to the period compiler:

| flag | why |
|---|---|
| `-fno-lifetime-dse` | postdates 3.4.2. Its **absence** is the period semantic, which is exactly why the modern build has to ask for it (finding F1272). |
| `-fno-pie` | no PIE to disable |
| `-fno-stack-protector` | the Gentoo `ssp`/`pie` patches were off in the object |

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
| `make phase` (GCC 13) | green, and still required |

The modern build stays. It compiles in seconds against minutes, it is the
portability check, and `make check64` still proves the tree is 64-bit clean.
It is no longer the thing that decides.

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
`dftc.c`'s deliberate widening.

**What one removal cost the modern build**, and it is the only one:
`tools/gccdiverge.json` declares `t_fft`'s four checks, 47,955 of 476,100
words. `four1`'s butterfly narrowing is not expressible in standard C under
`-fexcess-precision=fast` — not by assignment, not by `(float)` cast, not by
a named `double` temporary. Only `volatile`, which speaks to GCC 13 alone.

**The rule that made the difference**, and it fired on the very first removal:
*a shim the period compiler also needs is a fact about the object, not a thing
to delete*. Deleting `round32` outright failed the period gate at 592/4356 —
our `resample` is not unrolled where the object's is, so it never runs out of
registers. That sent the search toward a spelling that states the narrowing
instead of hoping for it.
