# Spectral sign and evaluation-boundary controls

The original `displaySpectralParams` has four late FCOMPP sign comparisons,
full-width SBB/AND/ADD sign arguments, and four FLDL double scale loads (three
pool references and one rematerialised stack value). The retained body has
four earlier FCOMP comparisons, four byte-result extensions and four FLDT
extended-precision scale loads. These are separate observations, not proof
of a unique helper signature or source expression.

[The domain](spectral-sign-carrier-domain.md) crosses helper-result and local
cache width, three observed evaluation factorizations, and coherent DF versus
XF fractional intermediates. The final cross contains **24 complete-TU cells,
20 unique source texts, nine raw emissions and 264 shared-body verdicts**.
It reproduces the retained raw baseline with the fixed Gentoo profile,
DSPLIB_REPRODUCE_BUGS, recorded complete commands and selected assembler.

No cell produces an exact gain or loss. Only displaySpectralParams changes;
all ten bystanders, symbol metadata, named data and six edprintf calls remain
unchanged. DF cells change only the two expected constant pools; the audit
records their complete before/after payloads rather than discarding them.

| Factoring | XF, byte cache | XF, int cache | DF, byte cache | DF, int cache |
| --- | ---: | ---: | ---: | ---: |
| Retained sign first | SIZE9 | SIZE11 | SIZE1 | SIZE19 |
| Whole/fraction first, sign last | SIZE29 | SIZE19 | SIZE3 | SIZE13 |
| Direct helper arguments | SIZE5 | SIZE5 | SIZE13 | SIZE13 |

Each table entry is the absolute size difference from the 619-byte original;
it is not a distance over instructions or evidence of recovery. Changing
only the helper return width is raw-inert for each cache/factoring/mode pair.
The direct-argument cells have no cache and therefore duplicate source text
across the nominal cache axis.

The controls distinguish several mechanisms:

- A wider cache removes the byte extension, but the XF retained-order cells
  still have the early FCOMP comparison.
- Sign-last and direct-argument XF cells restore four FCOMPP comparisons;
  direct arguments also remove the byte extensions without changing the
  helper return type. The original native sign width therefore does not
  uniquely establish that return declaration.
- DF fractional intermediates remove FLDT, but emit **four FMULL memory
  multiplies, zero FLDL loads**. They do not recover the original scale-load
  lifetimes, including its fourth-block double rematerialisation.

No reconstruction source is adopted. In particular, SIZE1 does not justify
accepting a body whose operand/lifetime structure remains different. The
finite width/factoring/fractional-mode domain is closed. A useful new witness
would identify the original scale's capture/storage lifetime or a helper/TU
boundary; another nearby type or statement permutation is not such a witness.
This does not close the separate whole-object inline-profile question.

```
python3 tools/gcc3_spectral_sign_carrier.py --domain docs/history/pcm/spectral-sign-carrier-domain.md
python3 tools/gcc3_spectral_sign_carrier_audit.py
```

Replay after header drift requires `--historical-headers` and an archived raw
baseline through `--baseline-dir`. Artifacts are under
build/gcc3-spectral-sign-carrier; the independent audit writes
build/spectral-sign-carrier-audit.json. No runtime, fuzzing or mutation was
executed for these rejected candidate sources.

## Integrated validation

The fresh strict census remains **1085/1852 EXACT, 120105 original exact bytes**;
668 SIZE, 65 BYTES, 29 REGALLOC and 5 UNRESOLVED. Ratchet passes. All300
canonical objects remain raw-identical throughout the screens and equal their
faithful partial-link inputs. No source, header, fixture or build profile
changed, so this apparatus/findings batch does not repeat the unchanged period
runtime suite. The preceding source checkpoint's permitted period387/0 remains
the last runtime measurement; t_fuzz stays excluded, and no full-phase claim
is made here.

The spectral audit, both result-screen bounds and Python compilation pass.
Structural targets pass, including285 suites/10038 static anchor locations,
2265 offset annotations and14445 references with zero unresolved or repointed
citations. Static anchor validation does not execute mutations. The new
historical detector control reports two nominated calls, including the wrong
Descrambler result consumption. No fuzzing, mutation or portability execution.
