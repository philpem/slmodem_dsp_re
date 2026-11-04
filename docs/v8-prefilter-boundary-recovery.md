# V.8 and V.90 source-boundary recovery

The 2026-11-04 exploration retains two native-backed partial recoveries. Neither
function reaches REGALLOC or EXACT. Five source cells across two complete TUs
change only the two target bodies; all eleven protected EXACT functions, data
and bindings remain unchanged. The baseline is `9c100bbfa` after merging PR #583.

## V.8 phase-reversal detector

`v8_phase_rev_detect` at native `0x78900` differs from the previous source at
several independently observed boundaries:

| Native observation | Recovered source |
|---|---|
| SI loop increment/compare at `0x789d0..0x789f0` | `int i`, replacing a short counter |
| Sign-extend the back index before its test/address at `0x78a10` | `short back`, replacing an int narrowed only on one arm |
| Sign-extend `run`, compare with full-width `half << 2` at `0x78960..0x78970` | Preserve integer promotion of `half * 4`; remove the short truncation |
| Capture `pr + 0x14` at `0x7891b`, retain and consume it for window accesses | Local `short *window = pr->window` |
| Threshold failure and correlation failure join the same increment at `0x78aa5` | One short-circuit conjunction with a common failed-predicate action |
| Read input at `0x789e0`, after state updates and the optional diagnostic, then publish it at `0x789e4` | Store a fresh `in[i]`, replacing captured `x` |

The input reread changes behaviour when the input aliases a field that the
function updates. It restores the blob's operation; it adds no repair or bug
conditional. The early `x` remains necessary for the correlation/energy work.
The member alias is consumed by real reads/stores and preserves the same owner,
window byte base +0x14, short element width and indices. It adds no barrier,
qualifier, helper or artificial consumer.

Four cumulative source cells test widths, then input reread, then window alias,
then the shared increment. Their nonpadding instruction totals are 127, 128,
133 and 130, against native 135; the original is 130. Those totals are diagnostic,
not acceptance criteria. Every cell remains NONMATCH. Initial RTL confirms the
width boundaries and late input read; the final shared-increment cell has the
native common action and preserves short-circuit threshold reads. The final
longest mnemonic run is 13/135 versus 10/135 at baseline. Remaining scheduling,
allocation and temporary spills are not assigned speculative source spellings.

### Initialized alias history and negative control

The new `t_v8sig` section starts both components through their initializers and
makes 64 one-sample calls with input pointing at each component's own `run`
short. Every pointer names valid storage, count is one, and the initialized
half-window remains 32. The calls cover a complete window-index wrap. This is
an initialized component alias history, not modem lifecycle or waveform-input
coverage. No internal state is planted to force a diagnostic callback.

Each call checks reference publication against its current `run`, checks the
reconstruction likewise, and compares the complete object with `diff_eq_obj`.
A final wrap check gives 193 checks. The reference's analytical checks pass.
The old source fails 128/193 checks: 64 stale publications and 64 differing
objects. All pre-existing sections remain green. The recovered source passes
193/193. Ordinary disjoint-input histories also remain covered.

This sensitivity test exercises the read age directly, without replacing the
diagnostic function or inventing callback behaviour. It does not independently
exercise the wider run threshold: the initialized half-window stays 32.

## V.90 prefilter selection

`V90PreFilter::autoSelection` at native `0x45580` has two six-element loops.
Native `JBE` at `0x455af` and `0x45611` contrasts with signed `JLE` from the old
`int i`. Changing only that local to `unsigned int` restores both branches.
The first RTL changes from `compare:CCGC`/`gt` exits to `compare:CC`/`gtu` exits.
The cell changes only `autoSelection` among sixteen TU bodies; all eight EXACT
members and data/bindings are unchanged. The target remains NONMATCH, with 108
nonpadding instructions against native 107 and the same 36/107 longest run.

This discrepancy had been recorded but not tested in the earlier prefilter
read-age ledger. It was not part of the closed `selectFilter` experiment domain.
The current unchanged full-TU control is reproduced through the experimental
compiler path before the new cell. No declaration/lifetime palette follows it.

All floating expressions, SF target stores, long-double accumulators, unordered
NaN update semantics, diagnostic calls and recovered post-diagnostic database
reads remain unchanged. The fixed index domain 0..5 does not itself distinguish
signed and unsigned execution results; this is native predicate recovery, not
a new behavioural defect. Existing constructor-to-selection, precision and NaN
checks remain the period oracle.

## Controls, scope and stopping point

Both unchanged full-TU controls are raw identical to the faithful production
objects under the configured Gentoo GCC 3.4.2-r2 and reproduction flags. Final
period objects are raw identical to their reviewed experimental candidates,
including the V.8 source comments. The focused baseline and retained candidate
each pass five period binaries: `t_v8sig`, `t_v8util`, `t_v8hs`,
`t_v90prefilter` and `t_v90pftab`. The test-only old-source control fails only the
new alias section, as described above. No profile or grader change is made.

The census inventories 608 NONMATCH functions; this wave does not claim a deep
audit of all of them. Parent history navigation and Astra's eight-function
source/history screen admit the two families above. Astra independently reviews
the native boundaries, shared action and initialized alias history. Raw commands,
source/object hashes, all 22 TU grades, pass dumps, controls and review are under
`scratch/fresh-discriminator-wave/` and `scratch/fresh-admission-astra/` in the
owned worktree. Failed cells do not trigger global gates or CI; retained changes
receive one frozen integration gate and census.

## Final local validation

`make phase J=8` passes 398/398 period binaries and every structural check.
The added section changes the check denominator within `t_v8sig`, not the number
of binaries. Its retained run has 22 passing sections and 3,963,906 checks;
`t_v90prefilter` has 16 passing sections and 131,293 checks. The five focused
binaries all pass. Actual period and faithful objects are raw equal for 300/300
TUs; only `V8Detector.c` and `V90PreFilter.cpp` change from baseline. The fresh
faithful census is unchanged: 1,205 EXACT, 39 REGALLOC, 608 NONMATCH, zero
unresolved, denominator 1,852. The partial link still differs from the reference.

Seven compiler invocations comprise two raw controls and five source cells;
their recorded compiler-command wall time totals 3.16 seconds, excluding grading,
period tests, integration gates and CI. Model usage is not available here and no
usage saving is inferred from compiler speed. Exploration and local validation
finish within seventeen minutes; the one-hour allowance is a ceiling.
