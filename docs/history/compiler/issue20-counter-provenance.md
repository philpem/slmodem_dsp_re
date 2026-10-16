# Issue #20: recover the fax transmit counter's name

Baseline: `b243a409`, the three-global BSS correction in PR #95.

## Direct evidence

Reference symbol 567 is FILE `cDATAtx.c`; its surviving local symbol 568 is
`cDATAtx_counter`, LOCAL OBJECT, four bytes at `.bss+0x8c0`. The original
diagnostic string independently spells `cDATAtx_counter %d\n`.

The reconstruction instead emitted `DATAtx_counter` from `class1tx.c`. This
change restores the original identifier in its two tentative declarations,
uses and comments. The two declarations still denote one static object.
No strings, statements, declaration positions or linkage are changed.

The original counter's FILE ownership is directly evidenced by the local
symbol stream. That is stronger than associating a GLOBAL symbol with a nearby
FILE record. The functions and counter have not yet been extracted into the
original TU: first enumerate the reference counter's relocation referrers and
audit their helper dependencies, then measure the complete split.

## Measurements

The reviewed Gentoo GCC 3.4.2-r2 partial build and `make phase` pass with bug
reproduction enabled: **375 period tests passed, 0 failed**, structural checks
all OK. The exact-function set remains **828/1,852**, with zero gains/losses
and **79,916/720,125** exact function bytes.

The affected TU's `.text` (13,540 bytes), `.data` (296 bytes), `.bss` size
(8 bytes), `.rodata.str1.1` (964 bytes) and `.rodata.str1.4` (3,724 bytes)
are unchanged. Shared-name binding now agrees for **2,442/2,442** names;
reference-only names fall **77 -> 76**, candidate-only names **132 -> 131**.

Positioned bytes remain **55,398/943,398**, exact relocations **952/18,317**,
exact symbol records **247/2,907**, exact section descriptors **67/92**, and
candidate NOBITS **2,812/2,836** bytes. Correcting a name does not make its
complete record exact while placement and order still differ. The strict
comparator still exits 1 (`DIFFERENT`). Mutation fixtures are unchanged;
the gate reports 264 stale suites, zero never recorded, not a new mutation run.

Local evidence: `/tmp/issue20-datatx-evidence/`.

## Other investigated candidates

- `V92MappingParamsInt.cpp` has no surviving local anchor for the five
  functions currently in `V92ParamsInfo.c`; their interval is not new proof
  of ownership. No rename or language change is retained.
- The reference-only 16-byte `default_voice_configuration` has no identified
  reference relocation. A same-sized `struct voice_config` does not identify
  its defining TU. No speculative global is added.
- The reconstructed encoded buffer's extra terminator byte is intentional.
  Its size and compiler-generated local suffix require a separate
  bug-reproduction/storage investigation, not an opportunistic shrink or
  forced assembler spelling.
