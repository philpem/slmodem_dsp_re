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

## Wave 22: memory carriers, the full native-form screen, and two unswitched loops

Wave 21 counted only register-form immediate tests (`a8 ib`, `f6 c0..c3 ib`).
This wave counted the memory form too (`f6 /0` with a mod != 3 ModRM): a fresh
screen over the same 1,852 symbols found **25** functions with at least one
mask under-represented in ours. Five TUs took the helper. The table's "native"
column is the object's own form at the named mask; `bytes` are ours against
the blob's.

| Function | mask, native form | LCS/run before | LCS/run after | ours bytes (blob) | grade before -> after |
| --- | --- | ---: | ---: | ---: | --- |
| `v22_data` | 0x80, `testb mem` | 160/43 | 161/43 | 956 (946) | SIZE 10 -> SIZE 10 |
| `setfinalrate` | 0x80, `testb mem` | 143/22 | 151/22 | 921 (905) | SIZE 18 -> SIZE 16 |
| `V34descrambler` | 0x04, `testb mem` | 29/5 | 60/9 | 232 (236) | SIZE 24 -> SIZE 4 |
| `decoderv34` | 0x04, `testb mem` | 122/12 | 182/68 | 713 (714) | SIZE 28 -> SIZE 1 |
| `probeselect` | 0x80 x5, `testb mem` | 599/54 | 640/54 | 6678 (6173) | SIZE 517 -> SIZE 505 |

**`v22_data`, `setfinalrate`: the signed-byte fold, once more.** Both sources
already wrote `field & 0x80`, but GCC folded the direct expression into a
signed byte compare (`cmpb $0x0,mem; jns`). Passing the unsigned-byte field
straight to the helper restores the object's memory `testb $0x80,disp`: the
object's tests are at `0x88af3` in `v22_data` and `0x5efd3` in `setfinalrate`.
No byte local is wanted here; the operand contract is the opposite of the
wave-21 register sites.

**`V34descrambler`/`decoderv34`: the conditional tap belongs inside the loop.**
The object does not compute `tap` once and shift by it. It has **two copies of
the descrambler loop**, one with `shr $0x5` and one with `shr $0x12`, selected
by `testb $0x4,0x122` (`0x5bb03` in the standalone body and `0x5bc9e` in the
copy inlined into `decoderv34`). Our source had hoisted
`int tap = (s->flags & V34_SCR_ANSWERER) ? 18 : 5;` above the loop, so GCC
if-converted the ternary to `sbb` arithmetic and could not unswitch. Writing
the conditional tap **in the loop body**, with the helper on the byte cast,
lets the period compiler unswitch it back into the object's two constant-shift
loops. `decoderv34`'s four-point slice counter is also `short`, not `int`:
the object emits `movswl; cmp $0x4`, and that one type change takes the
function from SIZE 28 to **SIZE 1** (713 against 714 bytes).

**`probeselect`: five `m[...] & 0x80` tests.** Two become the object's native
memory `testb` (`0x616be` at `0xa9e8`, `0x61831` at `0xa9e6`). The three at
`0xa97e` come out register-carried (`test $0x80,%al`) because the value is
live in a register there; the mask and the branch are recovered but the
operand carrier differs. LCS rises by 41 and the body shrinks by two
instructions. That is an LCS acceptance, not an exact claim.

**Rejected or left alone.** `V22FP_control` was retried with its bounded cell
set and left unchanged: baseline SIZE 6, LCS/run 31/10, 45 instructions;
direct helper SIZE 2 and LCS 32 but `testb $0x8,0xc(%reg)` where the object
has `test $0x8,%dl`; byte-local carrier LCS 32 but 44 instructions and 128
differing bytes (BYTES); local plus mask constant LCS 30 and no test at all.
`evaluateRxJMSequence` gained the native `testb $0x80,0x1(%rbx)` but its
LCS/run stayed 127/11, so it was reverted. `V8UpdateModemParameters`' helper
came out register-carried where the object's test is memory and LCS fell
174 -> 171; reverted. Inlining `rebuildJMSequence`'s `all_flags` expression
added 34 instructions for one LCS point (298 -> 299, ours 787 against 752);
reverted. The **B.103** TU has no under-represented mask: `B103FP_modem` is
the only difference and it carries *extra* 0x20/0xdf tests in ours. The **V.22**
TU has only `V22FP_control` left.

Still excluded after inspection: `DataCarrierDetectV17/V27/V29`,
`V34demodulate`, `V92ModulusEncoder::reset` and the `V92Jd` constructors test
shifted values; `initTxSequence` (0x80008), `modulatevector` and the
`v34handshak` rows are 32-bit tests; `initdigital`'s 0x20 is the high byte of a
16-bit `0x2000` word; `VPcmV34Create`'s 0x1 is a boolean stack local;
`modem_serrint`, `receiver`, `rebuildJMSequence` (0x8/0x1) and
`v8handshakinit` need a structural recovery larger than the operand change.

Wave 22 passed `make period J=3`: **394 passed, 0 failed**. The grade census
is unchanged at **1,154 EXACT / 1 UNRESOLVED / 35 REGALLOC / 89 BYTES / 573
SIZE** over 1,852, grade 0-or-1 **1,190**, link-selected EXACT **1,157**;
`byteident --ratchet` is OK at the same floor. No EXACT or grade-0/1 symbol
moved in either direction. Blob sha256 unchanged
(`1f3e56d0...cf0b379`).
