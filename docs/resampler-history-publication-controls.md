# Closed Resampler history-publication controls

At b550e9c6 adopting constructor blob174B/current172B. Blob initializes
a private null pointer at0x34c53, optionally allocates then publishes owner+8
at0x34c59. Current publishes null before allocator and then reloads owner.
This motivates the publication family independently of score.

[Three predeclared complete-TU cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947310886): baseline, null local/conditional allocation/one owner store, direct conditional-expression owner assignment. Baseline172B/SIZE2 raw-reproduces production. Both late forms raw-agree:174B/BYTES44, neither exact. Three sources/two complete emissions;16/25 exact unchanged, no gains/losses. Only adopting C1/C2 canonical bodies change;26 global bindings and all25 functions preserved. Designing constructor remains unchanged.

Late publication recovers the pointer materialization/store/reuse boundary,
but source initialization/compare scheduling still differs. Correct byte
length does not establish recovery. No src adoption. No reset-helper, store
order, register or profile permutations justified by this finite result.
Further work requires another discriminating source boundary.

Full configured CXX flags + mandatory bug define, Gentoo3.4.2-r2 and selected
executed assembler2.15.92.0.2 identities/commands/hash/inventories/changed-body
disassembly/RTL retained in build/playbook-resampler-history. Replay
tools/playbook_resampler_history.py --domain URL above. Runtime, whole-tree
census and partial-link gates NOT RUN: candidate not adopted. No fuzzing or
mutation execution. F11580.
