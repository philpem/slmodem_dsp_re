# Dial string verdict and diagnostic ownership

Blob IsDialStringInvalid uses SETBE after CMP1 (unsigned verdict) and resolves
the syntax-table pointer before the final debug-level test (0x7af6d..7af7e).
Retained source uses a signed local/SETLE and evaluates the helper only inside
the printf branch. Four complete TUs cross unsigned grade with an explicit
post-analysis syntax pointer computed before the debug gate. No helper/API,
flags, unrelated constructor/order changes. Positive body must be exactly149B
and all data/table targets/bindings/nonexact neighbours reviewed. A syntax
callback is not invented; its existing pure helper stays unchanged.

Result: early syntax acquisition restores149B; unsigned cross leaves BYTES2. Every mnemonic/operand matches except the unrelocated local CALL: original AnalyseDialString displacement -1344, candidate -416. The local callee/layout is still different; do not adopt the caller for a near-hit or silently mask that displacement. No existing exact names are lost, but DialerCreate changes through TU history and is retained in the full audit. A helper/body/TU witness is required before reopening this line.
