# CID wrapper receiving-result width and nonzero dispatch

Pinned856c1ecb. Original CID_process0x4f5 sign-extends the short cid_progress result, TESTS full EAX forzero, then DEC EAX/nonzero branches to failure0x4fa. Baseline explicitly receives short res and tests its low word against1 first, then full result nonzero. This supports an int receiving carrier and outer nonzero-result gate followed by success/failure split. Existing reconstruction signatures/cid_reset controls and Data byte/hex domains are separate.

Predeclare four completeTU cells: baseline; int receiving res only; outer res!=0 dispatch with inner res==1 success versus failure; both. Preserve ret entryzero and perchunkzero, signedchunk clamp/shortcountout/retirement, every host/debug call and input owner, all messages/string traversal. Original has no ret entry initialization for nonpositive count, but that undefined-source edge is NOT changed in this behaviorally inert domain. Raw baseline and everyTU body/metadata/nameddata/canonicalreloc audit mandatory, targetstrict exactness. No padding/flags/header edits/arbitrary ordering.

Measured: Four valid cells: int receiving carrier inert SIZE18, nonzero dispatch57, both58. No strict gains/losses; no adoption.

Data audit qualification: dispatch changes anonymous .rodata.str1.1 emission order, so the unchanged-nontext audit correctly refuses that cell. Separate CID complete ledger asserts every named object/metadata record, exact string multiset including multiplicity, and records full canonical nontext before/after. No string content or call target changes; these remain negative controls, not adopted source.
