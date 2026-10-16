# V.32 sequence-generator `top` local experiment

## Question and source evidence

`InitGenSequence` was `SIZE(3)`: 71 bytes against the blob's 68.  The only
extra instruction was `movzwl %ax,%eax` between `dec %eax` and the two
16-bit field stores.  This is a computed-value narrowing, not evidence that
either destination field has a different type.

The retained source was:

```c
	unsigned short top = (unsigned short)(total / width - 1);	/* D401 */

	FIELD_US(hdx, V32HDX_GEN_INDEX_MASK) = top;
	FIELD_US(hdx, V32HDX_GEN_INDEX) = top;
```

The two finite alternatives changed only the declaration and retained the
arithmetic expression and both narrowing stores verbatim:

```c
	int top = total / width - 1;	/* D401 */
```

```c
	unsigned int top = total / width - 1;	/* D401 */
```

## Controls and toolchain

The three cells were `unsignedshort`, `inttop`, and `unsignedinttop`.  Each
overlaid only `/src/src/pump/v32/v32seq.c`, preserving its basename and source
path.  The unchanged overlay raw-matched
`build/tc_repro/src_pump_v32_v32seq.c.o`, SHA-256
`629935dc4907ea03f90990f36e2e6f6f765d218a12058e04cbcd8eee1cebe9ee`.

Published image:
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, using the native image
user and explicit compiler path
`/usr/i386-pc-linux-gnu/gcc-bin/3.4`.  The compiler identified itself as
Gentoo GCC 3.4.2-r2 and had POSIX checksum `157945443 330495`.  Its actually
selected assembler was GNU assembler 2.15.92.0.2 at
`/usr/lib/gcc/i386-pc-linux-gnu/3.4.2/../../../../i386-pc-linux-gnu/bin/as`,
checksum `3346421025 268144`.

The complete flags, copied from the retained `.build-config`, were:

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude
-D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-DDSPLIB_REPRODUCE_BUGS
```

The shared helper appended `DSPLIB_REPRODUCE_BUGS` last.  Exact commands and
compiler output are retained per cell in `command.json` and `compile.log`;
`toolchain.txt` records compiler and selected-assembler identity.

## Results

All cells compiled and retained the same 13-symbol denominator.

| cell | exact | gain | loss | changed function bodies |
|---|---:|---|---|---|
| `unsignedshort` | 9/13 | none | none | none; raw baseline match |
| `inttop` | 10/13 | `InitGenSequence` | none | `InitGenSequence` only |
| `unsignedinttop` | 10/13 | `InitGenSequence` | none | `InitGenSequence` only |

The two full-width cells produced byte-identical 2,788-byte objects, SHA-256
`6f95503fcccf8e068ecf6080aa1e6497e6c095b04c682a7f0bd7fe6be60a3919`.
In both, `InitGenSequence` is positionally `EXACT`, 68 bytes against 68, and
the unwanted `movzwl %ax,%eax` is absent.

No non-local function emission changed.  All function relocation maps equal
the baseline; the complete partial object retains all nine relocation records
in order and value.  All non-text contents are equal.  Of 17 symbol records,
16 are exact; the sole difference is `InitGenSequence`'s size changing from
71 to 68.  Its kind, global binding, default visibility, `.text` section and
start offset remain unchanged.  Thus `symbol_records_equal_baseline` is
correctly false because size is part of the record, not because a binding or
export changed.  Positional `.text` comparison reports 31 changed bytes
because removing three bytes shifts the following text within the unchanged
total section; per-function comparison proves no following body changed.

## Conclusion

Both full-width declarations are code-generation preimages in this bounded
family.  Prefer plain `int`: both `unsigned short` parameters undergo integer
promotion, so `total / width - 1` has type `int`; `unsigned int` adds a
conversion for which the object supplies no distinguishing evidence.

This does not retype either 16-bit field.  It removes an unsupported early
narrowing while the two stores still perform the required low-16-bit
conversion.  The `total < width` case is unchanged: the promoted expression
produces `-1`, and both stores write `0xffff`, just as the old unsigned-short
local did.  The unsigned candidate reaches the same stored low bits by modulo
conversion.  Division-by-zero and shift behaviour recorded by D401/D402 are
unchanged.

Artifacts and full machine-readable per-symbol results are in
`build/v32seq-top-experiment/results.json`.  This is a source candidate from a
three-cell full-TU experiment; no differential or project gate was run.
