# CID core exhaustive block-length assignment

Original cid_progress enters rate checks without a zero length seed: its EDI length is established in the non-FSK arm0x8ff7a..8ff8f or non-DTMF arm0x8ff9a..8ffad. Current int len=0 adds a hoisted zero at entry. At least one of mode!=0 and mode!=1 is true for EVERY int mode, so that default value is never read; no valid or invalid mode depends on it. This is the supported removal of an unreachable initialization, not an arbitrary declaration permutation. CID reset's closed store/call families are excluded.

Predeclare two completeTU cells: baseline and uninitialized len declaration whose existing exhaustive assignments stay exactly where they are. All mode reloads, array copy increments, short carriers/status/loops, call arguments, outputs and diagnostics remain fixed. Raw completeTU baseline and all-body/metadata/data/canonical relocation audit mandatory; only strict target exactness authorizes source adoption. No bystander-only register gains, flags, headers, padding or source changes for a synthetic fixture.

Measured: both valid fullTU cells raw reproduce baseline; cid_progress remains SIZE108 and no exact gains/losses. Exhaustive initializer removal is inert, domain closed, no adoption.
