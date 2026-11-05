# Null datapump and V8 message recovery, 2026-11-05

`null_process` is EXACT at 59 bytes after restoring signed count consumption
and post-guard destination acquisition together. `V8SetMessage` retains the
native unsigned selector split, improving SIZE20/412 bytes to SIZE11/403
against native392. The V8 graph nomination is an infeasible path, not a moved
encoding call or a demonstrated behavior defect.

The starting revision is `d2999755459bee2ce3e4af7b922aa98545bc7111`.
The [compact evidence](evidence/null-v8-recovery-20261105.json) records input,
compiler, object and analysis identities, experiment cells, closure and gates.
Full artifacts are under `/mnt/zfs/dev/slmodem/scratch/null-v8-20261105/`
and the isolated worktree's `build/null-v8/`. Source proposals, fixtures and
period validation use the Gentoo GCC 3.4.2-r2 image and unchanged production
flags. Ordinary, dump and production objects are literally identical for both
selected TUs. No mutation execution, snapshot refresh or anchor maintenance ran.

## Null: signed consumption and late destination interact

The native body guards with CMPW/JLE at `0x9f0ec/0x9f0f0`, captures link `+0`
only after admission, copies one word, increments and sign-extends the low-word
index, then reloads count for CMPW/JG at `0x9f10e/0x9f111`. The unsigned-short
callback interface does not imply unsigned consumption in this leaf. Conversely,
these signed comparisons do not uniquely identify the original formal type.
The shared callback and other slots retain their unsigned-short pointer type.
The selected source interprets `(short)*count` at each test and declares the
destination inside the loop; GCC hoists its invariant acquisition onto the
surviving loop path, after the initial guard.

| Signed count consumption | Post-guard destination | Result |
| --- | --- | --- |
| no | no | SIZE2, 61 bytes |
| yes | no | BYTES14, 59 bytes |
| no | yes | SIZE2, 61 bytes |
| yes | yes | EXACT, 59 bytes |

The isolated signed correction has the right guard and tail predicate but the
wrong owner acquisition. The isolated late destination gives no grade gain.
Their combination closes the function; no flags, artificial carriers or formal
interface changes are involved. The five other emitted bodies remain literal
matches to baseline, already EXACT. Exports, order, allocated data and data
relocations remain unchanged.

The source reload matters. If count aliases destination word zero, the first
copy can shrink or grow the bound. Replacing the load with a signed local
snapshot would discard native behavior. The header now describes signed
low-word consumption, indirect count changes and repeated reloads explicitly.

## Fixture domain and negative control

There are five native `.rodata` references to null_process, slots 0..4 in the
13-entry callback table. FAXVMI_process forwards its incoming fifth argument
as the leaf's fourth/count argument; its `&n` temporary is the ignored third
argument. Thirteen direct native FAXVMI calls in twelve owners supply seven
RX-derived low words, four TX-derived low words and two literal zeros.
No local upper clamp establishes the sign domain, but neither does this audit
establish protocol-generated high-bit counts. Normal VMI storage has 200
destination words and 50 secondary-buffer words.

`t_nulldp` retains its original 171 assertions and adds 190 across ten paired
calls, each following native/reconstructed null_create and caller-owned buffer
installation. Five disjoint cases use counts 0, 1, 160, 200 and 32767. The large
case is an exposed link-component boundary with adequately sized caller storage,
not the normal VMI allocation. Five explicitly synthetic aliases use count at
destination[0]: 1→0, 1→3, 3→1, 0x8000→0 and 0xffff→0. Same-type pointers and
absence of restrict make these C-admissible at that component boundary;
observed FAXVMI callers use separate scalar count storage.

For the high-bit probes, input[0] is zero. The old source wrongly copies one
word over the count, then safely exits when it reloads zero. Native skips.
This exposes four old-source assertion failures, confined to destination and
consumed-count comparisons. No disjoint high-bit old-source call is executed:
its signed index could wrap while the unsigned bound remains positive.
The selected source passes both new groups, 95 assertions each. Positive aliases
also distinguish a reloaded bound from a snapshot. All input/output storage,
guards, scalar bytes, secondary buffer and pointer-normalized complete link
state are compared with `diff_eq_obj`; pointer retention is checked separately.
Input and destination each have 65535 data words plus guards.

These are component fidelity results. Synthetic aliases do not establish an
ordinary protocol history. No fax source-reconstruction phase is reopened.

## V8: helper boundaries and value feasibility

> 🧠 **From Hindsight memory (Residual compiler-stage family batch)** — the
> unsigned switch-expression recipe and Wave 44's parked helper-null join were
> historical leads. This wave independently reproduces the current baseline
> and six full-TU cells; old verdicts are not current validation.

The native `which==3` branch is physically after charFlip, but dynamically
precedes encoding. Both native and current graphs report branch→call reachable
and call→branch unreachable. Only branch-arm reachability differs: native
`[false,true]`, current `[true,true]`, normalized to JE. On the current invalid
selector arm, the helper returns NULL; the subsequent shared pointer test can
only reject. The graph does not propagate that zero fact and explores the
impossible nonnull continuation. There is no feasible call-age discrepancy.

Native lacks this helper-null test, a real code-generation difference whose
source boundary remains unresolved. The graph false positive supplies no new
witness reopening the historical helper-null palette. Selector identities map
to the same four embedded sequence owners in both bodies, and neither selector
admission lies inside the encoding loop.

| Helper boundary | Signed selector | Unsigned selector |
| --- | --- | --- |
| Pointer result | 412 / SIZE20 (baseline) | 403 / SIZE11 (selected) |
| Direct rejection diagnostic | 477 / SIZE85 | 392 / BYTES104 |
| Boolean result/output pointer | 457 / SIZE65 | 415 / SIZE23 |

The six cells include baseline; five are nonbaseline controls. Both pointer
forms retain the null join at initial RTL, GCSE and bypass. Unsigned selection
changes the initial RTL split from signed GT plus a separate zero predicate to
unsigned LTU. The native-sized direct diagnostic removes the null join but
moves `rc=0` before dispatch, changes V8Control's registers and store scheduling,
and reorders pooled strings. Its goto into a constant-false rejection block is
a diagnostic carrier, not proposed reconstruction source. Byte extent alone
would have selected the wrong result.

Only the unsigned pointer form is retained. Six neighbouring function bodies,
all seven start addresses and their order, exports, five allocated-data sections
and all 34 data relocations are unchanged. The generic data analyzer cannot
resolve section-relative string destinations; raw section payloads/layout and
relocation tuples were compared separately. No unsupported target is counted
as resolved. Existing setter component fixtures cover selectors -1..4, lengths
0..20, object state, return and diagnostics at levels 1..3. Separate framing
and history fixtures are retained. No new semantic defect was demonstrated.

Stop this six-cell source family. Reopening requires independent original
helper/return-boundary evidence explaining both removal of the null join and
the native `rc` definition boundary, or a new feasible native graph witness.
A smaller byte gap or the same infeasible path is insufficient.

## Validation and transfer

Focused period execution of `t_nulldp t_v8util t_v8history t_faxvmicp` passes
4/4. `make phase J=8` passes 399 period tests / zero failures and the
structural checks. The final census is 1,139 EXACT / 28 REGALLOC / 72 BYTES /
613 SIZE out of 1,852, one EXACT gain and no protected losses. Across all 300 production objects only the two target objects
change, and among all thirteen emitted functions only the two targets change.
No EXACT/REGALLOC member is lost. The selected null function is included in an
explicit focused graph comparison even though becoming EXACT removes it from
the default inexact screen denominator.

Transfer the count lesson only where native predicates and owner timing support
both properties. For graph nominations, establish selector/iteration identity
and propagate guard values before classifying operation-age or behavior changes.
The next named lead is `_recieve_silence_state`: audit FPM_rms's write footprint,
entry argument correspondence and legal aliases before interpreting its call/
word8-read difference. Then screen fresh small-loop signed-boundary witnesses;
do not repeat the closed V8 helper family without new evidence.
