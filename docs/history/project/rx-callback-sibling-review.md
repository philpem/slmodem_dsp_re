# Receive control/status sibling review

Read-only review of8functions in4complete receive TUs (V17/V21/V27/V29), excluding root-owned transmit controls/statuses, fax_class1_status and selectFilter. Three alreadyexact: V17RX_control,V27RX_control,V27RX_status. Five nonexact: V17RX_status(SIZE10),V21RX_control(SIZE1),V21RX_status(SIZE5),V29RX_control(BYTES74),V29RX_status(SIZE9).

All eight original/retained pairs have one RET and matching CALL counts (one each, except V27RX_status zero); all already zero EAX near entry. This detector reports denominators and fires on the three knownexact bodies rather than treating silence as evidence. V21RX_control also matches the original two literal-one writes and shared reinit-return epilogue. The latter's captured request byte/TEST-SETNE source family is already recorded in fax-rx-flags-domain.md; V29RX_control snapshots are closed in batch50-fax-v29-control-domain.md. No new return/control-graph witness reopens either domain.

Receive statuses have original shared epilogues already; their differences involve load widths/byte-flag operand graphs, not a missing shared-result statement. No candidate source control was declared or compiled from these siblings. This is a bounded8-function decline, not a global remaining-wrapper exhaustion claim.

Reproduce tools/rx_callback_sibling_review.py; build/rx-callback-sibling-review.json retains source/object mappings, original/retained instruction rows, grades and RET/CALL/initial-XOREAX/literal-one counts.
