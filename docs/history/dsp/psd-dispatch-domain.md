# PSD output dispatch control

Pin b6db3cfc plus the witnessed frame/pos-before-division source recovery
from size-spill-results.md. Complete TU/current headers, fixed Gentoo profile
and mandatory bug define. Baseline must reproduce current canonical object raw.

Original dispatch compares option1, branches equal to peak, branches signed
less-or-equal to the lower option, then compares2. Candidate if-chain compares1,
then2, then tests0. A three-case switch is an independently witnessed source
hypothesis. Two cells only: recovered-home baseline and switch with all three
case bodies preserved verbatim, break after each, default break. Preserve option
values, unknown-option no-op, FFT/window/rounding/division and peak semantics.

Prediction: switch lowering recovers the original ordered dispatch graph.
Falsifier: unchanged graph or new unsupported operations/changed case behavior.
No multiplier/local-type/output-loop axis. Check full TU bodies/bindings/data,
retain a partial dispatch recovery only if original instructions support it.
No fuzzing/mutation execution; final batch gate for adoption.

## Result

Two cells/26 live common-body verdicts. Baseline reproduces the recovered-home
canonical object raw. Switch recovers cmp1/je/jle/cmp2/je followed by the default
epilogue; the peak destination is +385 in both original and candidate. Lower
option and linear destinations remain +528/+487 versus original +541/+491.
Only process changes; twelve bystanders, symbol metadata, named data, allocated
data and nontext relocations stay unchanged. All eight exact bodies stay exact.
Process changes from626B/SIZE9 to618B/SIZE17 against original635B. Retain the
original dispatch graph despite the larger size gap; it is a partial recovery,
not an exact gain. Case bodies are preserved verbatim inside separate blocks,
each with break; default retains the no-op for other options. Arithmetic,
FFT, narrowing and division boundaries remain unchanged.

Peak-loop/x87 operand ownership, zero-bin normalization placement, register
allocation and padding remain separate questions. The source now matches the
original dispatch category, so do not permute case order to chase bytes.
Investigate those independent witnesses before further source controls.

Reproduce with:

```
python3 tools/gcc3_psd_dispatch.py --domain docs/history/dsp/psd-dispatch-domain.md
python3 tools/gcc3_psd_dispatch_audit.py --integrated
python3 tools/gcc3_size_spill_audit.py --integrated --psd-dispatch
```

Commands/config/header/source/object hashes, initial and later RTL, all grades
and the checked dispatch window are saved in build/gcc3-psd-dispatch. The stack
observer controls precede the switch; their integrated check uses the explicit
--psd-dispatch flag to verify the subsequent chosen source/object raw rather
than claiming that those observations traced the switch cell.
