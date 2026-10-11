# Tone reversal: sequential energy and sample captures miss

Baseline9c020b45, Gentoo3.4.2-r2 with its executed assembler2.15.92.0.2,
complete retained profile/current headers and DSPLIB_REPRODUCE_BUGS. Original
FPM_TONE_find_rev547B. Domains were recorded before compilation:
[opening cross](residual-tone-reversal-domain.md) and
[arithmetic-only captures](residual-tone-reversal-arithmetic-domain.md).

| Sequential energy updates | Full current-sample capture | Bytes | Strict verdict |
|---|---|---:|---|
| No | No |547| BYTES463 |
| Yes | No |583| SIZE36 |
| No | Yes |543| SIZE4 |
| Yes | Yes |546| SIZE1 |

The opening hypothesis incorrectly extended original sample reuse through the
final history store. Original0xab295 retains a current sample for correlation
and energy;0xab245 reloads it AFTER the rev_age store0xab239. Both full-capture
controls remove that final reload, so neither is supported, regardless of size.
The compile measurements are valid; the source hypothesis is refuted. No runtime
alias result is claimed from ordinary disjoint-buffer fixtures.

The follow-up retains that final read and fixes sequential energy updates:

| Arithmetic current capture | Outgoing history capture | Bytes | Strict verdict |
|---|---|---:|---|
| No | No (raw sequential repeat) |583| SIZE36 |
| Yes | No |547| BYTES473 |
| No | Yes |531| SIZE16 |
| Yes | Yes |532| SIZE15 |

Five follow-up cells also include the raw production baseline. Nine complete-TU
cells /99 common body verdicts preserve exact7/11. All ten bystanders, symbol
binding/visibility/import/export and metadata/allocated nontext/BSS/relocations
remain identical. Both repeated complete objects reproduce raw. No strict gain,
exact loss, source/header adoption or score-driven extension of these domains.

The detector reports eight RTL stages per cell. In initial RTL energy's existing
definition is initialization plus one sum update; sequential controls instead
define it by initialization, addition, subtraction. Sample reads are three for
uncaptured source, one for unsupported full capture, and two for arithmetic-only
capture. Checking the last sample read against the final age store positively
detects both unsupported controls. This preserves an observable memory boundary,
not merely a preferred register lifetime. Saturated shorts still do not replace
the running int sums, and the two squares remain separately shifted.

Replay tools: residual_tone_reversal_reproduce.py,
residual_tone_arithmetic_reproduce.py and residual_tone_reversal_audit.py under
tools/. Full commands, sources/headers/objects and dumps are recorded under
build/residual-tone-reversal and build/residual-tone-arithmetic; audit JSON is
build/residual-tone-reversal-audit.json. New tools compile and refcheck/diff pass.
Production1079/1852 and source remain unchanged; previous period gate388/0 applies
to that unchanged source. No candidate runtime, fuzzing or mutation execution,
and no portability claim. A further experiment needs a fresh independent witness.
