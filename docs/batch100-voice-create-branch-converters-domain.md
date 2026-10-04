# VOICE_create branch-local converter publication

Original return/publication boundary stays fixed: call first, one receiving member store next, regardless of chosen rate. The previous direct-member experiment initialized a member before conditional calls and retained redundant zero stores, so its SIZE28 miss does not test that original publication boundary. Preserve it as a negative control; do not promote it.

Predeclare baseline, previously measured late-owner-read control, and one branch-local direct-member form: assign allocator result directly in each supported branch, zero only in the unsupported else arm. Repeat for the output member, then test the memory-owned pair. Remove the two result locals and trailing stores. No call, mode, cleanup, arithmetic, branch selector or order changes. Each member has exactly one publication after its chosen allocation on supported paths, as in the original. Full TU baseline and metadata/data/nontext/body audit mandatory; no permutations or flag fitting.

## Measured outcome

- baseline: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- late-owner-read-control: {'VOICE_create': ['BYTES', 71], 'VOICE_command': ['SIZE', 10], 'VOICE_process': ['SIZE', 84]}.
- branch-member-converters: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 6], 'VOICE_process': ['SIZE', 84]}.

No exact gain or loss. Five exact bystanders stay exact; no source adopted. Full TU metadata and named objects assert unchanged. Complete canonical nontext before/after ledger and diagnostic-string multiplicity controls are retained in `build/batch100-voice-complete-audit.json`; anonymous jump-table relocation offsets can move as nonexact bodies change. Those changes are recorded, not silently waived. Compiler commands, assembler identity, raw baseline, source/header hashes and RTL dumps are in `build/gcc3-batch100-voice-create-branch-converters`.
