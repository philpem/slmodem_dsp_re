# Waves 32–33: discriminate alias representation from compiler profile

Base: `c559cb89e5a9a595574cac07737049b1ed0ed831` (PR #412).
Issue [#409](https://github.com/philpem/slmodem_dsp_re/issues/409) records both
domains before compilation. These are diagnostic controls; no reconstruction
source, header or compiler option is adopted.

## Wave 32: cross independent translation units

The prior INFO0 experiment restored a missing final session read by withdrawing
strict alias analysis. That observation alone cannot choose an original profile.
This cross adds unchanged `V34RX.c` and `V34hshak.c`, retaining both captured and
saved live/direct-explicit INFO0 sources. Each is compiled with retained flags
and with `-fno-strict-aliasing`: eight complete objects, 156 emitted-body verdicts.
Of those verdicts, 110 have reference symbols; 46 are the handshake TU's 23 local
helpers emitted in both profiles. Every unchanged retained control reproduces
production: 7 INFO, 12 RX and 52 handshake bodies, 71 distinct bodies total.

| TU/source | Bodies changed by option | Relocations retained / withdrawn | Grade-0-or-1 gain / loss |
|---|---:|---:|---|
| INFO captured | 4 / 7 | 133 / 133 | none / none |
| INFO live | 4 / 7 | 133 / 133 | none / none |
| RX unchanged | 6 / 12 | 88 / 88 | none / none |
| Handshake unchanged | 38 / 52 | 2333 / 2315 | none / `ApplyBulkDelay` |

Symbol identities, bindings and section ownership remain unchanged in every pair.
Raw non-text payloads change in live INFO `.rodata` and in handshake `.rodata`,
`.rodata.str1.1` and `.rodata.str1.4`; these are recorded, not normalised away into
an assertion of complete-object identity. The stored ledger retains every raw
relocation record and section payload hash. Equal relocation counts elsewhere
do not establish equal relocation positions or targets.

### Independent native boundaries

`V34RX.c` explicitly models the 32-bit energy accumulator and its 16-bit high
half in a union (`v34recv.h`, receiver `+0x12c/+0x12e`). Both `V34agc` and
`V34demodulate` remain body-identical across the option. This witness does not
corroborate a need for the option.

`ApplyBulkDelay` is grade 1 under retained flags and loses it with the option.
The native and retained versions have 103 displayed instructions; the withdrawn
version has 99. Native `0x5ddc8` loads `dsplibs_debug_level` and `0x5ddf1` reuses
that register after the short `far_echo_enable` store at `0x5dddf`. The withdrawn
candidate compares the global in memory again after its short store (candidate
`0x6f7d`). The native/retained version has 17 relocations in this body, versus 16
in the withdrawn version. Counts are descriptive; the decoded operands establish
the different reuse boundary.

This conflicts with selecting the global option from the INFO0 reload on the
current source. It does not exclude a different original profile together with
different original declarations. Existing exact matches may compensate for
source representation; the comparison is not a monotonic-profile selection rule.

## Wave 33: test a source-level alias representation

A new four-cell diagnostic crosses the saved live INFO source with two read
representations and two compiler profiles. The existing `session_int` uses an
`int *` read. The diagnostic form reads the integer member through a union view
containing `int whole` and `short halves[2]`. This deliberately tests compiler
alias modelling; it is not a recovered original union declaration or a claim that
the underlying C++ session object had that layout.

| Read representation | Retained flags | Withdrawn strict aliasing |
|---|---|---|
| Integer pointer | 758 bytes, 118 differing bytes, 3 session reads | 774 bytes, 4 reads |
| Union view | 758 bytes, 110 differing bytes, 4 session reads | same 774-byte body, 4 reads |

All four complete objects compile, yielding 28 function verdicts. Both integer
controls reproduce the preceding seven-function controls. Under retained flags,
the union read carries alias set 0 in `07.addressof` and remains a memory read in
`08.gcse`; assembly has the fourth session `+0x6120` read. This supplies a
source-representation explanation for the reload without a global option. A
nearer byte count is not an adoption criterion; the union is not independently
witnessed as original source, and the candidate remains non-exact.

Only `V34GiveINFO0dBits` changes between retained integer and union forms. The
other six bodies, symbol identities/bindings and non-text payloads are unchanged;
both objects have 133 relocations. Grade-0-or-1 membership remains three names,
with no gains or losses. With the option withdrawn, all seven bodies match
between read representations. Independent read-only review checked the stored
ledgers, original disassembly, candidate instructions and RTL claims.

The result strengthens the distinction between original compiler-profile evidence
and source alias modelling. Further work needs an independent original ownership,
declaration or interface witness. Repeating nearby union/cast spellings to improve
the score would not supply it. The preceding INFO0 families remain closed.

## Reproduction and limits

Artifacts under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/` include
`w32_cross.py`, `w32-cross.log`, `w32-cross/results.json`, `w32_review.py`,
`w32-review.log`, `w32-bulk-{native,retained,withdrawn}.s`, `w33_union.py`,
`w33-union.log`, `w33-union/results.json`, `w33_review.py` and `w33-review.log`.
Both runners read production `.build-config`, translate include paths only,
append `DSPLIB_REPRODUCE_BUGS` through the shared helper, and print full commands
and the executed Gentoo compiler/assembler identities. Sources, objects, RTL,
saved assembly, symbol bindings, section hashes and relocations are preserved.
Gentoo GCC is 3.4.2-r2; assembler is 2.15.92.0.2 (20040927).

These experiments do not change production execution or claim new exact names.
`make phase J=8` passes 394 period differential binaries with zero failures and
all structural checks passing (`w32-33-phase.log`, lines 528/530). The session
handover records delivery details.
