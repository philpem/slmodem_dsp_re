# MRF result-width controls

F11667 follows the [declared three-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958359941) at fe0cace1. Gentoo GCC 3.4.2-r2 and executed assembler 2.15.92.0.2 use the saved production flags and mandatory DSPLIB_REPRODUCE_BUGS. Thirty full-TU compilations cover the helper and nine caller TUs, 80 function definitions. Each raw baseline reproduces its production object.

The baseline returns short. The two alternatives consistently change the shared declaration and definition to unsigned short or int, both returning (unsigned short)produced. Both alternatives emit identical raw objects in every TU. Only FPM_MRF_filter changes: bytes +330 and +418 change 0xbf to 0xb7, sign-extension to zero-extension. Length stays446 bytes versus blob533. All77 caller functions stay unchanged; all symbol metadata, named data, allocated nontext bytes/sizes and canonical relocations agree. No byte-exact function gain occurs. An unsigned-short API is the adopted spelling, not a uniquely recovered original formal type. Signed input counts and twelve callers' explicit narrowing remain unchanged.

The fresh component fixture uses actual init/free and the original B103 transmit coefficient bank, ten branches/nine decimation/270 taps. Input29490 produces32767 outputs; input29491 produces32768. Each is tested with zero and bounded ramp amplitudes ≤1000, keeping the 27-tap sum below signed32 overflow. Buffers have40000 samples. At32768 outputs the last written index is32767; its subsequent signed wrap is followed by exit, not another store. Counts producing more outputs are outside this fixture's valid boundary. The fixture compares the full return, all output including untouched suffix, all27 history samples and full owner after normalizing only the separate allocated history pointers. Four paired calls each report40031 checks. This is an initialized resampler leaf boundary, not public modem lifecycle evidence.

The original small-chunk fixture declared a short reference result and could not observe this bit. The new fixture declares an unsigned-int reference alias to read full EAX; the two explicit MOVZWL exits support that observation. Before adoption, both high-result calls fail exactly the return check, -32768 versus32768; all other checks agree. The corrected source passes its targeted period fixture.

An initial fixture used29491/29492/32767 with incorrect output geometry; larger requests let the blob's signed output index wrap before further stores. That run is INVALID and excluded, preserved in build/mrf-result-width-invalid/{fixture.c,fixture-binary,period.log,INVALID.txt}. Corrected baseline logs are /tmp/mrf-result-valid-baseline-period.log and /tmp/mrf-result-valid-patterns-baseline.log. Adoption log: /tmp/mrf-result-adopted-period.log. Production review measures300 objects: only src_dsp_fpm_mrf.c.o changes and exactly matches the audited unsigned-result candidate. The strict census remains925/1852 and95363 exact bytes, with no gained/lost exact symbols. A same-order baseline partial link replaces only this helper with its raw-reproduced baseline:99/99 section metadata,all2984 symbols agree; only two positioned text bytes differ. Canonical relocations are18218/18218 identical. The complete blob comparison remains DIFFERENT (70/92 shared section records exact,1023/18317 relocations,394/2907 defined symbol records); this is not completion. The fixed suite measures387/0. Initial phase failed solely on malformed finding heading; the corrected final phase passes387/0 and all structural checks (/tmp/mrf-result-phase-final.log). Static anchors:285 suites/10038 anchors, zero detached/nonunique.

Reproduce the matrix from a separate fe0cace1 checkout with the three retained MRF experiment scripts copied into tools/ and its original production objects built. The shared driver intentionally rejects header drift; do not run the historical matrix against the adopted header. The artifact audit and fixed fixture run on the adopted branch:

Matrix commands:

```sh
python3 tools/playbook_mrf_result.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958359941
python3 tools/playbook_mrf_result_audit.py
make period T=t_fpm_mrf_result
```

Artifacts: build/playbook-mrf-result/results.json and complete-object-audit.json. Close this return spelling family. The helper's remaining87-byte deficit needs independent loop/inlining evidence; do not widen input types or callers to fit call-site registers. No fuzzing or mutation execution.
