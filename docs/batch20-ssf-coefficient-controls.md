# Spectral shaping cached coefficient width and positive block guard

Four complete TUs at93d7eee1 cross only two independent observed boundaries:
1. Cached coefficient locals b0..b3 and a0..a3: long double vsfloat. Blob
progress spills those copied float coefficients with fstps and consumes them
with fmuls; reconstruction spills them fstpt and consumes fldt/fmulp. Coeff
loads themselves perform no arithmetic, so float caches preserve exact values.
No accumulator/intermediate width change, especially no getMetric ABI change.
2. progress entry early return for left==0 versus positive left>0 enclosing
body. Blob uses cmp0/jbe, ours test/je. Positive outer guards are an established
Playbook source CFG lever, not a flag/synonym sequence. Crossing them tests
whether coefficient width and entry CFG independently matter.

Do not expand coefficient widths, states or arithmetic operand permutations
if these four cells miss. Complete TU/bystander/data/metadata/reloc audit and
raw baseline reproduction mandatory; period gate deferred to batch owner.

## Result: closed without adoption

4/4 complete TU controls valid, exact set4/6 unchanged. Baseline progress
163vsblob172/SIZE9; float caches161/SIZE11. getMetric167vs172/SIZE5
unchanged. Positive entry enclosure changes neither residual. Initial RTL
cache modes fire XF→SF. Complete metadata/data/nontext/relocation audit clean.
No source adoption. Coefficient spill width is measurable, but its isolated
recovery does not reproduce the complete function or infer state widths.
