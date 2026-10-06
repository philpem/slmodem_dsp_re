# Transfer shortlist: live SI conversion excludes gratuitous XF producers

Read-only inventory against strict1070 baseline, using the archived complete production objects in `byteexact-eia6-x87/build/production-before`. No source variants, compiler invocations or profile experiments were performed. This is a bounded transfer screen, not a claim of complete source-type recovery or exhaustive remaining-function coverage.

## Scope and detector denominator

There are299 current C/C++ source paths. Excluding16 V34 paths and the shared `src/pump/v90/vpcm.c` leaves **282 translation-unit source paths inspected**, within the requested283 cap. `VpcmFloModem.cpp` is included. The explicit excluded paths are recorded in the artifact.

The parser strips comments/strings, recognizes ordinary out-of-line function definitions, and finds44 bodies containing explicit `long double`.29 map uniquely to emitted function bodies by demangled signature name.15 helper bodies have no emitted name match; none has multiple matches. Those helpers are not attributed to their inline consumers by this screen and are an explicit coverage limit. Header-only types/functions and long-double arguments without explicit long-double body text are outside this detector.

Among the29 mapped bodies,14 are nonexact against the original and at most800 original bytes. Four have original SI nonpopping `fistl` and fewer retained `fistl` / more retained `fistpl`: the existing EIA6 positive, two350-byte beta setters, and the closed773-byte `VPcmFloModem::getUinfoValue` domain. This fires on the known EIA6 positive; its large graph is not re-investigated here. DI `fistpll`, 16-bit conversions, and ABI-related blackman/other closed domains are not mistaken for SI evidence.

## Two small candidates, one coherent producer family

| Function/source | Original first live conversion | Occupancy | Calls before conversion |
| --- | --- | ---: | ---: |
| `V90Equalizer::setLinearEquBeta(float)`, `src/pump/v90/V90Equalizer.cpp:644` | `0x364c8`, SI FISTL | 2 | 0 |
| `V90Equalizer::setDfeBeta(float)`, `src/pump/v90/V90Equalizer.cpp:723` | `0x36628`, SI FISTL | 2 | 0 |

Symbols are `_ZN12V90Equalizer16setLinearEquBetaEf` and `_ZN12V90Equalizer10setDfeBetaEf`. Each original body is350 bytes and has one `fistl`; each retained body has none. The original first conversion sequence loads float argument beta, loads the current member beta for comparison, and pops the member comparison operand. The diagnostic arm then loads the scale and performs a **nonpopping register FMUL**, leaving scaled on top and beta below it. FISTL retains scaled at occupancy2. Immediately following `fld %st(0); fabs` proves that scaled is still consumed after conversion, rather than a dead value coincidentally converted with a different opcode. The fractional computation also consumes scaled. No earlier CALL obscures the stack-depth argument, and the entry/guard path gives ample spare x87 capacity.

The reconstruction explicitly declares `long double scaled = (long double)beta * [scale]f` inside each diagnostic arm, then formats its absolute integer and fractional parts. Under the hash-pinned stack-pass rule corroborated by existing actual Gentoo controls, a live XF input with spare capacity acquires a duplicate and REG_DEAD, then emits FISTPL. Thus the original live FISTL constrains this diagnostic producer against XF and leaves SF/DF alternatives. This is the same independently established mechanism as EIA6, applied to small functions with a less ambiguous entry graph. It does not decide the C type uniquely, remove rounding semantics, or authorize a score-driven type sweep.

## Closed domains and the new discriminator

`docs/batch100-v90-beta-lifetimes-domain.md` already measured four controls: unchanged, explicit whole local, shifted operand before publication, and both. All verdicts were unchanged; that source-lifetime domain remains closed. It explicitly held all types/casts/constants fixed. The newly verified **live XF save rule** is an independent discriminator absent from that experiment, so the first diagnostic scaled producer is a bounded new question rather than repeating the failed variable-placement controls.

F11353 audited redundant casts and retained meaningful casts in the **later MMX beta assignment**. Those casts and the defined masked shift helper belong to a different computation and must remain fixed when assessing the diagnostic producer. F11353's source-level reason to preserve expression types is not proof that the original diagnostic producer used XF. No new experiment or candidate source spelling is adopted in this report.

`VPcmFloModem::getUinfoValue` is excluded from the new shortlist: its work-vector/final-store/level-mode/log-mode domains are already recorded in the batch100 ledgers, and its conditional diagnostic loop has less straightforward call/lifetime history. This screen supplies no new operand discriminator for that closed family.

## Reproduce and review

```sh
python3 tools/gcc_x87_transfer_screen.py \
  --objects build/production-before \
  --census build/baseline-byteident.json
```

`build/gcc-x87-transfer-screen.json` records counts, explicit exclusions, unmapped helper names, exact source/function mappings, SI conversion sites and full original/retained instruction rows for the four detector hits. These are read-only codegen observations, with **two new eligible transfer candidates** after excluding the positive control and the closed family. Parent owns any subsequent finite domain declaration, full-TU compilation and complete-function decision.
