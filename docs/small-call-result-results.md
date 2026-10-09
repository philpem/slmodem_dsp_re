# Small direct-call screen and V90 terminal-control recovery

Baseline: merged PR281, `511a7c14`. Same Gentoo GCC 3.4.2-r2 image,
complete `.build-config` flags and headers; `DSPLIB_REPRODUCE_BUGS` enabled.
The compiler-selected assembler was executed: GNU assembler 2.15.92.0.2.
No optimization flags, ABI, types, constants, register constraints or layouts
were changed. The [declared domain](small-call-result-domain.md) precedes the
crossed compilation.

## Screen

`tools/small_call_result_screen.py` compares named PC32 E8 calls and E9 jumps,
retaining canonical relocation targets and addends. It nominates a function
only when total named direct-transfer multisets agree and an original ordinary
call became a retained sibling jump. It does not infer source syntax or cover
indirect calls. Counts are emitted copies, not the whole-tree unique denominator.

At the baseline: 300 TUs, 1886 shared emitted bodies, 1102 exact copies excluded,
550 small nonexact bodies (original size <=650 bytes), 41 unequal named-transfer
multisets excluded, five nominations. The historical RX-rate baseline is a
positive control; its current exact body is a negative control. The three
allocator wrappers are previously bounded ABI/prototype residuals and are not
reopened: K56FLEX_Create/Delete and _iir_filter_delete. In particular, Create's
four-argument caller and return use prohibit changing its prototype merely to
suppress a sibling call. The source already guards _iir_filter_delete.

## V90Modem::setSessionFlag

The original 71-byte function stores the flag on every path. Its digital arm
jumps to V90Modulator::setSessionFlag; its analog arm calls
V90Demodulator::setSessionFlag and cleans up the stack. A side load scheduled
before the distinct sessionFlag store does not establish an explicit capture
in the original source. The retained extra capture is crossed with a previously
examined switch terminal shape rather than retesting that switch alone.

| Side dispatch | Terminal structure | Result | 02.sibling markers |
|---|---|---|---:|
| Explicit capture | if/else | 64 bytes, SIZE 7 | 2 |
| Direct member | if/else | 64 bytes, SIZE 7 | 2 |
| Explicit capture | switch, digital return / analog break | 71 bytes, BYTES 7 | 1 |
| Direct member | switch, digital return / analog break | 71 bytes, EXACT | 1 |

All four initial RTL streams have two call_placeholders. Inspect 02.sibling
instead of treating the initial alternatives as the final calls. Both switch
cells recover the asymmetric ordinary/sibling transfer. Direct member dispatch
also removes the byte mismatch, including register operands. This identifies
a working source family, not a uniquely recovered spelling or compiler profile.
The direct dispatch reads the same field with no intervening call; the flag
store cannot alias side. Neither axis is sufficient alone.

Every other V90 function remains canonical-byte-identical (7/7), including the
nonexact reset and progress bodies. Exact count rises 5/8 to 6/8 with no losses.
Bindings, visibility, imports/exports, allocated nontext data, BSS, and nontext
relocations are unchanged; later text positions move with target length.

## DialerAbort negative cross

The four controls cross signed/unsigned progress guard and early-error return /
structured if/else common exit, preserving all callback and store ordering.
All remain 147 bytes against the original 145. The unsigned comparison recovers
the original guard but does not close the function. The common-exit forms are
raw whole-object-identical to their respective early-return controls, with two
02.sibling markers in every cell. All four initial streams have four
call_placeholders. All five bystanders and metadata/data remain unchanged.
No Dialer source edit is adopted; negative progress states gain no reachability
claim. Close this finite domain without inferring that every source preimage
or possible lever is exhausted.

## Reproduction

Build the baseline production cache and census before screening. The screen's
historical positive object can be regenerated using
`tools/v34_rxrate_lifetime_reproduce.py`; use `--positive-object` and `--census`
for alternative artifact locations. Screening and compilation are separate.

```
python3 tools/small_call_result_screen.py
python3 tools/small_call_result_reproduce.py --domain docs/small-call-result-domain.md
python3 tools/small_call_result_audit.py
```

Compilation is pinned to the baseline source revision. Eight complete-TU cells
produce 56 function verdicts. Both raw baselines reproduce; the audit asserts
all bystanders, metadata/data, final sibling counts, the exact positive and
negative misses. Commands, source/header/object hashes, compiler logs, RTL and
assembly are preserved under `build/small-call-result/`; the screen writes its
own denominator-bearing JSON. The production source must reproduce the winning
complete object before adoption is reported.

Measured production census: **1077/1852 EXACT**, 118337 original exact bytes,
up one function / 71 bytes with zero losses. Remainder: 673 SIZE, 66 BYTES,
31 REGALLOC, 5 UNRESOLVED. The existing 1074-name ratchet passes unchanged.
Production Gentoo builds 300/300 objects, zero failures; the full V90Modem object
is raw-identical to the winning control.

The initial gate reports period differential 388/0 but correctly fails six
static locators detached by the dispatch rewrite. Retarget all six in the same
function, keeping five behavioral operation labels; reverse and rename the
already-equivalent read-order control to capture-before-store. No mutation
execution or snapshot rerecording is claimed. Preserve the initial red log
separately from the final repaired gate.

Final repaired `make phase J=8`: **388 passed, zero failed**, all structural
checks green; 14417 references / 2984 findings, 285 static suites / 10038 anchors,
zero detached/nonunique/wrong-arm anchors. Modern portability is not claimed.
