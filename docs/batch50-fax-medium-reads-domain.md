# Medium completed fax original callback and member read boundaries

Base902f47fa complete retained Gentoo profile/config/raw baseline/assembler,
DSPLIB_REPRODUCE_BUGS. Independent source statements witnessed in blob:

_tx_scrambled_ones_state (681B) initializes result from *tx_count immediately
before FAXVMI_process, after callbacks/member/scratch stores, unlike current
entry snapshot. Also readiness is signed 32-bit compare of zero-extended FIFO
count versus signed tx_bytes_per_block, not current unsigned compare. Two
independent read/operand axes, four full-TU cells. Signed compare preserves
original negative-byte-count semantics explicitly; do not excuse by valid
ranges. All other loop/stores/predicates fixed.

faxvmi_hdlc_frame (928B) reloads parent framer after each unknown debug or
name-lookup call, unlike cached local owner. One consistent framer reload
control plus baseline, scalar bitpacking snapshots unchanged. No parameter,
field-type, header, spill, register or arbitrary loop factoring controls.
Audit all TU bodies, metadata, data/nontext, relocations and bystanders.
No test/fuzz/mutation execution; root final period/integrated census.
