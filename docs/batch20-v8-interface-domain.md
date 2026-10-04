# V8 public interface bounded source controls

Baseline93d7eee1, only src/v8/V8Interface.c, no header/flags changes.
Closed-family audit: no existing GetMessage guard/single-exit family found;
F15345 documents signed16 wordidx test (do not retype the shared field).
Blob initializes return -1, tests wordidx signed16 before widening it, then
sets return0 only for positive length. Current source widens wordidx first and
uses an early negative return. Four crossed source cells: early/shared return
versus local-int guard/direct member guard before establishing promoted n.
No count truncation, pointer alias casts, or arbitrary register fitting.
Whole7-functionTU metadata/data/binding/canonicalrelocs/bystanders decides;
close on negative rather than widening scope. Compiler/helperBUG flags fixed.

Initial GetMessage four cells: shared exit reaches full179B (BYTES76), direct
member guard alone merges baseline; shared+direct BYTES55. None exact. New
independent loop element evidence: blob uses MOVZWL for sequence word before
shift/byte narrowing; source uses signed short load MOVSWL. Cross unsigned16
word conversion with the four declared guard/exit forms (new4cells plus raw
baseline only), retaining sequence's shared type and charFlip argument byte.

Control: blob compares selector unsigned (JB), initializes return -1 before
switch, then leaves it unchanged on refusal. Source selector switch signed,
and repeatedly assigns -1 in rejecting arms. Four cells cross switch unsigned
at use with initialized rejection/positive predicate form. No public prototype
change or helper profile changes. For every32-bit selector, default behavior
remains unchanged. This bounds source spellings, not original selector type.

SetMessage: object dispatch selects sequence directly before accepted debug
banner; no selected-pointer NULL test survives. Source helper returns NULL and
caller then tests it, retaining an extra TEST branch. Eight cells cross literal
selection switch in caller versus helper return; signed versus unsigned switch
comparison (object JAE); rc initialization before versus after n==0 rejection
(object stackrc zero initialized only on accepted nonzero-length path).
Accepted stores/loop/diagnostic bytes unchanged. No sharedext_word/header edit.

Result: all21cells×7functions retain2exact and gain0,lose0. Original return
and guard boundary reaches full179B but BYTES55/76; unsigned word-load source
extension does not close either. Control unsigned/predicate and Set selector/
result lifetime crosses likewise negative. Full-TU metadata,nameddata,
allocated nonstringsections, canonical nontext relocs stable; unchanged original
strings can permute (literal bytes/multiplicities and reloc targets unchanged).
Direct Set selector cells also duplicate one original debug gate and alter
untouched Control scratch registers; normalized pass trace agrees through
27.flow2 and first differs28.peephole2 (existing F7812 lever3b). No source/header/flag adoption. Domains
closed; any retry needs independently observed new source boundary.

Nearest GetMessage (shared/direct+unsignedword) still has52 instructions versus
blob53. The object explicitly guards a signed16 wordidx before CWTL promotion,
then copies the promoted value into length while returning original count when
truncated. The previous two guard forms were promoted-int-local/member-direct,
not an explicit short local whose value is promoted only inside the accepted
arm. New2cells cross that distinct lifetime boundary with loopword unsigned
conversion, retaining shared exit. Rawhistoricalbaseline is control. No spills
or synthetic volatile; actual local short fieldvalue then intcount matches the
observable narrow guard/promote boundary. Close if no exact gain.

Explicit short-guard/promoted-length extension3cells (including rawbaseline)
also negative,2/7exact unchanged. Valid total24cells×7functions audited; no
source adoption. Distinguish original21-cell results from this new lifetime
control; no further local declaration permutation licensed by the observation.
