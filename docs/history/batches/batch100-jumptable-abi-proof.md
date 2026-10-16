# Bounded push-save ABI jump-table comparison

The previous jump-table proof refuses RxNextStateV29's ordinary i386 frame:
EBX is pushed, eight outgoing bytes are allocated, and seven relocated direct
calls occur in its five-way switch. Refusal does not establish different table
destinations. The extension proves that bounded frame before canonicalizing the
ordered destinations. It changes no non-relocated byte comparison.

The entry contract is the unique sized function symbol. Direct calls obey the
ordinary i386 callee-save and stack-return ABI. Entry saves are distinct EBX,
ESI, EDI or EBP, followed by one aligned allocation of at most256bytes.
Argument loads cannot read save slots or the return address. Fullword outgoing
stores cannot overlap them; frame addresses and indexed/partial stack accesses
are refused. Every reachable return releases the frame, pops saves in reverse
order, and preserves unsaved callee registers. Calls must be direct E8/PC32
relocations with addend-4 to named FUNC/NOTYPE symbols. Indirect callbacks,
unrelocated calls and external tail jumps remain refused. Additional data
relocations are restricted to decoded C7 /0 immediate MOV values and 83 /7
absolute-memory CMP address fields. The immediate may populate an outgoing
argument; its ESP displacement may not relocate. Frame sizes, stack offsets,
guard bounds and opcode bytes remain literal and cannot relocate.
Existing table extent, ownership, instruction-boundary, guard and incoming-edge
checks remain mandatory. GNU as self-LEA alignment no-ops preserve their value.

Run `python3 tools/toolchain/jumptable_batch100_abi_controls.py`:35 synthetic
static ELF frame controls,3 accepts and32 refusals; a rebased equal table gives
EXACT, a reordered table gives RELOC, and all32 invalid identical-object pairs
remain UNRESOLVED. The original supplies a separate positive: five ordered
function-relative targets106,151,207,269,321 and seven direct ABI calls.
No generated program is executed. Fixture padding is a separately named
function: unnamed leading padding makes GNU objdump --disassemble print bytes
before the requested symbol and truncate its tail, so that fixture was invalid
for byteident integration and is excluded. No production comparator extraction
rule is changed here.

Existing jumptable_controls, jumptable_batch50_stack_controls and
jumptable_integration_controls also pass. Archive the previous prover and run
old/new complete censuses on the same immutable300 master objects in fresh
processes. At856c1ecb the entire census JSON is identical:1020/1852 exact,
108349 exact bytes; no apparatus-only gain. The source cwd for this measurement
is the preceding batch worktree with an empty src/include diff against856c1ecb.
A measurement attempted from edited source correctly failed the staleness guard
and is excluded. Records:build/batch100-prover-baseline-audit.json and
build/jumptable-batch100-abi-controls/results.json.

Independent review found that initial non-control relocation containment was too broad. Six added refusal controls cover relocated frame allocation/release, guard bound, argument/stack-store offsets and opcode bytes. The whitelist rejects all six while preserving actual outgoing address immediates and diagnostic absolute-memory comparisons. F11788 records the corrected proof and revalidation.
