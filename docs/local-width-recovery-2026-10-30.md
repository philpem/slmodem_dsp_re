# Local width recovery and allocation-count recovery

Base: `fea708d11f4495b452115dde8ebf650346560f48` (PR #446).
Compiler: Gentoo GCC 3.4.2-r2, the recovered production flags, with
`DSPLIB_REPRODUCE_BUGS`. Object alignment and partial-link placement are deferred.

Eight function bodies recover native source boundaries. One reaches EXACT;
one moves from SIZE to BYTES. The other six remain SIZE and are retained for
independently observed instruction/type boundaries, rather than byte proximity.

| Function | Reference bytes | Before → after bytes | Grade after | Canonical instruction LCS before → after |
| --- | ---: | ---: | --- | ---: |
| `V92Jd::setJdPhase` | 87 | 86 → 87 | EXACT | 18 → 28, of 28 |
| `Descrambler<int,int>` constructor | 107 | 110 → 107 | BYTES | 25 → 27, of 30 |
| `V32FP_recreate` | 3733 | 3740 → 3741 | SIZE | 524 → 528, of 779 |
| `V17RX_create` | 3201 | 3075 → 3075 | SIZE | 408 → 412, of 634 |
| `FPM_TONE_create` | 753 | 663 → 692 | SIZE | 94 → 98, of 188 |
| `dpskinit` | 347 | 357 → 359 | SIZE | 64 → 65, of 81 |
| `txmit` | 392 | 360 → 376 | SIZE | 70 → 74, of 105 |
| `BwChDem_Progress` | 710 | 723 → 748 | SIZE | 110 → 112, of 184 |

LCS uses `structsim.py`'s register-agnostic instructions and operands. It is
supporting evidence, not a REGALLOC proof. Byte counts include emitted alignment
inside each symbol's extent. No padding or placement changes are proposed.

## Native witnesses and bounded controls

`V92Jd::setJdPhase` retains the converted Q16 phase as unsigned, matching the
constructor's recovered phase-mask boundary. The native loop uses `mov $1`,
variable `shl`, `test` and `setne`; a signed phase word instead produces a
right shift and mask. The unsigned local and conversion recover all 87 bytes
and all 28 instructions. The other 20 function bodies remain unchanged.
The existing `t_jdmpleaves` fixture exercises positive and negative finite
phases, every low phase bit, whole-object state and guards. No new float domain
or tolerance is introduced.

The Descrambler constructor forms an unsigned element count `b + c + 1` before
scaling by `sizeof(T)`. Native `lea 1(base,index)` followed by `shl $2` differs
from the former scaled `lea` with displacement four. The count control was
already measured in the historical `v8crc-v92cp-ctor-screen.md` addendum at
`9f1199b57`; this is a current replay and adoption, not a new discovery. Function
size is restored, but different stack-restore ordering still fails REGALLOC.
The header has 27 recorded production consumers; only this int/int constructor
body changes. The allocation count's unsigned wrapping arithmetic is preserved.

`V32FP_recreate` has four native `INC/CWTL/CMPW` clear-loop edges, at
`0x7ea68`, `0x7eda8`, `0x7edc8` and `0x7eec8`. A short local counter restores
these independently of the already recovered inlined VTB initializer. The inclusive bounds
one and five give two and six iterations. The byte gap grows by one; this remains
a type recovery, not an exact function.

`V17RX_create` has four corresponding native short-counter extensions at
`0x97558`, `0x9758e`, `0x97608` and `0x976dd`. Its one induction variable is
short, with fixed loops ending at 5, 127, 7 and 159. All allocation and ownership
behaviour is retained.

`FPM_TONE_create` uses short kernel length and induction values: the kernel
edge at `0xaab60..0xaab69` normalises the low word and compares words, and the
80-word reversal-history clear at `0xaab97..0xaab9d` has `INC/CWTL/CMPW`.
The four fixed IIR-history clears are straight stores in the native body.
A counter-only control incorrectly introduced another short loop there;
retaining the four direct stores removes that new mismatch. Adding the short-length
control restores the kernel comparison width. The final combined control
keeps both boundaries, while larger allocation/publication and ordering gaps
remain. The configured length is already a signed short; its positive loop
terminates by 32767 without counter overflow.

`dpskinit` restores the short counter in its 48-word receiver clear; the
other native short clear is inherited from `fsk_clear`. `txmit` has two `LEA/MOVSWL` induction
edges, with word comparisons against the narrowed modulator sample count.
Both the count and counter are short. `BwChDem_Progress` normalises its sample
counter's low word before comparing it with the signed short block length.
These loops terminate within their stated short count bounds; no new negative
count or overflow behaviour is introduced.

## Validation and remaining work

Eight unchanged full-TU controls reproduce their saved production objects
byte for byte. Each candidate is checked against every sized function in its
TU, symbol interfaces, allocated non-text payloads and call relocation targets.
The shared header is rebuilt through all production consumers. The final
whole-tree audit changes exactly eight of 300 objects; the other 292 are raw
identical. Across the eight changed TUs, exactly eight of 128 sized function
bodies change, with no protected EXACT/REGALLOC losses. The 28 allocated
non-text sections and 512 symbol interface records remain equal, and the
function relocation-target multisets remain equal.

Focused period validation passes **10 tests, 0 failures**. Final
`make phase J=8` passes **394 tests, 0 failures**, including the structural
checks. The final byte-identity ratchet passes: **1163 EXACT, 37 REGALLOC,
1 UNRESOLVED, 93 BYTES and 558 SIZE**, over 1852 compared symbols.
Compared with the parent, EXACT increases by one, BYTES increases by one and
SIZE decreases by two; REGALLOC is unchanged. Link-selected EXACT is 1166.
These are local compiler measurements; CI's existing EXACT/UNRESOLVED split may
differ and must be reported separately. Fixtures, comparison rules, tolerances
and compiler flags are unchanged.
Astra independently reproduced the V32 baseline and measured the same partial,
with no bystander change; final read-only review found no blocking source issue.
Its initial Docker-access denial is an excluded preflight. All eight baseline
controls and final production builds were also completed by the parent.

The next wave should seek another independent source/type discriminator in the
remaining SIZE functions. Stack-restore ordering in Descrambler is unresolved;
do not pursue declaration or register palettes. The six partials retain wider
structural gaps, so matching a clear-loop edge does not establish the rest of
any constructor or runtime function. Keep object alignment deferred.

Local evidence: `scratch/multiple-gains/`, `scratch/jd-phase/`,
`scratch/descrambler-count/`, `scratch/v32-recreate-width/`,
`scratch/V17RX_create-width/`, `scratch/FPM_TONE_create-width/`,
`scratch/dpskinit-width/`, `scratch/txmit-width/` and `scratch/bwch-counter/`.
