# V.90/V.92 narrow-field screen

At `eccf0f2ae817cec09e93886ed7635604600a75dd`, a bounded screen of
five fields found **zero supported width or signedness corrections**. No
source or header change is retained. This closes these five nominations;
it does not establish that every V.90/V.92 field is correct.

The question was whether a narrow argument or use had previously been
mistaken for the width of its backing storage. The prediction was that an
independent full-width writer or reader would contradict a narrow member.
The falsifier was agreement of independent original accesses with the
current layout. No compiler variants were warranted after that screen.

## Original evidence

| Field | Original accesses | Result |
| --- | --- | --- |
| `V92Mapper::scale`, +0x00, `short` | `reset` stores `%dx` at 0x510b3; `process` loads with `filds` at 0x5111c | Both storage width and signed arithmetic agree |
| `V92Mapper::mode`, +0x02, `unsigned char` | `reset` stores `%al` at 0x510b0; `process` loads with `movzbl` at 0x5110a | Byte storage and unsigned read agree |
| `V92Mapper::bits`, +0x26, `short` | `reset` writes with `movw` at 0x510b8/0x510c8; `process` reads with `movswl` at 0x510ef | Both storage width and signed read agree |
| `V90Phase3Demodulator::short_400`, +0x400, `short` | `reset` stores `%bp` at 0x210e9; cross-class `V90Demodulator::progress` stores `%si` at 0x1dede; both decision methods use `cmpw` at 0x24e9a, 0x24f20, 0x22df4 and 0x22e8e | Independent writers and four readers agree on 16-bit storage; zero tests do not independently settle signedness |
| `V90Phase3Demodulator::short_414`, +0x414, `short` | `reset` sign-extends its ninth argument at 0x20fc1 and stores `%si` at 0x20fde | The store supports 16-bit storage. No current source reader supplies independent signedness evidence; no correction is justified |

The +0x400 cross-class writer was followed through the phase-3 pointer;
the displacement alone was not treated as an owner identity. The +0x414
argument's mangled `short` type concerns the call boundary. Its sign
extension cannot, by itself, establish the member's signedness.

The current layouts leave natural alignment space after +0x400 and +0x414.
Their offset assertions locate the following fields but cannot establish
that those gaps belong to a wider preceding field. Here the independent
16-bit accesses provide the relevant storage evidence.

## Reproduction and limits

Use `tools/dis.py ref/slmodemd/dsplibs.o SYMBOL`. Complete output for seven
original functions is saved under `build/recovery-fields-v90/`, with the
full mangled symbol as each `.txt` filename: both `V92Mapper` methods,
phase-3 reset and both decision methods, `V90Demodulator::progress`, and
`V92Modulator::reset`. The last was an additional reconnaissance read;
its two byte stores are outside the five-field nomination denominator.

Reference SHA-256:
`1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.
The checked header hashes were
`0d3ac36f941288b1fef3b7be707fd1bc91151adf0c15a8e3293e484802b5fb9d`
for `V92Mapper.h` and
`bbde7304cccbac86d8c1cecdff8860583bd0f7cac46b72e7d2bb0cd06b6afb68`
for `V90Phase3Demodulator.h`.

No experiment build, differential test, exact census or strict partial-link
comparison was run for this documentation-only negative result. No byte
identity improvement is claimed. The packer domains closed in
[the preceding report](recovery-packers-next-results.md) were not reopened.

Reopen a nomination when a new original access, ownership derivation or
callee boundary contradicts its current type. Nearby declaration spellings
or a smaller SIZE gap would not provide that evidence.

## Lesson for the playbook

Audit the backing storage independently of a narrow call argument. Record
each writer's actual store width, each reader's extension or arithmetic
width, and the ownership path for cross-class accesses before changing a
member. A sign-extended `short` argument does not prove signed member
storage; a narrow comparison does not prove a full-width field either.
Offset assertions constrain locations and leave type hypotheses open.
For this screen, matching independent writers and readers closed four
width nominations, while the fifth retained an explicit signedness gap.
