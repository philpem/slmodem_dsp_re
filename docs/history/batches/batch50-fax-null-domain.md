# Null process count use and receiver init result ownership

902f47fa fixed profile. Blob null_process compares *count as a signed word
(cmpw/jle/jg); ours promotes an unsigned short to int. Cast only at comparison
use, preserving public signature. Independently place dp buffer read inside
positive-count guard as original blob does. Baseline plus signed-count,
guarded-owner and both controls; no type/layout changes.

_rx_look_carrier_init ends one XOR shared by countdown store and return;
ours has a second XOR. One ordinary return-assignment control makes zero
value ownership explicit without changing calls or store. Byte comparison
must settle adoption; no register constraints or padding/cursor controls.
