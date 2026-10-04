# VOICE_process outgoing scale expression

Original output conversion loop e9d..ebd loads the float scale once, duplicates that cached value, and FMULS the outgoing sample directly from memory. Baseline instead FLDS the sample and multiplies by the cached scale register. Predeclare the original scale-first spelling `VCE_LINE_OUT_SCALE * v->to_line[i]`, without reassociation, type changes or intermediate stores. Cross only with the already measured combined early output-frame/block ownership witness, yielding four complete-TU cells. Input double scaling remains fixed because its original operations already have the baseline pattern. No operand-order enumeration elsewhere or register assignments. Preserve truncating short conversion, loop bounds, callback ordering and exact values; NaN payload precedence remains an explicit semantic limit for a nonexact candidate. Full TU audit, no adoption absent strict targeted exactness.

## Measured outcome

- baseline: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- output-scale-first: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- early-owners-control: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 77]}.
- scale-first-early-owners: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 77]}.

No exact gain or loss. Five exact bystanders stay exact; no source adopted. Full TU metadata and named objects assert unchanged. Complete canonical nontext before/after ledger and diagnostic-string multiplicity controls are retained in `build/batch100-voice-complete-audit.json`; anonymous jump-table relocation offsets can move as nonexact bodies change. Those changes are recorded, not silently waived. Compiler commands, assembler identity, raw baseline, source/header hashes and RTL dumps are in `build/gcc3-batch100-voice-process-scale-operand`.
