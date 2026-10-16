# toneiir_create: a missing allocation-failure edge

Baseline96ef2bb6. The reference at0x7c43f calls sysdep_malloc(168), moves the
result to EBX, clears the return register, tests EBX and returns through its
common epilogue when allocation fails. Retained source omitted the check and
proceeded into the44-byte configuration copy with a null destination.
This is a source-control-flow defect, not an inferred register-allocation
symptom. Successful initialization and the original uninitialized envelope
read (D14) are unchanged.

Four complete-TU cells cover the raw baseline, a check after optional allocation,
and checks within the allocation arm returning either literal zero or the
null pointer. All preserve8 functions/globals and change only toneiir_create;
3/8 exact functions remain exact. Baseline299B/SIZE2; outside/nested literal
checks309B/SIZE12 with identical complete objects; nested pointer return297B/
BYTES114. Equal length does not recover the function. No byte-exact gain or
loss; no further source or flag adjustment is justified by these results.
Retain the ordinary nested literal-zero failure return because it restores
the directly observed behavior, independently of its score.

The blob and candidate still differ in clamp layout, reset scheduling and
return carriers. These remain open only with new source/pass evidence;
this four-cell allocation-guard domain is closed.

[Initial domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945482004)
and [nested discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945491545).
Replay `python3 tools/playbook_toneiir_allocation.py --domain URL`.
Artifacts in build/playbook-toneiir-allocation include complete compiler commands,
source/header hashes, executed Gentoo GCC3.4.2-r2/assembler2.15.92.0.2 identities,
inventories, canonical verdicts and changed-body disassembly. The unchanged
complete TU raw-reproduces; DSPLIB_REPRODUCE_BUGS is enabled throughout.

## Fixed failure fixture

A one-shot harness allocator control rejects exactly the next request, counts
that rejection and allocates no storage. Reset cancels the control. The new
fixed component-boundary case covers supplied/default configurations, forces
one rejection per side, checks both null results and allocation accounting,
and then checks successful recovery and balanced deletes.20 added checks;
no fuzzing, mutation execution or modern portability work.

Unchanged source fails this fixture with exit139 (period0 passed/1 failed),
while the blob returns null before the source crashes. This demonstrates the
new apparatus detecting the known defect; the corrected source must pass the
same fixture and the complete fixed phase before commitment.

## Retained validation

Fixed Gentoo make phase385 passed/0 failed; the new failure case reports20
passing checks in t_toneiir. Structural references14,225/2,689 headings clean;
static anchors285 suites/10,038 all unique. Complete comparison build300/300,
zero failures. Only toneiir.c.o changes and raw-reproduces the declared nested
literal-zero candidate. Read-only data and string sections remain unchanged.
Whole-tree866/1852 and84,159 exact bytes unchanged, identical exact-name sets.

Complete same-order300-object partial links remain DIFFERENT (strict exit1).
Positioned equality68,308 ->68,326 /943,398; candidate allocated bytes914,158
->914,142. Exact section records70/92 and symbol records394/2,907 unchanged;
exact relocation records1,022 ->1,018 /18,317. Padding/layout shifts explain
why a longer function need not make the whole object longer. These are
reported effects, not evidence of byte-exact recovery.

The adjacent _iir_filter_create explicitly does not check allocation in the
blob, as its source comment already records; no failure guard is added there.
Its remaining6-byte register-colour difference and _iir_filter_delete's known
sibling-call residual are not reopened by this create-path finding.
