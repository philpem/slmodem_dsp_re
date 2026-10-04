# V34 echo-history rollback boundaries

Local pre-compile declaration, master5fac8659, merged PR255. Findings F11704-F11705
reserved locally. #22 read through the02:08:14 constellation/V22 results;
PR245's active files excluded. No fuzzing or mutation execution.

V34EchoHistoryBackwardClean has404 bytes in both objects. Prior findings bound
signedness/control differences but no local source domain is recorded for this
body. Reference: unsigned whole-prefilter comparison, signed n-versus-half-span
comparison, prefilter cursor formed before positive-count test, one remaining
count retained across both echo loops, and JLE skip into each rewind loop.
Source: unsigned n-versus-half-span test, cursor formed after positive-count
test, repeated (int)(n-span) arguments, indexed for(k=0;k<n;k++) helper.

Fully cross four two-valued axes,16 complete v34filters.c TUs:
1. Retained unsigned test vs explicit signed (int)n > (int)span; span's SHR
   and initial unsigned histogram comparison unchanged. This follows the
   reference's signed JLE and preserves unsigned subtraction before narrowing.
2. Repeated rewind-count expression vs one int remaining=(int)(n-span) local
   passed to both helpers, keeping the same count alive across the first clear.
3. Retained positive-count else-if vs an else block that forms the prefilter
   cursor/end before its nested positive-count test, matching the blob load scope.
4. Retained helper for loop vs while(n>0) countdown, with the same wrap/store
   and no body on nonpositive counts; helper has only these two users.

Predict signed comparison restores its signed branch class, count local avoids
recomputation and forces the persistent count plus per-loop copy lifetime,
cursor scope restores its before-test load, countdown may restore JLE entry.
The full cross may recover the404-byte body. Falsifiers: source collisions,
unchanged asserted RTL boundaries, raw baseline mismatch, metadata/data/canonical
relocation drift, any bystander/exact loss, or no exact cell. If no hit, stop this
family and inspect the first remaining pass boundary before proposing a domain.
No flag/header/ABI/register-name changes and no source-padding fits.

Gentoo GCC3.4.2-r2 with complete saved .build-config flags, mandatory
DSPLIB_REPRODUCE_BUGS appended last, executed assembler identity and -da dumps.
Fresh300-object baseline, complete TU review and make phase required for adoption.

Result: sixteen valid cells,26 functions/48 named data per TU,13 exact in
every cell; no gain/loss and only V34EchoHistoryBackwardClean changes. Cache-only
moves BYTES254→119; signed+cached+early-cursor reachesBYTES111. Countdown changes
wrap/exit CFG and produces SIZE12/17 rather than the reference. The source family
is closed, not adopted. In particular, no register/declaration synonyms follow
from the lower differing-byte score. Source and object hashes plus dumps are
retained in build/gcc3-v34-rewind-boundaries/results.json. The final rebuilt
baseline and archive each raw-match all300 merged production objects.
