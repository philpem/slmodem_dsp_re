# FPM tone allocation boundary

`FPM_TONE_create` now follows the native unchecked allocation path when
`DSPLIB_REPRODUCE_BUGS` is enabled. Without that define it retains the existing
NULL return on allocation failure. This is a source-fidelity correction,
not a byte-exact gain.

## Native witness and prior decision

The reference at 0xaacd9 requests 0x108 bytes. After the malloc call at
0xaace0 it saves EAX in ESI, sets the owned flag to one, and jumps to 0xaaa25.
There is no allocation-result NULL test or failure return between that call
and configuration setup. The reconstruction added such a test. The new
preprocessor guard removes only that defensive branch in reproduction mode.
The incoming-state NULL test still selects allocation normally.

F11631 and `tools/playbook_tone_alloc_guard.py` previously measured the same
unsupported guard at source pin `a50cc9cf`. That candidate was left unadopted
with no exact gain and a worse length gap, despite unchanged bystanders and
no conflicting native witness. The current two-endpoint replay starts from
`6b16d64d9`, raw-reproduces the canonical Gentoo object, and preserves that
negative exactness result rather than choosing source by size.

## Complete-TU result

| Endpoint | Constructor bytes | Verdict against native 753 bytes |
| --- | ---: | --- |
| Existing allocation return | 669 | SIZE(84) |
| Native unchecked path in bug mode | 663 | SIZE(90) |

All 11 common functions are audited. Only `FPM_TONE_create` changes; the
other ten bodies and their canonical relocations are unchanged. Seven exact
bodies remain exact, with no gains or losses. Nontext section bytes and
metadata, nontext relocations, strong exports and resolved imports remain
unchanged. The two named data objects remain intact. The first RTL dump
already removes the post-allocation comparison and failure-return edge; this
is not a late scheduler or register adjustment.

The replay uses the complete frozen `.build-config`, the recovered Gentoo
GCC 3.4.2-r2 and assembler, and the shared helper's final mandatory bug define.
It records actual commands, source/object hashes, all body verdicts and
canonical relocations, and compiler dumps. The selected generated source
must equal the adopted source. It does not run differential fixtures.

## Validation and limits

Focused period validation passes three binaries with zero failures:
`t_fpm_tone_own`, `t_fpm_tone` and `t_tonecreate`. The last exercises the
separate tone constructor and is an additional neighbouring regression.
The relevant ownership fixture reports 249 checks for caller-supplied
creation, 10 for paired supplied-state deletion and 11 for self-allocated
creation/deletion. The tone fixture reports 237 default-config and 289
NULL-config creation checks, alongside its signal-generation coverage.
No artificial allocation-failure fixture was added. Normal successful
constructor histories validate preserved behavior; they do not establish
an equality claim after native allocation failure and NULL dereference.
The nonbug fallback remains textually unchanged under its guard.

All four mutation suites targeting this TU were inspected: `fpmtonekill`
6 locators, `fpmtonerev` 29, `fpmtonetickstd` 2 and `fpmtonecfgstd` 4.
All 41 original locators remain unique and their target text is unchanged.
There are zero displaced operations, registry edits or mutation executions.
No snapshot was rerecorded. The parent integration runs the full phase gate.

```sh
python3 tools/recovery_fpm_tone_allocation.py \
  --objects build/fpm-tone-guard-baseline --out /tmp/fpm-tone-allocation
make period T='t_fpm_tone_own t_fpm_tone t_tonecreate' J=8
```
