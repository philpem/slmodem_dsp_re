# VOICE_create converter ownership after allocation

Pinned 856c1ecb. Original VOICE_create stores input converter into v+0x18 at 0x8cc, calls RcFixed_Create for the output converter at 0x8e7, then reloads input converter from v+0x18 at 0x8ee before its failure test. The baseline source instead tests the earlier rc_in local after the second call. v has escaped through memset and the allocation calls are external, so a saved return pointer and the memory-owned handle are different load lifetimes. The late read is a direct original witness, independent of root's lower-layer voice receive and DLE fixes.

Predeclare baseline and replace only the converter-pair failure test with v->rc_in/v->rc_out. All converter modes, assignments, construction order, cleanup, block arithmetic, commands and allocation guards remain unchanged. No declaration permutations or forced spills. Full TU unchanged baseline reproduction and metadata/named data/nontext relocations plus every bystander must be checked. This recovers original ownership after a call; if nonexact do not adopt merely for size.

## Measured outcome

- baseline: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- memory-owned-converter-pair: {'VOICE_create': ['BYTES', 71], 'VOICE_command': ['SIZE', 10], 'VOICE_process': ['SIZE', 84]}.

No exact gain or loss. Five exact bystanders stay exact; no source adopted. Full TU metadata and named objects assert unchanged. Complete canonical nontext before/after ledger and diagnostic-string multiplicity controls are retained in `build/batch100-voice-complete-audit.json`; anonymous jump-table relocation offsets can move as nonexact bodies change. Those changes are recorded, not silently waived. Compiler commands, assembler identity, raw baseline, source/header hashes and RTL dumps are in `build/gcc3-batch100-voice-create-converter-owner`.
