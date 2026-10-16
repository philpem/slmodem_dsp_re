# V29 original request byte snapshots

902f47fa profile. V29RX_control's blob snapshots ctl1 once before child
boolean store and uses the same byte for reinit; snapshots ctl0 before first
conditional child clear and uses same byte after it. Current source repeatedly
loads members, including a reload after first child write. This is the
opposite of V17/V27's witnessed reload family. Independently capture ctl1 and
ctl0 at their first current uses with unsigned-char locals, retaining all
predicates/stores/calls and child member accesses. Four full-TU cells.
No register-local permutations, declarations or shared type changes. Original
snapshot can matter if caller request and child state overlap; exact object
boundary decides. Preserve invalid control bits and self-reinit behavior.
