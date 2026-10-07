# Installed Gentoo postreload trace for SDM

This is a read-only dynamic discriminator on the three already-declared saved
full-TU SDM controls: baseline, mask-postincrement and compound-postincrement.
No source family is reopened and no widening is fabricated through a debugger.

## Reproduction and authority

Run from the integration worktree:

```sh
python3 tools/gentoo_cc1_sdm_trace_reproduce.py \
  --controls build/sdm-postreload \
  --output build/gentoo-cc1-trace-full
python3 tools/gentoo_cc1_sdm_trace_audit.py \
  --controls build/sdm-postreload --artifacts build/gentoo-cc1-trace-full
```

The driver extracts the installed image's unchanged `cc1`, verifies its hash,
records its version and image ID, and reads each actual saved `compile.log` cc1
command. It replays the saved preprocessed full TU with **all** saved command
options including `-da -dP`. Only input/output/apparatus paths are remapped. The
original preprocessing command and complete period configuration, including the
final bug define, remain in the parent's reproduction results. Debugging a
preprocessed input does not re-run that preprocessing step.

The image-installed compiler is unstripped and contains full DWARF and local
symbols for `reload_cse_simplify_set`, `reload_cse_simplify_operands`,
`cselib_lookup` and `cselib_record_set`. Its SHA-256 is
`80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc`.
`tools/gentoo_cc1_sdm_trace.py` refuses another binary because its observation
addresses are pinned to this compiler. It reads frames, locals and RTL/value
structures; it makes no inferior function calls and changes no RTL or source.
Debugger software breakpoints temporarily alter process instruction bytes in the
normal debugger manner; the installed compiler file is neither rebuilt nor replaced.

The image lacks gdb. The trace uses extracted unchanged image cc1 under host
GDB 15.1 and the host 32-bit runtime. This is **not** a claim that the debugger
ran inside the original Gentoo runtime. The image-selected assembler is executed
inside the original image and reports GNU assembler 2.15.92.0.2. For every control,
the complete raw and debugged objects are identical to the saved container-built
object: **3/3 controls, nine complete objects in three equal triples**. This
controls the observed compiler output despite the runtime distinction.

`commands.json` records exact executed argv, directories and logs;
`results.json` records source paths, path mappings, preprocessed-input hashes,
saved cc1 commands and full-object hashes. The driver rejects failures, Python debugger errors and object mismatches.
The independent audit requires visit denominators, value identities and the
actual forwarding/cost events; two removed-event controls must be refused. A prior
scratch attempt with a Python syntax error is explicitly retained as
`build/gentoo-cc1-trace/invalid-script-syntax.log` and excluded from evidence.

## What the running compiler actually does

Each full-TU trace observes two simplify-set visits at the existing wide read
UID38 and two at the selected late UID (56 plain, 57 compound). The baseline's
UID56 is a final store, not a late reread: UID identity is per-control and cannot
be used to align semantics without checking patterns.

For mask-postincrement, the first postreload traversal records:

| Observation | Dynamic value state |
|---|---|
| UID47 store through SI register 4 | HI value 7 acquires `MEM:HI(VALUE:SI)` alongside AX:HI; destination address is SI value 1 |
| UID54 copy old cursor to register 1 | Same SI address value 1 now has registers 1 and 4 as locations |
| UID53 advance cursor register 4 | Register 4 gets a new SI value 8746; old address value 1 retains register 1 |
| UID56 reread through register 1 | Address lookup returns SI value 1; memory lookup returns HI value 7 with memory and AX:HI locations |
| Candidate selection | AX:HI cost 2 versus memory cost 4; `simplify_set` returns 1 |

The UID54-before-UID53 order above is the actual traversal, even though the
UIDs are numerically reversed. Pointer addresses in the log identify one run's
objects; use the printed values/modes and location relations when comparing runs.
These dynamic identities corroborate the alias route: saving the old cursor
does not break cselib's knowledge of the preceding store.

The second traversal sees an AX:HI self-copy at UID56. `simplify_set` returns 0
and the operand fallback returns 1. The decisive memory forwarding happened in
the **first simplify-set/cselib path**, not in that later fallback. The compound
control repeats this mechanism at UID57. Its extra early input read must not be
mistaken for survival of the late reread.

The existing UID38 is a real `ZERO_EXTEND:SI(MEM:HI)` input read. In all three
controls and both visits its recursive cselib lookup returns NULL, simplify-set
returns 0 and the fallback returns 1. A subsequent SI value is recorded with
`ZERO_EXTEND:SI(VALUE:HI)` and an SI register location. **UID38 occurs before the
store.** It demonstrates the real wide-input lookup path; it does not establish
that a late zero-extended read after the store would miss or resist forwarding.

## Bounded conclusion

The current plain and compound HI rereads are eliminated because the actual
postreload value table retains the HI stored value in AX across the old-cursor
copy and cursor advance. This is now a dynamic compiler observation, stronger
than attributing the rewrite from the before/after dumps alone. The original's
late wide load is a next width/use discriminator, but the present trace supplies
no uniquely recovered source widening and no authorization to tune types for a
byte score. Any further control needs an independently supported original
operand/use graph, complete unchanged raw baseline and full-TU audit.

Final integration validation: `make phase`388passed/0failed, structural gates
green (14386references/2964finding headings;10038static anchors). Three known
source controls/9live body grades add no gains/losses. Production remains
1074/1852 strict exact; source/header/flag inputs are unchanged.
