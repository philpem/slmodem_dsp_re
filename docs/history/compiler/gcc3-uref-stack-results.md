# Uref: known-callee alignment closes the frame residual

Base f2cdb66a (merged PR264); isolated branch, no structure/layout changes.
Issue22 read-only. No mutation or fuzz execution. The other session owns
structure inventory and PR263.

## Evidence and crossed reproduction

F11796 recovered every non-stack instruction of updateUref but left the
0x14 frame versus original0x10. Read-only GDB observations of the installed
Gentoo cc1plus show identical allocations in both source inputs: two HI2
controlword slots, a QI1 saved-code slot, then a zero-size BLK alignment
request, ending at8 local bytes. There is no extra four-byte local.

GCC's frame calculation adds8 outgoing bytes and4 alignment bytes. The
known, already-emitted unitePhasesInfoOfUref callee advertises128-bit incoming
alignment, propagated through cgraph_rtl_info. Its retained nanf("") library
call requires that alignment. The blob instead materializes quiet NaN
0x7fc00000 directly, with no corresponding call. The period math.h NAN macro
expands to __builtin_nanf("") and supplies that same payload. This restores
32-bit known incoming alignment without altering optimization flags or
function order. All four allocation requests remain identical.

| Callee initializer | Uref half addition | TU exact/34 | Uref verdict |
|---|---|---:|---|
| nanf("") | float0.5f |15|BYTES86|
| nanf("") | double0.5 |15|BYTES10|
| NAN | float0.5f |15|BYTES81|
| NAN | double0.5 |16|EXACT240B|

Neither factor alone closes Uref. This is a crossed source recovery grounded
in independent constant-versus-call and expression-mode witnesses. It does
not uniquely identify the author's exact NaN spelling. Adopt only the last
cell. The callee itself stays BYTES280, its historical register/x87 residual;
F7848's720 declaration permutations remain closed. The later F11201
builtin-to-library rewrite explains why the new call witness was absent from
that earlier investigation.

## Complete object audit and limits

Four crossed TUs reproduce baseline raw; every symbol binding/visibility,
named-data identity and BSS is retained. NAN adds exactly two SF quiet-NaN
pool entries; diagnostic string values are unchanged, with reordered pool.
All other allocated non-text bytes are unchanged after separately auditing
seven relocated table destinations. Four destinations shift with the changed
inline studyUrefHandler body and still reach unique instruction boundaries.
The double addition also changes studyUrefHandler's inline copy, size4940 to
4924, which stays nonexact (blob SIZE395 becomes SIZE411). This is recorded,
not concealed by the standalone gain.

Only three actual bodies change in the winner: Uref, its callee, and that
inline consumer. Fifteen raw body comparisons differ because constant-pool
addends move; the other twelve have identical relocation-masked instructions
and identical typed relocation identities. Existing UNRESOLVED anonymous-data
comparisons remain unresolved; the production comparator is untouched.

A bounded screen covers all four remaining nanf("") initializers in src/include.
Four ADID second-initializer controls add no gain; four designer pair controls
also add no gain (13/24 exact each). Original constant/no-call regions improve,
but whole bodies do not close. They remain diagnostics and are not adopted.
Designer audit verifies all23 canonical bystanders per cell and accounts for
one NaN constant and, only with both initializers, removed nanf import/empty
argument string. ADID second plus first removes the corresponding import too.
No further NaN spelling sweep is justified.

## Installed compiler observation

The observer uses the actual driver-preprocessed TU and cc1plus command,
not a reconstructed argument list. The binary SHA256 is
b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d.
DWARF confirms32-bit long/entry ABI. Software breakpoints read entry cdecl
arguments and finish values; no inferior calls, cursor forcing or state writes.
For each of four cells, driver assembly, unobserved host assembly and observed
assembly agree raw. Reassembly with the selected executed Gentoo assembler
agrees with the driver object raw. Each cell yields4 target allocations,
15 frame layouts, and two known-callee alignment lookups. The independent
frame arithmetic replay validates60 layouts and refuses missing target,
wrong final allocation and corrupt callee boundary controls.

The first observer was invalid: it read the wrong CONST_INT union member and
rejected a legal zero-size BLK request. Its incomplete report is preserved as
stack-observe-invalid-first.json and excluded. Corrected observations retain
explicit errors/completion denominators. Frame layouts include intermediate
reload calculations; their identical arithmetic is checked, while final
reload completion is required rather than assumed for every invocation.

Official GCC3.4.2 sources explain the observations:
[frame arithmetic](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.c),
[callee boundary propagation](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/calls.c),
[compiled-callee availability](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/cgraph.c),
[local allocation](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/function.c).
These sources corroborate the installed-binary measurements; they are not
claimed to reproduce every Gentoo patch byte.

## Replay

Run from repository root, with Docker,32-bit host execution, GDB Python and
pyelftools available. First build and archive all300 objects plus .build-config
at pinned f2cdb66a in build/production-before. A fresh replay must use that
archive, not the newly changed production objects. Every source reproducer pins f2cdb66a and appends
DSPLIB_REPRODUCE_BUGS after configurable flags. Complete driver/compiler/
assembler commands and versions are retained in build artifacts.

```sh
python3 tools/gcc3_uref_stack_reproduce.py --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_uref_stack_observe.py
python3 tools/gcc3_uref_stack_cross.py --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-uref-stack-cross
python3 tools/gcc3_uref_nan_second.py --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_uref_stack_audit.py
python3 tools/gcc3_uref_designer_nan_reproduce.py --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_uref_designer_nan_audit.py
```

Fourteen valid driver inputs:2 initial,4 crossed,4 second-initializer and4
designer, including repeated baselines. The two final full-object audits
cover12 TUs,368 strict body verdicts. Observational host runs are additional
raw controls, not extra independent source cells. Static anchor retargeting
preserves the existing rounding fault; no mutation harness runs.

## Next discriminator

Screen near matches whose non-stack instructions already agree, trace actual
slot requests and known-callee boundaries, and distinguish padding from locals
before any source hypothesis. A frame delta alone does not identify a profile
or a dead local. This mechanism can unlock another caller only where the
blob independently witnesses a different callee call/constant family. Keep
structure cleanup separate and retain closed negative domains.

## Production gate

Strict whole-tree census1045→1046/1852;113169→113409 exact reference bytes,
one gain240B and zero exact losses. The changed ADID object equals the audited
cross winner raw;299/300 other production objects and .build-config unchanged.
Final make phase J=4 exits0:388 period differential tests passed/0failed,
structural checks green.285 suites/10038 static mutation anchors remain unique;
no mutants or fuzz cases executed. FindingF11801 records this final boundary.
