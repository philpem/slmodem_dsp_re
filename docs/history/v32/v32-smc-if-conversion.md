# V32 encoder: the mask needs destination identity

Baseline `6eff516d`, Gentoo GCC 3.4.2-r2 and its selected assembler
2.15.92.0.2. Four predeclared full-TU domains comprise24 valid compilations,
16 distinct sources and15 complete object emissions. All cells retain3
functions/5 global definitions and their bindings, unchanged data sections,
and0/3 exact functions. No production source adoption, no candidate runtime
or partial-link gates. No fuzzing or mutation execution.

## Shifted tag is a separate conversion boundary

The blob reads mode with movsbl in abs/tcm; dif needs the full short for
dispatch/state indexing but follows its shifted tag with cwtl. Keep the shared
mode field short. Cross retained/abs-only/all-three short shifted-tag locals
with retained/previously tested abs countdown+cursor traversal:6 cells, raw
production control reproduces. Short tags recover the corresponding loads/
conversion, but all-three tags grow dif369 ->385 bytes. Abs remains152 or154
versus blob156; tcm stays360 versus374. No exact gains or losses.

[Tag domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945850276).
Replay `tools/playbook_v32_smc_tag.py --domain URL`; artifacts
`build/playbook-v32-smc-tag`. Its traversal/int-tag object also raw-replays the
preceding traversal domain's both cell.

## Read the actual if-conversion predicate

The recovered local `gcc-3.4.2.tar.bz2` member `gcc/ifcvt.c` has SHA256
`d4b24ab571233c4c6b201bd502ddbb1324bfbae759deb235bfca97ac443eb0ca`.
Gentoo1.1, PIE8.7.6.5 and protector3.4.1-1 patch archives contain no patch
header targeting this file; protector mentions it only as Makefile context.
At line1041, `noce_try_store_flag_mask` requires one alternative to be zero
and the other to be the destination itself. It implements conditional clearing
of an existing value, not an arbitrary ternary.

The saved abs ce1 ternary assigns reg91 from reg92[next] or zero. Both arms
are already SImode, but reg91 differs from reg92, failing that necessary
predicate. This is stronger evidence than attributing rejection to short
locals. Replace only the helper's ternary with ordinary source:

```c
if (next >= limit)
    next = 0;
return next;
```

Cross that conditional update with all six tag/traversal cells:12 valid
compilations. Raw production control reproduces, all six retained-helper
objects raw-replay the tag domain. Conditional update converts one block in
each of the three functions and recovers setl/neg/and. All three canonical
bodies change; table relocation targets/addends and data remain unchanged.
This demonstrates the diagnostic firing; no opcode-only adoption.

| Traversal/tag parent | Conditional abs bytes | dif bytes | tcm bytes |
| --- | ---: | ---: | ---: |
| retained/int or abs-short |157|347|367|
| retained/all-short |157|363|367|
| countdown+cursor/int or abs-short |159|347|367|
| countdown+cursor/all-short |159|363|367|

[CFG domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945917548).
Replay `tools/playbook_v32_smc_ifcvt.py --domain URL`; artifacts
`build/playbook-v32-smc-ifcvt`. Full commands derive from saved `.build-config`
through the shared experiment helper, with mandatory DSPLIB_REPRODUCE_BUGS,
identity, source/header/object hashes, complete inventories, changed-body
disassembly and RTL dumps preserved.

## Recover a value boundary without asserting original local types

The conditional short helper leaves an extension after the mask; the blob
does not. Stage four cells on countdown+cursor/all-short/conditional: helper
argument/return/next int, cached widx int, neither, both. Preserve the next
initializer's `(short)(widx + 1)` and signed-short limit. The cached value is
always a sign-extended short or zero, so widening the carrier preserves the
arithmetic boundary; this is a hypothesis, not runtime validation.

| Cell | abs bytes | dif bytes | tcm bytes |
| --- | ---: | ---: | ---: |
| staged baseline |159|363|367|
| helper int |158|362|366|
| caller int |159|363|367|
| both |155|342|363|

Raw staged baseline reproduces. Caller-only produces the same complete object;
the other two controls change all three bodies. Both removes post-mask
extension but changes the comparison to dword cmp/setg. No exact hits.

[Carrier domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945935830).
Replay `tools/playbook_v32_smc_carrier.py --domain URL` after the CFG tool;
artifacts `build/playbook-v32-smc-carrier`. The tool seeds its raw control from
the saved predecessor and checks the seed on replay.

Stage a final two-cell domain on both: raw replay versus explicit
`(short)next >= limit` at the comparison use. The latter restores cmpw/setl,
giving abs156B/BYTES51, dif343B/SIZE14, tcm364B/SIZE10. All three bodies change,
no exact hits; raw staged baseline reproduces. Abs's complete loop region
offset0x40..0x84 is now68 bytes identical, including its canonical table
relocation. Its prologue load order/register choices and final stores still
differ. Equal size and an exact loop are not a whole-function recovery, and
the earlier alpha comparator rejection is operand ordering, not proof of a
pure register-colouring residual.

[Comparison domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945947457).
Replay `tools/playbook_v32_smc_compare.py --domain URL` after the carrier tool;
artifacts `build/playbook-v32-smc-compare`, including audit.json. Data sections
are byte-identical across all24 cells; relocation counts/targets/addends remain
one abs table, two dif table references and three tcm table references, though
their instruction offsets move. Symbol inventories and binding checks are
complete-TU controls, not a claim to reconstruct original TU boundaries.

These finite tag/CFG/carrier/comparison families are closed. The result
establishes a compiler prerequisite and recovers a loop family, not the
author's unique spelling. A reopening needs independently observed lifetime,
owner/load factoring or TU/profile evidence; no declaration/register-order
permutations. Existing production remains868/1852 exact, with the previously
validated fixed Gentoo phase385/0. This diagnostic-only change does not rerun
or claim fresh runtime validation of rejected source cells.
