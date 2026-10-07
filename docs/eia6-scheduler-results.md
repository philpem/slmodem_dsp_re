# EIA6: conversion use, floating lifetime and conditional reload

Base75e7ef4b, after merging PR279. Retained Gentoo3.4.2-r2 and executed
assembler2.15.92.0.2; complete commands/config/header hashes are preserved
in each experiment ledger. No profile change. PR263's V34/shared-structure
paths and issue22 writes stay excluded.

## First distinguish the passes

Two verbose scheduler replays reproduce PR279's complete raw objects.
The fixed SF default-math control's fraction FIX183 precedes magnitude
ABS200/FIX206 through19.life. At20.combine,183 disappears and its conversion
is substituted into argument-copy207 after206. Conversion order therefore
diverges before register allocation and scheduling.

Scale load181 stays after whole conversion422 through31.bbro, then sched2
hoists it into the preceding integer copies. Its only incoming dependency
edges are24 and114; neither requires the whole conversion or deviation load.
Verbose issue indices are114:39,181:42,168:60,422:66. Do not explain both
movements as one reg-stack or scheduler effect.

## Bounded source-use and lifetime controls

Four fraction-use cells hold the SF/default-double/copy/predicate graph fixed:
production, old conditional argument, builtin abs(frac), explicit earlier
if(frac<0) frac=-frac. The last two emit identical complete objects. Both
restore fraction truncation before magnitude and magnitude FISTPL directly
to outgoing+8, but are SIZE8. Neither alone is adopted.

Original FABS4538c is early and its result lives through fraction arithmetic
to integer conversion453c5. Three magnitude-lifetime cells test only that
new independent witness: production, prior builtin fraction, and an early
double-valued fabs result with integer conversion retained in the call.
The last control is790B BYTES132. All first109 canonical instructions are
identical, including relocation identities and branch targets. This recovers
the entire diagnostic and zero-test prefix, not merely opcode forms.

## The last blocker is a real tail edge

The remaining conditional-copy pipeline uses two value registers, versus
three in the original. Original's last jump is .+485; candidate is .+482,
landing three bytes earlier at the shared params reload. The original arm
bypasses that reload and reuses its already acquired parameter pointer.
Candidate must keep this in EBX for the reload, while original can reuse EBX
as a third copy register. This is an original use/branch witness, unlike
inferring source targets solely from register-colored rejoin addresses.

Three final controls: production, early magnitude, and placing the final
params reload in else. The nonzero arm still reloads after its callback;
the zero arm reloads after the earlier diagnostic callback. Both paths then
reuse p in the four shared final copies. No callback-invariance assumption,
fixed register or guessed stack slot is needed.

**The else-reload control is strict EXACT790B**, including padding and
relocations. The full TU goes6/16→7/16 exact, with0 losses. All15 bystander
bodies, imports/exports/bindings, allocated data, BSS and nontext relocations
remain unchanged. This source graph is adopted; it is not claimed to be the
only original C spelling. The production comment is updated to retire its
now-resolved relational-predicate history.

Combined audit:12 complete-TU cells,192 live body grades,108 selected
single RTL streams,8 raw-object repeat comparisons,1 gain/0 losses. The
detector positively observes combine sinking FIX183→207, sched2 hoisting181,
the builtin/if identical objects, and the109-instruction original prefix.
No unmapped selected final floating operations. Every allocated/nontext
metadata item agrees with production in all cells.

## Production validation

Production make tc repeats the exact-control full object. Of300 production
objects, only V90PreFilter changes. Strict census1070→1071/1852, exact original
bytes116851→117641; exact-name set gains only setParamEia6 and loses none.
make phase exits0: period differential388 passed/0 failed, structural gate
green. Final make refs exits0:14383 references/2951 finding headings,
285 static suites/10038 anchors,0 detached/nonunique/no-op/wrong-arm.
No fuzzing or mutation harness execution; no modern portability claim.

Separate byteident --ratchet exits1 against the historical810-symbol floor:
V90Parameters C2 is BYTES4 in both unchanged baseline and production after,
and its entire defining object is unchanged. The current baseline exact set
already excludes it. Preserve that red check; do not re-bless the floor or
attribute its existing discrepancy to the new gain. All300 raw objects and
full baseline/current exact sets were compared, so0 new losses is measured.
Resolution requires reconciling that historical floor with its original
profile/source provenance or recovering the constructor under valid controls.

## Independent beta result

The read-only beta trace examines3 prior cells,2 setters,24 streams.
AND31 exists initially; regmove makes it destructive, forcing the unmasked
member store before masking. The original shift preserves the count and
publishes afterward. No reaching count bound has been established, so no
mask removal or new source variant is justified. See
[beta-shift-publication-trace.md](beta-shift-publication-trace.md).

## Replay and next steps

Run the four tools with matching *-domain.md and retained baseline directory:
eia6_scheduler_reproduce.py, eia6_fraction_use_reproduce.py,
eia6_magnitude_lifetime_reproduce.py, eia6_tail_reload_reproduce.py.
Then eia6_scheduler_audit.py --prior-root pointing to PR279's
build/eia6-x87-default-math directory. Independent beta trace takes --dumps
pointing to PR279's beta-x87-mode/V90Equalizer directory.

Close these finite spelling domains. Transfer the causal method to wrappers
with callback-bearing arms that converge on a pointer reload: compare the
actual branch destination and live-this dependency before trying statement
order. For beta, require an original lifecycle/domain bound or independently
supported defined computation that makes masking redundant. Do not remove
defined masks by opcode proximity. This pass makes no global byte ceiling
claim and does not reopen #22.

Follow-up F11855/F11861 localizes the previously preserved constructor floor
failure to2efa968b and28.peephole2; see [historical controls](v90-parameters-constructor-ratchet-history.md)
and [stage proof](v90-parameters-constructor-stage-proof.md). No type rollback
or floor lowering is adopted.
