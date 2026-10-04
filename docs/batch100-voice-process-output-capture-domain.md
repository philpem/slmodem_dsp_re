# VOICE_process output-frame and block snapshots

Pinned 856c1ecb. Original saves output-ring block address before input resampling: d48 reads out_ring.blk, d53 forms its frame pointer, d5b stores it at stack40, then d7f calls the input RcFixed_Resample. The saved pointer is the second resampler output at ed9/eed after host/core/callback calls. Baseline instead reloads out_ring.blk immediately before the second resampler. Independently original saves block length at d26..d2e, feeds first resampling, and uses that saved value for outlen at ed1..ee5; baseline reads v->block late at 0x89f. These are actual memory ownership ages across calls, not subobject address register spellings.

Predeclare four full-TU cells: baseline, output frame capture before input resampling, outlen snapshot before input resampling, their cross. Existing unsigned ring/min bounds and all copy/float conversion/voice/host calls, statuses, counts, retirement and toggles stay fixed. Output pointer and outlen are existing second-call arguments; no artificial spills, volatile, added writes or flags. Both captured values must follow original observed early reads, even if callbacks mutate owner fields. This is object-backed reconstruction, not a global reordering matrix. Full TU bodies, metadata/named data/canonical nontext audit and raw unchanged baseline required.

## Measured outcome

- baseline: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- early-output-frame: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 83]}.
- early-output-length: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 100]}.
- early-frame-and-length: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 77]}.

No exact gain or loss. Five exact bystanders stay exact; no source adopted. Full TU metadata and named objects assert unchanged. Complete canonical nontext before/after ledger and diagnostic-string multiplicity controls are retained in `build/batch100-voice-complete-audit.json`; anonymous jump-table relocation offsets can move as nonexact bodies change. Those changes are recorded, not silently waived. Compiler commands, assembler identity, raw baseline, source/header hashes and RTL dumps are in `build/gcc3-batch100-voice-process-output-capture`.

Original snapshots are real call-age witnesses; any observable callback-mutated-owner difference needs a separately reachable lifecycle fixture before a nonexact source adoption. This byte batch does not invent callbacks or execute fuzzing.
