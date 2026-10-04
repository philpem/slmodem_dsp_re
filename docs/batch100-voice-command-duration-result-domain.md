# VOICE_command DTMF result construction

Pinned 856c1ecb. Original DTMF arm at 0xa80..a9d reads the symbol, seeds the outgoing duration result to one, divides unsigned duration by ten, then replaces the one only when the quotient is nonzero. Current source mutates the quotient to one on a zero result. The diagnostic remains before all outgoing reads and prints the unfloored quotient. The two integer results are mathematically identical, but the conditional result lifetime differs.

Predeclare baseline versus named quotient followed by default result one and nonzero replacement, only inside the DTMF arm. Preserve unsigned division, zero floor, all info-owner captures, diagnostics, mode/argument construction and shared voice-command tail. No operand permutations, snapshots across the diagnostic, or source padding. Full TU control and all bystander/metadata/data/nontext audits. Close this finite result-boundary family if nonexact.

## Measured outcome

- baseline: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- default-result-first: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 16], 'VOICE_process': ['SIZE', 84]}.

No exact gain or loss. Five exact bystanders stay exact; no source adopted. Full TU metadata and named objects assert unchanged. Complete canonical nontext before/after ledger and diagnostic-string multiplicity controls are retained in `build/batch100-voice-complete-audit.json`; anonymous jump-table relocation offsets can move as nonexact bodies change. Those changes are recorded, not silently waived. Compiler commands, assembler identity, raw baseline, source/header hashes and RTL dumps are in `build/gcc3-batch100-voice-command-duration-result`.
