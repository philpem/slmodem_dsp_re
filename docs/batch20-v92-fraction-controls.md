# V92 fraction expression mode control

Two complete-TU cells93d7eee1: unchanged and ordinary float fraction
`__builtin_abs((int)((v-(float)(int)v)*1.0e6f))` replacing explicit long-double
operand promotions in frac_of. Keep public float argument, million constant,
truncating integer conversion, subtraction order and abs behavior. Blob's
first two diagnostic fractions load million with flds before conversion and
use fmulp; ours uses a memory fmuls after the subtraction. Float arithmetic
mode can alter x87 expression scheduling despite retained excess precision,
as independently established in realfft. The literal remains identical.

No gain-scale cast change, helper order/inline flag, argument permutation,
modern compiler fit or overflow semantics change. Stop if fullbody misses;
no expanding neighboring precision spellings. Require baseline raw identical,
fullbody/all data/metadata/relocation audit; gate only at batch end.

## Result: closed without adoption

2/2 complete TU controls valid, exact set4/5 unchanged. Both setter bodies
2679vsblob2695/SIZE16. Other four public bodies remain exact. Full metadata,
data, nontext bytes and relocation audit clean. No source adoption; no gain
scale changes or neighboring fraction spelling sweep.
