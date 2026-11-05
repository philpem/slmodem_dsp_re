# Pulse dialler coupled recovery, 2026-11-05

`IsPulseDialerReady` becomes EXACT373 and unchanged-source `SetPulseBreakTime`
becomes EXACT88. The complete TU has eleven emitted functions; the other nine
retain their bytes and relocation targets. No header, field-layout or public
interface change is retained. The source recovery combines unsigned expression
semantics, callback value ages and the native shared advance/return graph.

Starting revision: `993c7a974ff2aa31565ce570f31fbde3df3add34`. The
[evidence record](evidence/pulse-recovery-20261105.json) retains compiler,
source/header, parsed-object and test identities, experiment cells, closure and
gates. Frozen sources, dumps, commands and reports are under
`/mnt/zfs/dev/slmodem/scratch/pulse-wave-20261105/`; integrated evidence is in
the validated worktree's `build/pulse-wave/`. The production compiler remains
Gentoo GCC 3.4.2-r2. Mutation execution, anchor/snapshot maintenance and modern
portability campaigns are outside this wave.

## The coupled source graph

The owner returned by modem_get_param is nullable. Native captures its self
pointer at +0x10 before the entry diagnostic and retains that owner through
later host calls; it reads the host modem at root+0x04 immediately before each
modem_set_param. All changed timer/count accesses retain owner, offset and
DWORD width: make+0x5b4, break+0x5b8, remaining+0x5c0, elapsed+0x5c4,
off-hook+0x5c8.

Native CMP/JAE at0x32ef/0x32f1 consumes elapsed/break unsigned. The later
break+make is a 32-bit modular ADD and unsigned CMP/JB. Source now casts both
members before addition; casting a signed sum afterward would retain possible
signed overflow. Setter int transport and int field declarations remain valid
models of the transported bits without uniquely establishing interpretation.

Hook-on publishes its flag before the optional diagnostic, which uses entry
remaining, then calls the host. Hook-off clears its flag, calls the host and
reads remaining afterward for its diagnostic. Both arms rejoin at0x3331,
reread elapsed and remaining, and advance elapsed by five. Other paths can
retain their existing values because no callback intervenes. On pulse expiry,
native clears elapsed, stores decremented remaining, then shares the increment.
A common source advance and returned field predicate reproduce those joins.

| Complete-TU cell | IsPulseDialerReady |
| --- | --- |
| baseline | SIZE68 /441 bytes |
| A: unsigned local/operands | SIZE68 /441 |
| B: callback read ages | SIZE26 /399 |
| AB | SIZE26 /399 |
| Shared advance, separate/combined predicates | SIZE22 /395; SIZE17 /390 |
| Shared return, separate/combined predicates | BYTES145 /373; BYTES4 /373 |
| Three unsigned field-home diagnostics | BYTES4 /373 each |
| Direct unsigned member comparisons | EXACT373 |

The no-gain A control is retained as evidence. It restores a native source
property needed by the completed graph. Stopping at isolated grade movement
would miss it. Shared-return alone also reaches native extent with145 byte
differences; a size match would select the wrong endpoint.

The BYTES4 control has the native extent and90 decoded instructions. Its
named elapsed local makes initial RTL compare break against elapsed with LEU;
direct member consumption compares elapsed against break with GEU. The second
comparison has the analogous orientation. This distinction precedes allocation.
Three temporary pulse.h unsigned-field diagnostics retain BYTES4 under the
local-consuming source; they received TU-only diagnostics and are not proposed
header changes or whole-header validation. Native branch signedness alone
does not select a unique original field declaration.

SetPulseBreakTime improves BYTES19→EXACT88 without a source edit. Its normalized
RTL agrees through flow2; peephole2 first changes the scratch register carrying
argument constant14 from EAX to EBX. This accounts for measured collateral at
the compiler-selection boundary. The actual final native bytes, rather than a
model of historical compiler state, establish the gain. Function order matches
native. All five allocated noncode sections,403 bytes, and22 relocation targets
remain unchanged, including four call_op entries and eighteen rodata fields.

The work preserves and compares every intermediate candidate. No protected
regression needs repair in the selected endpoint; that result does not imply
that an intermediate regression would have been a veto. AGENTS now explicitly
requires tracing affected EXACT neighbours and testing coupled repairs.

## Fixture domain and sensitivity

Country PulseDialMakeTime/PulseDialBreakTime fields are u8; ordinary make/break
values are at most255 each and their sum at most510. Hook flash uses a u8 value
times ten, still small. The public setters accept int with no clamp. High-bit
inputs are therefore API-expressible component probes, not established normal
modem configurations.

The actual host modem_set_param forwards hook and pulse requests to the driver
ioctl. Inspected ALSA, modemap, socket and test implementations establish no
synchronous pulse-API reentry. Callback controls remain explicitly synthetic
external-effect probes. They reenter the same side once through public
PulseDialDigit, after the first hook restore or second hook break. The latter
occurs with elapsed5, so resetting elapsed through the public load exposes a
stale snapshot. Hook-off diagnostics also observe the new remaining count;
hook-on diagnostics retain the earlier value.

The fixture starts with zeroed component storage and uses public setters,
PulseDialDigit and bounded polls. It does not claim a full call_create or
end-to-end host lifecycle. Full objects are compared after normalising only
separate self/modem addresses, whose retention is checked separately. Each host
call records a parameter event and normalized component state. Cumulative event
prefixes are compared at every public boundary, with callback counts, trace
capacity and exact diagnostic transcripts checked independently.

Ten groups supplement the existing eighteen: ordinary/disjoint controls, four
callback cases at debug0/2, and four unsigned/high-bit/modular-duration probes.
The retained old-source run keeps all18 existing groups green and fails seven
new groups. Its330 failed assertions include consequences of earlier divergent
states/traces, not330 independent defects. The modular-wrap probe passes old
source and serves as an arithmetic negative control. Source-dependent event
counts produce separate old/selected assertion denominators in the evidence.

The fixture's own INTMAX+1 setup used signed overflow. It now performs unsigned
addition before conversion; apparatus overflow cannot decide the reconstruction.
A first log-preservation script failed after the initial old-source execution;
the complete old control was rerun, with source restored in finally and the
full failing binary, run log, source hash and assertion ledger retained before
the final gate. No tolerance or assertion was weakened.

## Validation and transfer

Focused `t_pulse t_callprog_create t_dialerprog t_call` passes 4/4. Full
`make phase J=8` passes 399 period tests / zero failures and structural checks.
`t_pulse` passes 28 groups / 11,964 assertions, including 3,862 in the new groups;
the old-source run has 11,916 / 3,814 respectively. The final census is
1,144 EXACT / 28 REGALLOC / 70 BYTES / 610 SIZE out of 1,852. The 300-object
comparison changes only call.c's object and its two recovered bodies, with no
EXACT/REGALLOC losses. Ordinary, dump and production objects agree literally.
The native graph screen has bounded nested-owner and repeated-call coverage;
the explicit pulse comparison pairs two anchors and one relation, with two
ambiguous call anchors. Its clean result is not the exactness proof.

Next investigate `IsDialStringInvalid` in the full Dialer TU. Native uses an
unsigned SETBE for analyser grade<=1 at0x7af89..0x7af8c while source compares
signed int. It prepares the grade-name lookup before the second debug guard;
source requests it inside the guard. Audit the analyser's return domain and
all consumers, cross unsigned consumption with lookup placement, and analyse
collateral before rejecting either. No negative-grade behavior defect or
original enum declaration follows from this nomination alone. Keep previous
flag and spelling palettes closed without a fresh witness.
