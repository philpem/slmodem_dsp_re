# Four current register-only targets

The [declared domain](gentoo-scratch-target-domain.md) compiled four unchanged
complete TUs at ca84f1cc; all raw-reproduced production. All 24 shared body grades
are retained. Five observed/raw/saved object triples agree, including one
independent V27_SDM repeat: 80 searches,878 candidates, all accepted replacements.
The prior six-input positive/refusal audit also passes unchanged.

| Target | Searches | Observation |
| --- | ---: | --- |
| _iir_filter_create | 0 | Direct persistent scratch selection does not account for its colors; later renaming changes6 UIDs |
| FloatFIR::reset | 1 | UID 58 memory subtraction split selects ECX at cursor 2, leaves 3; selected register survives renaming |
| dp_v22_init | 3 | Symbol stores select ECX/EAX/EAX; renaming produces EDX/EAX/ECX; original has ECX/EDX/EAX |
| SDMv27_init | 0 | Patterns agree through27flow2,28peephole2 and30rnreg; register colors are already present before scratch selection |

FloatFIR's reference uses EDX for the taps load/subtraction. Its preceding block
processor makes one search, entering 0 and selecting EDX, leaving 2; reset then
selects ECX. This identifies current causality, not the reference's unobserved
cursor or an original source preimage. Zero searches exclude direct search in
the target, not all compiler history. No source/profile edit or strict gain
belongs to this tracing domain. Two targets make searches, two do not.

The manifest-driven replay is reusable for saved full-TU controls. Targets have
one RTL body each. Other C++ clone declaration names can repeat, so events are
not silently assigned to their emitted C1/C2 symbols; complete-body grading
still covers every symbol. Clarification to the domain's reporting requirement:
all 24 bodies are listed, but only the four non-cloned targets receive an exact
per-symbol scratch count. A missing target fails the audit.

```
python3 tools/gentoo_scratch_targets_reproduce.py --domain docs/gentoo-scratch-target-domain.md
python3 tools/gentoo_peep2_search_reproduce.py --manifest build/gentoo-scratch-target-manifest.json --output build/gentoo-scratch-target-traces
python3 tools/gentoo_scratch_targets_audit.py
```

Artifacts: build/gentoo-scratch-target-inputs andbuild/gentoo-scratch-target-traces,
including actual compiler/assembler commands, pinned hashes, stage streams,
full object equality and audit.json. Manifest entries use label, directory,
compiler, input; relative directories resolve beneath this worktree. No compiler
state writes, fuzzing or mutation execution. Findings F11875–F11876.
