# Ring constructor source-factoring control

Baseline93d7eee1, only src/service/ringDetector.c; no rd.c/header edits.
F11351 and docs/cid-dcr-audit.md close plain/explicit-inline Reset controls,
store-order families and six temporary families. Do not repeat those cells.
This new two-cell family retains baseline call versus duplicating the exact
Reset source body in Create after malloc (as a nested scope). No flags, added
spills, qualifiers, local permutations or forced-inline attributes. The object
contains the same Reset operations in both emitted functions; whether a call
with inline eligibility or duplicated original statements supplied the source
is not uniquely identified. Complete TU symbols/data/bindings/bystanders and
canonical relocations decide whether this independent factoring closes either
body; close on a negative result. Known unchanged Delete/GetLastRing positives
exercise exact detector. Period differential deferred to root end-batch gate.

Result: 2/5 exact unchanged. Reset remains SIZE32 (449/417 bytes);
Create literal-body variant improves SIZE411→17 (452/435 bytes), but does
not close. Only Create body changes. Metadata, named data, allocated nontext,
canonical nontext relocations are identical; text removes only Reset call
and adds exactly three original diagnostic relocations. Delete/GetLastRing
remain exact positive controls. No source change adopted; domain closed.

Execution isolated into /tmp/slmodem-batch20-ring at93d7eee1 because the
primary data worktree already has authorized adopted shared-header changes.
Initial invocation there was rejected by the driver's unchanged-header guard
before compilation; preserved /tmp/batch20-ring-factoring.log as invalid
preflight. Valid two-cell results build/batch20-ring-factoring/results.json
and full-audit.json in the isolated ring worktree. Retained baseline object
raw-reproduced, compiler/assembler identity and complete BUG-defined flags
recorded. No local/parameter/order extension after this negative result.
