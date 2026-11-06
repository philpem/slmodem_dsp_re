# V.34 baud-cap recovery, 6 November 2026

`chkForceBaudRate` advances from SIZE5 to REGALLOC at the native 389-byte
extent. The census becomes 1,146 EXACT / 29 REGALLOC / 71 BYTES / 606 SIZE over
1,852 functions: one REGALLOC gain and no EXACT loss. The
[evidence ledger](evidence/baud-cap-recovery-20261106.json) records the source
controls, input identities, whole-TU effects and validation.

## Coupled source properties

Native code captures the session pointer at `0xa479`, before the diagnostic
branch. It initialises the six-byte local array before reading configuration,
then acquires the separate `obj+4` owner at `0xa490`. Its cap predicates at
`0xa4e7..0xa50b` use unsigned JBE/JA. The reconstructed source previously
loaded configuration before initialising the array, fetched the session after
the diagnostic, used the complete-object receiver members directly and consumed
the cap through signed comparisons.

| Control | Grade |
| --- | --- |
| Baseline | SIZE5 |
| Unsigned comparison consumption alone | BYTES272 |
| Early session capture alone | SIZE5 |
| Both | BYTES264 |
| Both plus the initial interior-owner view | BYTES264 |
| Both plus late acquisition of that view | BYTES55 |
| Both plus configuration load after array initialisation and late owner acquisition | REGALLOC |

An explicit copy of the output-bank parameter has no object effect and is not
retained. No flag, artificial carrier, initializer permutation grid, extra
operation or declaration change was used. Both earlier source and selected
source already emit MOVZBL, SHR AL, MOVZBL for the cap transport; a byte-local
change is unnecessary.

The selector is an unsigned configuration byte shifted five, so its domain is
0..7. Unsigned comparison consumption matches the native instruction semantics
without proving a unique declaration or a negative-selector bug. Session
capture is source-observation recovery: the actual diagnostic host only records
output and cannot replace the session pointer.

The existing member-derived access helper preserves the receiver addresses and
32-bit widths. V.90 remains `obj+0x24c = (obj+4)+0x248`; K56Flex remains
`obj+0x250 = (obj+4)+0x24c`. The session pointer is still at `obj+0x3548`, its
allow bytes at `session+0x217`, and output shifts remain 16-bit members at bin
stride 44, offset 14. Eighteen Gentoo layout assertions independently check these
and the associated configuration/session layouts. The view does not identify a
unique original engine type.

## Complete TU and compiler boundary

All 56 emitted functions have native counterparts; there are no extra emitted
helpers. Fifty-four bodies are literally unchanged. The target changes as
above. `k56FlexPhase34` remains SIZE28: its only body-byte change is the local
call displacement at `+0x19b`, still targeting `getMPrecvdBits`, because the
caller moves by 16 bytes. This is a placement effect rather than a new
instruction or register choice. No previously EXACT or REGALLOC body regresses.
All 300 objects were checked:299 are literally unchanged and only
`VPcmV34Main.cpp` changes.

Exports and symbol order remain unchanged. All 89 relocated rodata words retain
the same function owner and intrafunction offset; 67 raw addends move by 16 bytes.
Relocation-zeroed data identity does not mean raw rodata identity. The other five
allocated noncode sections remain byte-identical.

The selected ordinary, dumped, replayed, observed and integrated TU objects
agree at SHA256
`42f930b602e1341e72ab90f86bfd3d06b2ac12dfa56fdce608ae45180ba3e6d7`.
The residual scratch forms first appear at peephole2; 14 observed register-rename
decisions explain the final lanes. This identifies the current compiler
boundary, not a proof that no original source property could change its inputs.
Reopen with a separately supported predecessor, lifetime or operand-property
recovery that changes that stream. Another register-fitting spelling palette
is not supported by the present evidence.

The owner graph screen admits 49/56 pairs and refuses 7; the stack screen
admits 12/56 and refuses 44. These counts are unchanged after recovery. For this
target, the owner layer pairs one anchor and compares zero relations; the stack
layer refuses the CLD-prefixed frame. Its clean owner result cannot establish
that the independently observed source differences were absent or equivalent.

## Differential domain

The existing 800-case sweep remains synthetic component exploration. It drives
receiver and allow-list values beyond the producer-supported examples and does
not demonstrate negotiated modem reachability. The new 144 cases cross eight
cap selectors, two low-bit patterns, three ordinary receiver arms and debug
levels 1/2/3. They use producer-supported allow values and initialised bins,
comparing the complete projected root/session state, expected bin writes,
configuration immutability and diagnostic bytes, calls and boundaries.

The session backing is a partial 0x6200-byte fixture rather than a constructed
0x7f68-byte VPcmFloModem. The method consumes only the backed allow region;
this establishes a bounded component-method input domain, not constructor or
full modem/audio lifecycle coverage. The logger has no reentry hook. No
negative cap, fabricated callback or old-fail behavior claim is introduced.

The expanded test passes on both old and selected source: 38 groups and 63,365
checks, including the new 144 cases/1,873 checks. The full `make phase J=8` passes 399 period tests with zero failures and all
structural checks. Citation checks are rerun after the documentation update. Mutation infrastructure
is not used.

## Next transfer

Investigate `VPcmV34InitiateHangUp` in the same TU. Native captures `p3548` at
`0x6be1` before diagnostics and clears the three rate members through `obj+4`.
Current source loads the session after those publications and the status test.
Native also stores `sess+0x173e` at `0x6c0f` before loading its demodulator at
`0x6c16`; current generated code reverses those two operations. Cross the
early capture/interior view with the independently audited publication ages,
checking aliases, fixture domains and complete TU effects before interpreting
an instruction-age difference as observable behavior. No experiment on this
next target is claimed by this wave.
