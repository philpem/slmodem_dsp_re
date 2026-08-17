# `V90Equalizer::process` — the complete decode

*The 9,364-byte hub at `0x38d80`..`0x3b213`,
`_ZN12V90Equalizer7processEPfjPsS0_Rj`. Finding 5700 is the argument; this is
the transcription. Every line below was read from `tools/dis.py` on
`slmodemd/dsplibs.o`; nothing here came from a decompiler.*

**Status: WRITTEN, MOSTLY DRIVEN, NOT CLOSED -- 74.25% of 532 lines.**
The whole function is in `src/pump/v90/V90Equalizer.cpp` and
`test/unit/t_v90equproc.cpp` drives it in three groups at 140,205 differential
checks with `make period` at zero failures. **Six of the seven state arms, the
state 4 jump table, all five re-convert blocks and the whole fixed-point half
are now driven** -- findings 6500-6503, and 6502 is the region-by-region
register of what is left.

What remains undriven is **state 1 PHASE3, its eighteen-entry jump table,
`<TAIL-P3>` and state 6 CHANNEL_VERIFY** -- about 180 lines, all of them
waiting on a `V90Phase3Demodulator` fixture rather than on anything the object
forbids. Findings 6200-6203 are the first batch; 6201 records what the object
makes undrivable (`state` outside 0..6, `mmxMode` with states 0, 1, 2 and 6,
and three of the eighteen phase 3 sub-cases), 6500 adds a fourth -- the
`dfeSum` cast at the three forward re-convert blocks, which is driven sixteen
times each and still cannot be adjudicated because the value is dead -- and
D850 and D851 are the two deviations.

**AND THE `(short)` ASYMMETRY IS SETTLED.** §5's three forward re-convert
blocks narrow the DFE output at exactly one of the three, and that is the
object's: `fistpl 0xbc` at RECONVERT-A (0x39c5c) and RECONVERT-C (0x3a914)
against `fistps 0x9a` plus `movzwl`/`cwtl` at RECONVERT-B (0x3a70a). It is
also DEAD -- inverting the cast at all three sites moves zero of 19,231
checks while a change to the statement above it moves 177. Finding 6500.

**Two places below were re-read from `dis.py` and did not survive.** §3.4's
fixed-point LMS is prose here and prose cannot be written from; the arithmetic
is in finding 6202. And §5's "plus 4 more when `((short *)block_b4)[0] != 0`"
is wrong: RECONVERT-D steps `in` by `2*j + 2` floats normally and `2*j + 1`
when the held sample is present. Treat every summary below as a pointer to an
address, not as a transcription.

---

## 0. Signature and frame

    void V90Equalizer::process(float *in, unsigned n, short *outSym,
                               float *outFloat, unsigned &nOut)

`void`: all three returns (`0x38f5a`, `0x39abb`, and the `ret` after
`linearEquFadeEdges`) fall off the end without setting `%eax`.

Four pushes and `sub $0xcc`, so `%esp` sits `0xdc` below entry:

| slot | meaning |
|---|---|
| `0xe0` | `this` |
| `0xe4` | `float *in` |
| `0xe8` | `unsigned n` |
| `0xec` | `short *outSym` |
| `0xf0` | `float *outFloat` |
| `0xf4` | `unsigned *nOut` |

Locals that matter (the rest are outgoing-argument slots `0x04`..`0x1c`):

| slot | meaning |
|---|---|
| `0x4c` | `j`, the output symbol index |
| `0x74` | `cur`, the `short` read cursor over `block_b4` (fixed-point arm only) |
| `0x7c` | `e`, the fixed-point error |
| `0x80` | `softInt`, the soft output as an `int` holding a `short` |
| `0x84` | `updateCoefs` — see finding 5700 §3 |
| `0x88` | `decision`, `cwtl`-widened from the `short` each slicer returns |
| `0x8c` | `soft`, the float soft output |
| `0x9a`/`0x9c`/`0xa0` | `fists`/`fistpl`/`fistpll` scratch |
| `0xac`/`0xae` | x87 control-word scratch for the round-toward-zero idiom |
| `0xb0` | `y`, the linear equaliser's output |
| `0xb4` | `d`, the decision-feedback filter's output |
| `0xb8` | `leSum`, the fixed-point `y` |
| `0xbc` | `dfeSum`, the fixed-point `d` |

## 1. Two struct changes this batch owns

`+0x14c` is **not** padding. `0x3b1b7` reads it and hands it to
`ResamplerTimingOffset::setTimingOffset(float)`:

    3b1b7:  8b 8b 4c 01 00 00   mov  0x14c(%ebx),%ecx
    3b1c2:  89 4c 24 04         mov  %ecx,0x4(%esp)
    3b1cb:  e8 ..               call ResamplerTimingOffset::setTimingOffset

so it is a `float` and `pad_14c[4]` should become a named field. It is the
last four bytes of the 0x150 object.

`+0x13c` and `+0x140` are the mean-error before/after pair. `word_13c` takes
`meanErrorEnergyMean` at `0x3aa09` *before* the statistics are recomputed, and
`word_140` takes `word_13c / meanErrorEnergyMean` at `0x3a485` afterwards —
which is what the format string at `0x9540` calls
`ph4MeanErrorEnergyBeforeToAfterUpdateRatio`. That string names `+0x140`
outright; `+0x13c` is named by the arithmetic that feeds it.

Nothing else in any peer class needs modelling. `V90Parameters` (0x558),
`V90Phase3Demodulator` (0x42c), `V90Phase4Demodulator` (0x351c),
`V90Demapper` (0x1eb8), `V90Resampler` (0xb4), `V90ConnectionEvaluator`,
`V90SpectralVerifier` and `V90PreFilter` already carry every offset this
function reaches.

## 2. Prologue

    stateCount = 0;                                   /* +0x64 */
    cur = (short *)block_b4 + 1;
    if (mmxMode) {
        const float *s = in;  unsigned i = n;
        while (i != 0) { *cur++ = (short)*s++; i--; }  /* RC=11, truncating */
        if (word_68) {
            ((short *)block_b4)[0] = (short)word_6c;
            word_68 = 0;
            cur = (short *)block_b4;
            n++;
        } else {
            ((short *)block_b4)[0] = 0;
            cur = (short *)block_b4 + 1;
        }
    } else {
        if (word_68) n++;          /* cmp $1 / sbbl $-1 -- the borrow idiom */
    }
    updateCoefs = 1;               /* planted at 0x38d81, before everything */
    j = 0;
    nOut = n >> 1;                 /* LOGICAL: see docs/cleanup.md §2 */

The fixed-point arm's conversion loop keeps its input pointer in a register
and never writes it back to `0xe4(%esp)`, so `in` is untouched by it; the
float arm consumes `in` inside the symbol loop instead.

**`word_68`/`word_6c` are the held-over odd sample and its value.** Named from
use, and the finding says so: the flag is set in the epilogue exactly when
`n` is odd, and the value is the sample the loop could not pair.

## 3. The symbol loop

    for (; j < nOut; j++) {
        if (mmxMode) { ...§3.1... } else { ...§3.2... }
        /* 0x39250 -- the join AND the switch's default */
        outSym[j] = (short)decision;
        if (mmxMode) { ...§3.4... } else { ...§3.5... }
    }

`mmxMode` is re-read from memory at every test — every arm ends by reloading
`0xb0(this)` into the `0x54(%esp)` spill slot after its calls — so it is a
field reference in the source and not a cached local.

### 3.1 The fixed-point arm, `0x38fcc`

    s0 = (unsigned short)cur[0];
    s1 = (unsigned short)cur[1];
    cur += 2;
    ec = array_ecAligned;                       /* +0xf0 */
    k  = word_20Saved;                          /* +0xf8 */
    ec[k] = (short)s0;
    k--;  word_20Saved = k;
    ec[k] = (short)s1;

    dfeSum = mmxDot(dfeMmxCoefsAligned, array_12cAligned, dfeLength);
    dfeSum /= dfeMmxOutputConversionFactor;     /* +0x108, idivl */
    leSum  = mmxDot(linearEquMmxCoefsAligned, &ec[k], linearEquLength);
    leSum  /= linearEquMmxOutputConversionFactor;   /* +0xc8, idivl */
    softInt = (short)(leSum - dfeSum);          /* sub then cwtl -- FORCED */

`mmxDot` counts **up** with a signed `jl` (`0x39035`, `0x39088`), so its index
and bound are `int`:

    static int mmxDot(const short *h, const short *x, int n)
    { int s = 0, i; for (i = 0; i < n; i++) s += (int)h[i] * (int)x[i]; return s; }

The two `idivl`s are signed divides by fields the header already types `int`;
they stay divides (six signed power-of-two divides exist in the whole object
and none of them is here — finding 1044).

### 3.2 The float arm, `0x390d0`

    if (word_68) {                                       /* 0x394b0 */
        array_18[word_20] = word_6c;  word_68 = 0;
        word_20--;  array_18[word_20] = *in++;
    } else {
        array_18[word_20] = in[0];
        word_20--;
        array_18[word_20] = in[1];  in += 2;
    }
    d = fdot(array_44,  dfeCoefs,       dfeLength);
    y = fdot(&array_18[word_20], linearEquCoefs, linearEquLength);
    soft = y - d;                    /* fsubrs 0xb0(%esp): mem - st0 */
    softInt = (short)soft;           /* fists -- TRUNCATING, NOT popped */

`fdot` counts **down** with an unsigned `ja` and carries two accumulators:

    static float fdot(const float *x, const float *h, unsigned n)
    {
        float a0 = 0.0f, a1 = 0.0f;
        while (n > 3) {
            a0 += x[0] * h[0];   a1 += x[1] * h[1];
            a0 += x[2] * h[2];   a1 += x[3] * h[3];
            x += 4; h += 4; n -= 4;
        }
        while (n != 0) { a1 += *x++ * *h++; n--; }
        return a1 + a0;
    }

Even indices into the top-of-stack accumulator, odd indices **and the whole
scalar tail** into `%st(1)`, one `faddp %st,%st(1)` at `0x39177` to combine.
GCC 3.4.2 at `-O3` neither unrolls nor reassociates a float sum, so both the
unroll and the second accumulator are in the source.

### 3.3 `switch (state)`, `0x390bd`

    cmp $0x6,%ecx ; ja 39250 ; jmp *0xc00(,%ecx,4)

| state | arm | address |
|--:|---|---|
| 0 | `RESET` | `0x3a0cb` |
| 1 | `PHASE3` | `0x3a07b` |
| 2 | `PHASE4` | `0x39f65` |
| 3 | `DATA` | `0x39d55` |
| 4 | `RRN` | `0x39d0c` |
| 5 | `FPE` | `0x39b87` |
| 6 | `CHANNEL_VERIFY` | `0x39b21` |

Every arm ends by reloading `mmxMode` and `state` and joining `0x39250`,
which is also the `default`. See finding 5700 §2 for why the tail's
comparisons against 10..16 are live.

### 3.4 The fixed-point error tail, `0x39272`

    e = (short)(softInt - decision);
    if (abs(e) > 300 && state > 1) {
        if (word_94 <= 1)
            edprintf("V90Equalizer: High momentary error, symbol#%d, "
                     "error %d, soft Decision %d\r\n", j, e, softInt);
        word_94++;  updateCoefs = 0;
    } else if (state != 10 && state != 11 && state != 12 && state != 16 &&
               state != 13 && state != 14 && state != 15 && word_94 > 2) {
        if (++updateCoefs == 4) {
            edprintf("V90Equalizer: nof consecutive errors = %d\r\n", word_94);
            word_94 = 0;
        }
    }

The comparison order — 10,11,12 folded into `(unsigned)(state-10) > 2`, then
16, then 13, 14, 15 — is the source's `&&` order and not a reassociation GCC
is free to choose, because the fold only happens across *adjacent* terms.

Then, at `0x392d0`:

    diff = (short)(leSum - decision);
    if (updateCoefs) {
        /* two fixed-point LMS loops, each a 32-bit accumulator split across
           an aligned/unaligned short pair: the high half read SIGNED
           (`movswl`) and the low half UNSIGNED (`movzwl`), recombined with
           `shl $16 ; or`, updated by `(coef * beta) >> shift`, and split
           back with `mov %ax` and `sar $0x10 ; mov %ax`. */
    }
    /* the array_12c shift, the word_20Saved decrement and its wrap: 0x39421 */
    word_78 += (unsigned)(e * e);
    block_b8[j] = (short)softInt;

### 3.5 The float error tail, `0x394e1`

    fdec = (float)(short)decision;         /* filds -- a 16-bit load */
    err  = soft - fdec;
    /* long double, and that is what selects the object's encoding: see
       finding 5701.  Every `float` spelling emits `fcoms mem; jbe` and sends
       a NaN error down the NOT-high arm; this one emits
       `fld %st(0); fabs; flds; fcomp %st(1); jae` and sends it down the high
       arm, which is what the object does.  Both operand orders work. */
    long double aerr = __builtin_fabsl((long double)err);
    if (aerr > 300.0) {
        if (state > 1) {
            if (word_94 <= 1)
                edprintf("V90Equalizer: High momentary error, symbol#%d, "
                         "error = %c%d.%03d,   soft Decision = %c%d.%03d\r\n",
                         j, sign(err), iabs(err), frac3(err),
                            sign(soft), iabs(soft), frac3(soft));
            word_94++;  updateCoefs = 0;
        } else goto notHigh;
    } else { notHigh: ...the consecutive-clean counter, as §3.4... }

    if (updateCoefs) {
        for (i = 0; i < dfeLength; i++)
            dfeCoefs[i] += (dfeBeta * err) * array_44[i];
        for (i = 0; i < linearEquLength; i++)
            linearEquCoefs[i] += (-linearEquBeta * (y - decision))
                                 * array_18[word_20 + i];
    }
    /* 0x39830, reached whether or not the update ran, and rejoined at
       0x39840 from the update path: */
    memmove up array_44 by one; array_44[0] = y - decision;
    word_20--;  if ((int)word_20 < 0) <wrap: 0x3a2d4>;   /* OPEN: see below */
    /* `fmul %st(0),%st ; fistpll 0xa0(%esp)`, and then the LOW 32 BITS of that
       64-bit result are added.  A C `(unsigned)(err*err)` reproduces it only
       while the product is in range and is UNDEFINED outside it -- D561's
       shape with our side as the undefined one.  Convert through 64 bits. */
    word_78 += (unsigned)(long long)(err * err);
    outFloat[j] = soft;

**The two updates use different errors.** `0x396e0` is `fmul %st(2),%st`,
reaching past `y - decision` to `err`; `0x39967` is `fmul %st(1),%st`, taking
`y - decision` itself. One stack slot apart, and a suite with a zero-length
DFE cannot tell them apart.

The `%c%d.%03d` triple is the tree's existing `edprint_stat` shape
(`src/pump/v90/V90Equalizer.cpp`), except that the scale here is a **float**
`1000.0f` loaded once with `flds` and reused for both values, not a
`long double`.

**OPEN: `word_20` and `word_20Saved` are `unsigned int` in the header and the
object tests their SIGN.** `0x3985f` is `dec %eax; js 3a2d4` and `0x39457` is
`dec %eax; js 398f5`. Written literally against an unsigned field the test
folds to false and the wrap never runs, so either both fields are `int` -- and
`reset`'s `word_1c - linearEquLength - 1` can go negative, which supports that
-- or there is an `int` local the object is decrementing. The `js` is the
forced encoding; the declared type is this batch's to settle, and it is not
settled here.

The wrap at `0x3a2d4`:

    word_20 = word_1c - linearEquLength - 1;
    for (i = linearEquLength; i-- > 0; )
        array_18[word_1c - linearEquLength + i] = array_18[i];

which is the same expression `reset` plants at construction. The fixed-point
twin is `0x398f5` over `array_ecAligned`/`word_20Saved`.

## 4. The arms

### state 6 — `CHANNEL_VERIFY`, `0x39b21`

    decision = phase3Demod->getDecision(soft);          /* float argument */
    st = phase3Demod->word_30;
    if (st) {
        stateCount = st;
        if (st == 0x39) resampler->setBllState(V90_BLL_FROZEN, 1);
    }

### state 5 — `FPE`, `0x39b87`

    decision = phase4Demod->getDecision((short)softInt);
    st = phase4Demod->int_0028;
    if (st) {
        stateCount = st;
        if (st == 0x1d && enterDataPhase()) <RECONVERT-A>;
    }

### state 4 — `RRN`, `0x39d0c`

    decision = phase4Demod->getDecision((short)softInt);
    st = phase4Demod->int_0028;
    if (st) {
        stateCount = st;
        if ((unsigned)(st - 0x1c) <= 0x19) switch (st) {   /* .rodata+0xc1c */

        case 0x1c:
            if (connEval->word_90 && phase4Demod->int_0038) {
                edprintf("V90Equalizer: Freezing equ & dfe on silence "
                         "between Ed and Rt\r\n");
                setLinearEquBeta(0.0f);  setDfeBeta(0.0f);
            } else {
                setLinearEquBeta(params->LINEAR_EQU_DATA_BETA);
                setDfeBeta(params->DFE_DATA_BETA);
            }
            break;

        case 0x1d:
            if (enterDataPhase()) <RECONVERT-C>;
            break;

        case 0x28:
            setLinearEquBeta(params->LINEAR_EQU_TRN2D_BETA);
            if (spectralVerifier->word_28 == 2)
                setDfeBeta(params->GERMAN_PBX_DFE_TRN2D_SLOW_BETA);
            else if (preFilter->isV90WithEia6()) {
                edprintf("V90Equalizer: Rtnot on  EIA6 DFE SLOW\r\n");
                setDfeBeta(params->EIA6_DFE_TRN2D_SLOW_BETA);
            } else
                setDfeBeta(params->DFE_TRN2D_BETA);
            break;

        case 0x2c:
            if (preFilter->isV90WithEia6()) {
                edprintf("V90Equalizer: after rrn (EIA6): set fast...\r\n");
                setLinearEquBeta(params->LINEAR_EQU_DATA_BETA);
                setDfeBeta(params->EIA6_DFE_TRN2D_RRN_BETA);
            }
            break;

        case 0x35:
            edprintf("V90Equalizer: Freezing equ & dfe on silence "
                     "between Ed and Rt\r\n");
            setLinearEquBeta(0.0f);  setDfeBeta(0.0f);
            break;
        }
    }
    <TAIL-P4>          /* 0x3a17f, shared with the PHASE3 arm's sub-cases */

`<TAIL-P4>`, `0x3a17f`:

    if (phase4Demod->state == 3 &&
        phase4Demod->linearMappStudyStart == phase4Demod->countInState) {
        word_a4 = 1;  meanErrorCount = 0;  meanErrorFull = 0;
    }
    if (phase4Demod->state == 5 || phase4Demod->state == 4) word_a4 = 0;

### state 3 — `DATA`, `0x39d55`

    decision = demapper->hardDecision((short)softInt);
    if (demapper->linearMappStudyEnabled)
        demapper->linearMappingStudy((short)softInt, (short)decision);
    if (phase4Demod->detectRRN((short)decision)) {
        stateCount = 0x23;
        demapper->linearMappStudyEnabled = 0;
        if (dsplibs_debug_level > 1)
            dsplibs_debug_printf("V90Equalizer: disable linear mapping "
                                 "study.\n");
        if (enterRRN()) <RECONVERT-D>;
    }
    if (phase4Demod->detectFPE((short)decision)) {
        stateCount = 0x25;
        demapper->linearMappStudyEnabled = 0;
        if (dsplibs_debug_level > 1)
            dsplibs_debug_printf("V90Equalizer: disable linear mapping "
                                 "study.\n");
        if (enterFPE()) <RECONVERT-E>;
    }

`<RECONVERT-D>` (`0x39df5`) and `<RECONVERT-E>` (`0x39eda`) are the *inverse*
of A/C: they run when the equaliser has just LEFT fixed-point mode, converting
`leSum`/`softInt`/`dfeSum` back into `y`/`soft`/`d` with `fildl`, stepping
`in` past `8*j` (plus 4 more when `((short *)block_b4)[0] != 0`), and
converting `block_b8[0..j-1]` back into `outFloat[0..j-1]`.

### state 2 — `PHASE4`, `0x39f65`

    if (soft > 32767.0f)       soft =  32767.0f;
    else if (soft < -32767.0f) soft = -32767.0f;     /* NaN clamps LOW */
    decision = phase4Demod->getDecision(
                   (short)(phase4Demod->state > 1 ? soft : y));
    st = phase4Demod->int_0028;
    if (st) {
        stateCount = st;
        if (st == 0x17) {
            setLinearEquBeta(params->LINEAR_EQU_TRN2D_INITIAL_BETA);
            if (spectralVerifier->word_28 == 2)
                setDfeBeta(params->GERMAN_PBX_DFE_TRN2D_FAST_BETA);
            else if (preFilter->isV90WithEia6()) {
                edprintf("V90Equalizer: RiNot on EIA6 DFE fast\r\n");
                setDfeBeta(params->EIA6_DFE_TRN2D_FAST_BETA);
            } else
                setDfeBeta(params->DFE_TRN2D_BETA);
            resampler->setBllState(quickConnect ? V90_BLL_TRN2
                                                : V90_BLL_TRN2_INITIAL, 1);
        } else if (st == 0x18) {
            if (spectralVerifier->word_28 == 2) {
                edprintf("V90Equalizer: middle of TRN2d\r\n");
                setDfeBeta(params->GERMAN_PBX_DFE_TRN2D_SLOW_BETA);
            }
            if (preFilter->isV90WithEia6()) {
                edprintf("V90Equalizer: middle of TRN2d "
                         "(DFE EIA6 CONDITION)\r\n");
                setDfeBeta(params->EIA6_DFE_TRN2D_SLOW_BETA);
            }
        } else if (st == 0x1c) {
            setLinearEquBeta(params->LINEAR_EQU_DATA_BETA);
            setDfeBeta(params->DFE_DATA_BETA);
        } else if (st == 0x1d) {
            if (enterDataPhase()) <RECONVERT-B>;
        }
    }
    /* 0x3a024 */
    if ((phase4Demod->state == 3 || phase4Demod->state == 2) &&
        params->LINEAR_EQU_TRN2D_INITIAL_DURATION
            == phase4Demod->countInState) {
        setLinearEquBeta(params->LINEAR_EQU_TRN2D_BETA);
        if (!quickConnect) resampler->setBllState(V90_BLL_TRN2, 1);
    }
    if (phase4Demod->state == 3) {
        if (phase4Demod->linearMappStudyStart == phase4Demod->countInState)
            word_a4 = 1;
        if (phase4Demod->demapper->short_1ea4 && flag_144) {
            flag_144 = 0;
            calcMeanErrorStatistics();          /* return value discarded */
            meanErrorFull = 0;
            word_13c = meanErrorEnergyMean;
            meanErrorCount = 0;
        }
        if (phase4Demod->demapper->short_1ea6 && flag_146) {
            flag_146 = 0;
            calcMeanErrorStatistics();          /* return value discarded */
            word_140 = (meanErrorEnergyMean == 0.0f)
                       ? 0.0f : word_13c / meanErrorEnergyMean;
            if (dsplibs_debug_level > 1)
                dsplibs_debug_printf("V90Equalizer: ph4MeanErrorEnergy"
                    "BeforeToAfterUpdateRatio = %c%d.%03d\r\n", ...word_140);
        }
    }
    if (phase4Demod->state == 5 || phase4Demod->state == 4) word_a4 = 0;

### state 1 — `PHASE3`, `0x3a07b`

    decision = phase3Demod->getDecision(soft);       /* float argument */
    st = phase3Demod->word_30;
    if (st) {
        stateCount = st;
        if ((unsigned)(st - 3) <= 0x11) switch (st) {   /* .rodata+0xc84 */

        case 3:
            if (quickConnect) {
                resampler->setTimingOffset(timingOffset);   /* +0x14c */
                resampler->countStateSamples = 1;
            } else {
                for (i = 0; i < linearEquLength; i++) linearEquCoefs[i] = 0.0f;
                if (mmxMode)
                    for (i = 0; i < linearEquLength + 8; i++) {
                        linearEquMmxCoefs[i] = 0;   /* the RAW pointers */
                        array_d8[i] = 0;
                    }
                resampler->setBllState(V90_BLL_INITIAL, 1);
            }
            break;                          /* both fall to 0x3a1c6 */

        case 8:  setLinearEquBeta(params->GERMAN_PBX_LINEAR_EQU_DIL_BETA);
                 setDfeBeta(params->DFE_DIL_BETA);                    break;
        case 9:  resampler->resetSdHalfBaudDft();                     break;

        case 10:
            setLinearEquBeta(params->GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA);
            if (dsplibs_debug_level > 1)
                dsplibs_debug_printf("V90Equalizer: DfeProtectionOnDil "
                                     "= %d \r\n", (int)short_08);
            setDfeBeta(short_08 ? params->DFE_DIL_HIGH_UCODE_BETA
                                    / (float)short_08
                                : params->DFE_DIL_HIGH_UCODE_BETA);
            if (resampler->bllState) {
                savedBllState = resampler->bllState;
                resampler->setBllState(V90_BLL_FROZEN, 1);
                edprintf("V90Equalizer: DemodDilInHighUcodesStage -> "
                         "freeze timing\r\n");
            }
            edprintf("V90Equalizer: DemodDilInHighUcodesStage\r\n");
            break;

        case 11:
            setLinearEquBeta(params->GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA);
            setDfeBeta(params->DFE_DIL_MED_UCODE_BETA);
            edprintf("V90Equalizer: DemodDilHighUcodesStageTerminated "
                     "setting medium ucode beta\r\n");
            break;

        case 12:
            setLinearEquBeta(params->GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA);
            setDfeBeta(params->DFE_DIL_MED_UCODE_BETA);
            edprintf("V90Equalizer: DemodDilInMedUcodesStage\n");
            break;

        case 13:
            setLinearEquBeta(params->GERMAN_PBX_LINEAR_EQU_DIL_BETA);
            setDfeBeta(params->DFE_DIL_BETA);
            edprintf("V90Equalizer: DemodDilInMedUcodesStageTerminated "
                     "setting normal dil beta\r\n");
            break;

        case 14:
            setLinearEquBeta(0.0f);  setDfeBeta(0.0f);
            edprintf("V90Equalizer: DemodDilInitialErrorRelaxation => "
                     "freeze LE & DFE\r\n");
            goto ENTERPHASE4_JOIN;          /* 0x3abe8 -> 0x3a1c6 */

        case 15:
            setLinearEquBeta(params->LINEAR_EQU_DIL_ERROR_RELAX_BETA);
            setDfeBeta(params->DFE_DIL_ERROR_RELAX_BETA);
            resampler->setBllState(V90_BLL_DIL, 1);
            edprintf("V90Equalizer: DemodDilInitialErrorRelaxTerminated "
                     "=> unfreezing LE & DFE and Timing.\r\n");
            break;

        case 16:
            setLinearEquBeta(0.0f);  setDfeBeta(0.0f);
            if (resampler->bllState) {
                savedBllState = resampler->bllState;
                resampler->setBllState(V90_BLL_FROZEN, 1);
            }
            edprintf("V90Equalizer: DemodDilInFreeze => freeze LE & DFE "
                     "& timing.\r\n");
            break;

        case 20:  enterPhase4();  break;    /* 0x3a1b7 */
        }
    }
    <TAIL-P3>          /* 0x3a1d0 */

The `case` labels are `phase3Demod->word_30`; the table is indexed by
`word_30 - 3`, so index 0 is `word_30 == 3` and index 17 is `word_30 == 20`.
Indices 1..4 and 14..16 fall to `<TAIL-P3>`.

**These names are the author's.** Every one of cases 10..16 prints a string
that spells its own stage, which is class-1 evidence under CLAUDE.md's
ordering. They are NOT recorded as an enum here because they belong to
`V90Phase3Demodulator`, and `agent-v90pf3d` is live on that class — finding
3511's exact shape. Whoever writes this function should either take the
p3d rename in the same branch or leave the labels numeric with these strings
beside them.

`<TAIL-P3>`, `0x3a1d0`:

    if (phase3Demod->state == 0 && phase3Demod->byte_424) {
        if (mmxMode) {
            resampler->SdHalfBaudDft((float)array_ecAligned[word_20Saved + 1]);
            resampler->SdHalfBaudDft((float)array_ecAligned[word_20Saved]);
        } else {
            resampler->SdHalfBaudDft(array_18[word_20 + 1]);
            resampler->SdHalfBaudDft(array_18[word_20]);
        }
    }
    if (phase3Demod->state == 3 &&
        params->LINEAR_EQU_TRN1D_FREEZE_DURATION == phase3Demod->word_2c &&
        !quickConnect)
        setLinearEquBeta(params->LINEAR_EQU_TRN1D_BETA);
    if (phase3Demod->state == 3 &&
        params->DFE_TRN1D_FREEZE_DURATION == phase3Demod->word_2c &&
        !quickConnect)
        setDfeBeta(params->DFE_TRN1D_BETA);
    if (quickConnect && phase3Demod->state == 4) {
        if (params->LINEAR_EQU_QC_TRN1D_FREEZE_DURATION
                == phase3Demod->word_2c)
            setLinearEquBeta(params->LINEAR_EQU_DATA_BETA);
        if (params->DFE_QC_TRN1D_FREEZE_DURATION == phase3Demod->word_2c)
            setDfeBeta(params->DFE_DATA_BETA);
    }
    if (phase3Demod->state == 5  || phase3Demod->state == 10 ||
        phase3Demod->state == 11 || phase3Demod->state == 12 ||
        phase3Demod->state == 16 || phase3Demod->state == 13 ||
        phase3Demod->state == 14 || phase3Demod->state == 15)
        updateCoefs = phase3Demod->word_408;
    if (phase3Demod->state == 17)
        updateCoefs = phase3Demod->word_408;
    else if (phase3Demod->state == 4 &&
             params->NOF_DD_SYMBOLS_BEFORE_MEAN_ERROR_DIAG_PHASE3
                 == phase3Demod->word_2c)
        word_a4 = 1;
    if (phase3Demod->state == 10 || phase3Demod->state == 13) word_a4 = 0;

### An observation about the two state fields

`0x39717` tests `V90Equalizer::state` against 10, 11, 12, 16, 13, 14, 15 and
`0x3a25a` tests `phase3Demod->state` against 5, 10, 11, 12, 16, 13, 14, 15 --
the same odd ordering, with 10..12 folded into one range test both times. That
is the strongest available hint that the two fields share one enum type, and
it is why `V90Equalizer::state` can hold values the `enter*` family never
writes (finding 5700 §2). Recorded as an observation and not acted on: no
string names any of 10..16, and a wrong name is worse than a pad.

### state 0 — `RESET`, `0x3a0cb`

    decision = (short)soft;        /* no slicer at all */

## 5. The four re-convert blocks

`enterDataPhase`, `enterRRN` and `enterFPE` can flip `mmxMode` *inside* the
symbol loop, and each caller then re-expresses the in-flight state in the
other representation. **They are not four copies of one block**, and the
difference is measurable:

| block | site | caller | `dfeSum` from `d` |
|---|---|---|---|
| A | `0x39bd9` | FPE, `int_0028 == 0x1d` | `fistpl` → `(int)d` |
| B | `0x3a682` | PHASE4, `int_0028 == 0x1d` | `fistps` + `cwtl` → `(int)(short)d` |
| C | `0x3a89c` | RRN, `int_0028 == 0x1d` | `fistpl` → `(int)d` |
| D/E | `0x39df5`, `0x39eda` | DATA, after `enterRRN`/`enterFPE` | the inverse direction |

A, B and C all take `leSum = (int)(short)y` and `softInt = (int)(short)soft`,
convert the remaining `n - 2*j` input floats into `block_b4` from index 0
(setting `cur = block_b4`), and convert `outFloat[0..j-1]` into
`block_b8[0..j-1]`. Only B truncates `d` through 16 bits. Two sites agree and
one differs, so it is three source sites and not one helper — and writing them
as one helper would be wrong at exactly one of the three.

## 6. Epilogue, `0x38ea7`

    if (mmxMode)
        for (i = 0; i < nOut; i++) outFloat[i] = (float)block_b8[i];
    word_70 += nOut;
    if (word_70 >= (unsigned)errorEnergyMeanBlockLen) {
        word_7c = sqrtf((float)(unsigned)word_78 / (float)(unsigned)word_70);
        meanErrorEnergyCurrent = errorEnergyMeanK * meanErrorEnergyCurrent
                                 + (1.0f - errorEnergyMeanK) * word_7c;
        connEval->updateAvePdsnr(meanErrorEnergyCurrent, word_70);
        word_70 = 0;  word_78 = 0;
        if (word_a4) {
            meanErrorEnergy[meanErrorCount] = word_7c;
            if (++meanErrorCount == V90EQU_MEAN_ERROR_LEN) {
                meanErrorCount = 0;  meanErrorFull = 1;
            }
        }
    }
    if (n & 1) {
        word_68 = 1;
        word_6c = mmxMode ? (float)*cur : *in;
    }
    if (++word_34 == params->LINEAR_EQU_FADE_EDGES_CYCLE) {
        word_34 = 0;
        linearEquFadeEdges();
    }

Both `fildll`s push a zero high word first, so `word_70` and `word_78` are
read **unsigned**. `1.0f - errorEnergyMeanK` is `dc eb`, which objdump prints
as `fsubr` and which the architecture calls `FSUB` — `ST(3) = ST(3) - ST(0)`
— and the divide is `de f1`, `FDIVRP`, `ST(1) = ST(0)/ST(1)`. Read the
`<== Intel:` annotation `tools/dis.py` appends, never the AT&T mnemonic
(findings 245, 2156).

**`n == 0` with `word_68` set reads `in[0]`** at `0x39a79`. That is the
object's behaviour and any harness must pass a buffer with at least one
element so the reconstruction does not reach undefined behaviour on a trial
the blob survives (D561).

## 7. What a suite has to drive

Seven state arms times two `mmxMode` paths, plus 18 `word_30` cases, plus 5
distinguished `int_0028` cases, plus both `dsplibs_debug_level` settings —
four print sites are gated on `> 1` and never execute below it. Prove the
coverage from `tools/debugcov.py`'s line count over the new file; an assertion
that the arms were driven is not a measurement.

Five observable channels, and the last one is the one that gets forgotten:
`outSym`, `outFloat`, `nOut` through the reference, the equaliser's 336 bytes
through `diff_eq_obj` — **and the peers**. `setBllState` mutates the
resampler, `linearMappingStudy` the demapper, `updateAvePdsnr` the connection
evaluator, and an arm whose only effect is on a peer passes every comparison
that ignores them.

Bound the input at `n <= 511`: `block_b4` is 0x400 bytes (512 shorts, written
from index 1) and `block_b8` is 0x200 with `nOut = n/2`.

`test/unit/t_v90equ.cpp` already carries `equ_arena`, `mmx_setup` and
`mmx_run`; `test/unit/t_v90eqdata.cpp` carries the peer-seeding pattern
(identical varied bytes into both sides, `params` re-installed on top,
a saved seed to prove which peers were touched). Extend those rather than
starting again.
