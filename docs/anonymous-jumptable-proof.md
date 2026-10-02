# Bounded anonymous jump-table proof

F11657 recovered MakeTxData’s body and five table destinations, but the strict
comparator cannot identify its anonymous .rodata target. The [next declared
investigation](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5956557544)
is a general destination proof with actual ELF controls. This first stage is
separate analysis; byteident.py and the strict census are unchanged.

    python3 tools/toolchain/jumptable_controls.py
    python3 tools/toolchain/jumptable.py OBJECT FUNCTION

The implementation uses pyelftools and objdump. It supports little-endian
ELF32/i386 ET_REL objects with one uniquely owned, sized function and a single
absolute memory-indirect dispatch. CMP immediate / unsigned JA / JMP must be
adjacent, with the same 32-bit index and scale four. The unsigned bound admits
at most 256 entries. The exact table interval must fit an allocated,
non-writable, non-mergeable PROGBITS section with unambiguous section identity.
Each slot needs exactly one R_386_32 relocation to an instruction boundary
inside the same uniquely owned function. Named-object overlaps, aliases,
missing/overlapping entries and additional function relocations refuse proof.

The identity retains entry count and ordered (function, offset) destinations,
including repeated entries. It does not fingerprint raw table bytes or erase
addends. Code and table rebasing preserve identity; changing or permuting a
valid destination produces a different proved identity.

The guard proof assumes ordinary ABI entry at the unique function symbol.
It rejects known direct and table edges, named entries and absolute/PC-relative
relocations into the guard/dispatch instructions, including instruction
interiors. Direct section edges include LOOP-family instructions. Additional
indirect control transfers, far transfers, unsupported control-prefix forms,
explicit stack writes and unusual returns are refused. This is deliberately
a narrow supported domain, not a proof about arbitrary external jumps,
self-modifying code, corrupted return addresses or signal intervention.
Unsupported forms stay unproved and cannot justify automatic exact grading.

Review exposed missing LOOP-family handling, far transfers, prefix-token
parsing and entries into CMP interiors. Those gaps were corrected and each
has a real assembled negative control. Initial named assembler labels also
made the positive fixture correctly refuse its exposed dispatch entry; the
fixture now uses discarded local labels. No failed setup is counted as a
valid positive run.

The executed control denominator is 44 synthetic ELF objects: seven accepted
objects establish three equal pairs and two distinct identities; 37 objects
are refused. Controls cover changed/permuted destinations, repeated slots,
wrong bounds/index/scale/guard, guard bypass, count clobber, instruction
interiors, owner ends, cross-function destinations, sized and zero-sized
aliases, writable/truncated/misaligned tables, missing/duplicate/overlapping/
wrong-kind entry relocations, duplicate section names, out-of-section targets,
extra indirect transfers, far and prefixed transfers, external direct and
relocated entries, return-address stores and stack-pointer writes. Every
refusal prints its reason. These ELF fixtures are measurement apparatus,
assembled with the recorded host assembler; they are not reconstructed source
or a fuzz/mutation harness.

Two real objects independently prove one equal table identity: reference
MakeTxData and the period production object, with destinations
65,103,131,163,28. Their table bases differ (34348 versus zero), while guard
and dispatch relocation offsets are identical (16 and 24). Strict grade
remains UNRESOLVED. Results and all assembly/ELF fixtures live under
build/jumptable-controls.

The next step is to use this single shared proof in canonical relocation
resolution, while retaining every existing unresolved-section safeguard.
Run integration controls through the actual body/verdict path, preserve
unsupported fixtures as unproved, and compare the complete 1852-function
census before claiming any recovered exact function. Whole-object metadata
and unused nontext remain separate controls. Source and all 300 production
objects are unchanged in this detector stage; the previous fixed period gate
is 386/0 and the authoritative strict count is 924/1852, 95168 exact bytes.
