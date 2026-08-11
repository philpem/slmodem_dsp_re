# Task #60 — the V.90/V.92 C++ core

A working checkpoint for the 50 C++ symbols (16,003 bytes) that
`v34handshak` reaches and this tree has not written.  `docs/fastpass.md` says
why #60 is a hard predecessor; this file is the part a resuming session needs
in front of it.

## The constraint that shapes every commit — measured, not assumed

Every test binary links all of `$(OBJ)`, the harness, and `dsplibs_ref.o`.
The reference object has had *every* symbol it defines renamed to `ref_*`, so
**it can never satisfy an undefined reference from our side.**  A C++ method
that calls a method nobody has written yet therefore does not fail its own
test — it fails the link for the whole suite.

Measured on this branch, by compiling a one-method `.cpp` containing a single
call to an undeclared-but-undefined method and running `make -k all`:

    69 of 69 test binaries failed to link, with one undefined reference each

`make phase` without `-k` stops at the first, `t_encode`, which is the least
informative binary in the suite and names none of the C++ involved.  So the
symptom of an unclosed batch is *`t_encode` fails to link*, and the cause is
never in `t_encode`.  Recorded because that is a confusing half hour if you
meet it without warning.

**Therefore work lands in callee-closed batches.**  A batch is a set S where
every callee of every member of S is either in S or already in `src/`.  That
is *not* the same as marching down `callgraph.py --order`, which is a
topological sort of the whole closure; compute the closure per target with
`callgraph.py --order --of <symbol>` and take what it reports as missing.

## The mangling is a free type oracle, and also an obligation

`c++filt` recovers every signature, with the author's own enum and struct
names — argument count, order and types for all 50 without reading an
instruction.  The C half of this project has never had that.

It runs the other way too.  Reproducing the mangled symbol means reproducing
the declaration exactly: class name, method name, every parameter type and
cv-qualifier.  An `int` where the original had `unsigned`, a tidier name, a
dropped `const` — and the compiler emits a *different* symbol which links
against nothing, and the method is silently not the function.  The mangled
name is the specification.

`src/dsp/FloatIIR.cpp` with `test/unit/t_genericiir.cpp` is the working
precedent: ordinary C++ carrying the original's own names, and the compiler
emits the blob's exact symbol.

## Batches, in dependency order

Closures below are from `callgraph.py --order --of`, with `have` entries
(already in `src/`) omitted.  `edprintf`, `ulaw2linear`, `alaw2linear`,
`linear2ulaw`, `linear2alaw`, `bitreverse` and `txrxdmainit` are all `have`.

| # | batch | members | closed? |
|---|-------|---------|---------|
| 0 | warm-up (#59) | ~~`getbit` 433, `ApplyBulkDelay` 467 — both leaves; `getMPrecvdBits` 895, callees all `have`~~ **DONE** — finding 227; `getMPrecvdBits` needed a `.cpp` after all, and it is the precedent for a `ref_` alias that is `extern "C"` *and* `regparm` | yes |
| 1 | `V90Jd` / `V92Jd` | ~~`V90Jd::getBitVector` 537, `unPackReset` 20; `V92Jd::packJdData` 665, `packJdPhaseData` 681, `getJdBitVector` 22, `getJdPhaseBitVector` 22, `unPackJdReset` 20, `unPackJdPhaseReset` 20~~ **DONE** — findings 229 and 230; all eight, and the C++ class fixture the later batches copy | all leaves |
| 2 | `V90Phase3Modulator` | ~~`generateV92Symbol` 2044, `generateV90Symbol` 1790, `reset` 479, `resetDILGenerator` 410, `setSessionFlag` 11, + 64 B table~~ **DONE** — findings 231 and 232; all six, **plus four weak `Scrambler<unsigned char,int>` members nothing counted** | it was not — see below |
| 3 | `V90PreFilter` | ~~`selectFilter` 800, `setParamEia6` 790, `autoSelection` 359, `isV90WithEia6` 69, `displayParamEia6` 1, all of `FloatFIR` 723~~ **DONE** — findings 234, 235 and 236; **23,860 B of tables, not 10,532** | it was not — see below |
| 4 | the leaf remainder | the stubs and setters — see the table below | mostly leaves |
| 5 | `VPcmFloModem`, `V90Phase3Demodulator` | the two whose objects reach through into an enclosing session | last |

**`V90PreFilter` moved from 2 to 3.**  It first looked like the lightest batch
and is in fact the heaviest of the three: ~2 KB of code, plus 10,532 B of
static tables, plus the whole of `FloatFIR`.  The Jd family is the batch that
proves the C++ fixture pattern with the fewest moving parts — eight leaf
methods, objects of 140 and 216 bytes, no static data, no vtables — so it goes
first, and `V90Phase3Modulator` follows because it needs Jd and carries only
64 B of table.  Table extraction is a different kind of work and should not be
entangled with proving the fixture.

`FloatFIR` is taken whole (723 B unique across six members) rather than just
the `setCoefficients` that `selectFilter` calls: the class must be declared
either way, and leaving five members unwritten only re-opens the batch for
whatever calls them next.  `nm` totals 858 because the constructor and
destructor each appear twice — C1/C2 and D1/D2, byte-identical — which GCC
emits automatically from one definition.

## The THIRD closure: weak template members, which `callgraph.py` also cannot see

Finding 231.  `callgraph.py` enumerates `T` symbols.  An implicitly
instantiated C++ template member is `W`, in its own `.gnu.linkonce.t.*`
section, and the blob has **thirty-one** of them across `Scrambler<h,h>`,
`Scrambler<h,i>`, `Scrambler<i,h>`, `Descrambler<h,i>` and
`Descrambler<i,i>`.  They are renamed `ref_*` like everything else, so one
left unwritten fails the link for all seventy binaries with the same
`t_encode` symptom as an ordinary unwritten callee.

Batch 2 was briefed as closed and was not: `V90Phase3Modulator::reset` calls
`Scrambler<unsigned char,int>::reset` and both `generate*Symbol` call
`process`, which calls `resetHistoryIndexes` and `copyHistoryTail`.  Four
functions, 216 bytes, invisible to the tool.

**So a batch's closure is `callgraph.py`'s answer plus the `W` symbols its
members reference.**  Get them from the relocations:

```
python3 tools/dis.py ../slmodemd/dsplibs.o <symbol> | grep -o 'R_386_[A-Z0-9]* .*' | sort -u
```

`FloatFIR` (batch 3) and `LowPassFIR<float>` are templates too.  **Neither
turned out to be one in the blob**: `FloatFIR`'s six members are all `T`, and
nothing batch 3 wrote reaches `LowPassFIR<float>` or any other weak symbol.
Batch 3's relocation sweep over all eleven of its symbols found `edprintf`,
`sysdep_malloc`, `sysdep_free`, the four static data symbols and nothing else.

**And `tools/dis.py` mis-disassembles every one of the thirty-one**: `st_value`
is 0, which it reads as a `.text` offset, and it prints unrelated bytes with
relocations from other sections interleaved.  Use
`objdump -dr --section=.gnu.linkonce.t.<symbol>` for those, and do not trim
its output — the relocation lines are the point.

## The OTHER closure: static data, which `callgraph.py` cannot see

`callgraph.py` tracks calls.  The 50 also reference **static class data
members**, which are ordinary defined data symbols in the blob, therefore
renamed `ref_*`, therefore not satisfiable from the reference object — the
same total link failure as an unwritten callee, and invisible to the tool that
enumerates the closure.

Measured by disassembling all 50 and collecting `R_386_*` targets that resolve
to defined data symbols:

```
 4960  D  V90PreFilter::preFilterCoefType3                       <- 1 fn
 2480  D  V90PreFilter::preFilterCoefType2                       <- 1 fn
 2480  D  V90PreFilter::preFilterCoefType1                       <- 1 fn
  612  D  V90PreFilter::dataBase                                 <- 3 fn
   64  D  V90Phase3Modulator::codeSegmentsBoundriesLookupTable   <- 3 fn
-----
10596  bytes across 5 symbols
```

**So #60's real payload is 16,003 + 10,596 = 26,599 bytes**, and every count
that says 16,003 is counting text only.  It lands entirely on batches 2 and 3;
every other batch is data-free.  `tools/tabdump.py` is the right tool, and
`src/pump/b103/b103_tables.c`, `src/dsp/fpm_iir_coeffs.c` and
`src/core/rc_coeffs.c` are the precedents.

### `refLoopsType*` is batch 3's after all — data referencing data is a fourth closure

`V90PreFilter` has **ten** static members totalling 23,860 B, not the nine at
~21 KB an earlier estimate gave, and there is no `refLoopsType3`.

A first pass split them, and the split was wrong.  It is kept here because the
*reasoning* is the trap and deleting it would let the next person repeat it —
every one of the six `refLoopsType*` tables is referenced by exactly one
function, and it is `VPcmV34InitiateRetrain` every time, which is a #59 C
function outside this task:

```
WRONG:
#60 batch 3     preFilterCoefType1/2/3 + dataBase          10,532 B
#59 retrain     refLoopsType1,2,4,5,6,7                    13,328 B
                                                    total  23,860 B
```

**THAT SPLIT IS WRONG, AND IT COST THE BATCH AN HOUR.**  It was derived from
`.rel.text`, and the sixteen relocations that matter are in `.rel.data`:
`dataBase` is read by three of batch 3's five methods and its own definition
points at `refLoopsType1, 2, 4, 5, 6` and `7`.  Data referencing data is a
FOURTH closure, invisible to `callgraph.py`, to a `.rel.text` sweep and to the
weak-symbol check of finding 231.  All ten static members are batch 3's, and
#60's static data is **23,924 B**, not 10,596.  Finding 234.

## Virtual classes: the vptr shifts every field by four

The object has four vtables — `Resampler`, `V90Resampler`, `ResamplerTiming`,
`ResamplerTimingOffset` — and `ResamplerTimingOffset::setTimingOffset` is one
of the fifty.  Finding 228 has the detail.  What matters when sizing an object
here: **the largest-displacement bound stays right, but the field map derived
from it is shifted four bytes** for those four classes, and a struct correct in
size and wrong by four in every offset passes a size check and fails
everything after it.

`tools/cppstruct.py <class>` answers "is this polymorphic" without going near a
vtable: a destructor listed with a `D0` variant is a deleting destructor, which
GCC emits only for a virtual one.  Check it before laying out any class.

Do **not** declare real `virtual` members to model this.  GCC then emits a
vtable, which needs every virtual method defined or a key function present,
re-opening the link closure for what may be a twenty-one-byte setter.

## Object sizes

Bounded by the largest `this`-relative displacement each class uses
(finding 215).  `V90Phase3Modulator` is **920** measured (below) and
`V90PreFilter` 1,280 is still a bound.  Both are the tractable shape: allocate a buffer, call `reset` on both sides, compare with
`diff_eq_obj` — but seed the buffer with varied bytes rather than zeroing it,
which is finding 230's first rule and the reason batch 1's clear loops could
be checked at all.

**A DISPLACEMENT IS NOT A SIZE.**  Batch 1 measured this: `V90Jd`'s largest is
+0x8c and `V92Jd`'s +0xd8, and both are four-byte stores, so the objects are
**144 and 220** bytes — not the 140 and 216 this document used to give.  Add
the width of whatever sits at the bound before allocating anything; 916 and
1,280 above were bounds and had not had that addition made.
`V90Phase3Modulator`'s +0x394 is a one-byte store, so the object is
**0x398 = 920**; finding 231 has the full field map, which batches 3 and 5
should read rather than re-derive.  1,280 is still a bound.

`V90Phase3Demodulator` reaches 43,336 and `VPcmFloModem` 32,612.  This file
used to say that both "almost certainly" indexed *through* `this` into an
enclosing session object rather than being that large.  **That was a
hypothesis, and for `VPcmFloModem` it is now measured and it is wrong**
(finding 273): all five members of the wave 3 batch load `this` from their own
stack slot and address +0x612c, +0x7dce, +0x7dd6 and +0x7ed4 straight off it,
with no intervening load, so the object is at least 0x7f28 = 32,552 bytes.
`V90Phase3Demodulator`'s 43,336 has NOT been checked and is still a
hypothesis.  Either way the check is finding 268's -- trace the base register
of every candidate back to the prologue -- and it is three lines of reading.

`VPcmFloModem` also embeds a whole `V90Modem` at +0x1758 (finding 274), which
is why it is that size.  Both classes may still need the real lifecycle
(`reset` -> `enterPhase3` -> use) before the REST of their members can be
driven; the five in wave 3 did not.

`V90PreFilter` is **40 bytes**, and 1,280 was never a `this` displacement:
`setParamEia6` touches `this` at exactly one offset, +0x1c, and reaches +0x490
inside the `V90Parameters` block that lives there, while `isV90WithEia6` reads
that block's +0x500.  The largest `this` displacement across all twenty-four
members is +0x24.  Finding 234 has the field map.

**A layout is not settled by a passing test alone.**  The harness fill makes
untouched memory compare equal on both sides, so a field the function never
writes proves nothing about where it lives (findings 223, 224).  Each class's
*size* comes from the measured maximum displacement, not from the field list,
and unmodelled regions stay `pad_*` rather than being guessed into fields.

## The 50, by class

Sizes in bytes.  Method names are shown without the class prefix.


### V90Phase3Modulator — 5 symbol(s), 4734 bytes

| 2044 | `generateV92Symbol()` | `_ZN18V90Phase3Modulator17generateV92SymbolEv` |
| 1790 | `generateV90Symbol()` | `_ZN18V90Phase3Modulator17generateV90SymbolEv` |
| 479 | `reset(PcmType, unsigned char, Phase3ModulatorState, unsigned int, V90Jd*, V92Jd*, tagV90DILdescriptor const*, unsigned int)` | `_ZN18V90Phase3Modulator5resetE7PcmTypeh20Phase3ModulatorStatejP5V90JdP5V92JdPK19tagV90DILdescriptorj` |
| 410 | `resetDILGenerator(tagV90DILdescriptor const*)` | `_ZN18V90Phase3Modulator17resetDILGeneratorEPK19tagV90DILdescriptor` |
| 11 | `setSessionFlag(unsigned int)` | `_ZN18V90Phase3Modulator14setSessionFlagEj` |

All five are written, with the 64-byte static member and the four weak
`Scrambler<unsigned char,int>` members the closure needed.  Two more signatures
were left undeclared for a while, on the argument that declaring a constructor
or destructor makes the class non-trivial and deletes the default members of
the union the test fixture uses — **that is no longer the position** (finding
1255).  `Scrambler` acquired both first (finding 871), so the class was
non-trivial before either of these was declared and every fixture union already
carries the empty pair that restores its own; and `V90Modulator` calls the
constructor, so the symbol has to exist.  Both are written and tested:

    V90Phase3Modulator(V90Parameters *, unsigned int)   C1,C2   123 B
    ~V90Phase3Modulator()                               D1,D2    22 B
    Scrambler<unsigned char, int>(unsigned, unsigned, unsigned)  C1  102 B
    ~Scrambler<unsigned char, int>()                            D1   29 B

### VPcmFloModem — 6 symbol(s), 2420 bytes

**All six are written** — 2,420 bytes.  Five in findings 273-278 with
mutation suite `vpcmflomodem`; `enterPhase3` in findings 297-300 with
mutation suite `vpcmep3`.

The class is 32,552 bytes at least and that is measured, not bounded away:
see finding 273 and the correction above.  A whole `V90Modem` is embedded in
it at +0x1758 (finding 274), and +0x1760 and +0x612c are a `V90Phase2Info` and
a `V92Phase2Info` (finding 275).  `V92Phase2Info` is a class this tree had
never declared and now has, data-only, in `include/dsplib/V92Phase2Info.h`.

A `tagV90DILdescriptor` is embedded at +0x004 as well, and it fills the whole
of what used to be `pad_0004`: 0x004 + 0x213 = 0x217, which is where
`flags_0217` begins.  Finding 297.

| 773 | `getUinfoValue(short)` | `_ZN12VPcmFloModem13getUinfoValueEs` |
| 704 | `setPhaseIIinfo(int*, int)` | `_ZN12VPcmFloModem14setPhaseIIinfoEPii` |
| 417 | `getV90CpBits(short*)` | `_ZN12VPcmFloModem12getV90CpBitsEPs` |
| 270 | `enterPhase3()` | `_ZN12VPcmFloModem11enterPhase3Ev` |
| 158 | `getV90JaBits(short*)` | `_ZN12VPcmFloModem12getV90JaBitsEPs` |
| 98 | `setPcmSessionType(int)` | `_ZN12VPcmFloModem17setPcmSessionTypeEi` |

### V90PreFilter — 5 symbol(s), 2019 bytes

All five are written, with all ten static data members (23,860 B) and the
whole of `FloatFIR`.  Two signatures the header declares and deliberately does
not define, because their callees are not written:

    V90PreFilter(__tHardwareCodecTypes__, V90Phase2Info *, V90Parameters *)
                                                        C1,C2   552 B
    ~V90PreFilter()                                     D1,D2    19 B

| 800 | `selectFilter()` | `_ZN12V90PreFilter12selectFilterEv` |
| 790 | `setParamEia6()` | `_ZN12V90PreFilter12setParamEia6Ev` |
| 359 | `autoSelection()` | `_ZN12V90PreFilter13autoSelectionEv` |
| 69 | `isV90WithEia6() const` | `_ZNK12V90PreFilter13isV90WithEia6Ev` |
| 1 | `displayParamEia6()` | `_ZN12V90PreFilter16displayParamEia6Ev` |

### V92Jd — 6 symbol(s), 1430 bytes

| 681 | `packJdPhaseData()` | `_ZN5V92Jd15packJdPhaseDataEv` |
| 665 | `packJdData()` | `_ZN5V92Jd10packJdDataEv` |
| 22 | `getJdPhaseBitVector()` | `_ZN5V92Jd19getJdPhaseBitVectorEv` |
| 22 | `getJdBitVector()` | `_ZN5V92Jd14getJdBitVectorEv` |
| 20 | `unPackJdReset()` | `_ZN5V92Jd13unPackJdResetEv` |
| 20 | `unPackJdPhaseReset()` | `_ZN5V92Jd18unPackJdPhaseResetEv` |

### (free function) — 2 symbol(s), 1091 bytes

| 895 | `getMPrecvdBits(tagV34Object*)` | `_Z14getMPrecvdBitsP12tagV34Object` |
| 196 | `calculateDilLength(tagV90DILdescriptor*, PcmType)` | `_Z18calculateDilLengthP19tagV90DILdescriptor7PcmType` |

### V90Phase3Demodulator — 2 symbol(s), 828 bytes

| 801 | `reset(PcmType, unsigned char, Phase3DemodulatorState, unsigned int, V90Jd*, V92Jd*, tagV90DILdescriptor*, short, short, float, unsigned int)` | `_ZN20V90Phase3Demodulator5resetE7PcmTypeh22Phase3DemodulatorStatejP5V90JdP5V92JdP19tagV90DILdescriptorssfj` |
| 27 | `setSessionFlag(unsigned int)` | `_ZN20V90Phase3Demodulator14setSessionFlagEj` |

### V90Equalizer — 3 symbol(s), 788 bytes

| 350 | `setLinearEquBeta(float)` | `_ZN12V90Equalizer16setLinearEquBetaEf` |
| 350 | `setDfeBeta(float)` | `_ZN12V90Equalizer10setDfeBetaEf` |
| 88 | `enterPhase3()` | `_ZN12V90Equalizer11enterPhase3Ev` |

### V90AutoDigitalImpDetector — 2 symbol(s), 614 bytes

| 503 | `reset(unsigned char, PcmType, short)` | `_ZN25V90AutoDigitalImpDetector5resetEh7PcmTypes` |
| 111 | `resetLinearMapping()` | `_ZN25V90AutoDigitalImpDetector18resetLinearMappingEv` |

### V90Jd — 2 symbol(s), 557 bytes

| 537 | `getBitVector()` | `_ZN5V90Jd12getBitVectorEv` |
| 20 | `unPackReset()` | `_ZN5V90Jd11unPackResetEv` |

### V90Demodulator — 2 symbol(s), 534 bytes

| 448 | `enterPhase3()` | `_ZN14V90Demodulator11enterPhase3Ev` |
| 86 | `setSessionFlag(unsigned int)` | `_ZN14V90Demodulator14setSessionFlagEj` |

Both are written.  The object is **0x298** and the class has its own header,
whose field map comes from the 1,002-byte constructor rather than from either
member; findings 291, 293 and 296.

### V90Phase2Info — 1 symbol(s), 508 bytes

| 508 | `printInfo() const` | `_ZNK13V90Phase2Info9printInfoEv` |

### V90ConstellationDesigner — 1 symbol(s), 86 bytes

| 86 | `setMinMaxRates(unsigned int, unsigned int)` | `_ZN24V90ConstellationDesigner14setMinMaxRatesEjj` |

### V90Modem — 1 symbol(s), 71 bytes

| 71 | `setSessionFlag(unsigned int)` | `_ZN8V90Modem14setSessionFlagEj` |

### V90Modulator — 1 symbol(s), 64 bytes

| 64 | `setSessionFlag(unsigned int)` | `_ZN12V90Modulator14setSessionFlagEj` |

### FloatFIR — 1 symbol(s), 58 bytes

Taken whole in batch 3: all six members, 723 bytes unique, 858 by `nm` because
C1/C2 and D1/D2 are byte-identical copies of one definition.  Only
`setCoefficients` is in the fifty, because it is the only one `v34handshak`
reaches.

| 58 | `setCoefficients(float*, unsigned int)` | `_ZN8FloatFIR15setCoefficientsEPfj` |
| 287 | `process(float const*, float*, unsigned int)` | `_ZN8FloatFIR7processEPKfPfj` |
| 182 | `process(float)` | `_ZN8FloatFIR7processEf` |
| 105 | `FloatFIR(unsigned int, float*, unsigned int)` | `_ZN8FloatFIRC1EjPfj` |
| 61 | `reset()` | `_ZN8FloatFIR5resetEv` |
| 30 | `~FloatFIR()` | `_ZN8FloatFIRD1Ev` |

### V90SdDetector — 1 symbol(s), 52 bytes

| 52 | `reset()` | `_ZN13V90SdDetector5resetEv` |

### V90SpectralVerifier — 1 symbol(s), 46 bytes

| 46 | `reset()` | `_ZN19V90SpectralVerifier5resetEv` |

### V92EchoCanceller — 1 symbol(s), 37 bytes

| 37 | `setEchoDelay(unsigned int)` | `_ZN16V92EchoCanceller12setEchoDelayEj` |

### V90Phase4Demodulator — 1 symbol(s), 26 bytes

| 26 | `setSessionFlag(unsigned int)` | `_ZN20V90Phase4Demodulator14setSessionFlagEj` |

### ResamplerTimingOffset — 1 symbol(s), 21 bytes

| 21 | `setTimingOffset(float)` | `_ZN21ResamplerTimingOffset15setTimingOffsetEf` |

### V90Phase4Modulator — 1 symbol(s), 11 bytes

| 11 | `setSessionFlag(unsigned int)` | `_ZN18V90Phase4Modulator14setSessionFlagEj` |

### K56FlexFloModem — 4 symbol(s), 8 bytes

| 3 | `getK56FlexMpBits(short*)` | `_ZN15K56FlexFloModem16getK56FlexMpBitsEPs` |
| 3 | `getK56FlexJaBits(short*)` | `_ZN15K56FlexFloModem16getK56FlexJaBitsEPs` |
| 1 | `setMinMaxRates(int, int)` | `_ZN15K56FlexFloModem14setMinMaxRatesEii` |
| 1 | `enterPhase3FullDuplex()` | `_ZN15K56FlexFloModem21enterPhase3FullDuplexEv` |
