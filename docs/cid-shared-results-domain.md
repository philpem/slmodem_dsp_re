# Caller ID packer: common branch-result stores

Baseline04eee73f, unchanged complete Rxcid.c and production Gentoo flags/bug
reproduction. Two independent hypotheses backed by original common word stores:
mark_bal increment/decrement join0x91bb5 and pack_pos increment/zero join0x91c60.
Unlike closed collector-position/accumulator width controls, these retain every
field/local type and change only branch-result ownership into one conditional
assignment. Preserve mark bit==1 versus position bit!=0 semantics, word casts,
all subsequent member re-reads, and every collector/state transition.

Four full-TU cells: raw baseline, mark only, position only, both. Inspect local/
global allocation and full bodies, including inlined callers. Require all
binding/nontext-data/BSS/relocation controls; report every changed nonexact
bystander and gain/loss. Adopt only a supported complete exact function, not a
near-sized carrier. No register constraints, width/flag/order permutations,
structure-owned files, mutation or fuzzing execution. Distinct branch-result
pseudos/common sink are the source-family evidence, not final register names.
