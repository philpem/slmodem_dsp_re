# V8Process pointer publication boundaries

Baseline153b22b0, complete retained Gentoo profile/current headers. Original
V8Process731B has one publication of each advanced/wrapped pointer per sample:
tx_ring_base at0x745e0 after output and bound selection, tx_sym_b at0x74626
after two sample stores and bound selection. The retained ring advance/field
wrap emits duplicated publication paths and a different loop layout; original
symbol path already has one publication but a different operand graph.

Four full-TU cells cross per-iteration local transmit-ring cursor and local
symbol cursor. Acquire ring cursor after tx_avail decrement; load/write one
output then advance/wrap local and publish once. Acquire symbol cursor after
pole_state store, write real/zero pair, advance/wrap and publish once. No locals
held across v8handshak or loop iterations, no owner/type/layout/flag changes,
predicate inversion, count spelling or register/declaration permutations.
Existing signed thresholds/status/changed behavior and pointer bounds preserved.

Raw baseline reproduction and full-TU review required. Capture boundaries are
observable witnesses, not unique original spelling. Any source adoption requires
supported complete output and deciding period differential. Miss closes cross;
no source compensation for bystander register colors. No fuzzing/mutation.
