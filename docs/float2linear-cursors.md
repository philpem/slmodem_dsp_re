# Float2Linear: pointer walks let GCC reverse the loop

Baseline d7e118ca, complete Beepgen.c TU. The blob advances source and
destination pointers and decrements a remaining count. Retained source uses
an ascending int index for both addresses. The reverse Linear2Float utility
is already exact with an indexed loop and remains unchanged.

The four-cell domain crosses signed positive countdown with source/destination
cursors. It avoids an eager decrement for negative n (especially INT_MIN),
and preserves gain==0, x87 multiplication/conversion and memory operation
order. Each cell preserves23 functions and22 globals/bindings; only
zFLTUTL_Float2Linear changes. The raw unchanged full object reproduces.

| Cell | Bytes | Verdict | TU exact |
| --- | --- | --- | --- |
| Ascending indexed source | 102 | SIZE1 | 7/23 |
| Positive countdown, indexed buffers | 111 | SIZE10 | 7/23 |
| Advancing buffers, ascending loop | 101 | EXACT | 8/23 |
| Positive countdown, advancing buffers | 93 | SIZE8 | 7/23 |

Retain the cursors-only cell: `for (i = 0; i < n; i++)` with
`*dst++ = (short)(*src++ * gain);`. It reproduces all101 bytes and the
canonical zero-constant relocation. The .09.loop dump explicitly reports
"Can reverse loop" and "Reversed loop", changing induction variable62 from
+1 initialized0 to-1 with a runtime initializer. Thus the machine countdown
does not prove an original source countdown. The explicit-countdown cells
miss; do not copy an earlier Playbook winner mechanically.

The source still reads each float before its corresponding short write,
uses the same gain and truncating conversion, and performs exactly n ordered
updates for positive n. Zero/negative n and zero gain leave output untouched.
No integer or floating arithmetic changes, public-signature change or profile
exception. CrossDataLinks and every other body remain unchanged.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945354047).
Replay `python3 tools/playbook_float2linear.py --domain URL`. Artifacts under
build/playbook-float2linear/ preserve full sources, commands, source/header
hashes, complete inventories/bindings, dumps and changed relocation-bearing
disassemblies. Shared Gentoo GCC3.4.2-r2, selected assembler2.15.92.0.2, saved
complete flags and mandatory DSPLIB_REPRODUCE_BUGS are recorded.

Existing t_beepgen compares33 output samples at8 gains including zero,
negative gains and out-of-range short products (264 direct Float2Linear
checks), plus CrossDataLinks and zero-count checks. It does not explicitly
call Float2Linear with negative n or INT_MIN: those no-op properties follow
from the retained ascending condition and complete code identity. No new
fixture, fuzzing or mutation execution.

## Adoption validation

Complete comparison build300/300, zero failures. Only src_service_Beepgen.c.o
changes and its full retained bytes raw-reproduce the cursor winner. Whole-tree
865/1852 ->866/1852, exact bytes84,058 ->84,159; only Float2Linear gains,
zero losses. Saved before/after objects and exact-name deltas live in
build/playbook-float2linear-adoption/.

Two Beepgen static anchors are retargeted: zero-gain sign-test fault and
rounding-offset fault keep their original changes on the cursor expression.
No mutation execution or snapshot refresh. All285 suites/10,038 static
anchors remain unique with none detached.

Same-order complete300-object partial links remain DIFFERENT(strict exit1).
Positioned equality68,308 /943,398 and allocated bytes914,158 are unchanged;
exact section records70/92, relocation records1,022/18,317 and symbol
records394/2,907 unchanged. Canonical function recovery does not establish
full-object identity.

Fixed Gentoo make phase passes385/0, structural checks clean. Reference census
14,224 refs/2,688 finding headings; static anchor census285 suites/10,038.
Replay-tool syntax and whitespace checks pass. No modern portability claim.
