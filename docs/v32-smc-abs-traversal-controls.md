# Absolute V32 encoder: traversal recovery leaves type boundaries open

Baseline5ec53f63. A read-only audit and independent parent inspection establish
blob SMCv32_encoder_abs156B traverses input with an advancing pointer and an
unsigned-short countdown. Its decrement/low-word extension/incw sentinel are
not the retained ascending unsigned-int indexed loop.

Four complete-TU source cells cross unsigned-short count-- !=0 with an input
cursor. Original count is unused after this loop; no fabricated counter copy
is added. Results: baseline152B/SIZE4, countdown154B/SIZE2, cursor149B/SIZE7,
both154B/SIZE2. All4 compile, all emissions distinct; complete raw baseline
reproduces. All3 functions/5 globals retain their inventories/bindings,0/3
exact unchanged, only abs changes. No production adoption.

Both recovers pointer advancement and the word countdown sentinel, but retains
ring-wrap branch/cwtl/movswl rather than blob setl/neg/and. Its tag reads the
signed-short mode field versus blob signed-byte load. These are distinct
unresolved operand/conversion boundaries, not evidence to fit declarations
or register names. F8249 already bounds ring-local widening on TxNoCarrierV32;
it must be read before proposing related changes, and does not itself test this
encoder's traversal. The traversal cross is closed; further traversal spellings
need a new discriminator. Parent requested a separate read-only helper/type
boundary audit; no such source changes have been compiled or retained here.

[Domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945774791).
Replay tools/playbook_v32_smc_abs.py --domain URL. Artifacts in
build/playbook-v32-smc-abs include full Gentoo GCC3.4.2-r2 and selected
assembler2.15.92.0.2 identity, saved .build-config commands with mandatory
DSPLIB_REPRODUCE_BUGS, source/header/object hashes, inventories and changed
canonical bodies/disassembly. Existing fixed t_v32smc covers positive ragged
chunks/modes/ring sizes; zero/max count are not explicit fixture coverage.
Candidate runtime and partial-link gates NOT RUN because no source adopted.
No fuzzing or mutation execution; no modem-lifecycle reachability claim.
