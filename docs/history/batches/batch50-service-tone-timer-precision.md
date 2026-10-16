# TONE elapsed expression precision

Raw902, shortreturnn seed, double0.125 literal with/without enclosing float
cast (fourcells). Reference fmuls exact0.125 cannot distinguish literalfloat
from double because exactconstants can use narrower x87memoryoperand. But
reference narrows completed elapsed through float slot before duration
comparison, currentfloat-expression assignment remains80bit. Author uses
unsuffixed doubleliterals elsewhere in thisfunction and TONE_create. Test
normal `tm=n*0.125+t->elapsed` and explicit completedexpression floatcast,
not artificialspill orvolatile. Preserve source keepfirst CFG and phase
work, headersshortreturnn consistently. x87 expressiontype and real GCC
passboundary distinguish source numericexpression before registerfitting.
AllTUraw/data/nontext/relocs/metadata/bystanders, no runtime experiments.

Measured result: 4cells doubleliteral/cast canonicalbodyinert249B/SIZE5, no gain. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
