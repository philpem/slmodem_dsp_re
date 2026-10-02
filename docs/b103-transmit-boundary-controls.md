# B.103 transmit lifetime and extension controls

F11666 records two sequential domains at6cde9a9d: [two lifetime cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958072288), then [eight crossed cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958117925). Both replay the complete B103prc.c TU with the saved .build-config, Gentoo GCC3.4.2-r2, executed assembler2.15.92.0.2 and DSPLIB_REPRODUCE_BUGS. Raw baselines reproduce production. Ten valid compiles cover eight source variants; the eight-cell cross has four distinct raw emissions because explicit versus implicit count conversion is code-identical.

The reference ModDataB103 reloads fp->dsp after FPM_FSM_modulate at0x8f9e3; TxNoCarrierB103 reloads at0x8fa55 before restoring scale. The retained source caches dsp across the call. Removing only those locals restores two root+0x54 loads per wrapper, versus one. It changes both wrappers plus inlined TxHdxDataB103, TxHdxMarksB103 and TxHdxSilenceB103. Strict exactness stays4/17.

The remaining four-byte wrapper deficits contain two CWTL instructions where the blob has MOVZWL AX. The next cross keeps MRF's signed-short count API/callee and all helper declarations intact. It crosses owner reload, unsigned-short wrapper results (matching header/definition, removing outer short return cast), and removal of the explicit short cast on unsigned nsamples. There is no fabricated caller prototype or header/definition divergence.

| Lifetime/result axes | ModDataB103 bytes, blob99 | TxNoCarrierB103 bytes, blob124 | Root loads per wrapper | Signed extensions per wrapper |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 92 | 117 | 1 | 2 |
| Unsigned result | 94 | 119 | 1 | 1 |
| Owner reload | 95 | 120 | 2 | 2 |
| Both | 97 | 122 | 2 | 1 |

Each implicit-count counterpart emits the same raw object as its explicit-count sibling. Thus an implicit conversion to the current signed-short formal does not explain the call-site zero extension. The unsigned wrapper result restores exactly one zero extension; no complete body becomes exact. Exactness remains4/17, without gains or losses.

All17 function definitions and three named data objects are reviewed. Symbol types, bindings, visibility, import/export inventory, named data, allocated nontext contents/sizes and canonical relocations agree between cells. The dedicated audit also verifies observed reload and extension counts against disassembly. No source, API, candidate-runtime or reachable pointer-alias claim is adopted.

Existing t_b103fp constructs paired owners via actual B103FP_create and has780 paired transmit calls in its initial sections:300 modulation calls, then360 silence plus120 interleaved modulation calls. It compares scale, resampler state/history, scratch and outputs. Supported1..6bit requests fit the constructor's scratch buffer. This establishes a created component boundary, not public modem history or permission to overwrite a pointer through modulator output. No fabricated pointer-corruption fixture is added.

Reproduce:

```sh
python3 tools/playbook_b103_tx_reload.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958072288
python3 tools/playbook_b103_tx_width.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958117925
python3 tools/playbook_b103_tx_audit.py
```

Artifacts: build/playbook-b103-tx-{reload,width}/results.json and complete-object-audit.json. Close both bounded families; no register/declaration/store-order permutations or unsigned helper-input prototype merely to fit the call.

## Separate MRF return evidence

A five-function screen across B103prc.c and v23tx.c covers20 function/four named-data definitions; it does not classify the remainder. It also observes both FPM_MRF_filter exits zero-extending produced counts in the reference, versus retained signed extension. Twelve direct relocation calls occur in twelve blob caller symbols. v23FP_rx_progress, DemodDataV32 and DemodDataB103 independently zero-extend AX; cid_modem explicitly narrows with CWTL. Preserve caller-local narrowing rather than propagating a guessed width through every consumer.

The filter's incoming count is read as signed16bits, but that instruction alone does not uniquely identify a prototype: a wider formal narrowed inside the function, or period old-style parameter declarations, could implement the same boundary. Neither is measured here. Likewise zero extension does not uniquely distinguish an unsigned-short return declaration from an int result carrying a narrowed unsigned value. The next independent check needs a fixed filter fixture which observes an output count above32767, supported by real initialization and ample buffers, then a bounded full-helper/caller source experiment. Existing initialization-only t_fpm_mrf does not test filter results; existing t_fpm_mrf_filter declares its reference result short and exercises small chunks, hiding that distinction. Do not adopt a type solely from a load/extension mnemonic.

Production stays925/1852; no reconstruction-source change, prior deciding fixed gate386/0. No fuzzing/mutation execution.
