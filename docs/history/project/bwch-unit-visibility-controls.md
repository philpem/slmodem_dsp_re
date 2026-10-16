# Backward-channel initializer visibility and GCC expansion

F11595. [Predeclared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949145026)
at 3f4d11d4 crosses the retained const table initializer with a tentative
const declaration and definition immediately after Create, against retained
flags and appended `-fno-unit-at-a-time`. No source or flag adoption.

| Cell | Create (blob 364 B) | Progress (blob 710 B) | Delete |
| --- | --- | --- | --- |
| baseline | 348 B, SIZE 16 | 649 B, SIZE 61 | exact |
| deferred | 348 B, SIZE 16 | 649 B, SIZE 61 | exact |
| no-unit | 348 B, SIZE 16 | 649 B, SIZE 61 | exact |
| deferred-no-unit | 364 B, BYTES 8 | 633 B, SIZE 77 | exact |

Four source/option cells, three complete emissions; baseline and deferred
raw-agree, baseline reproduces the retained complete production object.
All three functions and three globals remain defined; exact set stays 1/3,
with no gains or losses. No-unit changes Progress even when its size is
unchanged. Size recovery is not byte recovery: the combined Create fails
register-normalized comparison at row 8, store displacement +4 versus +2.
It is not merely register colouring.

The compiler mechanism is confirmed at initial RTL: only deferred-no-unit
retains a block_size_table reference within Create. Other cells fold entry 26.
GCC 3.4.2 expr.c:6973–7000 substitutes a constant constructor element for a
readonly local variable with an available initializer. opts.c:541–567 enables
unit-at-a-time at optimization level 2 and higher. The
[period GCC documentation](https://gcc.gnu.org/onlinedocs/gcc-3.4.2/gcc/Optimize-Options.html)
explains whole-unit parsing before code generation. The local source subset
lacks frontend/cgraphunit sources, so an exact frontend call stack was not
inspected. This is a mechanism diagnostic, not original-command-line recovery.

Named data audit compares all seven object owners, their values, canonical
relocation targets, sizes, types, bindings and visibility. These agree in all
four cells. Addresses/emission order differ: no-unit swaps Mark/Space and
Alpha/Beta; deferred-no-unit also emits the block table later. Its .rodata
is 144 B instead of 124 B, including a 20-byte gap before the table at +96.
Do not describe this as unchanged allocated data or a complete-object match.

Replay: tools/playbook_bwch_unit_visibility.py --domain <linked URL>.
Audit existing artifacts without compiling: tools/playbook_bwch_unit_audit.py.
Artifacts: build/playbook-bwch-unit-visibility, including complete-object-audit.json,
actual saved full commands, mandatory DSPLIB_REPRODUCE_BUGS, header/source
hashes, initial RTL and complete object inventories. Gentoo GCC 3.4.2-r2 and
executed selected assembler 2.15.92.0.2. Audit reports four cells and seven
owners per cell, and detects the expected initial-RTL table-reference change.

Close this source/option domain. The observation suggests inspecting the
original table's owner/binding/TU boundary and the differing initialization
stores before another experiment; it does not justify a per-file flag exception
or moving more declarations. No candidate differential, whole-tree census or
partial-link gate was run, and no runtime or lifecycle conclusion is inferred.
Production source remains the previously validated ratio-only correction;
last census 882/1852 exact, 87,605 exact bytes and fixed phase 385/0.

Follow-up object inspection: block_size_table is a 48-byte STB_LOCAL object
in .rodata, attached to bwchdem.c's local symbol group; its neighboring objects
have the same baseline ordering and relative offsets (Space +48, Mark +58,
IIR +68, AGC +100). This does not support a separately exported table as the
explanation. Do not infer function ownership from the last preceding STT_FILE
in the linked global-symbol tail. The remaining constructor store mismatch
is specifically the ordering of two zero stores at +2 and +4, rather than a
wrong final field value. No additional source-order permutation was compiled.
