# V34 final rate source recovery

`setfinalrate` reads negotiated fields at their points of use and selects the
two symbol-rate ladders through conditional chains. This recovers original
read boundaries and improves ordered instruction correspondence without
changing the retained Gentoo compiler profile.

The comparison base is `cc6377b2c6352923e4ad9f092cb905d0d3c4738a`.
The reference function starts at `.text+0x5ecb0` and occupies 905 bytes.
All compilation uses GCC 3.4.2-r2 from
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, with the repository's
`tools/toolchain/period.mk` flags and `DSPLIB_REPRODUCE_BUGS`.

## Original read boundaries

The reference tests transmit carrier bit 2 at `+0xa9de` within each applicable
TX arm. The first test is `testb $4,0xa9de(%ebx)` at `0x5ed16`, after the
pre-emphasis and TX-rate `bitreverse` calls. The reconstructed `high` local
captured this bit before the ladder. It now reads the byte in each arm.

The RX-rate conversion reloads the unsigned word at `+0xa9e0` at `0x5ed47`.
The earlier reconstruction retained the word loaded before the first
conversion. Both initial INFO words are now read directly where consumed.
The byte offsets and access widths are preserved: initial `+0xa9de` and
`+0xa9e0` reads are words; carrier choices read the byte at `+0xa9de`.

The native ladders use comparisons against rate codes 0, 2, 3, 4 and 5.
Unsupported codes retain the previous rate, scale and carrier. All table
choices and diagnostics remain present.

## Crossed source controls

Read placement and ladder shape were tested separately before combination.
The final control also removes the captured carrier Boolean, as the native
branch-local tests require.

| Control | Instructions | Longest run | LCS / longer stream |
| --- | ---: | ---: | ---: |
| Base source | 171 | 9 | 101 / 175 |
| Direct INFO reads, switches | 165 | 11 | 97 / 175 |
| Captured reads, conditional chains | 187 | 17 | 129 / 187 |
| Direct INFO reads, conditional chains | 178 | 22 | 136 / 178 |
| Direct reads and branch-local carrier tests, chains | 175 | 22 | 143 / 175 |

The retained function is 923 bytes against the reference's 905; the base is
822 bytes. The absolute size gap falls from 83 to 18 bytes, and LCS rises
from 57.7% to 81.7%. Matching instruction count does not establish byte
identity. Register choices, scheduling and remaining branch/value sharing
still differ; no EXACT gain is claimed.

## Complete translation unit review

All 53 defined function bodies were reviewed. Three bodies change after
masking relocated fields: `setfinalrate`, `setupreceiver` and
`detectRetrainReq`. The latter two are compiler-state collateral in unchanged
source: their size gaps change from 59 to 4 bytes and from 2 to 10 bytes,
respectively. All nine previously EXACT symbols among the TU's 29
reference-named symbols remain EXACT.

Five additional functions have changed relocation addends:
`v34handshakinit`, `probe_preemp_shape`, `t3m_txblock`, `v34handshak` and
`setTimingStateParameters`. These changes must not be described as raw body
identity. The conditional chains remove the two rate-switch tables, totalling
12 `.rodata` relocations. The section shrinks from 1976 to 1944 bytes,
including alignment changes. All other 215 ordered `.rodata` relocation
destinations retain their function owner and interior instruction offset.

The two `probe_preemp_shape` references are biased bases for `bg[idx - 6]`:
the encoded addresses are 96 bytes before `bg`, with the index scaled by 16.
That bias is unchanged. Named data, including `bg`, have identical contents;
the moved encoded base does not denote a new table. Exported bindings and
visibility remain unchanged. Named DSP constants and coefficient tables retain
their contents; generated switch tables are removed as described above.

## Validation boundary

The existing `t_v34hshak` fixture tests `setfinalrate` directly on fully sized
component storage, compares state and table contents, sweeps the low INFO
byte and carrier choices, and compares live diagnostic transcripts. Its
arbitrary INFO combinations are synthetic component fidelity inputs; they
do not establish a negotiated public modem history. The fixture also covers
the changed `setupreceiver` collateral. Full-suite period validation covers
the other TU consumers.

The focused command is:

```sh
make period T=t_v34hshak J=4
```

It passes one fixture with zero failures. No new fixture or mutation campaign
is required for the observed source read and control-flow recovery.

The full reconstruction gate uses Gentoo for its structural compiler calls
too. `offcheck.py` and `assertlive.py` ordinarily invoke host `gcc`/`g++`;
temporary wrappers route those names to GCC 3.4.2-r2 in a Gentoo container.
The wrappers prefix the existing period compatibility header and pointer-size
define, allowing `assertlive` to override or undefine that define in its
negative controls. `CXXFLAGS` for the parameter-header syntax checks contain
only Gentoo-compatible apparatus flags. Reconstruction and fixture compiler
flags remain those of the normal period gate. No modern compiler is used.

The retained baseline and final object reproduce the crossed experiment's
audited outputs. The Gentoo-routed full `make phase J=4` passes 394 period
fixtures with zero failures and all structural checks. The layout check covers
2288 annotations. Complete comparison builds contain 300 objects; only the
V34 handshake object changes against the base.

The complete exact-name outputs are identical: 1143 EXACT, 35 REGALLOC,
84 BYTES and 590 SIZE among 1852 shared symbols, with zero RELOC/UNRESOLVED.

## Partial link remains different

Both complete 300-input links use the same recovered input order and Gentoo
binutils. The baseline substitutes only the unchanged-base handshake object;
the other 299 objects are byte-identical between the base and candidate.
`recoverorder.py` gives the same order with freshly generated attribution as
with the tracked attribution record.

| Strict partial-link metric | Base | Candidate |
| --- | ---: | ---: |
| Exact section records | 76 / 92 | 76 / 92 |
| Exact positioned relocation records | 999 / 18317 | 997 / 18317 |
| Exact defined-symbol records | 457 / 2931 | 457 / 2931 |
| Equal positioned bytes | 72280 / 943398 | 72236 / 943398 |

Both semantic and raw whole-object verdicts remain DIFFERENT. Relocation
records include section positions: the changed text and removed switch tables
move otherwise unchanged consumers. Nineteen previously matching records no
longer match and seventeen newly match; the net loss is two. This is disclosed
partial-link collateral, despite unchanged EXACT function membership. No
comparator, ratchet or floor is changed to hide it. The PR remains a draft for
review of the source recovery and this tradeoff.

Related investigation: [issue 22](https://github.com/philpem/slmodem_dsp_re/issues/22).
