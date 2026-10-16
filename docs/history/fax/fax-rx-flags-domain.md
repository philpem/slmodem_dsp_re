# Receive-request flag capture

Four full-TU cells at240481e6. V21RX_control originala242d captures request
byte at offset0x0d before writing the separately addressed hdx int at a2437 and reuses
that byte for reinit at a2439. Retained source rereads the request after that
state write, which differs if owners overlap. The V29 snapshot family is
closed separately; V21 has not received this control. Candidate captures the
request byte after cfg.int_0008 write, before hdx write, with the two
observed masks and all other stores/calls fixed. Full unchanged baseline and
all body/binding/data/BSS/relocation checks. Close after this control; no
register/declaration alternatives or source adoption unless complete exact.

Invalid first run shifted already-byte flags/masks by8, eliminating both
tests; retained in build/fax-rx-flags-invalid-shift and excluded. Corrected
run uses actual byte member and its existing masks without shifts.

Before expansion, the captured-byte control still emits SHR/AND. Original
TEST/SETNE independently supports a default-zero result set to1 in a guarded
arm, the existing Playbook named-result lever. Cross that source-control-flow
boundary with byte capture; no memory-zero write added.
