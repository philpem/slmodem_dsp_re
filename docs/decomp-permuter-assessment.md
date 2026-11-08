# decomp-permuter assessment

decomp-permuter could automate a bounded set of source hypotheses for remaining
register-allocation differences. Its current code already supports i386; the
main integration work is preserving this project's translation-unit context
and using its strict comparator. Upstream score zero is unsafe as an acceptance
criterion: isolated controls produced false zeros for changed symbols, memory
offsets, branch destinations and, with relocations enabled, a changed destination
register. No production source, compiler flags or grading rules were changed
for this assessment.

The review used upstream commit
[`8556c81d80d1c1af98a858c8f4dc951357f29139`](https://github.com/simonlindholm/decomp-permuter/tree/8556c81d80d1c1af98a858c8f4dc951357f29139),
dated 6 October 2026. Its [README architecture list](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/README.md)
names MIPS, PowerPC and ARM32, but
[`X86_SETTINGS` and `get_arch`](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/objdump.py#L258-L302)
recognise ELF machine 3 and invoke host objdump. The README is incomplete on
this point; an initial i386 backend port is unnecessary.

The measured controls used Python 3.12.3, GNU assembler and objdump 2.42,
`as --32`, and upstream `Scorer` with `algorithm="difflib"`. Each object had
one global `probe` function containing the stated instruction sequence followed
by `ret`. There were 12 pairs and two scoring configurations, giving **24
evaluations**. These are decoder/scorer controls, not period-compiler or modem
differential tests.

* **Defaults:** `stack_differences=False`, `ign_branch_targets=True`, and the
  upstream objdump command, `objdump -d --no-show-raw-insn`.
* **Explicit options:** `stack_differences=True`, `ign_branch_targets=False`,
  and `objdump -dr --no-show-raw-insn`.

| Difference between target and candidate | Defaults | Explicit options |
| --- | ---: | ---: |
| Identical `mov 4(%eax),%edx` | 0 | 0 |
| `call foo` versus `call bar` | 0 | 5 |
| `mov foo,%eax` versus `mov bar,%eax` | 0 | 5 |
| `mov 4(%esi),%eax` versus offset 8 | 0 | 9 |
| `mov 4(%ebp),%eax` versus offset 8 | 0 | 9 |
| `mov 4(%esp),%eax` versus offset 8 | 5 | 5 |
| `mov 4(%eax),%edx` versus destination `%ecx` | 5 | 5 |
| `mov foo(,%eax,4),%edx` versus destination `%ecx` | 5 | 0 |
| `mov %al,4(%ebx)` versus source `%ah` | 5 | 5 |
| Conditional jump to second increment versus `ret` | 0 | 1 |
| `fdiv %st(1),%st` versus `fdivr` | 200 | 200 |
| `fstps 4(%eax)` versus `fstpl` | 200 | 200 |

The conditional-jump pair had the same remaining instructions:
`test %eax,%eax; je target; inc %ebx; inc %ecx; ret`. The changed target either
executes or skips `inc %ecx`. The destination-register controls establish a
lost operand distinction; they do not claim that the tiny functions' scratch
registers are externally observable ABI results.

The scratch reproduction script and complete normalised rows are retained at
`scratch/decomp-permuter-review/probe/scorer_controls.py` and
`scratch/decomp-permuter-review/probe/scorer-results.json` in the investigation
worktree. Run the script with Python from the pinned upstream checkout after
installing its dependencies. The table records the durable results even when
that scratch checkout is unavailable.

Five non-identical pairs score zero under defaults. The causes are visible in
the [x86 settings](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/objdump.py#L258-L283):
objdump omits relocations, branch targets are ignored by default, and the stack
offset regular expression matches `%ebp` and `%esi`, but omits `%esp`. An ESI
field displacement therefore disappears as though it were a stack-layout
difference.

Enabling relocations and both comparison options does not make the decoder
sound. [`parse_relocated_line`](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/objdump.py#L304-L323)
selects text after the last comma as the relocation operand. For both relocated
SIB loads in the table, the resulting row is:

```text
mov 0x0(,%eax,4),foo
```

The relocation replaces the destination register instead of the memory
displacement, making `%edx` and `%ecx` indistinguishable. The
[scorer](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/scorer.py)
also splits operand text on every comma, including commas inside SIB addressing.
It aligns mnemonics and adds heuristic penalties; it does not prove register
value flow, ABI behaviour or CFG equivalence. Its returned digest hashes the
normalised rows, so the demonstrated collisions also affect any deduplication
that relies on that digest.

An adapter should reuse the existing `tools/toolchain/byteident.py` decoding
and strict comparison boundaries. A diagnostic distance can rank candidates,
but reserve a successful zero for the existing strict EXACT result, with no
new allowances. Preserve relocation type, target and addend at the encoded
operand; memory base, index, scale, displacement and width; partial-register
identity; and instruction-boundary branch destinations. Variable-length x86
requires mapping branch destinations to decoded instruction boundaries rather
than comparing printed absolute offsets. Stack-slot equivalence requires
existing proof, not a register-name regular expression. Preserve x87 opcode,
operand order, stack effects and narrowing stores: the two positive detector
controls above do not establish a general x87 equivalence analysis. Unsupported
instructions or relocations must refuse acceptance.

The importer and candidate pipeline need separate treatment. The
[compiler wrapper](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/compiler.py)
invokes a shell command with temporary C input and `-o` output arguments, so it
can call the pinned Gentoo compiler. Container mounts must include those
temporary files; a scratch `TMPDIR` is one possible arrangement. Keep the
project's exact compiler image, flags, bug-reproduction definitions, forced
includes and headers, and compile the complete original translation unit.

Upstream's [AST extraction](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/ast_util.py#L106-L130)
replaces other non-inline function bodies with declarations and removes
`static` from function declarations. Importer's `--no-prune` does not eliminate
that extraction. Its [preprocessor](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/preprocess.py)
and [importer](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/import.py)
use host preprocessing and compatibility substitutions; the default assembly
import path is MIPS-oriented. These transformations can change the compiler
context responsible for allocation. Prefer enumerating an explicitly marked
source region and splicing it into an otherwise unchanged full translation
unit, bypassing reduction and AST regeneration. The untouched control must
reproduce the existing whole-TU object before trying alternatives.

The parser is a C parser. `PERM_IGNORE` and `PERM_PRETEND` can conceal unsupported
syntax, but do not establish faithful C++ method, template or overload handling.
Forcing `-x c++` in the compiler wrapper does not fix this parser limitation.
Begin with a C function. C++ support would need an independently validated
text-splicing or parser adapter, including preservation of method and ABI
contracts.

Use manual, finite enumeration only for this project's first trial. The
[permutation parser](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/perm/parse.py#L136-L147)
automatically enables randomisation when there are no effective choices.
Assert the expected finite count and reject randomisation explicitly. The
[randomiser](https://github.com/simonlindholm/decomp-permuter/blob/8556c81d80d1c1af98a858c8f4dc951357f29139/src/randomizer.py)
can change function return and parameter types, external types, statement order,
conditions, casts and masks. These are not all semantics-preserving changes.
A `compiler_type="gcc"` setting adjusts weights; it does not impose this
project's source-evidence policy. A better score would not supply the missing
source or ABI evidence.

Recommended next trial: allocate **45 minutes**, as a budget rather than an
implementation estimate, to one C function already diagnosed as an allocation
residual. Admit at most **16** independently justified declaration, scope or
lifetime variants, with each predicted effect recorded before compilation.
Do not replay closed source palettes. Spend at most the first 20 minutes on the
adapter and unchanged-TU control; stop if that control cannot reproduce the
current object. Before searching, require the unchanged candidate to score
zero against itself and the symbol, memory, branch, relocated-register,
partial-register and x87 negative controls to remain nonzero. Keep candidate
source and object hashes independently of upstream's normalised digest.

Success means a strict whole-function EXACT improvement under the period
compiler, no collateral function regression in the full translation unit, and
passing relevant period differential coverage and the required `make phase`
gate before integration. A REGALLOC certificate alone is not an EXACT gain.
Stop after the admitted cells or the time limit; a lower heuristic distance
without strict improvement is an unsuccessful trial, not permission to widen
the search or weaken the comparator. No port or modem-source permutation was
attempted in this assessment.

## Bounded trial result (F12140)

The approved follow-up at `d61e5e8c8` screened all 17 C symbols among the 39
REGALLOC symbols and admitted one native-backed type/lifetime experiment in
ApplyBulkDelay. The native retained value is narrowed before its clamp and
later uses. `PERM_GENERAL(int back;,short back;)` generated exactly two finite
outputs through upstream's `perm_parse` and `RootPerm.evaluate`, with random
mode explicitly disabled. Only that declaration was spliced into the full TU;
upstream AST reduction, host preprocessing and scorer were bypassed.

The unchanged full-TU control reproduces the production object byte for byte.
The short-local candidate also produces that identical object: all 29 functions,
nontext data, bindings and relocation consumers remain equal. The target stays
REGALLOC with 103 ordinary instructions per side. The initial RTL differs but
retains an SI pseudo for the local; no final allocation effect survives. The
candidate is discarded and this source spelling domain is closed. No original
type is uniquely established, and there is no source promotion.

Control and candidate compile wall times, including container startup, are
5.27 and 3.58 seconds. Four comparator controls establish self identity and
rejection of a changed byte, relocation destination and computed displacement.
No runtime or broad gate is run for the discarded cell's identical object.
The reporting/tool integration separately passes `make phase`: 399/399 period
tests plus structural and 19 vector controls. Commands, image digest, flags,
hashes and results are in `scratch/diagnostic-vector-permuter/`.

Finite enumeration is operational, but this trial supplies no evidence that
broader permutation will improve source recovery. The next priority is the
[diagnostic vector and byte-lane checker finding](method/metric-vector.md),
rather than extending declaration or register-colour palettes.
