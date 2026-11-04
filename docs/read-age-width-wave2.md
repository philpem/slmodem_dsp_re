# Equalizer pointer copies and LowPassFIR read ages

The 2026-11-04 follow-up starts at `d7d7530c0` (PR #584). It retains two
native-backed source partials and the requested coefficient-member names.
Neither target reaches REGALLOC or EXACT. No compiler-profile or grader change
is made, and no new allocator callback or owner-alias lifecycle is claimed.

## Separate high- and low-word coefficient arrays

The V90Equalizer constructor makes two independent allocations at native
`0x3b7bd` and `0x3b7d3`. Each requests `2 * (linearEquLength + 8)` bytes; their
results enter owner offsets +0xd4 and +0xd8. Each pointer is aligned separately,
with aligned results published at +0xdc and +0xe0. The accesses have two-byte
strides. Coefficient reads combine a signed high word with an unsigned low word
as `(high << 16) | low`; writes split the integer back into the same planes.
Thus each coefficient is logically 32 bits, stored in two separate 16-bit arrays,
not one contiguous interleaved 32-bit allocation.

The member names now describe these demonstrated storage roles:

| Offset | Previous member | Current member |
|---|---|---|
| +0xd4 | `linearEquMmxCoefs` | `linearEquMmxCoefsHigh` |
| +0xd8 | `array_d8` | `linearEquMmxCoefsLow` |
| +0xdc | `linearEquMmxCoefsAligned` | `linearEquMmxCoefsHighAligned` |
| +0xe0 | `array_d8Aligned` | `linearEquMmxCoefsLowAligned` |
| +0xe4 | `linearEquMmxCoefsSkew` | `linearEquMmxCoefsHighSkew` |
| +0xe8 | `array_d8Skew` | `linearEquMmxCoefsLowSkew` |

These names express the recovered bit roles, not the author's original spelling.
Types, owner offsets, raw/aligned relationships and skew units remain unchanged.
All current source/header/test references change one-to-one; historical records
and reproduction scripts keep their historical names. A naming-only complete
Gentoo TU compile is raw identical to the baseline object.

## Entry pointer copies in linearEquFadeEdges

Native `V90Equalizer::linearEquFadeEdges` captures +0xdc/+0xe0 at entry
(`0x36981`, `0x36987`) and retains those values through float conversion/window
stores, reusing them at `0x36aa1`/`0x36aa5`. Previous source spelled the members
anew before the final split conversion and emitted a second load pair.

Two ordinary local short pointers now capture those members at entry and feed
both linear conversion loops. The DFE pointers, unsigned bounds, numerical
expressions, overlapping-window order and low/high split-store order are
unchanged. First RTL has one member-load pair; global allocation spills the
locals, and the final split loop reads the saved values rather than the members.
The experiment predicts a load epoch, not a preferred register assignment.

The target remains NONMATCH: 124 ordinary instructions versus native 129,
longest mnemonic run 22/129, unchanged from baseline. Existing `t_v90equ`
coverage includes 80 seeded component fade cases in fixed/float modes and
with overlapping windows. It does not prove a production owner-pointer-alias
history, and this recovery is not presented as a demonstrated behavioural fix.

## LowPassFIR member and per-iteration pointer epochs

In the five-argument `LowPassFIR<float>::design`, native code uses the published
`taps` member across calls rather than retaining incoming `nTaps`: after free
at section +0x10e, after allocation at +0x15c, after hamming at +0x16b, and for
the initial x expression at +0x73. Argument validation still uses incoming
`nTaps`. The source now follows those member observations for allocation sizes,
hamming's count and the initial unsigned-wide count conversion.

Native also captures `coefficients` before each sinc call (+0x93/+0x95 before
+0x9c), then consumes that same pointer for the iteration's multiply/store.
A local `T *coefs` expresses this directly. First RTL captures it before sinc;
the final candidate loads at +0x94 before the call at +0x9d and consumes the
captured base at +0xb3/+0xb6. Member-count reloads remain after the other calls.

Two cumulative cells test member counts, then the per-iteration pointer. The
second remains NONMATCH: 127 ordinary instructions versus native 126; longest
mnemonic run increases from 13/126 to 37/126. Template-T recurrence narrowing,
x87 normalization, NaN rejection, adoption, window copying and call targets
remain unchanged. Existing `t_lowpassfir`/`t_resampler` fixtures cover these
numerical and ownership obligations. No artificial allocator mutation or
replacement sinc is introduced to manufacture a behavioural difference.

## Complete controls and collateral

Complete unchanged TU controls reproduce the baseline objects raw, using the
configured Gentoo GCC 3.4.2-r2 flags and reproduction mode. The selected census
owns 53 functions in the two TUs; the complete objects contain **60 defining
copies** (35 equalizer, 25 Resampler), including additional weak copies. The
independent review covers all 60: only the two targets change; all **37 EXACT
copies**, non-executable data, raw string sections, bindings and resolved
relocation-consumer counters remain unchanged.

An older local canonical helper labels every non-`.text` section as data.
Consequently its initial LowPass cell report says data inequality: the sole
changed entry is the target's executable `.gnu.linkonce.t` section. Reviewing
actual ELF SHF_EXECINSTR flags distinguishes that code from data. This audit
changes no production comparison or proof rule.

Five focused period binaries pass: `t_v90equ`, `t_v90equnumeric`, `t_v90eqdata`,
`t_lowpassfir` and `t_resampler`. Final integration results are recorded below.
Controls, commands, hashes and dumps are in `scratch/readage-width-wave2/`;
Astra's admissions, exclusions and all-copy review are in
`scratch/readage-width-astra2/` in the owned worktree.

## Parked FP_Pow investigation

The native FP_Pow has a short post-incremented counter and two unconditional
signed-short `Fact_FP` reads. Finding F11450 already establishes that the six
stored entries are followed by a seventh read into `IIRFilterScales[0]`, value 3;
the source instead guards the six-entry table and substitutes that result.

Three controlled cells test a short counter, reproduction-only direct lookup,
and the old-value post-increment condition. They all remain NONMATCH, at 59,
53 and 58 ordinary instructions respectively versus native 55. Other TU bodies,
data and bindings remain unchanged. None is retained or represented as having
passed a runtime differential. The unchanged raw control reproduces the baseline object.

A future direct-read recovery needs a separately validated two-site memory
context that preserves the real six-short table and normal-mode repair. The
existing D302 binder admits one different, six-byte table and a -2 addend; it
must not be reused on these twelve-byte, zero-addend, two-read inputs by
relaxing its checks. No table padding, data-layout fiction or grader exception
is introduced to obtain a hit in this wave.

## Final local validation

`make phase J=8` passes all 398 period differential binaries and every structural
check. The five focused suites pass, with no added or weakened fixture.
Actual period and faithful objects are raw equal for 300/300 translation units;
only `V90Equalizer.cpp` and `Resampler.cpp` differ from baseline. Both final
objects are raw identical to the independently reviewed experimental candidates,
including after the member renames and explanatory comments.

The fresh faithful census remains 1,205 EXACT, 39 REGALLOC, 608 NONMATCH and zero
unresolved out of 1,852; no function is promoted or regresses. The partial link
still differs from the reference. Six source cells were investigated: three
FP_Pow cells remain unretained, while the equalizer cell and two cumulative
LowPass cells establish the retained graph. There are three unchanged full-TU
controls plus a separate naming-only raw-equivalence control. Failed cells did
not run integration gates. The retained batch receives one frozen integration
checkpoint. Native-array representation evidence and the naming request are
included in that same checkpoint.
