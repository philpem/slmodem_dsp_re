# V21 transmit control owner and request boundaries

902f47fa fixed profile. V21TX_control blob samples its hdx child only after
request-null guard and cfg/flag stores; current source captures hdx before
that guard. Blob snapshots request flags once before child boolean store
and reuses it for reinit, whereas current source rereads. Cross only late
hdx sampling and unsigned-char request flags snapshot at first current use.
Keep all predicates/scale/config/flag stores/calls and result owner fixed.
Full-TU audit; no mask rearrangements or shared headers. Null request must
retain original no-child-read behavior, and original request overlap boundary
is part of source fidelity.
