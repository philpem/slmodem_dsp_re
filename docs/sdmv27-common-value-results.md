# SDMv27 common branch value: one exact gain

The [two-cell domain](sdmv27-common-value-domain.md) follows the installed
zero-search/no-renaming witness for this initializer. Its two cfg loads feed
one original nbits store at 0x9a83f. The old source writes that destination in
each branch; GCC's 24lreg retains separate HI result pseudos 60/61, each locally
assigned EAX. This reserves EAX against the owner pointer during global allocation.

The single conditional assignment keeps one HI result pseudo 60 with two
branch definitions and one use. It cannot take the separate local allocations;
the later allocation yields owner EAX/result EDX, reproducing every instruction,
byte position and named relocation in the original 89-byte SDMv27_init. The
original null-config defect remains: the later cfg->nbits read is unguarded.
No register constraints, volatile values, changed widths or flags are used.

Full-TU control: raw baseline reproduced, all 3 function sizes/positions and
bindings agree, all allocated nontext data, BSS and canonical nontext relocations
agree. Scrambler and descrambler bodies are unchanged. Candidate verdicts:
initializer EXACT, scrambler SIZE 3, descrambler SIZE 52. Six total body verdicts.
One exact gain, zero losses. This is one supported source family, not proof that
the original author used this uniquely determined syntax.

```
python3 tools/sdmv27_common_value_reproduce.py --domain docs/sdmv27-common-value-domain.md
python3 tools/sdmv27_common_value_audit.py
```

Artifacts: build/sdmv27-common-value includes actual complete Gentoo commands,
selected assembler identity, mandatory bug define, header/source hashes, full
objects/disassembly, all RTL stages and audit.json. Source adoption is gated
on the default deciding period/structural suite; final results follow below.
No fuzzing or mutation execution. Finding F11877.

The candidate’s 27flow2, 28peephole2 and 30rnreg pattern streams agree:
its exact registers are already determined before scratch selection/renaming.
Baseline25greg allocates two remaining pseudos (58/59) against EAX already
occupied by local HI results; candidate25greg allocates three (58/60/59),
without that fixed EAX conflict. This is the measured allocation mechanism.

Final production V27_SDM object raw-identical to the audited candidate.
`make phase J=8`: 388 passed, 0 failed; all structural checks green.
`make tc J=8`: 300 sources, 300 objects, 0 failures. Static gates check
14,386 references, 2,974 finding headings and 10,038 anchors across 285 suites,
with no failures.

Final strict census: 1,075/1,852 exact, 118,176 original bytes; gain
SDMv27_init (+89 bytes), zero losses. REGALLOC decreases32→31. The historical810-name floor remains unchanged;
its known V90Parameters C2 loss is not cleared by this gain.
