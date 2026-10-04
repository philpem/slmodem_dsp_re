# Batch 100 data/fax replay package

This index covers 166 valid complete-TU controls against immutable baseline `856c1ecb`: 164 generic full-TU audit cells plus two V.29 selector cells. These are compiler/object comparisons of already reconstructed source, including bounded negative findings; they do not reopen fax reconstruction. The adopted data/fax package contributes 12 source gains and 1,760 exact bytes across seven TUs. Parent integration subsequently verified the complete batch separately.

A yes in the table means a cell includes a retained gain, sometimes supplied by an explicit predecessor; it does not claim a new gain for every family. The only twelve adopted symbols are listed in the combined batch results.

Every driver generates its finite source domain from the fixed Git revision and explicit predecessor transformations. It does not depend on the adopted working source. Seven formerly checkout-dependent drivers were corrected; pure-generator comparison checked 34 historical source cells (25 byte-identical files, nine differing only in formatting/comments). The V.29 Next driver was also compiled again after adoption.

| Family | Valid cells | Driver | Precompile domain | Includes gain, possibly from a predecessor |
| --- | ---: | --- | --- | --- |
| batch100-fax-carrier-arm | 2 | [batch100_fax_carrier_arm.py](../tools/batch100_fax_carrier_arm.py) | [batch100-fax-carrier-arm-domain.md](batch100-fax-carrier-arm-domain.md) | no |
| batch100-fax-class1-state-cfg | 4 | [batch100_fax_class1_state_cfg.py](../tools/batch100_fax_class1_state_cfg.py) | [batch100-fax-class1-state-cfg-domain.md](batch100-fax-class1-state-cfg-domain.md) | yes |
| batch100-fax-fifo-entry | 2 | [batch100_fax_fifo_entry.py](../tools/batch100_fax_fifo_entry.py) | [batch100-fax-fifo-entry-domain.md](batch100-fax-fifo-entry-domain.md) | yes |
| batch100-fax-fifo-read | 4 | [batch100_fax_fifo_read.py](../tools/batch100_fax_fifo_read.py) | [batch100-fax-fifo-read-domain.md](batch100-fax-fifo-read-domain.md) | no |
| batch100-fax-frame-reverse | 8 | [batch100_fax_frame_reverse.py](../tools/batch100_fax_frame_reverse.py) | [batch100-fax-frame-reverse-domain.md](batch100-fax-frame-reverse-domain.md) | yes |
| batch100-fax-gen-eq27 | 8 | [batch100_fax_gen_eq27.py](../tools/batch100_fax_gen_eq27.py) | [batch100-fax-gen-eq27-domain.md](batch100-fax-gen-eq27-domain.md) | no |
| batch100-fax-idle-bound | 2 | [batch100_fax_idle_bound.py](../tools/batch100_fax_idle_bound.py) | [batch100-fax-idle-bound-domain.md](batch100-fax-idle-bound-domain.md) | yes |
| batch100-fax-idle-union | 4 | [batch100_fax_idle_union.py](../tools/batch100_fax_idle_union.py) | [batch100-fax-idle-union-domain.md](batch100-fax-idle-union-domain.md) | yes |
| batch100-fax-no-carrier | 8 | [batch100_fax_no_carrier.py](../tools/batch100_fax_no_carrier.py) | [batch100-fax-no-carrier-domain.md](batch100-fax-no-carrier-domain.md) | no |
| batch100-fax-quality-owner | 8 | [batch100_fax_quality_owner.py](../tools/batch100_fax_quality_owner.py) | [batch100-fax-quality-owner-domain.md](batch100-fax-quality-owner-domain.md) | no |
| batch100-fax-reverse-loops | 8 | [batch100_fax_reverse_loops.py](../tools/batch100_fax_reverse_loops.py) | [batch100-fax-reverse-loops-domain.md](batch100-fax-reverse-loops-domain.md) | yes |
| batch100-fax-ring-writers | 8 | [batch100_fax_ring_writers.py](../tools/batch100_fax_ring_writers.py) | [batch100-fax-ring-writers-domain.md](batch100-fax-ring-writers-domain.md) | yes |
| batch100-fax-rx-epoch | 16 | [batch100_fax_rx_epoch.py](../tools/batch100_fax_rx_epoch.py) | [batch100-fax-rx-epoch-domain.md](batch100-fax-rx-epoch-domain.md) | yes |
| batch100-fax-sdm-init | 2 | [batch100_fax_sdm_init.py](../tools/batch100_fax_sdm_init.py) | [batch100-fax-sdm-init-domain.md](batch100-fax-sdm-init-domain.md) | no |
| batch100-fax-silence-half-lifetime | 2 | [batch100_fax_silence_half_lifetime.py](../tools/batch100_fax_silence_half_lifetime.py) | [batch100-fax-silence-half-lifetime-domain.md](batch100-fax-silence-half-lifetime-domain.md) | yes |
| batch100-fax-status-return | 12 | [batch100_fax_status_return.py](../tools/batch100_fax_status_return.py) | [batch100-fax-status-return-domain.md](batch100-fax-status-return-domain.md) | no |
| batch100-fax-sym-size-result | 2 | [batch100_fax_sym_size_result.py](../tools/batch100_fax_sym_size_result.py) | [batch100-fax-sym-size-result-domain.md](batch100-fax-sym-size-result-domain.md) | yes |
| batch100-fax-tx-config-lifetime | 2 | [batch100_fax_tx_config_lifetime.py](../tools/batch100_fax_tx_config_lifetime.py) | [batch100-fax-tx-config-lifetime-domain.md](batch100-fax-tx-config-lifetime-domain.md) | no |
| batch100-fax-tx-controls | 22 | [batch100_fax_tx_controls.py](../tools/batch100_fax_tx_controls.py) | [batch100-fax-tx-controls-domain.md](batch100-fax-tx-controls-domain.md) | no |
| batch100-fax-v17-scrambler-owner | 4 | [batch100_fax_v17_scrambler_owner.py](../tools/batch100_fax_v17_scrambler_owner.py) | [batch100-fax-v17-scrambler-owner-domain.md](batch100-fax-v17-scrambler-owner-domain.md) | no |
| batch100-fax-v21-control | 4 | [batch100_fax_v21_control.py](../tools/batch100_fax_v21_control.py) | [batch100-fax-v21-control-domain.md](batch100-fax-v21-control-domain.md) | no |
| batch100-fax-v27-scrambler-owner | 4 | [batch100_fax_v27_scrambler_owner.py](../tools/batch100_fax_v27_scrambler_owner.py) | [batch100-fax-v27-scrambler-owner-domain.md](batch100-fax-v27-scrambler-owner-domain.md) | yes |
| batch100-fax-v29-next-flags | 2 | [batch100_fax_v29_next_flags.py](../tools/batch100_fax_v29_next_flags.py) | [batch100-fax-v29-next-flags-domain.md](batch100-fax-v29-next-flags-domain.md) | yes |
| batch100-fax-v29-rx-flags | 8 | [batch100_fax_v29_rx_flags.py](../tools/batch100_fax_v29_rx_flags.py) | [batch100-fax-v29-rx-flags-domain.md](batch100-fax-v29-rx-flags-domain.md) | yes |
| batch100-fax-write-frame | 8 | [batch100_fax_write_frame.py](../tools/batch100_fax_write_frame.py) | [batch100-fax-write-frame-domain.md](batch100-fax-write-frame-domain.md) | yes |
| batch100-fdsp-create-stage-flags | 4 | [batch100_fdsp_create_stage_flags.py](../tools/batch100_fdsp_create_stage_flags.py) | [batch100-fdsp-create-stage-flags-domain.md](batch100-fdsp-create-stage-flags-domain.md) | no |
| batch100-v22-detect-clear | 4 | [batch100_v22_detect_clear.py](../tools/batch100_v22_detect_clear.py) | [batch100-v22-detect-clear-domain.md](batch100-v22-detect-clear-domain.md) | no |
| batch100-v22-trained-loops | 4 | [batch100_v22_trained_loops.py](../tools/batch100_v22_trained_loops.py) | [batch100-v22-trained-loops-domain.md](batch100-v22-trained-loops-domain.md) | no |

The 164-cell audit checks all symbol metadata/bindings, named allocated data, allocated nontext sections and relocation ownership, all bodies including non-exact bystanders, and exact-set losses. Expected helper import removals in literal-loop controls and the single dead `FIFO_CFG + 4` relocation in the transmitter-config lifetime control are individually bounded; neither is treated as an unexplained exception. V.29 Next receives its separate whole-TU audit accounting for the five changed ordered selector destinations.

To replay, run each driver with its linked domain, using the retained `.build-config` and shared experiment toolchain. For example:

```sh
python3 tools/batch100_fax_carrier_arm.py --domain docs/batch100-fax-carrier-arm-domain.md
```

All source compilation uses the retained Gentoo GCC 3.4.2-r2 profile and appended `DSPLIB_REPRODUCE_BUGS`; full commands and executed assembler identity are recorded by the shared driver. This package changes no profile. Baseline controls reproduce raw full-TU objects. Replay needs Docker/the period image, the repository blob, Python ELF tooling, and existing shared experiment helpers. No runtime, fuzzing or mutation execution is part of these replay tools.

After producing all generic family outputs, run:

```sh
python3 tools/batch100_data_unit_audit.py batch100-fax-carrier-arm batch100-fax-class1-state-cfg batch100-fax-fifo-entry batch100-fax-fifo-read batch100-fax-frame-reverse batch100-fax-gen-eq27 batch100-fax-idle-bound batch100-fax-idle-union batch100-fax-no-carrier batch100-fax-quality-owner batch100-fax-reverse-loops batch100-fax-ring-writers batch100-fax-rx-epoch batch100-fax-sdm-init batch100-fax-silence-half-lifetime batch100-fax-status-return batch100-fax-sym-size-result batch100-fax-tx-config-lifetime batch100-fax-tx-controls batch100-fax-v17-scrambler-owner batch100-fax-v21-control batch100-fax-v27-scrambler-owner batch100-fax-v29-rx-flags batch100-fax-write-frame batch100-fdsp-create-stage-flags batch100-v22-detect-clear batch100-v22-trained-loops
python3 tools/batch100_fax_v29_next_audit.py
```

The selector audit uses the adopted shared jump-table prover. The parent proved its apparatus support with positive/refusal controls and an old/new immutable-baseline census that remained 1,020/1,852 and 108,349 bytes; the 467-byte source gain is not an apparatus-only baseline gain.

Supporting tools:
- [batch100_data_screen.py](../tools/batch100_data_screen.py)
- [batch100_data_unit_audit.py](../tools/batch100_data_unit_audit.py)
- [batch100_fax_union_rtl_trace.py](../tools/batch100_fax_union_rtl_trace.py)
- [batch100_fax_v29_selector_trace.py](../tools/batch100_fax_v29_selector_trace.py)
- [batch100_fax_v29_next_audit.py](../tools/batch100_fax_v29_next_audit.py)

The union RTL trace consumes retained `.01.rtl`, `.06.cse`, and `.09.loop` dumps from the corresponding matrix; it places the discriminating join before global register allocation. The original selector trace records the older unsupported-proof result as historical evidence; the separate Next audit is the deciding proof.

Excluded runs: four `batch100-fdsp-init-clear-owner` cells duplicated a closed batch-50 helper/owner domain after an incomplete filename lookup. Their artifacts remain labelled redundant and excluded; the duplicate new driver/domain were removed. The initial V.17 scrambler driver assertion stopped before compilation and is also invalid/excluded. The unnumbered `docs/batch100-data-findings.md` is a delegate handoff draft, not part of this stable package. Parent central findings/playbook entries carry the adopted record.

The named matrix domains are now closed absent a new independent original operand/CFG/RTL witness. Negative cells are retained as stopping evidence, not invitations for register, scratch-cursor, declaration-order or profile fitting.
