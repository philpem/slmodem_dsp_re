# V22 MRF work-index controls

F11597. Four-cell early index initialization × separate increment domain at
b8081307. [Predeclaration](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949559073).
Baseline244B/SIZE16; separate211B/SIZE17; early244B/SIZE16; both227B/SIZE1,
versus blob228B. Four distinct sources/complete emissions; no exact gain,
exact1/3 unchanged. Only init changes; all3 functions/2 globals/type/binding/
visibility/named data and all allocated nontext bytes preserved. Production
baseline raw-reproduced; full flags/bugdefine/compiler/assembler/header hashes
and initial RTL retained in build/playbook-v22-mrf-index. Replay
 tools/playbook_v22_mrf_index.py --domain <linked URL>.

The combined variant has58 instructions versus blob60 after padding removal;
it also uses a different saved-register set and frame size. Its load precedes
store and increment, while blob computes the next index between load and store.
Reaching size within one byte is not a recoverable source preimage. No source
adoption or candidate runtime/census/partial gates. Existing t_v22_mrf coverage
is not a result for these unadopted candidates.

Close the four-cell family. A separate potential discriminator is an explicit
short RHS temporary before the original postincrement assignment: this orders
the coefficient load before destination-index computation without moving the
increment after the store. That is a different lifetime boundary; predeclare
before testing, and do not permute unrelated declarations/register carriers.

## RHS sampling control

F11598. [Four-cell predeclared follow-up](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949609371)
retains original work[k++] but loads coeff[i] into a short value first,
crossed with entry k initialization. Four sources/four complete emissions:
baseline244B/SIZE16, sample245B/SIZE17, early244B/SIZE16, both245B/SIZE17.
No exact gains or losses,1/3 exact unchanged; only init changes. All3 functions/
2 globals/type/binding/visibility/named data and allocated nontext agree.
Baseline raw-reproduces production. Full compiler command/hashes/RTL/audit
artifacts in build/playbook-v22-mrf-sample; replay
 tools/playbook_v22_mrf_sample.py --domain <linked URL>.
No source adoption or candidate differential/census/partial gates.

Scope review after two unsuccessful batches: park this initializer's local
index/sequencing line of inquiry. Neither observed instruction order nor a
nearly equal size uniquely identifies source. Further nearby short temporaries,
declaration permutations and postincrement synonyms are unsupported. Fresh
work should inspect operand/field/use boundaries in another function, or obtain
an independently justified early compiler-stage discriminator before reopening.
These eight compiled cells bound two explicit source families, not all valid
C programs or the original inline/register-allocation profile. Production
remains the period-validated ECC recovery:884/1852 exact,88,310 exact bytes,
fixed phase385/0. No fuzzing/mutation execution.
