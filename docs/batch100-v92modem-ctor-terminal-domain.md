# V92 constructor illegal-side terminal control

Pinned856c1ecb. Original V92Modem constructor illegal-side positive-debug arm ends directly in sibling JMP dsplibs_debug_printf0x13e49 after frame teardown; baseline CALL at+0xef then local cleanup/RET. Current default arm ends with break out of the switch, whereas an explicit constructor return after the same diagnostic gives a source terminal edge. This is the directly observed terminal call/cleanup boundary; allocation/parameter/register/declaration layouts are unchanged and not varied.

Predeclare two completeTU cells: baseline and replace ONLY final default break with return. Both constructor clones must be measured; ctor statements, side tests, caller arguments, member publications, all strings, debug predicates/levels and invalid-side untouched memory stay identical. Require raw completeTU control, metadata/data/canonicalrelocation/allbodyaudit, and strict targetexactness. Trace initial and final RTL if tail-call structure changes; do not adopt bystander-only gains or enumerate return synonyms if this misses.

Measured: Two valid cells: explicit terminal return restores original sibling call/frame and reduces both ctor SIZE40→6. No gain/loss until independently witnessed descriptor input age is crossed, recorded in terminal-owner domain.
