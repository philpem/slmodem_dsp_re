# Typed offset audit: bounded 45-row manual worksheet

Scope: commit `0e473a213b6be1dcc75e8fd0394e3f87c05ad66a`, V32 only. The first 20 rows are the first 20 clear raw-to-typed access replacements in `src/pump/v32/v32seq.c`, excluding accessor macro definitions and pointer-owner setup. Pairing follows statement position and RHS/operator role. Old constants were read from the commit's `include/dsplib/v32seq.h`; current offsets were independently printed by the Gentoo period GCC 3.4.2 probe in `seq_probe.c`. Hex values shown below are equal on the 20 ordinary rows.

| # | Old statement (commit parent) | New statement (audited commit) | Old offset | Current period `offsetof` | Result |
|---:|---|---|---:|---:|---|
| 1 | `FIELD_US(hdx, V32HDX_GEN_WIDTH) = width;` | `hdx->gen_width = width;` | `0x4e` | `0x4e` | same |
| 2 | `FIELD_US(hdx, V32HDX_GEN_PATTERN) = pattern;` | `hdx->gen_pattern = pattern;` | `0x52` | `0x52` | same |
| 3 | `FIELD_US(hdx, V32HDX_GEN_INDEX_MASK) = top;` | `hdx->gen_index_mask = top;` | `0x4c` | `0x4c` | same |
| 4 | `FIELD_US(hdx, V32HDX_GEN_INDEX) = top;` | `hdx->gen_index = top;` | `0x4a` | `0x4a` | same |
| 5 | `FIELD_US(hdx, V32HDX_GEN_MASK) = ...;` | `hdx->gen_mask = ...;` | `0x50` | `0x50` | same |
| 6 | `int width = FIELD_US(hdx, V32HDX_GEN_WIDTH);` | `int width = hdx->gen_width;` | `0x4e` | `0x4e` | same |
| 7 | `int pattern = FIELD_US(hdx, V32HDX_GEN_PATTERN);` | `int pattern = hdx->gen_pattern;` | `0x52` | `0x52` | same |
| 8 | `unsigned short index = FIELD_US(hdx, V32HDX_GEN_INDEX);` | `unsigned short index = hdx->gen_index;` | `0x4a` | `0x4a` | same |
| 9 | `FIELD_US(hdx, V32HDX_GEN_MASK)` (sequence mask operand) | `hdx->gen_mask` (same operand) | `0x50` | `0x50` | same |
| 10 | `FIELD_US(hdx, V32HDX_GEN_INDEX_MASK)` (sequence count operand) | `hdx->gen_index_mask` (same operand) | `0x4c` | `0x4c` | same |
| 11 | `FIELD_US(hdx, V32HDX_GEN_INDEX) = index;` | `hdx->gen_index = index;` | `0x4a` | `0x4a` | same |
| 12 | `FIELD_US(hdx, V32HDX_DET_WIDTH) = width;` | `hdx->det_width = width;` | `0x54` | `0x54` | same |
| 13 | `FIELD_I(hdx, V32HDX_DET_OUT_MASK) = out_mask;` | `hdx->det_out_mask = out_mask;` | `0x58` | `0x58` | same |
| 14 | `FIELD_I(hdx, V32HDX_DET_TARGET) = target;` | `hdx->det_target = target;` | `0x5c` | `0x5c` | same |
| 15 | `FIELD_I(hdx, V32HDX_DET_MASK) = mask;` | `hdx->det_mask = mask;` | `0x60` | `0x60` | same |
| 16 | `FIELD_I(hdx, V32HDX_DET_REG) = 0;` | `hdx->det_reg = 0;` | `0x64` | `0x64` | same |
| 17 | `FIELD_I(hdx, V32HDX_DET_MATCH) = 0;` | `hdx->det_match = 0;` | `0x68` | `0x68` | same |
| 18 | `int nbits = FIELD_US(hdx, V32HDX_DET_WIDTH);` | `int nbits = hdx->det_width;` | `0x54` | `0x54` | same |
| 19 | `int reg = FIELD_I(hdx, V32HDX_DET_REG);` | `int reg = hdx->det_reg;` | `0x64` | `0x64` | same |
| 20 | `int mask = FIELD_I(hdx, V32HDX_DET_MASK);` | `int mask = hdx->det_mask;` | `0x60` | `0x60` | same |

## Positive-control rows: five known V32stc mismatches

These rows use the same statement-role pairing, then compare the old numeric constant and the current typed member's separately measured period `offsetof`. The current values below came from `known_probe.c`.

| # | Old statement | New statement | Old offset | Current period `offsetof` | Result |
|---:|---|---|---:|---:|---|
| 21 | `FIELD_U16(hdx, V32HDX_SHORT_44) = 0;` | `hdx->short_46 = 0;` | `0x44` | `0x46` | mismatch |
| 22 | `st->r12 = (short)FIELD_U16(fp, V32FP_SHORT_182);` | `st->r12 = (short)(unsigned short)fp->ecc.pwr_out;` | `0x182` | `0x102` | mismatch |
| 23 | `st->r18 = FIELD_INT(fp, V32FP_INT_188);` | `st->r18 = fp->ecc.adapt_near;` | `0x188` | `0x108` | mismatch |
| 24 | `FIELD_S16(fp, V32FP_SHORT_100)` (second condition operand; status read also uses this member) | `fp->mrf.need` (second condition operand / status read) | `0x100` | `0xd4` | mismatch |
| 25 | `FIELD_S16(fp, V32FP_SHORT_102)` (first condition operand and `FPM_div` operand) | `fp->mrf.phase` (first condition operand and `FPM_div` operand) | `0x102` | `0xd6` | mismatch |

## Additional 20 selected V32fpctl pairs

These 20 statement-role pairs are selected accesses from `src/pump/v32/v32fpctl.c`, with nested-owner accesses given priority. The old and new sides are matched by their role in the same changed statement; offsets are compared only afterward. Parenthesized ellipses mark unmodified call arguments omitted from the display.

| # | Old statement | New statement | Old offset | Current period `offsetof` | Result |
|---:|---|---|---:|---:|---|
| 26 | `FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(HDX(modem), V32_HDX_MTD));` | `FPM_MTD_delete((struct fpm_mtd *)HDX(modem)->mtd);` | `0x38` | `0x38` | same |
| 27 | `FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(HDX(modem), V32_HDX_TONE2));` | `FPM_TONE_delete((struct fpm_tone *)HDX(modem)->tone2);` | `0x34` | `0x34` | same |
| 28 | `FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(HDX(modem), V32_HDX_TONE1));` | `FPM_TONE_delete((struct fpm_tone *)HDX(modem)->tone1);` | `0x30` | `0x30` | same |
| 29 | `FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(HDX(modem), V32_HDX_TONE0));` | `FPM_TONE_delete((struct fpm_tone *)HDX(modem)->tone0);` | `0x2c` | `0x2c` | same |
| 30 | `sysdep_free(FIELD_PTR(FP(modem), V32FP_BUF_5038));` | `sysdep_free(FP(modem)->decoder.vtb.paths);` | `0x5038` | `0x5038` | same |
| 31 | `FPM_SRE_free((struct fpm_sre *)(void *)FIELD(FP(modem), V32FP_SRE));` | `FPM_SRE_free(&FP(modem)->sre);` | `0x148` | `0x148` | same |
| 32 | `FPM_MRF_free((struct fpm_mrf *)(void *)FIELD(FP(modem), V32FP_MRF));` | `FPM_MRF_free(&FP(modem)->mrf);` | `0xc4` | `0xc4` | same |
| 33 | `sysdep_free(FIELD_PTR(FP(modem), V32FP_BUF_50CC));` | `sysdep_free(FP(modem)->rx_buf);` | `0x50cc` | `0x50cc` | same |
| 34 | `sysdep_free(FIELD_PTR(FP(modem), V32FP_CLEAN_BUF));` | `sysdep_free(FP(modem)->clean_buf);` | `0x50d0` | `0x50d0` | same |
| 35 | `sysdep_free(FIELD_PTR(HDX(modem), V32_HDX_BUF_A4));` | `sysdep_free(HDX(modem)->buffer);` | `0xa4` | `0xa4` | same |
| 36 | `have = FIELD_U16(FP(modem), V32FP_CLEAN_N);` | `have = FP(modem)->clean_n;` | `0x50d4` | `0x50d4` | same |
| 37 | `return (short *)FIELD_PTR(FP(modem), V32FP_CLEAN_BUF);` | `return (short *)FP(modem)->clean_buf;` | `0x50d0` | `0x50d0` | same |
| 38 | `FIELD_S16(fp, V32FP_SHORT_509C) = 0;` | `((struct v32_fp *)fp)->rx_smc.shift = 0;` | `0x509c` | `0x509c` | same |
| 39 | `FIELD_S16(fp, V32FP_SHORT_5098) = 0;` | `((struct v32_fp *)fp)->rx_smc.mode = 0;` | `0x5098` | `0x5098` | same |
| 40 | `FIELD_S16(dec, V32_DEC_SHORT_04) = 0;` | `dec->short_04 = 0;` | `0x04` | `0x04` | same |
| 41 | `FIELD_S16(fp, V32FP_SHORT_2E) = 0;` | `((struct v32_fp *)fp)->short_2e = 0;` | `0x2e` | `0x2e` | same |
| 42 | `FIELD_U8(modem, V32_OBJ_STATUS) = V32_STATUS_BAD_MODE;` | `((struct v32_modem *)modem)->status = V32_STATUS_BAD_MODE;` | `0x30` | `0x30` | same |
| 43 | `FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;` | `((struct v32_modem *)modem)->flags |= V32_FLAG_FAULT;` | `0x31` | `0x31` | same |
| 44 | `sdm->tap1 = (short)(FIELD_U16(sdm, V32_SDM_TAP1_POS) - shift);` | `sdm->tap1 = (short)(sdm->tap1_pos - shift);` | `0x02` | `0x02` | same |
| 45 | `sdm->tap2 = (short)(FIELD_U16(sdm, V32_SDM_TAP2_POS) - shift);` | `sdm->tap2 = (short)(sdm->tap2_pos - shift);` | `0x04` | `0x04` | same |

The old source calls the final two reads `FIELD_U16`, while the period-measured member type is `short` (signed, size 2). The read feeds a subtraction and is then explicitly converted to `short`; on the measured i386 target the signedness change preserves the low 16-bit result. These rows are address matches with a type-spelling difference, not functional-bug findings.

## Evidence and limits

Earlier offset-only probe runs are preliminary and superseded. The authoritative rerun uses `tools/recovery_typed_owner_probe.c` through `tools/recovery_typed_owner_probe.py`; the runner reads the active period `.build-config` and uses `tools/experiment_toolchain.py` to append `-DDSPLIB_REPRODUCE_BUGS` last. The full command and compiler/assembler identities recorded for that run are:

```sh
docker run --rm --platform linux/386 \
  -v /mnt/zfs/dev/slmodem/claude_re/build/worktrees/ghidra-assisted:/src \
  -v /mnt/zfs/dev/slmodem/claude_re/build/worktrees/ghidra-assisted/build/typed-offset-audit293:/work \
  -w /src ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  /bin/sh -c 'export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; gcc -c -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS -o /work/recovery_typed_owner_probe.o tools/recovery_typed_owner_probe.c && gcc /work/recovery_typed_owner_probe.o -o /work/recovery_typed_owner_probe && /work/recovery_typed_owner_probe'
```

It reported Gentoo GCC `3.4.2-r2` (GCC 3.4.2), target `i386-pc-linux-gnu`, and GNU assembler `2.15.92.0.2 20040927`.

The period probe reports selected member size and signedness as well as offset. For rows 1–20, the `gen_*`/`det_width` members are unsigned short (size 2); `det_out_mask`, `det_target`, `det_mask`, `det_reg`, and `det_match` are signed int (size 4). For rows 26–45, pointer members are size 4; `clean_n` is unsigned short (size 2); `rx_smc.shift`, `rx_smc.mode`, `short_04`, `short_2e`, and the two `v32_sdm` tap-position members are signed short (size 2); `status` and `flags` are unsigned char (size 1). Nested aggregate members' sizes are `sre` 144 and `mrf` 28. Known-control members measured: `short_46` signed short size 2; `ecc.pwr_out` signed short size 2; `ecc.adapt_near` signed int size 4; `mrf.need` and `mrf.phase` signed short size 2.

This is a bounded 45-row sample, not an all-V32 or all-commit denominator. It found no additional address mismatch in the 20 `v32seq.c` rows or 20 selected `v32fpctl.c` rows, and reproduced the five supplied V32stc positive controls. The positive controls are selected examples, not an inference that these five roles enumerate every bug occurrence. No ambiguous rows are included; no source or headers were changed.

Run the durable probe from any checkout after `make tc-repro`:

```sh
python3 tools/recovery_typed_owner_probe.py
```

Use `--config` to select another recorded period `.build-config`. The runner
checks 35 unique member paths: thirty cover the forty ordinary statement
rows, and five reproduce the selected historical wrong-member controls.
Statement pairing remains a manual historical review; this probe does not
automatically detect changed expressions or establish reachability.
