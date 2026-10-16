# FIFO result capture and published-count ownership

Baseline `4169cb3b`, complete `src/fax/cDATAtx.c` TU, retained Gentoo
GCC 3.4.2-r2 profile and final `DSPLIB_REPRODUCE_BUGS`. The opening
[domain](recovery-fifo-capture-domain.md) was nominated by the broader
[residual ledger](../tools/recovery_residual_ledger.py), outside the legacy
fax-excluding value-timing screen. No FIFO prototype changes were made.

## Independent original witnesses

The original null-state handler consumes the low unsigned word after
`FIFO_write` at `9d0d2` (`MOVZWL AX,ECX`, `9d0d7`) and after `FIFO_read`
at `9d0fa` (`MOVZWL AX,EAX`, `9d0ff`). The scrambled-ones handler does the
same at `9d404` / `9d409`. The retained source consumed full EAX at all
three boundaries. Both FIFO bodies compute unsigned-short counts and return
those values, so the caller casts preserve every valid return value while
restoring the original operand widths. This does not prove that the original
API return type was unsigned short; local captures suffice.

After those captures, the scrambled-ones residual has exactly two different
instruction bytes: the comparison's operand order and complementary signed
branch. The source compares an invented `rd` local after publishing it to
`*tx_data_count`. Comparing the published value instead recovers the complete
original 681-byte body and relocation targets. The null-state tail also
zero-extends the FIFO capacity in the original (`9d164`), whereas the source
sign-extended its shared signed field. A local unsigned-short cast recovers
that use without changing other readers' types.

## Finite experiments and result

The opening four full-TU cells cross the two read captures with the null-state
write capture. They restore the three predicted low-word boundaries and
preserve all other functions, but produce no exact-function gain.

A second domain crosses published-count comparison ownership, the null-state
capacity-read width and one original-position input-pointer acquisition:
eight cells plus an unchanged raw control. The pointer acquisition produces
no object change and is not retained. The retained cell uses the published
owner and zero-extended capacity. Each unchanged complete TU raw-reproduces
production. All nine second-domain compiles succeed; four cells recover the
same exact scrambled-ones body, so the original spelling is not uniquely
established. The capacity axis independently explains its one width byte.

| Shared body | Before | Retained |
| --- | --- | --- |
| `_tx_scrambled_ones_state` | SIZE3 | EXACT, 681 original bytes |
| `_tx_nulls_state` | BYTES106 | BYTES19, 435 bytes |
| Four other bodies | Unchanged | Unchanged |

The full TU improves from 2/6 to 3/6 exact, with no exact losses. All
allocated nontext bytes and NOBITS extents agree. The null-state residual
retains argument-load/store scheduling differences; the exhausted local
capture axis is not a reason to permute source declarations or force registers.
The signed `_tx_data_state` FIFO capture is deliberately outside this domain.

An opening generator safely refused a duplicate prose/code tail match before
any compile; its invalid log remains separate. The corrected generator
scopes the actual function body and writes a fresh output directory.

## Validation and replay

`make period T='t_class1txstates t_faxframing t_class1create' J=3` passes all
three requested binaries. The state fixture uses planted Class1 shells with
constructor-created independent FIFOs and a null-slot VMI; it is a bounded
component fidelity probe, not public modem lifecycle evidence. No new fixture,
tolerance, mutation descriptor or snapshot was required for the exact body.
The original high-bit capacity operand is an instruction witness; this work
makes no claim that a high-bit capacity is constructor-reachable.

The earlier selection containing nonexistent `t_faxfifo` was rejected before
any test ran. Its log is preserved as invalid, not counted as validation.
The integrated campaign's final full-phase and object census are recorded
separately in the implementation report.

```sh
python3 tools/recovery_fifo_capture.py --domain docs/recovery-fifo-capture-domain.md --baseline-dir build/tc_repro
python3 tools/recovery_fifo_owners.py --domain docs/recovery-fifo-capture-domain.md --baseline-dir build/tc_repro
```

These runners pin the opening revision's headers. Use `--historical-headers`
when replaying after later header changes, and provide the archived opening
object/config directory. Artifacts remain in `build/recovery-fifo-capture/`,
`build/recovery-fifo-owners-v2/` and `build/recovery-baseline/`; their JSON
records retain compiler commands, source/header/object hashes, per-body
verdicts, gains/losses and changed-body disassembly. No fuzzing, mutation
execution, modern portability or final-object equality is claimed.
