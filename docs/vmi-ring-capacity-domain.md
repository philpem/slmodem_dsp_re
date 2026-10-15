# Ring capacity: compare before narrowing

Baseline: `1ec83de9`. `FAXVMI_create` loads unsigned word capacities, adds 3
to the promoted `max_frame` at `0x95176`, and compares that full value with
`fifo_size` at `0x95179`. Only the selected fallback `max_frame + 3` is
narrowed at `0x95182`. The reconstruction narrows `need` before comparison.
For `max_frame = 65534`, `fifo_size = 8`, the original chooses ring size 1;
the reconstruction chooses 8. This is a component constructor discrepancy;
it does not establish a reachable fax-call configuration.

Test four complete-TU cells: retained word need versus promoted int need,
crossed with word versus int ring carrier. When need is int, explicitly
narrow the fallback arm, preserving the original selected-value boundary.
Keep configuration field types, downstream allocations and loop bodies.
Prediction: only promoted need fixes the boundary, and int need preserves the
original signed comparison over the positive promoted domain. Ring width
alone must not be treated as a repair.

Add a fixed constructor regression through documented null slot 0, no planted
internal history and no modem processing. Use ordinary, wrap-edge and equality
rows. Keep FIFO requests at most 8; choose wrap cases whose selected ring is
0, 1 or 2, so no large input buffer or out-of-bounds access is required.
Check both constructors' state and explicit expected size. Compare initialized
contents only through the smaller allocation while the old source differs.
Preserve the failing baseline; audit full TU data, exports and bystanders.
