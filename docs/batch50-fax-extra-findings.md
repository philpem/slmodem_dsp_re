# Completed-fax follow-up: two exact source boundaries and bounded negatives

Base 902f47fa. Retained Gentoo GCC3.4.2-r2/binutils2.15.92.0.2 profile,
complete saved command/config and DSPLIB_REPRODUCE_BUGS on every cell.
`tools/batch50_fax_extra_audit.py` audits 92 valid complete-TU cells:
unchanged named data, export/type/binding/visibility metadata, allocated
nontext, symbolic relocation target multisets and all unselected bodies;
zero exact losses. Fifteen additional V27 header consumers, including the
empty implementation TU, have raw-identical object proofs rather than
relying on a data-only inspector's interpretation of common symbols.
Two unique gains total 198 original bytes. No header, flag or runtime-test
changes; root owns the integrated byte census and final period gate.

## Adopted source boundaries

* `FAXVMI_delete` (114 bytes): the original reloads both child owners from
  the virtual modem after each unknown delete/free call. Current snapshots
  retained a deleted or callback-changed owner. Independent link-only and
  framer-only reloads miss; both together are exact. All other full-TU
  bodies, including its two previous exact bodies, remain unchanged.
* `GetT30FrameIDFromBuffer` (84 bytes): invalid-address default result is
  established before the valid-body result assignment. A named default
  result closes; positive guard alone misses. Positive guard plus the
  same default result is also exact, so branch spelling is not uniquely
  recovered. Adopt the smaller default-result change and retain that
  ambiguity. Entire two-symbol TU becomes exact.

## Independent, closed negative domains

* Virtual-modem clear loops: unsigned-short index at create/control is an
  original MOVZWL/word-boundary witness. Four cells reduce size gaps
  (create 103 to 68; control 58 to 26), no exact gain or loss.
* `V21TX_status`: late child-flag capture across the status store and first
  child flag clear, crossed with mask lifetime; five cells. Gap 6 to 5,
  no closure. Original callback/read boundary retained as evidence.
* SGD: original quotient CWTL supports short scale; independently bounded
  postdecrement loop. Five cells; `SGD_correlate` reaches equal 114 bytes
  with BYTES7, `SGD_pattern_det` remains SIZE2. No adoption.
* `V29RX_control`: original snapshots each request byte before subsequent
  child stores. Four cells; joint snapshots BYTES24 versus BYTES74
  baseline, no closure. No flag-type or struct changes.
* `V21TX_control`: original late child read after null request guard and
  stores, crossed with byte-flag snapshot. Four cells; both reach equal
  94 bytes with BYTES11. No adoption or register-fitting expansion.
* V27 descrambler count interface: consistent unsigned-short public count
  with short consumption at the inner callee, crossed with two caller
  cast removals. All 17 directly including TUs, 38 cells. Callee raw bytes
  identical, 15 other consumers raw identical. Data size gap 3 to 2,
  protocol gap 4 to 5. No gain or loss; header remains unchanged.
* V27 result lifetime: original unsigned quality-result mask with signed
  narrowing only at return. Four cells per TU crossed independently with
  the consistent public interface, eight cells; no gain/loss. No adoption.
* Ten V17/V29 transmitter handlers: original signed countdown compared
  against zero-extended budget. Baseline short budget comparison differs
  for high-bit budget: positive countdown wins in the original, negative
  budget wins in the reconstruction. Two independent type/use axes,
  followed by a witnessed member-decrement reload axis, sixteen full-TU
  cells total; no gain or loss. Preserve this whole-domain semantic
  discrepancy explicitly; no invented valid-input excuse, no register
  permutations and no source adoption disguised as byte progress. Future
  semantic recovery needs its own period boundary evidence, not a widened
  tolerance. Existing exact Idle predecessor gains were not modified.

Each finite domain has its own `docs/batch50-*-domain.md`, replay tool and
`build/batch50-*/results.json`. These are additional to the stable initial
83-cell, 16-gain package; its files and winning objects remain unchanged.
