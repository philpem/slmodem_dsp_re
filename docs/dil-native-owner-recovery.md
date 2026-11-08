# DIL indication: recover the native owners

`V34XF_IndicateDilReceived` now preserves both native owner pointers across
its diagnostic call. Astra's independent native-code audit identified this
witness; the delivery work independently checks the instructions, period
layout, differential behaviour, translation-unit collateral and partial link.
The baseline is `cc6377b2c`.

## Native evidence and addresses

The original function starts at `0xa2f0` in `ref/slmodemd/dsplibs.o`.
It forms `obj+4` at `0xa30f` and `obj+0x221c` at `0xa316`, before the
conditional diagnostic call at `0xa37f`. It stores receiver state 6 through
the first base at `+0x248` (`0xa323`) and clears the segment counter through
the second base at `+0x3a4` (`0xa330`).

Both types already exist. The recovery uses a `v34_engine` view beginning
at `obj->progress` and the existing `obj->tx` member. No layout or field type
changes. A separate Gentoo GCC 3.4.2-r2 layout probe confirms every changed
access:

| Access | Before | After | Width |
|---|---|---|---:|
| Receiver state | object `+0x24c` | object `+0x4` + engine `+0x248` = `+0x24c` | 4 bytes |
| Segment counter | object `+0x221c` + tx `+0x3a4` = `+0x25c0` | same nested offset through `txb` | 2 bytes |

The six-line source change retains the same two stores and constellation
selection. Its new evidence is the two distinct native bases and their
lifetimes, rather than another spelling of the final conditional stores.

## Gentoo instruction measurements

All compilation uses Gentoo GCC 3.4.2-r2 and the normal period flags. All
partial links use the same image's period binutils. No modern or x86_64
compiler result is used.

| `V34XF_IndicateDilReceived` | Bytes | Instructions | Longest run | LCS / 36 |
|---|---:|---:|---:|---:|
| Original | 150 | 36 | — | — |
| Baseline | 101 | 24 | 6 | 13 |
| Recovered owners | 133 | 30 | 12 | 27 |

The function remains `SIZE`, 17 bytes short. LCS is a measure of ordered,
register-canonicalised instruction correspondence, not an equivalence proof.

All 56 functions defined in this translation unit are compared. Its 30
previously EXACT functions remain EXACT. Only DIL and `k56FlexPhase34` change
raw body bytes. The latter retains 703 bytes and 155 instructions; its only
changed byte is function `+427`, the displacement of a direct local call.
The call moves from `0x2e1a` to `0x2e3a` and still targets
`getMPrecvdBits` at `0x17e0`. The other 702 bytes and all 13 relocation records
are identical. Its native-comparison run 13 and LCS 91/160 are unchanged.

The non-text data review finds 67 changed 32-bit words in `.rodata`, all
relocation addends targeting `.text` and all increased by exactly 32.
Their relocation records are unchanged. The other allocated file-backed
data sections are byte-identical; no coefficient, string or table content
change is introduced apart from these moved code addresses.

The clean baseline build produces 300 objects from 300 sources with zero
failures. Every rebuilt baseline object is SHA-256-identical to the pinned
baseline object set. The final whole-tree census and EXACT-name listing are
identical to that baseline: **1,143 EXACT, 35 REGALLOC, 84 BYTES and 590 SIZE**
over 1,852 shared symbols. No EXACT symbols are gained or lost.

## Differential boundary

The existing `t_v34pcmif` passes before and after the recovery under Gentoo:
one selected binary passed, zero failed, with 61,492 checks over 37 sections.
The two phase-3 indication sections report 59 and 100 checks respectively.

DIL's direct probes use constellation bytes `0`, `1`, `2`, `0x7f`, `0x80`
and `0xff`, with diagnostics disabled and enabled. Two further calls occur
in component indication histories at debug levels 0 and 1. These comprise 14
direct DIL calls. The fixture fills the prior object with a sentinel and
supplies valid storage; DIL reads no prior modem state before these stores. This is a
synthetic component-field probe and component call history, not evidence of
public modem negotiation reachability. No fixture or tolerance is changed.

The full Gentoo-routed `make phase J=4` gate passes: **394 period differential
binaries passed, zero failed**, followed by the successful structural phase
boundary. Local `gcc` and `g++` dispatchers route structural compilation
through the Gentoo container; the parameter/layout check uses
`-Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-fno-exceptions -fno-rtti -DDSPLIB_REPRODUCE_BUGS`. The dispatcher log records
455 Gentoo `gcc` and 148 Gentoo `g++` calls, with no `-m64` argument.
The separate upstream checkout is unavailable, so the gate explicitly
reports checking its seven files against the manifest only.

## Whole-object comparison and binding invariant

Both versions partially link all 300 manifest entries in the same recovered
order. The standard `tc_out` flags already include
`DSPLIB_REPRODUCE_BUGS`; the before and after links use those same flags and
the explicit Gentoo image.

| Partial-link measure | Before | After |
|---|---:|---:|
| Exact section descriptors / 92 | 76 | 76 |
| Positioned bytes / 943,398 | 72,280 | 72,132 |
| Exact relocation records / 18,317 | 999 | 993 |
| Ordered relocation matches | 989 | 983 |
| Exact defined-symbol records / 2,931 | 457 | 457 |
| Candidate file-backed size delta | −16,478 | −16,446 |
| Binding mismatches / 2,509 shared defined names | 0 | 0 |

The strict semantic and raw completion gates both fail before and after:
`partialcmp.py --require-exact --require-raw-exact` returns 1. The positional
relocation count decreases by six and positioned byte matches decrease by
148. These results remain visible. Independently inspecting the relocation
records finds 18 lost reference-position matches and 12 gained matches. All
18 lost records reappear at offset `+32` with every other field unchanged;
all 12 gained records existed at offset `−32` before the function grew.

The live `partial_metrics.py --check` gate passes with measured binding counts
`0 -> 0`; positional counts are informational in its implementation. Both
objects agree on all 2,509 shared defined-name bindings, with nine
reference-only and 28 candidate-only names. No comparator or ratchet is
changed.
