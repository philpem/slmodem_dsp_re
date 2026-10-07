# Small value-use controls: no source adoption

Baseline `a95a6c65`: 1074/1852 strict exact functions, 118087 original bytes.
Gentoo GCC 3.4.2-r2 and selected assembler 2.15.92.0.2, unchanged complete
configuration, mandatory bug reproduction. Three raw complete-TU baselines
reproduce. Eleven interpreted cells cover 99 shared body verdicts, zero gains
and zero losses. Production source is unchanged.

| Function / domain | Cells | Result |
| --- | ---: | --- |
| V32FP_GetCleanedSamples accepted-count fallthrough | 2 | SIZE6 in both; original branch restored, owner reload still absent |
| V22_MRF_init early permutation counter / separated increment / captured coefficient | 6 | SIZE16/16/17/1/17/17; no complete identity |
| fComputeRMSValueFloatBuf native reciprocal literal / reciprocal-first operands | 3 | SIZE4/10/4; operand-first cell raw identical |

The V32 and V22 controls change only their nominated function. All symbol
metadata, allocated nontext, BSS and canonical nontext relocations remain
identical. V22's one-byte size miss has different saved registers, allocation
and store scheduling; it is not a one-byte repair or an adoption.

The RMS double-expression cell emits the stack one, but at the tail rather
than across the residual loop, and adds a final float store/reload absent
from the original. Its pooled float one vanishes from `.rodata.cst4`; all
other allocated nontext, metadata/BSS/nontext relocations remain identical.
It changes three caller bodies (`CrossDataLinks`, `FDSP_DP_Run`, `bSearchEnergy`)
through inlining as well as the standalone helper; their complete verdicts
remain nonexact. The SF operand reversal emits the baseline raw object.

These bounded families are closed. An original owner reload calls for
independent alias/type or profile evidence; a register-lifetime difference
after recovering one boundary does not authorize declaration/register
permutations. A stack one does not uniquely establish a double literal, and
restoring that opcode without its lifetime and return narrowing is insufficient.

Reproduce:

```
python3 tools/v32_cleaned_path_reproduce.py --domain docs/v32-cleaned-path-domain.md
python3 tools/v22_mrf_counter_use_reproduce.py --domain docs/v22-mrf-counter-use-domain.md
python3 tools/rms_reciprocal_literal_reproduce.py --domain docs/rms-reciprocal-literal-domain.md
python3 tools/small_counter_use_audit.py
```

Artifacts are beneath `build/v32-cleaned-path`, `build/v22-mrf-counter-preload`
and `build/rms-reciprocal-operands`, including complete sources, commands,
compiler logs, hashes, RTL dumps, body grades and disassemblies.
