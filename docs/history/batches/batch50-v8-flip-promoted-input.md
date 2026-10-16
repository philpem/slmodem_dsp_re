# V8 nibble reversal promoted input

Declared domain before compile: complete V8global.c baseline902f47fa plus
one explicit unsigned working copy of the byte argument in charFlip.
Blob shifts a full register right four before nibble lookup. Reconstruction
shifts the low byte then zero-extends it, emitting six extra bytes. Public
byte argument/return and unsigned byte table remain unchanged. This is the
input promotion boundary before both lookups, not a table/signature rewrite
or register fitting. No prior compiled source local-promotion family found.
One candidate then close; no synonyms. Raw baseline/profile/bugdefine and
complete TU audits, original caller inlining impacts required. Parent phase
once at batch end; no fuzz/mutation/harness execution here.

Result: promoted working copy reproduces charFlip EXACT33B,7/13→8/13 exact
TU, zero losses, only charFlip changes. Initial RTL shifts SI rather than QI.
2/2 complete TU metadata/data/nontext/relocation and bystander audits pass.
