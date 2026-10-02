# TONE_read short phase and reflected-index storage

At599f0fa4 TONE_read blob121B/current113B. Word quadrant range comparisons
at0x7e251,0x7e276,0x7e296 and cwtl on reflected indices at0x7e25e,0x7e283,
0x7e2a3 distinguish it from current promoted arithmetic. Reference last range
is bounded0x601..0x7ff; baseline has open-ended signed p>=0x601.

[Eight predeclared width/range cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947558932) cross int/short masked phase, promoted/short reflected index, open/bounded last range. None exact. Combined short widths/bound yields121B/BYTES11; first/third conversions and all comparisons match, but compiler still folds inline short(p-0x400) into table relocation.

[Four predeclared storage controls](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947594274) include production baseline, staged inline cell, assignment of reflection back into short phase, separate short index local. Both storage forms raw-agree and produce121B/EXACT. Staged cell raw-replays previous full object; production controls raw-reproduce production. Twelve valid compiles/ten distinct sources/nine complete emissions.

Retain short phase and reflected assignments back into it, with explicit
quadrant bounds. No table/constants/branch ordering/profile or register names
change. All6 defined functions (4 blob-shared)/6 global bindings/types/visibility/nontext preserved;
only TONE_read canonical body/relocations change. One/four ->two/four exact,
no losses. Same full-TU result with two ordinary storage forms bounds an
assignment conversion boundary, not uniquely original spelling. F11584.

Existing fixed t_dualtone sweeps all65536 short inputs, with negative/nonzero
counts and cardinal/wrap controls; covers full input domain, not randomized
search. No fixture/mutation-anchor changes, no fuzzing or mutation execution.
Full configured retained C flags and mandatory DSPLIB_REPRODUCE_BUGS, Gentoo
3.4.2-r2/selected executed assembler2.15.92.0.2. Actual commands, hashes,
inventories/symbol records/data and changed disassembly/RTL retained in
build/playbook-tone-read and build/playbook-tone-read-storage. Adoption
artifacts build/playbook-tone-read-adoption. Replay corresponding tools/
playbook_tone_read.py and tools/playbook_tone_read_storage.py --domain URLs.

Retained300/300 comparison build, only src_dsp_FP_math.c.o changes and
raw-reproduces winner. Whole-tree878/1852 ->879/1852 exact,86,839 ->86,960
exact bytes, only TONE_read gain/no losses. Complete same-order300-object
partial links remain DIFFERENT (strict exit1): positioned68,284 ->68,283
/943,398; allocated914,158 unchanged; exact section70/92, symbol394/2907,
relocation1,020/18,317 unchanged. Per-function exactness does not establish
complete object/profile identity. The two current exported coefficient
helpers absent from the blob retain their complete bodies and bindings.

Fixed Gentoo phase385 passed/0 failed, including exhaustive65536-phase
fixture and negative/nonzero/cardinal/wrap controls. Structural14,238
references/2,713 finding headings and285 suites/10,038 static anchors clean.
No modern portability claim, fuzzing or mutation execution.
