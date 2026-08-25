# `V90Phase3Demodulator::getV90Decision(float)` — the decode

A resumable record of what was read out of the blob at `0x23830..0x258f0`
(8,379 bytes) for this one method.  Written as the work proceeded, per
`docs/largefunctions.md` item 5.  Everything here comes from `tools/dis.py`.

## Shape

A 34-way `switch` on `state` (+0x28), dispatched through a jump table at
`.rodata+0x854` covering 0..0x21.  Values 7, 8, 0x12 and everything above
0x21 land on the default block.

    short getV90Decision(float sample)
    {
            short decision;              /* NOT initialised -- see below */
            short s = (short)sample;
            word_2c++;
            switch (state) { ... }
            return decision;
    }

**The return type is `short`, not `void`.**  `getDecision(float)` at 0x258f0
calls this and then `cwtl` — sign-extends `%ax` — before returning, which it
would not need if the callee returned `int`.  The header's "spelled `void` for
want of evidence" no longer applies to this member.

**`decision` is genuinely uninitialised on the default path.**  The epilogue is
`mov %edi,%eax` and `%edi` is a callee-saved register the default block
(0x238b0) never writes, so states 7, 8, 0x12 and >0x21 return whatever the
caller had in `%edi`.  That is a local in a register with no reaching
definition, not a missing `return`.  D321 records it.

**`word_30 = 0` is per case, not hoisted.**  Case 0 proves it: it has no store
at the top and reaches `movl $0x0,0x30(%ebx)` at 0x248d5 on two of its three
paths only, as the `else` of an `if` that stores 9.  A single store before the
`cmp $0x21` would have made that block dead.

## Fields this method settles

| offset | was | is |
|---|---|---|
| +0x3cc | `unsigned int word_3cc` | `SerialDifferentialDecoder<int>` — 0x23f6b calls `_ZN25SerialDifferentialDecoderIiE7processEi` on `this+0x3cc` |
| +0x3f4 | `pad_3f4[5]` "nothing reaches it" | `unsigned int word_3f4` + one pad byte; case 3 and case 2 store `params->unnamed_4a4` / `params->unnamed_344` there |
| +0x420 | `pad_420[4]` "nothing reaches it" | `unsigned int word_420`; holds `TRN1_QC_DD_LENGTH` or `TRN1D_DD_LENGTH`, and case 4 compares `word_2c` against it |

`word_2c` is a free-running sample counter (`incl 0x2c(%ebx)` before the
dispatch) that every state resets to 0 on the way out; `word_30` is a
per-sample event code the caller reads; `word_04` is the RBS phase, 0..5,
advanced by `if (++p == 6) p = 0`.

`word_2c` and `word_14` are confirmed **unsigned**: `push $0; push %eax;
fildll` builds a 64-bit value with a zeroed high dword, which is GCC 3.4.2's
`(float)(unsigned int)` and never its signed form.

## The state table, from the author's own diagnostics

The case that prints "X TimeOut" *is* state X, so this mapping is sourced.

| state | block | name from its strings |
|--:|---|---|
| 0x00 | 0x241a9 | WaitForSd |
| 0x01 | 0x2414c | SdDemod |
| 0x02 | 0x240cb | (post-SdNot; enters TRN1d DD or TRN1d Known Data) |
| 0x03 | 0x24030 | TRN1dKnownData |
| 0x04 | 0x2453a | (TRN1d DD; enters "study reference Ucode") |
| 0x05 | 0x2443d | study reference Ucode |
| 0x06 | 0x245ed | WaitForJd |
| 0x07, 0x08 | default | — |
| 0x09 | 0x23f13 | JdDemod |
| 0x0a | 0x24727 | DILDemodFirstStudy |
| 0x0b | 0x24227 | DILDemodSecondStudy |
| 0x0c | 0x2434c | DILDemodThirdStudyStage |
| 0x0d | 0x23e8b | DILDemodQCfirstStudy |
| 0x0e | 0x23d95 | DILDemodQCsecondStudy |
| 0x0f | 0x23cc1 | DILDemodQCthirdStudy |
| 0x10 | 0x23beb | DILDemodErrorRelaxation |
| 0x11 | 0x23b7e | ProbingDILDemod |
| 0x12 | default | — |
| 0x13..0x19 | 0x23b70 | terminal: `word_30 = 0; decision = 0;` |
| 0x1a | 0x23b00 | WaitForQts |
| 0x1b | 0x23a6a | WaitForQtsNot |
| 0x1c | 0x23a03 | (enters ANSpcm demod) |
| 0x1d | 0x239ca | ANSpcm demod |
| 0x1e | 0x23989 | (waits for the ANSpcm energy drop) |
| 0x1f | 0x2397b | terminal: `word_30 = 0; decision = s;` |
| 0x20 | 0x23925 | (ANSpcm recovery, QTs timeout fake) |
| 0x21 | 0x238dc | (ANSpcm recovery watch) |

0x00, 0x03 and 0x1a are the three the header's `Phase3DemodulatorState` already
names, and they agree.

## `twoLevelDemod(float, int &)` appears here four times over

Cases 4, 5, 6 and 9 open with a block that is instruction for instruction the
body of `_ZN20V90Phase3Demodulator13twoLevelDemodEfRi` (0x215a0, its own blob
symbol):
the `short_a948`/`isAltRbs` choice between `linMapp` and `linMappAlt`, the
`fcomps` of the sample against 0.0f, the `SerialDifferentialDecoder<int>` and
the `Descrambler<int,int>`.  The `lea 0x98(%esp),%edi` and the accesses through
`%edi` are the `int &` out-parameter, which is why an ordinary local has its
address taken at all.

That symbol is GLOBAL and in `.text`, not weak and not in a
`.gnu.linkonce.t` section, so it was not declared `inline` — and GCC 3.4.2 at
`-O2` inlines nothing that is not.  **So the duplication is in the author's
source**, and the reconstruction writes the block out at each of the four
sites rather than defining the member and hoping.  Finding F2114.  **Whoever
writes `twoLevelDemod` should read this section first**; the four copies here
are its body.

The one thing that argument does not explain is the stack slot: the four
copies address their bit through `lea 0x98(%esp),%edi` as though its address
had been taken, which is exactly what the member's `int &` would do.  Register
pressure in an 8 KB function is the alternative reading and nothing settles it;
register allocation is on the "free, so ignore it" list either way.

## The magnitude-to-code expression

Twenty sites, all open-coded in the blob:

    (unsigned short)(pcmType == PCM_TYPE_MU_LAW ? 0xff - linear2ulaw(m)
                                                : linear2alaw(m) ^ 0xd5)

Written as a macro because a function would have to be relied on to inline, and
GCC 3.4.2 at -O2 inlines nothing not declared `inline`.

**The width is sixteen bits and `make similarity` is what settles it.** As
`unsigned char` the u-law arm comes out `not %al` at all twenty-two sites; the
object has `not %al` at the six feeding an `unsigned char` argument and
`movzbw %al,%cx; sub %ecx,%ebp; movzwl %bp,%eax` at the sixteen indexing a
table.  The value is 0..255, so that truncation cannot be observed by any test
— the compiler emitted it because the type asked for it, which is the
"forced, so act on it" side of CLAUDE.md's rule.

## The three tables it indexes, all in `V90AutoDigitalImpDetector`

`linMapp[phase][code]` at +0x0000, `linMappAlt[phase][code]` at +0x0600 and
`prevLinMapp[code]` at +0x0c00.  The phase index is truncated to 16 bits at
every one of those sites (`movzwl 0x4(%ebx)`), even though +0x04 is a 32-bit
field — the field only ever holds 0..5, so the truncation is invisible, but it
is in the object and it is reproduced.

## Codegen

`make similarity` reaches this translation unit — the header's claim that it is
one of the fifteen the period toolchain cannot compile is stale, and the
current build is 184 objects with 0 failures.  Our `getV90Decision` is 8,485
bytes against the blob's 8,379 and matches on 56% of its mnemonic sequence.
What is left is `je`/`jne` pairs with the arms the other way round, extra
`mov`/`shl` in the table-index addressing, and scheduling — all of it on
CLAUDE.md's "free, so ignore it" side.  Two forced differences were found and
acted on: `abs` and the sign of the sample, below.

`abs()` is `cltd; xor %edx,%eax; sub %edx,%eax`, and `x < 0 ? -x : x` is a test
and a branch under this compiler — 52 spurious `js` and 51 spurious `neg`
before the macro reached for `__builtin_abs`.  The sign of the sample is
`sar $0x1f; or $0x1`, and `x < 0 ? -1 : 1` is again a branch — 41 `sar` and 16
`or` missing, and all ten `imul` of a table entry by that sign gone with them.

## Deviations noticed

- D321 — the default path returns an uninitialised register.
- D322 — case 2 calls `jd->unPackReset()` with no null check, where case 3
  reaching the same code does check and diagnoses "ERROR: Null JdDetector".
