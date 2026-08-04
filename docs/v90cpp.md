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
| 0 | warm-up (#59) | ~~`getbit` 433, `ApplyBulkDelay` 467 — both leaves; `getMPrecvdBits` 895, callees all `have`~~ **DONE** — finding 226; `getMPrecvdBits` needed a `.cpp` after all, and it is the precedent for a `ref_` alias that is `extern "C"` *and* `regparm` | yes |
| 1 | `V90Jd` / `V92Jd` | `V90Jd::getBitVector` 537, `unPackReset` 20; `V92Jd::packJdData` 665, `packJdPhaseData` 681, `getJdBitVector` 22, `getJdPhaseBitVector` 22, `unPackJdReset` 20, `unPackJdPhaseReset` 20 | all leaves |
| 2 | `V90PreFilter` | `selectFilter` 800, `setParamEia6` 790, `autoSelection` 359, `isV90WithEia6` 69, `displayParamEia6` 1, plus `FloatFIR::setCoefficients` 58 | yes, with FloatFIR |
| 3 | `V90Phase3Modulator` | `generateV92Symbol` 2044, `generateV90Symbol` 1790, `reset` 479, `resetDILGenerator` 410, `setSessionFlag` 11 | needs batch 1 |
| 4 | the leaf remainder | the stubs and setters — see the table below | mostly leaves |
| 5 | `VPcmFloModem`, `V90Phase3Demodulator` | the two whose objects reach through into an enclosing session | last |

## Object sizes

Bounded by the largest `this`-relative displacement each class uses
(finding 215).  `V90Phase3Modulator` 916 and `V90PreFilter` 1,280 are the
tractable shape: allocate a zeroed buffer, call `reset` on both sides,
compare with `diff_eq_obj`.  `V90Jd` is 140 and `V92Jd` 216.

`V90Phase3Demodulator` reaches 43,336 and `VPcmFloModem` 32,612, which almost
certainly means they index *through* `this` into an enclosing session object
rather than being that large.  Those two may need the real lifecycle
(`reset` -> `enterPhase3` -> use) before they can be driven, which is why they
are last.

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

### VPcmFloModem — 6 symbol(s), 2420 bytes

| 773 | `getUinfoValue(short)` | `_ZN12VPcmFloModem13getUinfoValueEs` |
| 704 | `setPhaseIIinfo(int*, int)` | `_ZN12VPcmFloModem14setPhaseIIinfoEPii` |
| 417 | `getV90CpBits(short*)` | `_ZN12VPcmFloModem12getV90CpBitsEPs` |
| 270 | `enterPhase3()` | `_ZN12VPcmFloModem11enterPhase3Ev` |
| 158 | `getV90JaBits(short*)` | `_ZN12VPcmFloModem12getV90JaBitsEPs` |
| 98 | `setPcmSessionType(int)` | `_ZN12VPcmFloModem17setPcmSessionTypeEi` |

### V90PreFilter — 5 symbol(s), 2019 bytes

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

### V90Phase2Info — 1 symbol(s), 508 bytes

| 508 | `printInfo() const` | `_ZNK13V90Phase2Info9printInfoEv` |

### V90ConstellationDesigner — 1 symbol(s), 86 bytes

| 86 | `setMinMaxRates(unsigned int, unsigned int)` | `_ZN24V90ConstellationDesigner14setMinMaxRatesEjj` |

### V90Modem — 1 symbol(s), 71 bytes

| 71 | `setSessionFlag(unsigned int)` | `_ZN8V90Modem14setSessionFlagEj` |

### V90Modulator — 1 symbol(s), 64 bytes

| 64 | `setSessionFlag(unsigned int)` | `_ZN12V90Modulator14setSessionFlagEj` |

### FloatFIR — 1 symbol(s), 58 bytes

| 58 | `setCoefficients(float*, unsigned int)` | `_ZN8FloatFIR15setCoefficientsEPfj` |

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
