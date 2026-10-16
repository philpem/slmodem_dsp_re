# VOICE_create immediate converter publication

Follow-up to the separately measured late owner read: original each RcFixed_Create return is immediately stored into v->rc_in/out, and only memory-owned handles are read after the second call. Retaining result locals through a later failure test adds source lifetimes not present in that observed ownership sequence. Predeclare baseline, late-owner-read control, direct converter member assignments with initialized null members and a member failure test. Remove only the two rc locals and use their original receiving members at each same assignment/call/guard position. Do not change modes, selector order, teardown or block arithmetic. This is a bounded publication/ownership spelling, not a declaration/register permutation. Full TU bystanders/metadata/named data/canonical nontext audited, no flags or extra stores.

## Measured outcome

- baseline: {'VOICE_create': ['SIZE', 12], 'VOICE_command': ['SIZE', 18], 'VOICE_process': ['SIZE', 84]}.
- late-owner-read-control: {'VOICE_create': ['BYTES', 71], 'VOICE_command': ['SIZE', 10], 'VOICE_process': ['SIZE', 84]}.
- immediate-member-converters: {'VOICE_create': ['SIZE', 28], 'VOICE_command': ['SIZE', 6], 'VOICE_process': ['SIZE', 84]}.

No exact gain or loss. Five exact bystanders stay exact; no source adopted. Full TU metadata and named objects assert unchanged. Complete canonical nontext before/after ledger and diagnostic-string multiplicity controls are retained in `build/batch100-voice-complete-audit.json`; anonymous jump-table relocation offsets can move as nonexact bodies change. Those changes are recorded, not silently waived. Compiler commands, assembler identity, raw baseline, source/header hashes and RTL dumps are in `build/gcc3-batch100-voice-create-direct-converters`.

The pre-initialized direct-member cell adds redundant null member writes before conditional calls; it misses and is excluded as a reconstruction of the observed call/publication boundary. The separate branch-member domain supplies the discriminating original-store control. No field initializer is adopted.
