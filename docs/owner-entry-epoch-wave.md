# Bounded entry-memory owner tracing

The six-step recovery wave extends `gcc3_owner_trace.py` to identify nested
loaded-pointer owners during the first entry-memory epoch. It adds bounded
screening coverage; it neither proves semantic equivalence nor closes the
broader owner-provenance gap described in the recovery pilot.

## Supported boundary

An owner path starts at a cdecl entry argument. Each nested pointer load must
be a complete 32-bit read, and its full parent owner, byte offset and width
remain in the recorded origin. Copies, constant pointer adjustments and private
stack spills retain the origin. Paths are bounded to eight nested loads.

All incoming CFG paths must still be in the entry-memory epoch when a pointer
is loaded. Any non-private memory write, call, unknown instruction or backward
control-flow edge ends that epoch. Writes overlapping entry ESP or higher,
including partial writes to caller-owned argument slots, also end it. Only
private storage entirely below entry ESP is assumed disjoint from input
objects. Unknown aliases are never assigned an independent epoch.

A pointer captured before an effect remains that captured machine value if its
register or private spill survives. A pointer reloaded after the effect receives
site-local provenance and cannot pair across objects. Joins retain conflicts
as ambiguous; loop reloads cannot reuse an entry-epoch identity merely because
the static instruction address repeats. Call-return identities remain local.
No callee summaries or matching-by-call-ordinal were introduced.

## Controls and review

The baseline suite has 17 real-ELF control groups. The final suite has 28,
passing with normal Python and `python3 -O`. New controls exercise nested
paths with different registers and equivalent LEAs; differing owners, offsets
and load widths; unknown aliasing stores and calls; one mutating branch;
equal joins; captured-pointer stability; loop reloads; the depth bound; and
the FAXVMI root-versus-child false-nomination pattern.

Parent review found that exempting all stack writes would miss aliases through
caller-owned argument slots. Four hostile full/partial stack-write cases now
reject that correspondence, and a negative-offset private-spill case remains
supported. This boundary correction is part of the final implementation.

## Fixed-input screening

The reference is the repository's `ref/slmodemd/dsplibs.o`. Candidate files are
the existing `build/tc_out` objects at `4317aa3ed`; source was not changed.
The report hashes every object and the exact analyzer bytes, reads immutable
per-file snapshots, and rechecks inputs after the screen. Each row uses one explicit defining candidate copy; two Scrambler C1 names
appear in two constructor objects each. This screen does not replace worst-copy
COMDAT function grading.

The three domains contain 72 defining-copy pairs and 70 distinct names: the
pilot's 24 selected constructor copies, the 24 largest SIZE names, and the
24 smallest SIZE names with
native sizes from 1,000 through 2,000 bytes. The latter two selections sort by
native size and full symbol name; eligibility is a differing native/candidate
function size. Selection does not depend on a successful owner nomination.

Counts below are supported / unknown / ambiguous decoded MOV stores.
Whole-function refusals have separate instruction denominators in the JSON.

| Domain | Native before → after | Candidate before → after | Whole-function refusals per side |
| --- | --- | --- | ---: |
| Pilot constructors, 24 | 48 / 321 / 8 → unchanged | 49 / 324 / 8 → unchanged | 6 |
| Largest SIZE, 24 | 42 / 2,121 / 1 → unchanged | 16 / 2,145 / 0 → unchanged | 11 |
| Medium SIZE, 24 | 32 / 928 / 1 → 34 / 926 / 1 | 30 / 929 / 0 → 32 / 927 / 0 | 4 |

The additional supported stores are both in `initTxSequence` (1,001 native
bytes). Native `0x75874` loads a pointer from entry argument 1 at `+0xc48`;
`0x75880` and `0x75885` write words at child offsets zero and two. Candidate
offsets `+0x14`, `+0x20` and `+0x25` perform the same owner load and stores
with different registers. Independent `dis.py` ranges retain the instruction
bytes and confirm that complete nested path. Both stores match at the same
width, so they yield no new width-defect nomination.

All 72 final defining-copy pairs produce reports without tool errors. Both versions produce
zero nominations. The 21 whole-copy refusals per side remain explicit;
zero nominations cannot establish the absence of defects in these copies.
No reconstruction source gain or function-grade change is claimed.

## Remaining gap and next discriminator

Unknown register roots dominate the remaining decoded-store exclusions:
native/candidate counts are 270/274 in the constructor domain, 2,084/2,089
in largest SIZE and 858/869 in medium SIZE. Unsupported instruction handling
currently discards all register facts. The instruction inventory includes
relocation-bearing MOVs, suffixed comparisons and x87 operations; its raw
mnemonic counts are not a count of distinct unsupported semantic mechanisms.

The next bounded extension should preserve unaffected register facts through
independently modelled read-only instruction effects, with hostile implicit-write
controls. Indirect/table control-flow refusal is a separate large-function gap:
refused functions contain 27,911 native and 17,721 candidate instructions across
the three domains. Post-call loaded owners and call-return identities still need
independent correspondence evidence; they must not be paired by call ordinal.

## Reproduction and artifacts

Run the controls from the repository root:

```sh
python3 tools/gcc3_owner_trace_test.py
python3 -O tools/gcc3_owner_trace_test.py
```

The retained wave-owner-epochs evidence path's `build/six-step-wave/` contains the frozen
before-analyzer copy, `screen_owner.py`, `screen_owner_medium.py`,
`owner-screen.json`, `owner-medium-screen.json`, `owner-gap-summary.json`,
control logs and the two independent `initTxSequence` disassembly ranges.
The screen scripts record exact absolute inputs and tool hashes. Parent-wave
integration must retain those artifacts or the archive mapping.
Integration retained the evidence under the consolidated wave's
`worker-evidence/owner/`, with its original path preserved by the compatibility
mapping in `worker-cleanup.json`; the worker Git checkout was removed.

One initial medium screen detected analyzer changes during execution and
refused all 24 after-cells. That invalid run is retained as
`owner-medium-screen-invalid-analyzer-drift.json` and excluded. Intermediate
reports preceding the parent stack-alias correction are separately labelled;
only the final unchanged-analyzer reruns support the table above. Wall times
are recorded per screen; CPU and billing costs were not measured. No mutation
campaign, snapshot refresh or modem-lifecycle claim forms part of this work.
