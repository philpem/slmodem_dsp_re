# FPM tone filter ring and count recovery

`FPM_TONE_filter` now uses a self-clearing conditional for its ring index
and a signed-short postdecrement for its sample countdown. The reference
independently supports both operation sequences. The function remains
nonexact; its one-byte length gap is not a one-byte instruction difference.

## Native evidence and bounded controls

The native ring increment narrows at 0xab3f7, compares words at 0xab3f8,
and selects the index through SETL, NEG and AND at 0xab408/0xab410/0xab412.
The old ternary reconstruction instead branched. The selected source first
assigns the incremented signed word to the existing integer index, then
conditionally clears that same destination. Its tap count is a short, as
supplied by the configuration field, so the comparison remains word-sized.
The mask result remains an integer for history addressing.

Native countdown entry decrements EAX, sign-extends AX to save the new short
count, then increments AX to test the old count's word value. The same
sequence governs the backedge at 0xab46d/0xab473. The old reconstruction
initialised a sentinel to `count - 1` and tested it as an integer against
minus one. The selected signed-short postdecrement restores the native
narrowing and old-value word flags without changing the pointer advances,
filter arithmetic or final index publication.

F11570 and `tools/playbook_fpm_tone_ifcvt.py` at `5b1bed17` had recovered the
ring conditional but left it unadopted without candidate period validation,
because the whole body remained nonexact. The current cross starts from
`94caa17be`, including the already accepted constructor allocation guard,
and adds only the independent native old-count witness. It does not reopen
the earlier width/register spelling palette.

| Cell | Bytes against native 235 | Ring mask | Word old-count test |
| --- | ---: | --- | --- |
| Baseline | 238, SIZE(3) | no | no |
| Ring conditional and word taps | 233, SIZE(2) | yes | no |
| Short postdecrement count | 258, SIZE(23) | no | yes |
| Both, retained | 236, SIZE(1) | yes | yes |

The detectors accept the native operations and the corresponding positive
cells, and refuse their single-axis negative controls. GCC's first
if-conversion dump converts zero filter blocks for the ternary cells and
one for the conditional cells. The retained 0x14-byte frame agrees with the
reference. Register allocation, stack homes and scheduling still differ
throughout the function; the source was selected by the independent operation
witnesses, rather than nearest size.

All eleven emitted/common TU functions are reviewed in each cell. Only the
filter changes. The other ten raw bodies and canonical relocation tuples,
including the accepted unchecked-in-bug-mode constructor, remain unchanged.
Seven exact bodies remain exact. The two named data objects, all nontext
bytes and section metadata, nontext relocations, strong exports and resolved
imports remain unchanged.

## Counter behavior and validation

Zero count still skips the filter body and publishes the unchanged history
index. For negative counts the signed-short postdecrement retains the native
word wrap rather than replacing it with a positive-count guard. Under the
period compiler the iteration count is the count's unsigned 16-bit value;
minus one therefore processes 65535 samples. This describes the native
component behavior, not a normal modem input claim or a safety repair.

The existing period fixtures `t_fpm_tone` and `t_fpm_tone_own` pass with zero
failures. Ordinary constructor-backed filter streams cover single samples,
blocks of seven and 500, and a 40-of-53-tap configuration, totaling 249178
checks; the shared detector-index group adds 19002. The existing negative
count fixture already constructs both states and supplies 65600-element
sample arrays for count minus one, comparing 65535 outputs and detector
state in 65709 checks. No new negative/high-count fixture or planted internal
history was added. Existing owner/construction coverage remains intact.

The selected object first passed in isolated period output storage. After
approval, the production source passed the same two focused fixtures with
zero failures and its object raw-matched the selected replay object. The
subsequent rebase preserved the selected source and pinned inputs; the
post-rebase `--check-adopted` replay passes all four cells again. No new
production build was needed for that replay. No fixture, assertion or tolerance was changed. Existing mutation locators
were maintained as described below.

The four existing suites targeting this TU contain 41 source locators.
The initial post-rebase check found two detached filter locators, for late
wrap and negative-count handling. After parent approval, those same two
faults were retargeted: the conditional permits index equal to taps, and the
count loop becomes signed ascending so negative counts skip processing.
Labels and fault counts remain unchanged. All 41 locators then pass the
structural check; the initial failure is preserved alongside the repair.
No mutant was executed and no snapshot was refreshed. This is existing-fault
maintenance under the master anchor policy, not a new mutation campaign.

## Replay and actual header proof

`tools/recovery_fpm_tone_ring_count.py` pins source to `94caa17be` and requires
raw reproduction of the frozen canonical baseline before interpreting the
four cells. It records full compiler commands, source/object/header hashes,
compiler dumps, all eleven verdicts/relocations and complete metadata. The
recovered Gentoo GCC 3.4.2-r2 and selected assembler are identified, and the
shared helper appends the mandatory bug define after configurable flags.

Actual repository header contents must equal the pinned inputs. A compiler
`-M` invocation with the same flags then proves the resolved dependency paths:
`fpm_tone.h`, `fpm_phasor.h`, `fpm_iir.h`, `sysdep.h` and forced
`period_compat.h`, plus the selected compiler's own `stddef.h` and `stdarg.h`.
The full dependency output is preserved; no intended-but-unused overlay is
claimed authoritative. `--check-adopted` requires selected/production source
equality. Frozen baseline inputs live in `build/tone-ring-count-baseline`. The historical
source pin remains available through
`archive/pr293-before-rebase-20261018`; it is not repinned to a changed source
merely because the active branch was rebased.

```sh
python3 tools/recovery_fpm_tone_ring_count.py --check-adopted \
  --objects build/tone-ring-count-baseline --out /tmp/tone-ring-count
make period T='t_fpm_tone t_fpm_tone_own' J=8
```


## Final canonical and runtime validation

On the rebased `e6e527615` tree with this selected source, the canonical
Gentoo comparison object and period runtime object both raw-match the adopted
four-cell replay endpoint. All three objects have SHA-256
`2a5fd99fc7d8c44d7e006897cc56cbcd6aafc2e1a5e8c93ace6dea9121a8cd00`.
Rechecking all eleven bodies against the frozen baseline confirms that only
`FPM_TONE_filter` changes; its ten neighbours, seven exact matches and complete
nontext/binding/import controls remain intact. The constructor remains
663 bytes, SIZE(90), with its accepted allocation guard policy unchanged.

The parent's independent four-cell replay also passes. Its full `make phase`
run exits zero with 392 period fixtures passed, zero failed, and structural
checks green (`build/witness293-rebase-tone-phase.log`). This is the full
reconstruction gate, not a portability or mutation-campaign result. The
focused two-fixture results above remain separate from that suite count.


The read-only canonical census (`TC_OUT=build/tc_repro python3 tools/toolchain/byteident.py --json-out <report>`) measures 1109 EXACT out of
1852 compared symbols, covering 127904 original exact bytes. The remaining
rows are 633 SIZE, 78 BYTES and 32 REGALLOC, with zero UNRESOLVED or RELOC.
Three COMDAT names have differing copy verdicts and are scored by the worst
copy, as the census reports. The tone family contributes the same seven exact
names established by the complete-TU controls; its filter remains SIZE(1).
No ratchet floor or comparator rule was changed for this measurement.
