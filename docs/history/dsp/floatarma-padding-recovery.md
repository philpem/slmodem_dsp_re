# FloatARMA outer padding guards

At b550e9c6 constructors596B versus612B blob. Denominator padding has
two opposite-order pretests at0x474a2/0x474a4 and0x474a6/0x474a8, numerator
at0x474f5/0x474f7 and0x474f9/0x474fb, before member-index initialization.
Current bare loops have one pretest each. F873/F874 recover constructor/
member-index semantics, F7829 emission order; none closes outer guards.

[Four predeclared complete-TU cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947399002):

| Cell | C1/C2 complete body |
| --- | --- |
| baseline |596B/SIZE16|
| denominator guard |612B/BYTES171|
| numerator guard |596B/SIZE16|
| both guards |612B/EXACT|

Four sources/four complete emissions. Only both-guard cell recovers both
clones; no size-only adoption. Baseline raw-reproduces production. All7
functions/global bindings/nontext data preserved; only constructor clones
canonical bodies/relocations change. Three/seven ->five/seven exact, no
losses. Same allocations/copy/normalization/member-index/reset, no register,
statement-order or profile permutations. Source outer guards independently
explain duplicate branches before index assignment; a compiler interaction
means isolated numerator guard does not change size. F11582.

Retain both outer guards. Reset afterward leaves the member cursor at
m_ylen, so skipped-loop member initialization is not the final constructor
state. Existing t_floatarma alternates both clones, compares all buffers/
state, rounded lengths, padded coefficients, four allocations per side/exact
bytes, normalization and destructor/reset/process. Static denominator-pad
anchor retargeted to guarded block, same leave-padding-unwritten fault.
No fuzzing or mutation execution.

Full configured CXX flags and mandatory bug define, Gentoo3.4.2-r2/selected
executed assembler2.15.92.0.2 recorded. Replay tools/playbook_floatarma_padding.py
--domain URL above. Actual commands/hash/inventories/changed disassembly/RTL
in build/playbook-floatarma-padding; retained adoption artifacts in
build/playbook-floatarma-padding-adoption.

Retained300/300 build changes only src_dsp_FloatARMA.cpp.o, raw-matching
winning full-TU cell. Whole-tree876/1852 ->878/1852 exact,85,615 ->86,839
exact bytes; only constructor clones gain/no losses. Complete same-order
300-object partial links remain DIFFERENT (strict exit1): positioned68,568
->68,284/943,398 bytes; allocated914,126 ->914,158; exact section70/92
and symbol394/2907 unchanged, relocation1,025 ->1,020/18,317. Local body
recovery moves later layout and does not establish original global profile.
Negative positional movement is recorded, not optimized away with padding.

Fixed Gentoo phase385 passed/0 failed, with full retained profile.
Structural14,238 references/2,711 findings headings and285 suites/10,038
static anchors clean. No modern portability, fuzzing or mutation-runtime claim.
