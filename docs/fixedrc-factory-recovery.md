# FixedRC factory and destructor ownership recovery

F11632–F11634. Baseline53c3bd00, retained Gentoo GCC3.4.2-r2/assembler2.15.92.0.2
profile and mandatory DSPLIB_REPRODUCE_BUGS. Production916→917/1852 exact,
94,525→94,638 exact bytes; sole gain RcFixed_Delete, no exact losses. Keep
factory and Reset residuals visible: this is paired source/ownership recovery,
not complete factory byte recovery or an original-profile claim.

[Full factory domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953590689): two cells, literal production versus observed
common allocation/mode dispatch and paired host ownership. Blob range guard
rejects negative or above999 before allocation;0..999 allocates8B common
handle and stores stateNULL. Modes0/1 allocate40B state and6/174/30B scratch;
2..19 allocate420B state, write up/down then dispatch twenty coefficient/tap
cases before common reset. Only400 history bytes or4B of each scratch array
are cleared. No allocation failure checks or whole-block calloc promise.
For20..999 the reference reaches sysdep_memset(NULL+4,0,400), an invalid access
inferred from CFG, not a measured crash experiment or returned valid handle.
All actual mode0..19 coefficient/tap choices retain existing same-TU tables;
forward declarations enable direct switch references without moving definitions.

Full signed candidate Create783B/SIZE14 versus769B blob (baseline699/SIZE70);
Reset270/BYTES41, Delete109/SIZE4. No exact gain in this initial domain.
[Unsigned-mode discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953641715): production/signed repeated controls raw
reproduce; unsigned formal plus isolated matching prototype changes Create
786B/SIZE17. It removes signed branch tests but is still nonexact. Original
signed formal with unsigned predicates remains possible; no API-type adoption
or adjacent cast/declaration permutations.

[Destructor lifetime cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953705188): five cells—production, repeated signed full
factory, state-free guard extent, child reevaluation, both. Actual blob stateNULL
bypasses state free at0xb0d9f/0xb0da1; source originally guarded only children
and freed state unconditionally. Actual child frees reload h->state at0xb0de0/
0xb0dee/0xb0dfc; source cached k1 through unknown external calls. Ordinary
conditional state free plus repeated child expressions restores113B EXACT.
Guard-only95B/SIZE18, reevaluation-only101B/SIZE12, both113B EXACT; parent109B.
Only Delete changes against full factory parent. Keep authentic h/state/kind
guards and primitive releases, with no invented scalar/array delete type.

Ten valid compilations across three domains yield six distinct source/object
emissions. Complete TU emits8 functions,5 common comparisons,22 named data.
Exact1→2/5; production changes only Create/Reset/Delete, five bystanders remain
unchanged. Existing symbol types/binding/visibility/exports and every named
data value/relocation target/offset agree. Intentional imports sysdep_malloc/
sysdep_free/sysdep_memset replace calloc/free/memset. Raw .data and .rodata
prefixes agree; a20-entry relocated switch table appends80B to .rodata. Do
not claim unchanged whole nontext size or raw relocation inventory. Parent
independent reviews and complete-object-audit.json retain all distinctions.

The retained cell is both guards/reevaluations with original int formal;
allocation/deallocation hooks stay paired per F8530. Reset and factory source
boundaries restore independently observed semantics despite remaining byte
residuals. Factory783B and Reset270B are not represented as exact or selected
for minimum size. No flags changed, register forcing, new coeff arrays or
padding. Header/source/fixture comments correct the former unsupported lookup
sentinel-to-identity and null-reset promises. The kind1 resampling machine is
still unimplemented and explicitly outside this factory/deletion change.

Fixed new t_rcownership covers80 real create/delete cycles:20 modes crossed
with all four constructor/destructor sides. It also labels2 raw empty-handle
probes synthetic and2 null-handle guards separately. Hook ledgers must agree
and release every allocation without bad/null frees. Baseline negative control
fires162/652 checks (period0passed/1failed); adopted source passes652/652.
This detector fails visibly on the known old hook mismatch before trusting its
clean run. No allocationfail/invalidmode/crash fixture or planted resampling
history. Existing t_rcresample still covers all18 kind0 modes and dirty/reset/
fresh mode2/3 histories. Kind1 processing-gap probes remain separately labelled.

F8530's six V22 bridge trees are restored in t_v22del: real blob creation at
9600Hz, blob/source teardown and exact ledgers,48 additional checks pass.
Six native trees and original72 ledger checks still pass. This closes the
historically deferred ownership witness without tolerances. New fixed binary
increases deciding phase denominator385→386, all386 pass/0fail; structural
checks and285 suites/10038 static anchors remain clean. No fuzzing or mutation
execution, and no new mutation suite/anchor retargeting needed.

Adoption artifacts build/playbook-fixedrc-factory-adoption retain all300
baseline objects/config/manifest. Exactly one production object changes and
raw-matches the promoted cell. Census confirms917/1852/94638 exact bytes with
one gain/no losses. Complete same-order300-object partial links remain
DIFFERENT (strict exit1 each): positioned equal68,698→68,839 /943,398 (+141),
allocated914,558→914,798 (+240), exact sections70/92, symbols394/2907 and
relocations1026/18317 unchanged. Function gain does not close object identity.
No modern portability claim. Phase and positive/negative witness logs saved.

Replay tools/playbook_fixedrc_factory.py, tools/playbook_fixedrc_mode_type.py
and tools/playbook_fixedrc_delete_lifetime.py with the linked domain URLs from
the declared53c3bd00 base worktree: header identity is enforced. Artifacts
build/playbook-fixedrc-factory, build/playbook-fixedrc-mode-type and
build/playbook-fixedrc-delete-lifetime retain commands/source/header hashes,
actual full flags/toolchain identity, objects, RTL and disassemblies. Generators
fire2/3/5 cells; repeated production/signed controls raw reproduce. Domains close
without more type/control/lifetime spellings; fresh evidence must explain a
distinct remaining source or compiler boundary.
