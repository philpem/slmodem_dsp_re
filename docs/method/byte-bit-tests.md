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
`initTxSequence` and CL in `rebuildJMSequence`; those were independent transfer
candidates at the time, and wave 21 applied them below. Do not infer a
whole-object ceiling from this census.

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

## Wave 21: the remaining `0x80` sites, and a wider byte-test screen

A per-function screen of all 1,852 blob symbols found 98 functions carrying a
byte-immediate TEST (`a8 ib` or `f6 c0..c3 ib`), 18 of which had at least one
mask whose count in our object was lower. All four `0x80` sites are now
recovered: the two in this document, plus `initTxSequence` DL and
`rebuildJMSequence` CL.

The operand is the part that had to be controlled. Passing `req->ctl1` or
`arg->flags` straight to the helper leaves a memory `testb $imm,disp(%reg)`
on the period compiler; loading the byte into a local first puts the value in
a register and reproduces `test $imm,%al`/`%dl`. `V29RX_control` then becomes
byte-exact, and `V29TX_control`/`V21TX_control` recover both immediate tests.

| Function | LCS/run before | LCS/run after | Ours bytes (blob) before -> after | Grade 0 before -> after |
| --- | ---: | ---: | ---: | --- |
| `V29RX_control` | 33/20 | 37/37 | 110 (110) -> 110 (110) | BYTES 24 -> EXACT |
| `V29TX_control` | 34/14 | 38/24 | 129 (126) -> 128 (126) | SIZE 3 -> SIZE 2 |
| `V21TX_control` | 26/10 | 30/14 | 94 (94) -> 94 (94) | BYTES 60 -> BYTES 61 |
| `initTxSequence` | 141/20 | 142/20 | 894 (1001) -> 879 (1001) | SIZE 107 -> SIZE 122 |
| `rebuildJMSequence` | 297/16 | 298/16 | 3132 (3134) -> 3126 (3134) | SIZE 2 -> SIZE 8 |

The two V.8 builders gain the native test and one LCS instruction, but their
remaining residuals still dominate; the helper also shortens each body, so the
size gap widened rather than closed. That is an LCS acceptance, not an exact
claim. The three control functions are shorter and closer.

The screen's other candidates did not qualify as genuine byte-valued sites:
`DataCarrierDetectV17/V27/V29`, `MTK_phasor`, `V34demodulate`,
`DILdescriptorPacker`, the `V92Jd` constructors and `V92ModulusEncoder::reset`
test shifted or 32-bit values, and `v34handshak` is the known inlining-boundary
artefact. `V22FP_control` was tried and dropped: the register-carrier form
moved 128 bytes at grade 0 where the baseline had a six-byte size gap, so the
direct form's one-instruction LCS gain did not justify it.

Wave 21 passed `make period J=3`: 394 differential tests, zero failures, all
structural checks. `make byteident-ratchet` is OK at 1,153 EXACT, 1,190 grade
0-or-1 and 1,156 link-selected EXACT over 1,852 symbols, with the existing
`dp_vpcm_exit` displacement unchanged.
