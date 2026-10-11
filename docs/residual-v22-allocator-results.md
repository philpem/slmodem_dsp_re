# V.22 bystander: alias-number correction and scratch history

This corrects F11903's interpretation of the literal dump differences. The
unchanged Detect_1s does **not** diverge at global allocation/reload. Twelve
25.greg patterns differ only in printed memory alias-set IDs: thresh11→10,
n12→11, ms13→12 and temporary14→13. The correspondence is bijective; other
observed IDs retain their identity. Registers, addresses, offsets, field names,
sizes, alignments and all alias relationships agree. After checking that
correspondence, patterns agree through27.flow2. Never erase alias information
globally: a split, merge or change to universal class0 must be refused.

Read-only installed Gentoo cc1 observation independently confirms three raw
complete-TU controls, nine boundary snapshots and45 per-instruction reload
choices. Baseline, combined ACK control and independent baseline repeat have
identical pseudo assignments after local allocation, at reload entry and after
reload; every recorded reload input, class/mode, selected register, spill order
and round-robin cursor agrees. Three optional stack-state names are unavailable
in this compiler's DWARF and support no conclusion. No inferior call/write.

The actual instruction differences begin at peephole2:

| Scratch site | Baseline cursor → register → cursor | Combined cursor → register → cursor |
|---|---|---|
| Threshold multiply, UID94 |50 → EDX →2|13 → ESI →14|
| Zero verdict store, UID33 |2 → ECX →3|14 → EAX →49|

Their generated patterns are UIDs198/199 and201/202. Register renaming expands
the four differing patterns to seven. Complete scratch traces agree through the
three preceding Detect_Retrain searches. Baseline ACK has three searches, the
combined ACK one; their different exits are inherited by Detect_1s. This is the
previously established persistent peephole cursor mechanism, not a new global
allocator cause or evidence of original compiler state. Three raw scratch
controls validate46 searches/283 candidate visits; independent baseline repeat
is identical. No source compensation, flag change or gain is adopted.

The alias comparison fires six controls: pure identity renumbering passes,
offset/register changes remain visible, and alias merge/split/universal-class
changes fail. Six stages report their raw and checked counts. Existing literal
counts remain useful measurements but do not locate a code-generation change.

Replay after generating the saved ACK controls:

```
python3 tools/gcc3_alias_stage_audit.py
python3 tools/residual_v22_allocator_observe.py
python3 tools/residual_v22_scratch_reproduce.py
python3 tools/residual_v22_allocator_audit.py
```

Build artifacts record commands, compiler/preprocessed/object/observer hashes,
header hashes, complete flags and all events. Gentoo cc1 hash is checked against
the shared pin; image assembler is executed. Saved/plain/traced objects agree
raw for all three boundary controls and all three scratch controls.

Two preliminary runs rejected raw assembly-comment equality: first explicit
declaration heap pointers, then string_cst heap pointers. They are preserved
under build/residual-v22-allocator-invalid-* and excluded. The final check
normalizes only explicit tree annotation addresses; symbols/real constants and
every instruction remain. A preliminary per-instruction observer used an
incorrect RTL union field; its failed log/JSON are preserved and excluded.
Installed DWARF confirms CONST_INT uses u.hwint; final controls rerun completely.

Production remains1079/1852 EXACT with source/headers unchanged. New tools
compile, positive/refusal audits, reference and diff checks pass. Previous
production period gate388/0 remains the source validation; no candidate runtime,
fuzzing, mutation execution or portability claim.
