# Queue full-query ownership in the changed V92 TU

Baseline57dbbf52. F11679's isFull/spacelocal cells recover the85B scalar write
but lose734B V92Modulator C2 to a split-store carrier. Since that baseline,
initiateRRN and initiateFPE publish their state before the diagnostic call;
these two observed source corrections change the full TU's emitted history.
No Queue or constructor permutations are authorized. This is a contextual
replay of a previously positive source family, not new source spelling recovery.

Three cells: unchanged full V92Modulator TU/header; scalar write calls its
existing isFull helper; scalar write captures existing unsigned space then
checks zero. Preserve ring arithmetic, return ABI, cursor store and all callers.
The helper already exists and has an independently decoded use in progress.
Predict helper factoring still recovers the85B write; explicitly test whether
the prior C2 loss survives the changed context. Falsifier is any exact loss or
incomplete write identity. A miss closes this current-context replay; no
nearby declaration, position, predecessor or flag fitting follows.

Use current hashed headers and complete Gentoo configuration, reproduce-bugs
appended, executed assembler identity, all dumps. Require raw production
baseline reproduction; audit all emitted bodies, metadata/data/BSS/relocations
and every exact bystander. Trace the lost constructor store through RTL if it
survives, without claiming original RTL or a source fix from scratch colour.
No fuzzing/mutation execution. Adopt only complete exact-supported source
with a production census and batch-end phase gate.
