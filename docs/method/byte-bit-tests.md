# Recovering an immediate byte bit test

A byte mask operation can reproduce GCC 3.4.2's `test $0x80,%al`
(`a8 80`) without assembly or a compiler-option change:

```c
static int byte_flag_test(unsigned char value, unsigned char mask)
{
    return value & mask;
}
```

Use the same operand contract for the mask chain. Under the recovered Gentoo
GCC 3.4.2-r2 compiler and the normal reconstruction flags, this example emits
one byte load followed by `a8 08`, `a8 20` and `a8 80`, with no extra extension:

```c
unsigned char flags = *p;

if (byte_flag_test(flags, 0x08))
    first();
else if (byte_flag_test(flags, 0x20))
    second();
else if (byte_flag_test(flags, 0x80))
    third();
```

The helper is automatically inlined. Both input types and the promoted `int`
result matter: the controlled family crossed four value types, three mask
types and three result types, giving 36 test functions in one complete translation unit. Only the unsigned-byte/unsigned-byte/int cell reproduced all three
AL tests without a second extension in the all-helper chain. These controls
establish a compatible operand contract; the object cannot identify the
original helper name, exact spelling or whether it lived in a header.

## Why the obvious expression differs

A direct unsigned-byte `flags & 0x80` condition can be folded into a signed
byte comparison before RTL expansion. It then emits `test %al,%al` with a
sign branch. A promoted integer mask can instead survive as a SImode test,
which emits `a9 80 00 00 00`.

The helper delays knowledge of the constant mask. With two unsigned-byte
parameters, GCC narrows the AND itself to QImode (8 bits) before extending its result
to `int`. Constant propagation after inlining and combine then produce a
single-bit extraction from a widened reference to the original QImode
register. The i386 splitter recognises that widened reference and restores
QImode, leaving an immediate byte TEST. Using the same helper for all three
masks lets all tests share that one byte carrier.

Gentoo RTL dumps confirm the progression: initial QImode AND and result
extension; combine's `zero_extract:SI(subreg:SI(QI ...), 1, bit)`; and the
post-reload byte tests. The corresponding upstream GCC 3.4.2 implementation
is in `c-typeck.c`'s `build_binary_op` arithmetic narrowing and i386.md's
`*testqi_ext_3` splitter. The upstream source explains the mechanism; the
Gentoo compilation is the measured authority.

## Controls and transfer limits

Applying a signed-byte cast and an ordering comparison can also produce a
byte-immediate test:

```c
if (((signed char)flags & 0x80) > 0)
    third();
```

That control uses a different route through delayed comparison folding. In
the full V.8 wrapper it introduces a second carrier and tests DL; in the V.32
handler it changes block layout. It is not the retained source form.
Likewise, applying the helper only to the last mask is insufficient in the
minimal chain: a separately promoted copy survives for the first two masks.

On baseline `64c87d44e`, complete-TU controls measured:

| Function | Source | Bytes | Instructions | LCS | Longest run | `a8 80` |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `v8_process` | baseline | 623 | 153 | 143 | 63 | 0 |
| `v8_process` | helper across modulation masks | 620 | 152 | 144 | 63 | 1 |
| `v32_data` | baseline | 860 | 214 | 148 | 41 | 0 |
| `v32_data` | helper across status masks | 860 | 214 | 169 | 43 | 1 |

Only the target function changes among the five V.8 and four V.32 defined
function bodies. The helpers emit no standalone body. Native sizes remain
590 and 859 bytes, respectively, so opcode recovery leaves both functions
non-exact. The V.8 post-switch argument guard is a separate residual.
No field type, address, read boundary, compiler flag or fixture changes.

A disassembly census of the reference `.text` found two `a8 80` instructions,
in these two functions. The same byte-immediate operation occurs in DL in
`initTxSequence` and CL in `rebuildJMSequence`. Their existing reconstruction
does not reproduce it; these are independent transfer candidates, not gains
claimed by this change. Do not infer a whole-object ceiling from this census.

Both separate source changes passed `make phase J=3`: 394 differential tests,
zero failures, and all structural checks. Each changes one of 300 canonical
objects and reproduces its private control's target body. The three protected
symbol sets remain unchanged: 1,150 EXACT, 1,187 grade 0-or-1 and 1,153
link-selected EXACT over 1,852 symbols; the phased ratchet passes.

The isolated opcode control has one positive and three known negatives:
ordinary unsigned-byte condition, integer operand, and integer mask parameter.
Only the byte/byte/int helper chain produces all three immediate AL tests
without an extra register extension. No modern compiler is part of this
investigation.
