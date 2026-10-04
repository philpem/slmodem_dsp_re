# Batch 20 ownership and exclusions

Baseline93d7eee1, independent of PR245. Never inspect re/.
No fuzzing/mutation; full period gate deferred to integrated batch.

## Other session: excluded from edits

- docs/diffcoder-operand-order.md
- docs/findings.md
- docs/method/refinement.md
- docs/v8crc-v92cp-ctor-screen.md
- docs/v90-jd-arm-order.md
- docs/v90jd-unpackword-recovery.md
- include/dsplib/DiffCoder.h
- include/dsplib/Scrambler.h
- include/dsplib/v22fp.h
- src/core/FixedRC.c
- src/core/dp_param.c
- src/dsp/FloatFIR.cpp
- src/dsp/FloatIIR.cpp
- src/dsp/fpm_fse.c
- src/pump/v22/v22mod.c
- src/pump/v32/V32TXHDX.c
- src/pump/v32/V32dec.c
- src/pump/v34/v34info.c
- src/pump/v90/V90Jd.cpp
- src/pump/v90/V90MappingParamsInt.cpp
- src/pump/v90/V90Phase3Demodulator.cpp
- src/pump/v90/V90Phase3Modulator.cpp
- src/pump/v90/V90Phase4Demodulator.cpp
- src/pump/v90/V90Phase4Modulator.cpp
- src/pump/v90/V90SpectralShaper.cpp
- src/pump/v90/V92CP.cpp
- src/pump/v90/V92Phase4Modulator.cpp
- src/service/Beepgen.c
- src/service/Fdsp.c
- src/service/rd.c
- test/mutations/v90jd.json
- test/mutations/v90jdstd.json
- test/mutations/v90p3ddec.json
- test/mutations/v90p4dctor.json
- test/mutations/v92cp.json
- test/mutations/v92cpcrc.json
- tools/dump_wave2.py
- tools/dump_wave2_targets.py
- tools/playbook_amplitude_ext_jdnot_arm.py
- tools/playbook_ctor_tu_position.py
- tools/playbook_descrambler_count.py
- tools/playbook_diffcoder_flip.py
- tools/playbook_e2u_wave2.py
- tools/playbook_fdsp_dprt.py
- tools/playbook_fdsp_dprt_r2.py
- tools/playbook_floatfir_ctor.py
- tools/playbook_queue_segptr.py
- tools/playbook_rd_nodefault.py
- tools/playbook_scrambler_taps.py
- tools/playbook_v34_residual.py
- tools/playbook_v8crc_msb_v92cp_deforder.py
- tools/playbook_v8crc_v92cp_ctor.py
- tools/playbook_v90jd_locals.py
- tools/playbook_v90jd_pos.py
- tools/playbook_v90jd_unpackword.py
- tools/playbook_v90rdetector_notfamily.py
- tools/playbook_v90rdetector_uncond.py
- tools/playbook_v92cp_cross.py

Recheck issue22 and PR245 before integration. Shared findings/Playbook receive
only isolated additions; findings allocated centrally, not concurrently.

## Our isolated assignments

- Data agent: b103, v23, v22 except v22mod.c/V22.c, v32 except V32dec.c/V32TXHDX.c; b103fp.h return controls only.
- DSP agent: src/dsp C files except fpm_fse.c and tables; no headers.
- V90 agent: CPpck, DilDescriptorSettings, Phase2Info, TRN2dDesigner, SignBitsExtractor, ModulusEncoder, ModulusDecoder, ConstellationPower, AutoDigitalImpDetector, MP, CP, Parameters and ConnectionEvaluator cpp files; no headers/templates.
- Root: remaining eligible paths after those exclusions.

All findings and Playbook additions are collected centrally at integration.
