# EIA6 x87 forms: operand mode, death notes, and earlier folding

This diagnostic examines the four existing full-TU EIA6 controls at `548b5edd`, without new candidates or compiler invocations. Twelve existing RTL streams and three hash-pinned official GCC 3.4.2 source files establish distinct selection mechanisms for conversion, multiplication and comparison. The observations are from the retained **Gentoo GCC 3.4.2-r2** executable; the explanation is from **stock official GCC 3.4.2** sources, not a claim that the Gentoo patch stack was inspected. The cached backend files were byte-compared against fresh official release-tag copies. The period image did not contain `/build/gcc-3.4.2/gcc` sources.

## Conversion: a live XF source can acquire a death note late

[Official reg-stack.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/reg-stack.c), lines551–577, strips `FIX` when locating the true source register. Lines1471–1482 then route a SET whose stripped source is a stack register through `move_for_stack_reg`. Lines1111–1145 handle saving that stack source. When it already has REG_DEAD, it is popped. Otherwise **a live XFmode source with spare stack capacity is duplicated**, using `gen_movxf`, and a REG_DEAD note is added to the original instruction. The condition examines the **source mode**, without testing whether the destination is an XF memory store or an SI integer conversion.

[Official i386.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.c), lines8077–8101, emits `fistp` if the physical stack-top register has REG_DEAD, otherwise `fist`, except that DI integer destinations always pop and explicitly duplicate a live input because hardware has no nonpopping DI form. SI conversions have no corresponding forced-pop requirement. [Official i386.md](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.md), lines4210–4263 and subsequent post-reload splits, supplies the fix_truncsi memory pattern and temporary control-word storage.

All four existing Gentoo cells independently fire this rule: their first XF→SI conversion has **no REG_DEAD in33.sched2**. At34.stack a new XF register move appears immediately before it and the conversion acquires REG_DEAD. In expanded-unequal this is conversion UID412 and inserted move UID439. Final assembly is `fld %st(0)` followed by `fistpl`. Thus interpreting final REG_DEAD as proof that the original logical x value was already dead would be wrong: the stack pass killed its newly duplicated physical copy.

A live SFmode or DFmode source bypasses that duplication condition, and an SI destination can therefore emit nonpopping `fistl`. There is no separate SF/DF precision or SI-destination condition in this rule. A live XF source with a completely full eight-register stack also bypasses duplication. Dead inputs pop regardless of SF/DF/XF. These are backend mode/liveness facts, not a unique mapping to C declarations.

Original `0x45365` loads the integer into an empty x87 stack following a void diagnostic, `0x45371` multiplies it, and `0x45384` converts with nonpopping `fistl`; x remains live for subsequent uses. Occupancy at that conversion is one. **Under the source rule corroborated by our actual Gentoo observations, this rules against a live XF source there and leaves SF/DF alternatives.** It does not select float versus double, establish intermediate rounding, or prove an unknown Gentoo patch cannot alter the general rule. Those stronger claims need separate evidence. No type variants were compiled by this diagnostic.

## Multiplication: memory versus register is selected before reg-stack

The x87 binary patterns admit a register operand or suitable SF/DF memory operand: i386.md14397 (`fop_xf_comm`) and14713 (`fop_xf_4`) distinguish register/register from a widened memory operand. i386.c7939–7943 emits the memory-width suffix when its operand is MEM. Lines7945–7958 emit the popping register form when the other register dies. The stack pass remaps/reorders registers and their deaths (reg-stack.c1540 onward); it does not convert the measured memory scale into a register scale.

In expanded-unequal, scale10000 operation UID181 remains register/register through22.regmove, with the scale register dying. By24.lreg it is `float_extend(SF MEM LC10) * XF REG`; this survives33.sched2 and34.stack and becomes memory `fmuls`. Original instead loads scale into a register and uses `fmulp`. The observed boundary is **between22.regmove and24.lreg**, not final assembly or reg-stack. This report does not attribute the fold to an exact internal routine without a finer trace.

## Comparison: register-zero folding is earlier still

The compare patterns accept a memory operand for ordered SF/DF compares (i386.md803–814); XF comparisons require register operands. `output_fp_compare` (i386.c8140–8175) emits double-pop `fcompp` when both stack-register operands die. Otherwise the table at8181 onward chooses `fcomp` when stack top dies; an SF memory operand supplies the `s` suffix. `compare_for_stack_reg` (reg-stack.c1292–1375) remaps death notes and permits double-pop when the dying second operand is adjacent to the dying top.

Expanded-unequal starts with register zero106;19.life still has that register in branch UID232. **20.combine folds it into SF memory LC11.** The branch remains memory-based through reload.27.flow2's late splitting exposes compare UID424; its final form is `fcomps` with one dying stack register. Original has `fldz; fcompp`, which needs a retained zero register and both register operands dying. Our same function's pre-diagnostic sign compare UID392 already has that two-register/two-death shape and emits `fcompp`: this is an internal positive control for the emitter rule. It is insufficient to change the spelling of `!=` alone and assume the original register-zero lifetime will follow.

## Reproduction and pinned inputs

Run the read-only tool after obtaining the source files below:

```sh
python3 tools/gcc_x87_eia6_mechanism.py \
  --fetch-source \
  --gcc-source build/gcc-x87-mechanism-source \
  --dumps build/eia6-prior-controls/V90PreFilter
```

It verifies all source hashes, reads only the named existing EIA6 function's instruction streams (skipping scheduler diagnostic prose), and writes patterns/death notes and folding boundaries to `build/gcc-x87-mechanism.json`. Its denominator is four cells / twelve streams, four conversion transformations and two independently asserted operand-form boundaries. No runtime, mutation, fuzzing, candidate compilation, flags matrix, source/header modification or inferred declaration permutation was performed.

| File | SHA256 |
| --- | --- |
| `reg-stack.c` | `5af035ff9ef4070fdefb1d5ba787f09d16a8f3efff1c14dfc66e05fbccb120b0` |
| `i386.c` | `bb46f666e8686bf902ae1f9f32fcbfb298259915c1368e669326c89adce34cdc` |
| `i386.md` | `2b62f98bc15ccdc268036b57da4afe23f3f9e2c5fcfdda5175758e3dc62719ab` |

Official raw paths are under `https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-3.4.2/gcc/`: `reg-stack.c`, `config/i386/i386.c`, and `config/i386/i386.md`. Source inputs are retained under the diagnostic build directory; complete executable/compiler provenance and commands remain in the previous EIA6 control artifacts. This bounded proof unlocks a source-mode discriminator while keeping the original upstream folding and lifetime questions open.
