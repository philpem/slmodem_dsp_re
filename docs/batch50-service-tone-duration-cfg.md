# TONE original expiration arm layout

Threecells raw902, shortreturnn seed, and expire-first direct duration tests
with explicit else. Reference compares duration against elapsed (JA keep),
then zero against duration (JAE keep), then expirationphase block precedes
saveelapsed arm. Current keep-first earlyreturn reverses x87 comparison
operands and differs on unordered values under period noIEEE flags. Use
`if(duration<=tm && duration>0) { expire } else { save }`, based on blob
operand/branch direction, not purely logical complement assumption. Preserve
floatlocals/cast/constants/numericorder. No deliberate spill, volatile orflag.
Short/intreturn alternatives previouscanonicalmap identical; shortsavedn
return lacks unique highbit proof. FullTU raw/headeroverlay/data/nontext/
relocs/metadata/bystanders. Parent period gate handles finaladoption ifexact.

Measured result: 3cells expire-firstSIZE16 and poolzero removal4B, no gain. Unordered semantics distinguish CFG forms. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
