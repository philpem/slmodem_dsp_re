# Diagnostic metric vector assessment

Report layout, relocations, control flow, operations, operands and strict
identity separately. Much of the underlying information already exists, but
the tools do not expose a unified function-level vector. Preserve EXACT,
REGALLOC, NONMATCH and unavailable evidence as the existing comparator reports
them. No weighted total, percentage or collection of individually favourable
axes should award a grade.

This assessment inspected the current implementations without changing tools,
grading rules or reconstruction source. It ran six pairs through the existing
canonicalisers to establish which distinctions their diagnostic measures hide.
It did not run a new whole-tree census or claim new function matches.

| Existing tool | Available measurements | Boundary or missing distinction |
| --- | --- | --- |
| `tools/toolchain/partialcmp.py` | ELF ABI; section metadata/order; positioned contents; NOBITS; relocations and relocation-section metadata; symbols/order; groups; raw and semantic exact booleans. Record axes include reference/candidate counts, exact-record intersections and first differences; several also report ordered matches. | A whole partial-object comparison, not a function CFG comparison. Positioned bytes and relocation sites change after preceding layout shifts. REL addends remain in contents, so the relocation-record axis alone does not measure the full destination expression. |
| `tools/toolchain/byteident.py` | Owned function bytes, canonical relocation targets, decoded instruction boundaries, strict grade, byte extents/delta, defining-copy denominator and the first register-renaming rejection. Link-selected identity is separate. | `verdict()` stops at the first applicable category: size, bytes, relocation mismatch or unresolved destination. A first rejection does not inventory all residual causes. There is no complete per-function vector of matched operands or edges. |
| `tools/toolchain/fidelity.py` | Reference/candidate ordinary instruction counts, longest contiguous canonical run and reference coverage; conservative treatment of defining copies and unavailable bodies. | Abstracts GP register identity, local branch targets and section-relative addends. It does not prove register relationships or CFG correspondence. A full reference run can coexist with candidate-only instructions. |
| `tools/toolchain/structsim.py` | Canonical longest run, LCS, LCS/longer denominator, padding counts and limited unmatched rows. | Coarser normalisation also collapses conditional-jump mnemonics, register widths, x87 stack indices and relocation kinds. SequenceMatcher leftovers are one alignment, not a unique explanation. |
| `tools/toolchain/framefree.py` | Narrow REGALLOC certificate with instruction-boundary branch mapping, fixed callee-register permutation, dominating definitions and stack-depth agreement at joins. | Deliberately restricted saves/restores, operations, entries and stack forms. It is useful existing CFG machinery, not a general paired-CFG metric API. |
| `tools/toolchain/callee_effects.py` and `jumptable.py` | Strict limited decoding, instruction/entry checks, bounded successor graphs and relocation/ABI-sensitive effects or jump-table certificates. Callee effects reports path counts, masks and defined return-register bits. | Callee leaf proof is bounded and acyclic; unsupported instructions and bindings refuse. Defined return bits are not an independent source return-type proof. These contracts cannot be extrapolated to arbitrary functions. |
| `tools/callgraph.py` | Interprocedural direct edges and indirect-call counts. | Not an intraprocedural basic-block graph; indirect destinations are not resolved. It supplies neither ABI return types nor whole-program control-flow completeness. |

The six canonicaliser controls used these exact distinctions. “Equal” means
only that the named normaliser erased the distinction.

| Pair | `fidelity.canonical` equal | `structsim.canon` equal |
| --- | --- | --- |
| `je .+8` / `jne .+8` | No | Yes |
| `je .+8` / `je .+12` | Yes | Yes |
| `mov %ax,(%ebx)` / `mov %eax,(%ebx)` | No | Yes |
| `fadd %st(1),%st` / `fadd %st(2),%st` | No | Yes |
| `mov @R_386_32:('symbol', 'x', 0),%eax` / same with `R_386_PC32` | No | Yes |
| `add %eax,%eax` / `add %eax,%ecx` | Yes | Yes |

These are 12 normaliser comparisons, not executable differential tests. They
confirm why either similarity measure can be useful diagnostically without
being an acceptance oracle. Existing `test_fidelity.py` already has explicit
controls for many preserved distinctions and for extra candidate instructions.

The proposed report should have two scopes. Keep the existing whole-object
`partialcmp` vector for section layout, symbol placement, raw bytes and link
records. Add a per-function diagnostic record that links to those placement
records and carries the existing strict grade unchanged. Do not combine
whole-object positional bytes with per-function instructions into one
denominator.

For each function and each defining copy, report the following independent
observations:

* **Extent and placement:** reference/candidate symbol extents, ordinary
  instruction counts, recognised padding rows/bytes, section and start offset.
  Keep declared extents distinct from successfully decoded bytes. Show body
  encoding differences separately from inter-function placement.
* **Relocations:** raw site/type/target/addend records and canonical destination
  identities. Separate site movement from target mismatch, unresolved section
  identity, missing records and unsupported relocation forms. A masked section
  addend is an unknown identity, not a matched destination.
* **CFG:** block/edge counts, edge kind, branch condition, fallthrough, local
  instruction-boundary target and external/call target. Report matched edges
  only under a stated correspondence. Keep indirect successors unknown unless
  an existing certificate supplies their complete domain.
* **Operations and operands:** matched mnemonics, immediate values, memory
  access widths and address components, GP register tokens/widths/byte lanes,
  x87 operands and narrowing operations. Report literal register differences
  separately from an existing successful renaming certificate. A canonical
  token match does not prove value-flow equivalence.
* **Stack and ABI evidence:** literal frame adjustments, save/restore and
  stack-slot observations, plus any applicable existing certificate and its
  assumptions. Leave inferred slot correspondence, return contracts and
  unsupported implicit effects explicitly unknown.
* **Strict identity:** the existing grade and refusal reason, relevant
  certificate identity, and final-object raw/semantic identity at their proper
  scopes. Diagnostic agreement must not override a refusal.

Each axis should state `reference_count`, `candidate_count`, matched,
mismatched, reference-only, candidate-only and unknown counts with its units
and mapping policy. Counts must distinguish unpaired records from paired but
unequal records. Include the first delta with both function-relative byte
offsets, instruction indices, original operands and canonical interpretation.
A first delta is an observation, not an attribution of the source defect.
Unavailable is not zero, and a missing axis must never render as 100%.

Branch displacement and CFG destination need separate fields. A short branch
and a near branch can reach corresponding instructions despite different
encodings and byte distances. Conversely, equal mnemonics and instruction
counts can conceal a changed successor. Function-relative byte targets solve
function-placement differences but do not solve internal instruction-length
changes. Resolve each target to a validated instruction boundary, then compare
under an explicit instruction/block map. Do not discard an intervening padding
target without proving its complete no-op corridor and entry boundaries.

Instruction correspondence is the main new uncertainty. Repeated instruction runs and reordered
independent instructions can admit several mappings. Keep literal row pairing,
heuristic sequence alignment and certified correspondence separate. Label an
alignment ambiguous when uniqueness has not been established; show conditional
counts under the chosen map without presenting them as a proof. Retain both
unmatched sides and the mapping identifier so an apparent improvement can be
reviewed. A general CFG isomorphism or dataflow proof is a separate project.

Pin input object hashes, compiler/flags, decoding version, normalisation
version, symbol identity and defining-copy/link-selected context. Report every
copy: taking the best layout from one COMDAT copy and the best operands from
another would describe a function that does not exist. Preserve existing
conservative aggregation and disclose axis-specific incomplete evidence.

A first implementation should be a read-only report adapter over the existing
parsers and grade output. Start with straight-line and direct-branch functions;
mark unsupported graphs explicitly. Reuse `partialcmp` rather than duplicating
its object census. Before trusting the report, demonstrate: identical inputs;
a pure placement shift; short/near encodings with the same mapped successor;
a changed successor with the same mnemonic stream; a changed relocation
target/addend; a field offset, scale or width change; a partial-register or x87
change; an insertion that shifts all following bytes; and repeated rows with
ambiguous alignment. The acceptance condition is correct diagnosis and
unchanged strict grades, not a higher aggregate score. This assessment proposes
that bounded adapter only; it does not implement it.
