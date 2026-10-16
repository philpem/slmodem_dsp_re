# SRE count transfer across the call boundary

## Declared domain

Baseline: upstream `4169cb3b3108a840a51bca9ebfa086102c4c69c6`, complete
Gentoo GCC 3.4.2-r2 reproduction objects. This experiment follows a new
post-call witness outside the legacy value-timing screen's fax family.
The existing unsigned MRF-return adoption remains fixed.

In DemodDataV17, DemodDataV27 and DemodDataV29 the original zero-extends AX
after FPM_MRF_filter. The retained source instead sign-extends it before
FPM_SRE_recover: each caller explicitly passes `(short)n` and the public SRE
formal is short. The original SRE callee independently reads a signed word
from its argument slot (`FPM_SRE_recover+0xd`, MOVSWL stack slot). These facts
bound the use width without uniquely identifying the original formal width.

Cross two axes for each complete fax TU: retained short versus int SRE
declaration, and retained signed-count cast versus unsigned local count.
This produces twelve caller cells. Add two complete fpm_sre.c cells (short
formal and int formal with signed-word narrowing in the existing remaining
local) and two V32int.c controls, which retain its explicit signed argument.
The declared total is sixteen complete-TU compiles over five TUs.

Prediction: only the joint int-formal/unsigned-caller cell restores the
original zero extension. The callee and V32 control must reproduce their
baseline objects byte for byte under the wider formal. A callee difference,
new caller operation or unexplained bystander/export/data change rejects this
proposed boundary. An exact count alone cannot establish the original API.
No neighbouring source/type/register/profile variants follow a miss.

The unchanged source must reproduce each retained full object before any
cell is interpreted. Record actual complete commands, hashes and the selected
compiler/assembler. Use experiment_toolchain.py to append bug reproduction
after configurable options. Compare all emitted functions, exact membership,
relocations, data, bindings and nontext sections. Run the selected candidate's
fixed component period tests before retaining it; the integrated phase gate
remains separate.

```sh
python3 tools/recovery_sre_boundary.py \
  --baseline-dir ARCHIVED_4169CB3B_TC_REPRO --historical-headers \
  --domain docs/recovery-sre-boundaries.md
```

Supply the archived opening object/config directory. Historical headers are
required when replaying after the retained public declaration changes.
Artifacts are written under `build/recovery-sre-boundary/` in the experiment
worktree. Source/header variants are isolated from production during the cross.

## Follow-up read boundaries

The completed sixteen-cell cross restores the predicted promotion without an
exact gain. Its unchanged controls reproduce all five full objects. The int
formal leaves the entire callee and signed V32 caller raw-identical. Joint
fax results are SIZE19 to SIZE16 (V17), SIZE8 to BYTES20 (V27), and SIZE2 to
BYTES11 (V29), with no changed sibling body or exact loss.

Full-body comparison exposes a separate source-read witness in each residual.
V17 and V29 originally load the START scratch pointer after the zero-count loop
guard; current source binds it before entering the loop. V27 originally writes
the outgoing MRF count slot at 0xa59bb before reading the child owner at
0xa59bf; retained source binds rx first. These observations justify three
bounded two-source domains on the fixed joint count control:

* V17 and V29: bind the scratch pointer in the loop body, allowing the compiler
  to hoist the invariant load into a guarded preheader. Keep the unsigned
  counter, input shift, stores and calls fixed.
* V27: use the child owner directly in the MRF call arguments. Keep all later
  owner reloads and argument values fixed.

Each TU has production, joint and read-boundary cells: nine full-TU compiles.
Require the joint objects to reproduce the preceding cross and inspect the
predicted guard/owner boundary, complete bodies and collateral. If the witness
does not appear, close that domain without neighbouring ordering variants.
Run `tools/recovery_sre_read_boundaries.py` with the same baseline/domain
arguments; artifacts use `build/recovery-sre-read-boundaries/`.

## Retained result and limits

All nine follow-up cells compile, and their production/joint controls reproduce
the corresponding earlier full objects. The guarded pointer forms recover
DemodDataV17 (415 bytes) and DemodDataV29 (398 bytes); direct owner arguments
recover DemodDataV27 (331 bytes). All three complete bodies and canonical
relocations are EXACT: three gains, 1,144 original bytes, no exact loss.
Only those bodies change across the three TUs. All other function bodies,
allocated nontext contents, named data, symbol inventories/bindings and ordered
relocation target sequences agree with their controls. The int-formal callee
and the signed V32 caller remain raw-identical complete objects.

The two matrices contain 25 full-TU compiles and 217 shared-function verdicts.
These counts include controls and repeated bodies, not 217 unique functions.
The original formal width remains unproved. The retained int slot expresses
the witnessed caller promotions while the callee explicitly narrows at its
existing short remaining local. No prototype inconsistency or instruction
barrier is introduced.

`tools/recovery_sre_boundary_audit.py` checks all nine follow-up cells and three
original post-call traces. Its small first-return-use primitive explores
independently proved direct edges and has nine controls: conditional paths
and signed extension fire; cycles, unknown jump-table destinations, unknown
aliases, intervening calls, carrier overwrites, unproved edge ownership and
exhausted state budget refuse. The production traces use no branch
edges before the first extension. This identifies operand use; it establishes
neither a public return type nor path reachability. It is not a general CFG,
alias or compiler-state reconstruction framework.

```sh
python3 tools/recovery_sre_boundary_audit.py --self-test
python3 tools/recovery_sre_boundary_audit.py \
  --output build/recovery-sre-read-boundaries/audit.json
```

The actual retained source passes Gentoo
`make period T='t_fpm_sre t_v17fax t_v27fax t_v29fax t_v32demod' J=8`: five
requested, zero unknown, five passed, zero failed. These are existing fixed
component fixtures; no new input history, tolerance, mutation execution or
public modem lifecycle claim is introduced. Static checks pass for 285 suites
and 10,040 mutation anchors, with zero skipped, invalid or misdirected anchors;
14,457 references resolve with none stale or pending.

An isolated partial link freezes all 300 opening object hashes and substitutes
only the three winning caller objects; the callee and V32 inputs need no byte
change. The text extent moves from 703,340 to 703,356 bytes and positioned
agreement moves from 69,291 to 69,207 / 943,398. Under comparator schema 2,
exact relocation records move from 987 to 986 / 18,317, while symbols remain
421 / 2,931. Both strict semantic and raw gates still fail. This layout-sensitive
decline accompanies three recovered original bodies; it is not suppressed.
Input hashes, link manifest, partial comparison and full census artifacts are
under `build/recovery-sre-partial/`. The integrated campaign must run its full
phase and report its combined results separately.

The initial unprivileged Docker attempt failed before any compilation; its log
is invalid and excluded. The successful runs record the Gentoo compiler and
selected, executed assembler alongside every complete command, including
register renaming and a final bug-reproduction define. No adjacent source,
register or flag domain is left running.
