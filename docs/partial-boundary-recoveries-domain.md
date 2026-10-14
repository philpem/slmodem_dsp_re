# Partial caller and owner-boundary recoveries

At e8393850, replay exactly two complete-TU cells per authorised function:
unchanged baseline and its independently witnessed boundary. Six TUs total.
This is an adoption review of already identified source boundaries, not a
reopening of finite source spelling or compiler option searches. The user
explicitly requested reviewing all three for partial fidelity improvements.

- v23FP_rx_progress passes an unsigned low word to FPM_MRF_filter in the
  original, but currently sign-extends it. Retain int input and all other uses;
  change only this argument cast to unsigned short. The caller changed since
  the earlier negative domain through diagnostics and AGC return recovery.
- RxNextStateV21 reloads root->hdx after each known-state diagnostic call in
  the original. Reload only on those three diagnostic arms; preserve all
  stores, default behavior and types. Audit the three source-unchanged inlined
  callers as collateral, not arbitrary bystander differences.
- faxvmi_hdlc_unframe reads the current parent across CRC/debug calls in the
  original. Preserve initial scalar snapshots and change subsequent accesses
  to current parent members. Review all changed reloads against the original,
  including output stores which could alias the parent. The earlier control
  remains a candidate, not an automatic adoption of every textual replacement.

Require raw baseline reproduction and fixed Gentoo profile/bug define-last,
selected assembler identity and full command/header/source hashes. Audit all
bodies, metadata, named data, constant/string pools and nontext relocations;
explain every collateral change. Adoption requires recovered original operand
or value-age boundaries and passing period differential checks; full byte
identity is measured separately. Preserve original bugs and all test checks.
No source/flag/ordering expansion after a miss. No fuzzing or mutation execution.
