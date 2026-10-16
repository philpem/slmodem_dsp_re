# V90Parameters C2: a valid historical floor and a measured collateral loss

The historical floor entry `_ZN13V90ParametersC2EP19_tagModemParameters` was valid when recorded. Its current BYTES4 failure is a real, reproducible register-role difference caused by a later **correct field-type recovery**, not a stale object, a new comparison-tool rejection or a stock/Gentoo compiler switch. Preserve the floor; no production reversion or ratchet lowering was performed.

## What differs now

Both original and current C2 are89 bytes with matching calls, tails, branches and stack extent. Original `0x2a8f1` zeroes EDX, loads14 into ECX at `0x2a8fa`, stores EDX to +0x4f8, and ECX to +0x4fc. Current reverses those two register roles. The four differing encoded bytes are the XOR ModRM, MOV-immediate opcode, and the two field-store ModRM bytes. All remaining normalized bytes/relocation identities agree. C1 remains exact.

## Historical evidence and controls

F7960 records an earlier C2 REGALLOC→EXACT bystander gain during six supported float-field recoveries in `setToDefault`, commit `02895aec` on2026-08-25. The constructor was not edited. Named ratchet entry first appears in `bf40be37ae2fa89b48be06f34608f47b3c3aa40c` on2026-09-06, when the floor acquired749 explicit names. It persists in the775 and810 floors. The recorded default image was stock `dsplibs-tc342`; Gentoo became the default at `5ac4c7bb` on2026-09-09. Later default CXX math flags were added at `5571c83c` on2026-09-10.

Eight valid complete-TU controls,72 emitted-body comparisons, isolate these explanations:

| TU/source and matching header closure | CXX profile | Compiler | C2 | Exact bodies |
| --- | --- | --- | --- | ---: |
| Current75e7ef4b | Current | Gentoo | BYTES4 | 6/9 |
| Current75e7ef4b | Current | Stock | BYTES4 | 6/9 |
| Floor bf40be37 | Floor | Gentoo | EXACT | 7/9 |
| Floor bf40be37 | Current | Gentoo | EXACT | 7/9 |
| Current75e7ef4b | Floor | Gentoo | BYTES4 | 6/9 |
| Immediately before2efa968b | Current | Gentoo | EXACT | 7/9 |
| Commit2efa968b | Current | Gentoo | BYTES4 | 6/9 |
| Floor bf40be37 | Floor | Stock | EXACT | 7/9 |

The Gentoo current control is raw byte-identical to archived production before interpreting any replay. Current stock and Gentoo bodies and complete metadata/data are identical. Floor stock and Gentoo bodies and complete metadata/data are identical too. These controls refute compiler-switch-only causation. Historical CXX profile uses `-fno-exceptions -fno-rtti`; current additionally uses `-fno-math-errno -ffast-math`. Historical floor source is exact under both profiles, current source nonexact under both: those additional options do not explain C2's loss. The full-TU review does expose a distinct `loadModemParamsData` body/anonymous-data change under the historical math profile, so the profile difference is not dismissed as universally inert.

The loss is localized to actual commit **`2efa968b411fd8f9a66288828fbd53eb5b126f18`**,2026-09-18, versus its parent `5fde7e1b49cce3e0b30ad5a8885f74dcd61ab1f8`. Only `V90Parameters.h` changes among the six compiled header inputs. The relevant semantic changes are:

- +0x434 `PDSNR_CURRENT_V34_DROP_THRESH_PHASE4`: `int`→`float`.
- Its `setToDefault` assignment: integer bit pattern `0x437a0000`→`250.0f`.

The type recovery has independent original-source evidence: the corresponding reader uses FLDS/FSTS and its diagnostic names the value. It must not be reverted merely to bank constructor bytes. Constructor, `initSession`, and `init` source remain unchanged.

With the current retained profile, pre/post-retype replay changes **only C2**. All eight bystander bodies—including `setToDefault` itself—are canonical-identical. All bindings/imports/exports, text positions, allocated named/anonymous data, BSS and nontext relocation identities are identical. Post-retype all nine bodies equal the current production bodies. Thus the complete-TU type/literal input changes can alter a later constructor's register choices even while the changed writer's emitted body remains identical. The precise GCC pass/internal state carrier is not yet traced; a global pseudo/UID or other compiler-stream explanation is an inference, not a demonstrated cause.

## Why the ratchet is red and what to do next

The810-name file still contains the historically valid name. Modern immediate exact sets exclude it, so new batches can correctly report zero new losses while the older membership floor remains red. This is not evidence that the original floor was fabricated, nor permission to call it green or silently lower it. Existing documentation in `anonymous-jumptable-proof.md`, `v34-small-rtl.md` and the Playbook preserved the failure but did not previously localize the change.

Next discriminating control is **diagnostic replay of these actual two source/header commits under one fixed retained profile**, adding dumps only and requiring raw equality with the already audited pre/post objects. Locate the first constructor pseudo/register-role divergence and trace its pass decisions back to the field-type input; review complete TU function emission order and scratch/rename histories. Do not permute declarations/registers/slots or reconstruct an integer workaround around the correctly typed float. This finite historical witness justifies compiler tracing; it does not yet justify a source fix or alternative original-profile claim.

## Reproduce

From this isolated checkout (source is asserted equal to75e7ef4b), with archived production baseline in build/reload-baseline:

```sh
python3 tools/v90_parameters_ratchet_ab.py
python3 tools/v90_parameters_ratchet_history.py
python3 tools/v90_parameters_ratchet_history.py --retype-era
python3 tools/v90_parameters_ratchet_history.py --floor-stock
python3 tools/v90_parameters_ratchet_audit.py
```

Artifacts live in `build/v90-parameters-ratchet-ab`: `results.json`, `history.json`, `retype-era.json`, `floor-stock.json`, `audit.json`, complete commands and identities, dependency files, focused historical source/header snapshots, source/header/object hashes and body grades. Compiler-selected assembler is executed: Gentoo GCC3.4.2-r2 selects GNU2.15.92.0.2; stock GCC3.4.2 selects GNU2.15. Every actual compiler command appends the required `DSPLIB_REPRODUCE_BUGS` define last, including the historical forensic controls; no configurable define is silently omitted. Replays use six actual compiler-reported source-header dependencies and refuse historical fallback to current headers. All eight controls retain the same emitted-symbol/binding/BSS/nontext universe. No V34 source, other-session structure source, issue edits, mutation/fuzz/runtime, production source/header edit, commit or push was performed.

The proposed dump-only control is now complete: [stage proof](v90-parameters-constructor-stage-proof.md)
locates the first divergence at28.peephole2, then final colors at30.rnreg.
Both constructor patterns agree through27.flow2; no source rollback or
spelling permutations were introduced.
