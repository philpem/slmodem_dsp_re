# Timing diagnostic modes: measured controls and corrected nomination

Base a95a6c65. Original adjustHalfBaudBpfGain664B versus retained655B:
original diagnostics execute hardware FABS before whole conversion and load
1000 from a DF .rodata.cst8 operand before register FMULP. Retained has
an XF .rodata.cst16 fractional multiplier. The initial nomination mistakenly
attributed an earlier FCOMP to conditional fabsf. Full-body inspection refutes
that attribution: retained has three FABS too, evaluated later; the earlier
comparison is sign computation. Both math-header forms invoke builtins.
Private whole_of uses fabsf and frac_of explicitly carries XF x/d. The helpers
have only this method's three print consumers. Earlier addPhase and
timingCorrection domains do not test these helpers and stay closed.

The recorded declaration was baseline, ordinary double fabs for whole, coherent double x/d
fraction arithmetic, and both (2x2), plus unchanged raw repeat. This is not
an arbitrary SF/DF/XF source-type sweep. Only the fractional memory-operand-mode
witness survives review. Whole-axis cells are excluded from justified source
recovery and closure inference: no missing FABS instruction was witnessed.
Preserve float helper argument/scale types, all scale values/call arguments,
fraction subtraction and integer absolute operation, three separately guarded
prints, member reloads, state/normalization/store computations and profile.

The original prediction was FABS and DF fractional-multiplier recovery. An exact
whole body, source-faithful arithmetic and unchanged full-TU bystanders/data/
bindings are required for adoption; absence of exact closes the domain with
no neighboring spelling/type matrix. Retain actual preprocessed helper/math
header expansion to distinguish source hypothesis from apparatus definitions.

Measured five complete TUs, 70 body grades, zero gains/losses, only target
changes. Baseline655B; DF fraction alone623B; double-whole alone655B; both655B,
original664B. All contain three FABS. DF fraction removes three FLDT but
does not reproduce original DF fractional loads. Thirteen bystanders, named
data/BSS/bindings and nonconstant nontext relocations remain unchanged;
expected constant-pool changes are in complete-tu-audit.json. Both unchanged
raw compiles reproduce the saved production object. No source adoption.

Preserve the unsupported whole-axis nomination and correction rather than
claiming its failure closes the source family. The supported fraction axis
closes without exact; no neighboring type/spelling matrix. A future whole
diagnostic question needs a different actual operand/order witness.
