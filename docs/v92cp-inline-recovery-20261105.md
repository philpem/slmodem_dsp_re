# V92CP byte float expansion: one exact recovery

`float2Bits(float, unsigned char *, int)` becomes EXACT at its native 125
bytes. The complete V92CP TU moves from 11 to 12 EXACT functions out of 15,
with no EXACT or REGALLOC losses. Only this function's canonical body changes.
`bitsToInfo` remains SIZE33; no caller source candidate is retained.

The baseline is `4317aa3ed`. The unchanged full TU was rebuilt through the
experimental path and reproduced the current production object literally.
Commands retain the Gentoo period configuration, C++ fast-math options and
final `DSPLIB_REPRODUCE_BUGS` define. Compiler commands, source/object hashes
and RTL dumps are under this checkout's `build/v92cp-inline/`.

## Native boundary and source recovery

At native `0x4ec00`, the mode dispatch shares one default/no-match return.
Within each greedy loop, one table coefficient stays on the x87 stack across
the byte output store: FCOM then the subtraction consumes that coefficient.
The old source allowed a reload after the store and emitted FCOMP/FSUBS.
The writable tables can be addressed through an unsigned-char output pointer,
so the read boundary is a source property rather than a harmless reload.

The retained source uses a switch, captures one float coefficient per iteration,
and places subtraction in the negated whole-predicate arm. This preserves
unordered routing; an ordered opposite comparison was not substituted.
Initialising the integer counter before cursor acquisition recovers the native
entry operations in both branches.

The initial eight-cell cross covers coefficient capture, switch dispatch and
predicate-arm ownership. Combined capture/switch/negated-arm gives BYTES17.
A direct-output-pointer spelling is identical and supplies no gain. Two finite
six-order domains then cover independent fabs/cursor/counter initialisations:
the mode-0 domain leaves only a mode-1 ADD/XOR order difference, and three of
six mode-1 orders become exact. This is a supported family, not unique original
source spelling. No register binding, attribute, barrier or compiler option
was introduced.

An explicit four-cell interaction record groups A as switch/counter-cursor
entry structure and B as coefficient capture/negated subtraction arm:
baseline SIZE22, A SIZE6, B SIZE19, A+B EXACT. The immutable comparison record
is `build/v92cp-inline/ledger/v92-byte-float2bits/`. Period evidence is imported
with hashes and its actual commands in that record; command-associated ledger
period fields remain not-run rather than claiming an unexecuted gate.

## TU and differential validation

All exports are preserved. Masked allocated data payload/layout is unchanged,
and all 17 relocated code-table entries retain exact function-relative targets.
Raw data bytes move with text placement; raw nontext equality is not claimed.
The other fourteen canonical bodies are unchanged, including `evaluateCRC`.

The selected candidate passes five existing Gentoo fixtures:

| Fixture | Checks |
| --- | ---: |
| `t_dilfreefns` | 167 |
| `t_v92cpcrc` | 4,107 |
| `t_v92cpb2i` | 31,905 |
| `t_v92cpeval` | 1,939 |
| `t_v92info` | 883 |
| Total | 39,001 |

The direct float2Bits group contributes 61 checks over fifteen finite values,
modes zero/one/two and guarded output storage. Mode two must write nothing.
This is exported-component coverage, not modem lifecycle coverage. The other
fixtures check TU collateral and existing receive histories/synthetic probes.

Each executable's selected 125-byte implementation is independently checked:
113 literal bytes plus all three absolute relocation fields. Named table
addresses are resolved against the linked symbol table; the anonymous zero-float
pool target is checked against the captured constant payload. All five bodies
match the selected candidate. Adding the explanatory source comment reproduces
the candidate object literally.

A known wrong `*p = 2` control fails 26 of the direct group's 61 checks while
the other groups pass. It is one selected negative control, no exclusions;
there is no mutation-campaign claim. All 212 existing anchors in six affected
suites remain valid without reanchoring.

## Closed caller experiment

Native `bitsToInfo` retains a byte-sized bits-per-symbol value across the
switch and rebuilds its quantum after reset. Three bounded source cells test
byte capture and product recomputation. None gains EXACT or REGALLOC. One
matches the native body length but changes over a thousand raw bytes through
spill and allocation effects, so it is not retained. The promoted sixteen-word
CRC graph already agrees; the next missing evidence concerns caller scalar
homes/lifetimes, not another CRC polynomial or narrowing spelling.

Initial predicate-generator multiplicity and ledger-import setup errors were
preserved as invalid apparatus attempts; the reported matrices completed with
corrected generators and a raw-equal baseline. No fixture, header, compiler
flag, mutation anchor or ratchet is changed by this proposal. Full phase and
whole-tree census are the parent's integration checks.

## Integration

The combined V22/V92 candidate passed `make phase J=8`: 399 period tests,
zero failures, plus all structural checks. Final census: 1,133 EXACT and
28 REGALLOC out of 1,852, with no EXACT or REGALLOC losses. V92CP masked
allocated payload/layout and all 17 strict code-table targets are unchanged.
