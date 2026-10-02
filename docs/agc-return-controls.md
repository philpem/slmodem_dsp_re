# Independent AGC return controls: no byte-exact gain

F11614. Baseline9a65b5a7, after the validated fourth-argument recovery.
[Minimal predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951436476)
and [twelve-TU extension](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951493466).

Six of nineteen blob calls consume EAX: V17/V21/V27/V29 full-width,
second V23 and BwChDem sign-extend AX. The other thirteen ignore it or read
memory, including B103's explicit post-call signal load. The callee's sole
return stores and leaves its full boolean in EAX. This motivates an ordinary
int return with two caller short conversions, not a short/_Bool API sweep or
an assertion that the original had conflicting declarations. A reconstructed
void header is not historical evidence.

Minimal two-TU/three-cell controls separate retained void/field, int return
agc->signal/field, and int return/direct V21 result. Six valid compilations,
seven functions, no gains/losses. Return-only changes the callee at556B versus
566B; V21 API-only raw-merges and direct consumption227→226B versus217B.
The twelve-TU extension applies exactly the six observed consumers and leaves
all other calls/fourth arguments/field reads unchanged. Thirty-six valid
compilations,116 functions/24 named data objects, no exact gains/losses.
Eight canonical bodies change in the consumption cell: callee, six consumers,
and RxDetMarkB103 (which discards the result). Four other full TUs raw-merge
across the cells; inspect per-cell emissions rather
than assume unused-return declarations are inert. Sizes556,538,649,394,226,
371,392 and246 respectively. All symbol types/binding/visibility/imports/exports,
data/allocated nontext bytes and controls agree. Complete audits and changed
bodies retained in build/playbook-agc-return-controls and
build/playbook-agc-return-consumers; source unchanged by these experiments.

The initial extension generator missed intervening V17/V27 comments. A failed
declaration-file creation allowed an empty-domain runner to start; it was
stopped, preserved and excluded under
build/playbook-agc-return-consumers-invalid-no-domain. Corrected generators
all validated before the linked declaration and complete valid rerun. Replay
wrappers now reject missing/empty/non-issue domain URLs before compilation;
the extension also preflights every source generator before its first compile.
An empty-domain negative control fires explicitly. Actual complete saved flags,
mandatory bug define, Gentoo compiler and executed assembler identity retained.
No fuzzing/mutation execution; no production adoption or new differential claim.

Close this finite return-consumption family for byte gains. The result does
not refute an original int return or prove reconstructed void; source spelling
and residual loop/register differences remain distinct. Reopen only with new
independent body-stage evidence, not declaration/layout permutations. F11613's
original116-function denominator was mistakenly reported as126 (and98 unchanged
as108); complete symbol tables correct it here. Function/census/gate results
are unaffected. Current production897/1852,90513 exact bytes,385 fixed tests/0
failures. Next source experiment requires its own stated discriminator/domain.
