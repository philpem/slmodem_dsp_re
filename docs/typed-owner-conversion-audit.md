# Historical V32 typed-owner access audit

Scope: commit `0e473a213b6be1dcc75e8fd0394e3f87c05ad66a`, V32 only. The initial worksheet below is sampled coverage; the complete sequence and control-file ledgers later in this document supersede those two samples. File paths in the historical ledgers refer to that commit, before later translation-unit consolidation. The first 20 rows are the first 20 clear raw-to-typed access replacements in `src/pump/v32/v32seq.c`, excluding accessor macro definitions and pointer-owner setup. Pairing follows statement position and RHS/operator role. Old constants were read from the commit's `include/dsplib/v32seq.h`; current offsets were independently printed by the Gentoo period GCC 3.4.2 probe in `seq_probe.c`. Hex values shown below are equal on the 20 ordinary rows.

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


## Additional bounded batches: 120 ordinary access occurrences

These two batches extend the worksheet with 40 selected statement-role pairs from `src/pump/v32/V32rxhdx.c` and 40 selected pairs from `src/pump/v32/V32.c`. Together with the original 20 `v32seq.c` and 20 `v32fpctl.c` rows, the audit records 120 ordinary access occurrences across four files, plus the five selected V32stc positive controls. Member paths overlap across batches, so unique-member denominators are reported separately and are not added together. These are bounded selections, not an exhaustive inventory of the commit.

Both probes were rerun against the canonical worktree configuration at `build/tc_repro/.build-config`. The exact compiler flags after the shared helper appended one reproduction define were `-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS`. The canonical config contains a duplicate reproduction define in its stored flags; the runners remove configured copies and let `tools/experiment_toolchain.py` append exactly one. The earlier probes used the outer `build/tc_out/.build-config`, whose filtered effective flags were identical; the canonical-config reruns below are authoritative.

### Additional 40 selected V32rxhdx accesses

The table numbers continue the worksheet at 46–85. Each row is paired by statement role before comparing its old constant with the measured typed member. Repeated fields remain separate access occurrences.

| # | Old access and `FIELD_*` type | New typed access | Old offset | Period member offset | Current member type / size | Address/width |
|---:|---|---|---:|---:|---|---|
| 46 | `FIELD_U32(hdx, V32HDX_TIMER)` | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 47 | `FIELD_S16(hdx, V32HDX_SYMBOL_LEN)` | `hdx->symbol_len` | `0x9e` | `0x9e` | `short`, 2 | match |
| 48 | `FIELD_S16(modem, V32_OBJ_PROTOCOL)` | `((struct v32_modem *)modem)->params.protocol` | `0x00` | `0x00` | `short`, 2 | match |
| 49 | `FIELD_INT(modem, V32_OBJ_OPTIONS)` | `((struct v32_modem *)modem)->params.options` | `0x10` | `0x10` | `unsigned int`, 4 | match |
| 50 | `FIELD_INT(hdx, V32HDX_INT_78)` | `hdx->state_left` | `0x78` | `0x78` | `int`, 4 | match |
| 51 | `FIELD_S16(HDX(modem), V32HDX_MODE)` | `HDX(modem)->mode` | `0x76` | `0x76` | `short`, 2 | match |
| 52 | `FIELD_U32(hdx, V32HDX_TIMER)` | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 53 | `FIELD_U32(hdx, V32HDX_LIMIT)` | `hdx->limit` | `0x80` | `0x80` | `unsigned int`, 4 | match |
| 54 | `FIELD_U8(modem, V32_OBJ_FLAGS)` | `((struct v32_modem *)modem)->flags` | `0x31` | `0x31` | `unsigned char`, 1 | match |
| 55 | `FIELD_U8(modem, V32_OBJ_STATUS)` | `((struct v32_modem *)modem)->status` | `0x30` | `0x30` | `unsigned char`, 1 | match |
| 56 | `FIELD_S16(hdx, V32HDX_STATE)` | `hdx->state` | `0x74` | `0x74` | `short`, 2 | match |
| 57 | `FIELD_U32(hdx, V32HDX_TIMER)` | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 58 | `FIELD_S16(hdx, V32HDX_SYMBOL_LEN)` | `hdx->symbol_len` | `0x9e` | `0x9e` | `short`, 2 | match |
| 59 | `FIELD_U16(hdx, V32HDX_SHORT_AA)` (assignment target) | `hdx->short_aa` | `0xaa` | `0xaa` | `short`, 2 | match |
| 60 | `FIELD_U16(hdx, V32HDX_SHORT_AA)` (sum operand) | `hdx->short_aa` | `0xaa` | `0xaa` | `short`, 2 | match |
| 61 | `FIELD_U16(hdx, V32HDX_SYMBOL_LEN)` (sum operand) | `hdx->symbol_len` | `0x9e` | `0x9e` | `short`, 2 | match |
| 62 | `FIELD_U32(hdx, V32HDX_TIMER)` | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 63 | `FIELD_S16(hdx, V32HDX_MODE)` | `hdx->mode` | `0x76` | `0x76` | `short`, 2 | match |
| 64 | `FIELD_U32(hdx, V32HDX_TIMER)` | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 65 | `FIELD_U32(hdx, V32HDX_LIMIT)` | `hdx->limit` | `0x80` | `0x80` | `unsigned int`, 4 | match |
| 66 | `FIELD_U8(modem, V32_OBJ_FLAGS)` | `((struct v32_modem *)modem)->flags` | `0x31` | `0x31` | `unsigned char`, 1 | match |
| 67 | `FIELD_U8(modem, V32_OBJ_STATUS)` | `((struct v32_modem *)modem)->status` | `0x30` | `0x30` | `unsigned char`, 1 | match |
| 68 | `FIELD_S16(hdx, V32HDX_STATE)` | `hdx->state` | `0x74` | `0x74` | `short`, 2 | match |
| 69 | `FIELD_U32(hdx, V32HDX_TIMER)` | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 70 | `FIELD_S16(hdx, V32HDX_SYMBOL_LEN)` | `hdx->symbol_len` | `0x9e` | `0x9e` | `short`, 2 | match |
| 71 | `FIELD_U16(hdx, V32HDX_SHORT_A8)` (assignment target) | `hdx->short_a8` | `0xa8` | `0xa8` | `short`, 2 | match |
| 72 | `FIELD_U16(hdx, V32HDX_SHORT_A8)` (sum operand) | `hdx->short_a8` | `0xa8` | `0xa8` | `short`, 2 | match |
| 73 | `FIELD_S16(hdx, V32HDX_SYMBOL_LEN)` (sum operand) | `hdx->symbol_len` | `0x9e` | `0x9e` | `short`, 2 | match |
| 74 | `FIELD_INT(hdx, V32HDX_INT_90)` | `hdx->int_90` | `0x90` | `0x90` | `int`, 4 | match |
| 75 | `FIELD_U32(hdx, V32HDX_TIMER)` (assignment target) | `hdx->timer` | `0x7c` | `0x7c` | `unsigned int`, 4 | match |
| 76 | `FIELD_S16(hdx, V32HDX_MODE)` | `hdx->mode` | `0x76` | `0x76` | `short`, 2 | match |
| 77 | `FIELD_S16(hdx, V32HDX_RTD)` (assignment target, originate arm) | `hdx->rtd` | `0x96` | `0x96` | `short`, 2 | match |
| 78 | `FIELD_S16(hdx, V32_HDX_SHORT_94)` | `hdx->turnaround` | `0x94` | `0x94` | `short`, 2 | match |
| 79 | `FIELD_S16(hdx, V32HDX_RTD)` (assignment target, other arm) | `hdx->rtd` | `0x96` | `0x96` | `short`, 2 | match |
| 80 | `FIELD_U16(hdx, V32HDX_SYMBOL_LEN)` | `hdx->symbol_len` | `0x9e` | `0x9e` | `short`, 2 | match |
| 81 | `FIELD_U16(hdx, V32_HDX_SHORT_94)` | `hdx->turnaround` | `0x94` | `0x94` | `short`, 2 | match |
| 82 | `FIELD_U16(hdx, V32_HDX_SHORT_9C)` | `hdx->short_9c` | `0x9c` | `0x9c` | `short`, 2 | match |
| 83 | `FIELD_U16(hdx, V32_HDX_SHORT_98)` | `hdx->short_98` | `0x98` | `0x98` | `short`, 2 | match |
| 84 | `FIELD_U16(hdx, V32_HDX_SHORT_9A)` | `hdx->short_9a` | `0x9a` | `0x9a` | `short`, 2 | match |
| 85 | `FIELD_S16(hdx, V32HDX_RTD)` (debug-print read) | `hdx->rtd` | `0x96` | `0x96` | `short`, 2 | match |

## Findings

The 40 selected accesses resolve to 18 distinct member paths. All 40 old offsets equal the period-measured member offsets, and the old access width equals the member width. This sample found no wrong address or width.

There are signedness differences worth documenting, but none is an address/width mismatch:

- `FIELD_INT` reads `V32_OBJ_OPTIONS` as signed `int`; `params.options` is measured as `unsigned int`. The access is used as `(options & 0x400) != 0`. The usual arithmetic conversion makes the signed value unsigned for this bitwise operation, preserving the tested bits.
- `FIELD_U16` accesses at `SHORT_AA` and `SHORT_A8` now name signed `short` members. The accumulator writes explicitly convert the sum to `unsigned short` before storing; on this i386 layout the stored low 16 bits are preserved.
- The `FIELD_U16` reads of `symbol_len`, `turnaround`, and the `0x9c/0x98/0x9a` shorts occur in an expression whose result is explicitly converted to `short` for the RTD assignment. The low 16-bit result is preserved on the measured target. These are not reported as functional bugs.

The rows are selected statement-role pairs, not every conversion in this file. The probe checks each of the 18 unique selected member paths, including offsets, sizes, and current signedness. It uses the active period `.build-config` and the shared `tools/experiment_toolchain.py`; the helper appends one `-DDSPLIB_REPRODUCE_BUGS` after configured flags.

The canonical-config run is recorded in `build/typed-offset-audit293/rxhdx_probe.log`; source and runner are `build/typed-offset-audit293/rxhdx_probe.c` and `build/typed-offset-audit293/run_rxhdx_probe.py`. Gentoo GCC 3.4.2-r2 and GNU assembler 2.15.92.0.2 were reported. The probe measured 18 distinct selected member paths; all 40 selected access occurrences matched their old offsets and widths.

### Additional 40 selected V32FP_recreate accesses

The table numbers continue the worksheet at 86–125. Selection prioritizes FP/HDX nested owners, decoder initialization, allocation slots and tone/MTD subobjects. Pairing follows the same assignment, call argument or indexed role in the historical diff; offsets were compared only after pairing.

| # | Role in `V32FP_recreate` | Old access / old offset | New typed path / current expected offset | Result |
|---:|---|---|---|---|
| 86 | Allocate FP subobject through modem | `FIELD_PTR(modem,V32_OBJ_FP)` `0x68` | `v32_modem.fp` `0x68`, ptr4 | same by root assertion |
| 87 | Store allocated HDX owner | `FIELD_PTR(modem,V32_OBJ_HDX)` `0x64` | `v32_modem.hdx` `0x64`, ptr4 | same by root assertion |
| 88 | Initialize HDX tone0 slot | `FIELD_PTR(hdx,V32_HDX_TONE0)` `0x2c` | `v32_hdx.tone0` `0x2c`, ptr4 | same; prior period probe |
| 89 | Initialize HDX MTD slot | `FIELD_PTR(hdx,V32_HDX_MTD)` `0x38` | `v32_hdx.mtd` `0x38`, ptr4 | same; prior period probe |
| 90 | Store allocated HDX state buffer | `FIELD_PTR(hdx,V32_HDX_BUF_A4)` `0xa4` | `v32_hdx.buffer` `0xa4`, ptr4 | same by root assertion |
| 91 | Store allocated receive buffer | `FIELD_PTR(FP(modem),V32FP_BUF_50CC)` `0x50cc` | `v32_fp.rx_buf` `0x50cc`, ptr4 | same by root assertion |
| 92 | Store allocated clean buffer | `FIELD_PTR(FP(modem),V32FP_CLEAN_BUF)` `0x50d0` | `v32_fp.clean_buf` `0x50d0`, ptr4 | same by root assertion |
| 93 | Set selected TX rate | `FIELD_S16(fp,V32FP_TX_RATE_INDEX)` `0x28` | `v32_fp.tx_rate_index` `0x28`, s16 | same by rate assertion/declaration |
| 94 | Set selected RX rate | `FIELD_S16(fp,V32FP_RX_RATE_INDEX)` `0x2a` | `v32_fp.rx_rate_index` `0x2a`, s16 | same by rate assertion/declaration |
| 95 | Select HDX mode during setup | `FIELD_S16(hdx,V32HDX_MODE)` `0x76` | `v32_hdx.mode` `0x76`, s16 | same; prior period probe |
| 96 | Index GPC with HDX mode | `FIELD_S16(hdx,V32HDX_MODE)` `0x76` | `v32_hdx.mode` `0x76`, s16 | same; role is table index read |
| 97 | Index GPA with HDX mode | `FIELD_S16(HDX(modem),V32HDX_MODE)` `0x76` | `v32_hdx.mode` `0x76`, s16 | same; prior period probe |
| 98 | Set TX scrambler tap position 1 | `FIELD_U16(sdm,V32_SDM_TAP1_POS)` `0x02` | `v32_sdm.tap1_pos` `0x02`, signed short2 | same address/width; signedness differs |
| 99 | Set TX scrambler tap position 2 | `FIELD_U16(sdm,V32_SDM_TAP2_POS)` `0x04` | `v32_sdm.tap2_pos` `0x04`, signed short2 | same address/width; signedness differs |
| 100 | Read tap position 1 before shift | same constant `0x02` | `v32_sdm.tap1_pos` `0x02`, short2 | same address/width; final `(short)` cast retains low16 result |
| 101 | Read tap position 2 before shift | same constant `0x04` | `v32_sdm.tap2_pos` `0x04`, short2 | same address/width; final `(short)` cast retains low16 result |
| 102 | Initialize nested AGC state | `FIELD(FP(modem),V32FP_AGC)` `0x1d8` | `v32_fp.agc` `0x1d8` | same by enclosing struct layout |
| 103 | Set FSE owner to decoder subobject | `FIELD(FP(modem),V32FP_DEC)` `0x5020` | `v32_fp.decoder` `0x5020` | same by decoder assertion |
| 104 | Read RX rate into local selector | `FIELD_S16(fp,V32FP_RX_RATE_INDEX)` `0x2a` | `v32_fp.rx_rate_index` `0x2a`, s16 | same |
| 105 | Clear retrain count via FSE owner | `FIELD_S16(DEC(fp),V32_DEC_RETRAIN_N)` `0x74` | `FSE(fp)->cfg.owner` cast to `v32_dec`, `short_74` `0x74` | same; owner set to `&FP(modem)->decoder` before FSE init |
| 106 | Clear variable-index symbol slot | `FIELD_S16(DEC(fp),0x52+2*i)` | `v32_dec.sym[i]`, `0x52 + 2*i`, short2 | same for i=0..5 by union layout |
| 107 | Clear decoder short at +4 | `FIELD_S16(DEC(fp),V32_DEC_SHORT_04)` `0x04` | `v32_dec.short_04` `0x04`, short2 | same |
| 108 | Clear decoder short at +6 | `FIELD_S16(DEC(fp),0x06)` `0x06` | `v32_dec.short_06` `0x06`, short2 | same |
| 109 | Initialize nested Viterbi state | `FIELD(DEC(fp),0x18)` via `DEC(fp)->vtb` | `v32_dec.vtb` `0x18` | same; decoder assertion |
| 110 | Install first encoder function | `FIELD(fp,V32FP_ENCODERS)[0]` `0x98` | `v32_fp.encoders[0]` `0x98`, fnptr4 | same |
| 111 | Install second encoder function | `FIELD(fp,V32FP_ENCODERS)[1]` `0x9c` | `v32_fp.encoders[1]` `0x9c`, fnptr4 | same |
| 112 | Install third encoder function | `FIELD(fp,V32FP_ENCODERS)[2]` `0xa0` | `v32_fp.encoders[2]` `0xa0`, fnptr4 | same |
| 113 | Initialize encoder selector | `FIELD_S16(fp,V32FP_ENCODER_SEL)` `0xa4` | `v32_fp.encoder_sel` `0xa4`, short2 | same |
| 114 | Install I constellation map | `FIELD_PTR(fp,V32FP_IMAP)` `0xa8` | `v32_fp.imap` `0xa8`, ptr4 | same |
| 115 | Install Q constellation map | `FIELD_PTR(fp,V32FP_QMAP)` `0xac` | `v32_fp.qmap` `0xac`, ptr4 | same |
| 116 | Reset tone0 slot for fresh create | `FIELD_PTR(hdx,V32_HDX_TONE0)` `0x2c` | `v32_hdx.tone0` `0x2c`, ptr4 | same |
| 117 | Tone0 create destination | `FIELD_PTR(hdx,V32_HDX_TONE0)` `0x2c` | `v32_hdx.tone0` `0x2c`, ptr4 | same |
| 118 | Tone0 create initial object argument | `FIELD_PTR(HDX(modem),V32_HDX_TONE0)` `0x2c` | `v32_hdx.tone0` `0x2c`, ptr4 | same |
| 119 | Reset tone1 slot | `FIELD_PTR(hdx,V32_HDX_TONE1)` `0x30` | `v32_hdx.tone1` `0x30`, ptr4 | same |
| 120 | Tone1 create destination | `FIELD_PTR(hdx,V32_HDX_TONE1)` `0x30` | `v32_hdx.tone1` `0x30`, ptr4 | same |
| 121 | Tone1 create initial object argument | `FIELD_PTR(HDX(modem),V32_HDX_TONE1)` `0x30` | `v32_hdx.tone1` `0x30`, ptr4 | same |
| 122 | Reset tone2 slot | `FIELD_PTR(hdx,V32_HDX_TONE2)` `0x34` | `v32_hdx.tone2` `0x34`, ptr4 | same |
| 123 | Tone2 create destination | `FIELD_PTR(hdx,V32_HDX_TONE2)` `0x34` | `v32_hdx.tone2` `0x34`, ptr4 | same |
| 124 | Tone2 create initial object argument | `FIELD_PTR(HDX(modem),V32_HDX_TONE2)` `0x34` | `v32_hdx.tone2` `0x34`, ptr4 | same |
| 125 | Zero HDX elapsed timer in constructor initialization | `FIELD_INT(hdx,V32HDX_ELAPSED)` `0x7c` | `v32_hdx.timer` `0x7c`, u32 | same; period probe |

## Finding

Among these 40 selected address roles, the period probe and independent old literals show no additional address mismatch. The tap-position rows change signedness from the old unsigned access macro to signed `short`; both old writes explicitly cast to `unsigned short`, and reads subtract `shift` then cast the result to `short`, so these rows are not classified as functional bugs from signedness alone. Row 20's owner indirection is identity-supported by the preceding assignment of the same decoder object to `fsecfg.owner` before `FPM_FSE_init`; no name-only inference was used.

The canonical-config run is recorded in `build/typed-offset-audit293/v32_probe.log`; source and runner are `build/typed-offset-audit293/v32_probe.c` and `build/typed-offset-audit293/run_v32_probe.py`. Gentoo GCC 3.4.2-r2 and GNU assembler 2.15.92.0.2 were reported. The probe measured 29 distinct selected member paths, with no unexpected offset or width differences, and reproduced all five known positive controls. The two tap-position members change signedness from the old unsigned access macro to signed `short`; their consumers preserve the low 16-bit result through explicit conversions, so they are not counted as functional-bug findings. The decoder owner path is tied to the preceding assignment of that same decoder subobject to `fsecfg.owner` before FSE initialization.

Overall, these four batches contain 120 ordinary selected access occurrences and five positive-control rows. Their distinct-member counts overlap and are not summed. No claim is made that these rows enumerate every substitution or every possible mismatch in the commit.


## Historical typed-owner access ledger: `v32seq.c`

Scope: raw access expressions in the parent of `0e473a213b6be1dcc75e8fd0394e3f87c05ad66a` replaced by typed expressions in that commit. Rows follow old source line order; repeated reads/writes are separate rows. Accessor macro definitions are excluded. Pairing uses statement role and owner identity before comparing offsets.

### Measurement

The period layout probe was run through `build/typed-offset-audit293/run_v32_probe.py`, which uses `build/tc_repro/.build-config`, the shared `experiment_toolchain` helper, and exactly one `-DDSPLIB_REPRODUCE_BUGS`. Compiler: Gentoo GCC 3.4.2-r2; assembler: GNU as 2.15.92.0.2. Flags: `-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS`.

The period probe measured `v32_modem.fp` at 0x68 (pointer, 4 bytes), `v32_modem.hdx` at 0x64 (pointer, 4 bytes), `v32_fp.rx_rate_index` at 0x2a (signed short, 2 bytes), and all `v32_hdx.gen_*` and `det_*` members. The `gen_*` entries are unsigned shorts at 0x4a, 0x4c, 0x4e, 0x50, 0x52; `det_width` is unsigned short at 0x54; `det_out_mask`, `det_target`, `det_mask`, `det_reg`, and `det_match` are signed ints at 0x58, 0x5c, 0x60, 0x64, 0x68. The probe also measured `v32_hdx.regs[0]` at 0x3c as a signed short, 2 bytes, matching old `V32HDX_REGS=0x3c` plus `reg * sizeof(short)`.

### Occurrences

`owner` means the old `FIELD_PTR(modem, ...)` load and the new typed pointer load. Old offset/type is followed by new path and independently measured layout.

| # | Old line | Old access (owner + offset/type) | New expression | Measurement / consumer check |
|---:|---:|---|---|---|
| 1 | 112 | `modem + 0x68`, pointer | `((struct v32_modem *)modem)->fp` | `v32_modem.fp` 0x68, pointer size 4; same owner |
| 2 | 113 | `fp + 0x2a`, signed short | `fp->rx_rate_index` | 0x2a, signed short, 2 bytes; array index consumed as short |
| 3 | 195 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 4 | 200 | `hdx + 0x4e`, unsigned short | `hdx->gen_width` | 0x4e, unsigned short, 2 bytes; assigned `width` |
| 5 | 201 | `hdx + 0x52`, unsigned short | `hdx->gen_pattern` | 0x52, unsigned short, 2 bytes; assigned `pattern` |
| 6 | 202 | `hdx + 0x4c`, unsigned short | `hdx->gen_index_mask` | 0x4c, unsigned short, 2 bytes; stores promoted `top` |
| 7 | 203 | `hdx + 0x4a`, unsigned short | `hdx->gen_index` | 0x4a, unsigned short, 2 bytes; stores promoted `top` |
| 8 | 204 | `hdx + 0x50`, unsigned short | `hdx->gen_mask` | 0x50, unsigned short, 2 bytes; casted mask result stored |
| 9 | 221 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 10 | 222 | `hdx + 0x4e`, unsigned short | `hdx->gen_width` | 0x4e, unsigned short, 2 bytes; promoted into `int width` |
| 11 | 223 | `hdx + 0x52`, unsigned short | `hdx->gen_pattern` | 0x52, unsigned short, 2 bytes; promoted into `int pattern` |
| 12 | 224 | `hdx + 0x4a`, unsigned short | `hdx->gen_index` | 0x4a, unsigned short, 2 bytes; copied to unsigned-short local |
| 13 | 229 | `hdx + 0x50`, unsigned short | `hdx->gen_mask` | 0x50, unsigned short, 2 bytes; mask operand after promotion |
| 14 | 231 | `hdx + 0x4c`, unsigned short | `hdx->gen_index_mask` | 0x4c, unsigned short, 2 bytes; mask operand after promotion |
| 15 | 234 | `hdx + 0x4a`, unsigned short | `hdx->gen_index` | 0x4a, unsigned short, 2 bytes; stores narrowed updated index |
| 16 | 241 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 17 | 243 | `hdx + 0x54`, unsigned short | `hdx->det_width` | 0x54, unsigned short, 2 bytes |
| 18 | 244 | `hdx + 0x58`, signed int | `hdx->det_out_mask` | 0x58, signed int, 4 bytes |
| 19 | 245 | `hdx + 0x5c`, signed int | `hdx->det_target` | 0x5c, signed int, 4 bytes |
| 20 | 246 | `hdx + 0x60`, signed int | `hdx->det_mask` | 0x60, signed int, 4 bytes |
| 21 | 247 | `hdx + 0x64`, signed int | `hdx->det_reg` | 0x64, signed int, 4 bytes |
| 22 | 248 | `hdx + 0x68`, signed int | `hdx->det_match` | 0x68, signed int, 4 bytes |
| 23 | 270 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 24 | 271 | `hdx + 0x54`, unsigned short | `hdx->det_width` | 0x54, unsigned short, 2 bytes; promoted into `int nbits` |
| 25 | 272 | `hdx + 0x64`, signed int | `hdx->det_reg` | 0x64, signed int, 4 bytes; copied into signed `int reg` |
| 26 | 273 | `hdx + 0x60`, signed int | `hdx->det_mask` | 0x60, signed int, 4 bytes; copied into signed `int mask` |
| 27 | 274 | `hdx + 0x5c`, signed int | `hdx->det_target` | 0x5c, signed int, 4 bytes; copied into signed `int target` |
| 28 | 298 | `hdx + 0x68`, signed int | `hdx->det_match` | 0x68, signed int, 4 bytes; receives masked register value on hit |
| 29 | 299 | `hdx + 0x58`, signed int | `hdx->det_out_mask` | 0x58, signed int, 4 bytes; right operand of signed-int `&` |
| 30 | 300 | `hdx + 0x64`, signed int | `hdx->det_reg` | 0x64, signed int, 4 bytes; stores current register on hit |
| 31 | 309 | `hdx + 0x64`, signed int | `hdx->det_reg` | 0x64, signed int, 4 bytes; stores register on no-hit exit |
| 32 | 317 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 33 | 319 | `hdx + 0x68`, signed int | `hdx->det_match` | 0x68, signed int, 4 bytes; returned as `int` |
| 34 | 328 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 35 | 330 | `hdx + 0x3c`, signed-short array base | `hdx->regs[reg]` | `regs` base 0x3c; signed short elements, 2 bytes; same indexed load |
| 36 | 340 | `modem + 0x64`, pointer | `((struct v32_modem *)modem)->hdx` | `v32_modem.hdx` 0x64, pointer size 4 |
| 37 | 342 | `hdx + 0x3c`, signed-short array base | `hdx->regs[reg]` | `regs` base 0x3c; signed short elements, 2 bytes; same indexed store |

All 37 body references are paired and measured; unresolved: 0. No address or width mismatch found in this file. The consumer checks confirm signedness and promotion behavior at each changed expression; dynamic array indexing retains the same signed-short element type and stride.

## Historical typed-owner access ledger: `v32fpctl.c`

Scope: commit `0e473a213b6be1dcc75e8fd0394e3f87c05ad66a` against its parent. This ledger covers all 77 raw `FIELD*` member accesses in the function bodies and four address-only aggregate arguments (`dec->vtb` cast-to-pointer became `&dec->vtb`). The 15 accessor/mapping macro definition lines are excluded from that denominator and reviewed separately below; their expansions are not counted as extra body accesses. `sysdep_free(modem)`'s added owner cast is not an offset access and is excluded. Pairing follows the old/new statement role, owner and operand position before offsets are compared.

### Measurement

`fpctl_full_layout.c` was compiled and run through `run_fpctl_full_layout.py`, using `build/tc_repro/.build-config` and `tools/experiment_toolchain.py` to append exactly one `-DDSPLIB_REPRODUCE_BUGS`. Compiler: Gentoo GCC 3.4.2-r2; assembler: GNU as 2.15.92.0.2. Full flags: `-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS`. Output and full command are in `fpctl_full_layout.log`.

The probe measured owner pointers `v32_modem.hdx` at 0x64 and `.fp` at 0x68 (4 bytes each); all directly referenced scalar, pointer, and aggregate members are reported in that log. Old offsets below are the numeric constants in the historical headers, or the measured old typed subobject offset for `dec->vtb`. `FIELD_PTR` reads a pointer value at the selected offset; `FIELD` forms an address at the selected offset. Offsets are owner-relative. For all rows, current offsets and widths equal old offset/width. The tap-position and turnaround expression rows had old FIELD_U16 reads and typed signed-short reads; each subtraction/result conversion yields the same low 16 bits on the period i386 target. Row 75's signed-short read converts back to the function's unsigned-short return type, preserving its 16-bit value. These are type-spelling differences, not functional-bug findings.

### Per-occurrence ledger

### Changed mapping macros (separate denominator)

These nine mapping macros changed definition, so their uses throughout the file now resolve through typed members even when a use-site statement did not change text. Their expansions are reviewed independently of the 77 raw member accesses and four VTB address arguments above. `DEC(fp)` did not change.

| Macro | Old raw owner-relative constant | New typed path | Period measurement |
|---|---:|---|---|
| `HDX(m)` | modem + 0x64 | `((struct v32_modem *)m)->hdx` | `v32_modem.hdx` 0x64, pointer 4 |
| `FP(m)` | modem + 0x68 | `((struct v32_modem *)m)->fp` | `v32_modem.fp` 0x68, pointer 4 |
| `SDM_TX(fp)` | fp + 0x30 | `&fp->scrambler` | `v32_fp.scrambler` 0x30, aggregate 24 |
| `SDM_RX(fp)` | fp + 0x50b0 | `&fp->descrambler` | `v32_fp.descrambler` 0x50b0, aggregate 24 |
| `SMC(fp)` | fp + 0x48 | `&fp->tx_smc` | `v32_fp.tx_smc` 0x48, aggregate 22 |
| `PPS(fp)` | fp + 0x60 | `&fp->pps` | `v32_fp.pps` 0x60, aggregate 56 |
| `RING(fp)` | fp + 0xb0 | `&fp->symout` | `v32_fp.symout` 0xb0, aggregate 20 |
| `ECC(fp)` | fp + 0xe0 | `&fp->ecc` | `v32_fp.ecc` 0xe0, aggregate 104 |
| `FSE(fp)` | fp + 0x204 | `&fp->fse` | `v32_fp.fse` 0x204, aggregate 19992 |

### Per-occurrence ledger

| # | Historical old line | Old access / owner-relative offset and type | New expression / period layout | Consumer / result |
|---:|---:|---|---|---|
| 1 | 164 | `FIELD_PTR(HDX, V32_HDX_MTD)` / 0x38 pointer | `HDX(modem)->mtd` / 0x38, pointer 4 | `FPM_MTD_delete`; same object pointer |
| 2 | 165 | `FIELD_PTR(HDX, V32_HDX_TONE2)` / 0x34 pointer | `HDX(modem)->tone2` / 0x34, pointer 4 | `FPM_TONE_delete`; same argument role |
| 3 | 167 | `FIELD_PTR(HDX, V32_HDX_TONE1)` / 0x30 pointer | `HDX(modem)->tone1` / 0x30, pointer 4 | `FPM_TONE_delete` |
| 4 | 169 | `FIELD_PTR(HDX, V32_HDX_TONE0)` / 0x2c pointer | `HDX(modem)->tone0` / 0x2c, pointer 4 | `FPM_TONE_delete` |
| 5 | 172 | `FIELD_PTR(FP, V32FP_BUF_5038)` / 0x5038 pointer | `FP(modem)->decoder.vtb.paths` / 0x5038, pointer 4 | `sysdep_free`; same heap allocation |
| 6 | 174 | `FIELD(FP, V32FP_SRE)` / 0x148 aggregate address | `&FP(modem)->sre` / 0x148, aggregate 144 | `FPM_SRE_free`; same object address |
| 7 | 176 | `FIELD(FP, V32FP_MRF)` / 0xc4 aggregate address | `&FP(modem)->mrf` / 0xc4, aggregate 28 | `FPM_MRF_free`; same object address |
| 8 | 178 | `FIELD_PTR(FP, V32FP_BUF_50CC)` / 0x50cc pointer | `FP(modem)->rx_buf` / 0x50cc, pointer 4 | `sysdep_free` |
| 9 | 179 | `FIELD_PTR(FP, V32FP_CLEAN_BUF)` / 0x50d0 pointer | `FP(modem)->clean_buf` / 0x50d0, pointer 4 | `sysdep_free` |
| 10 | 181 | `FIELD_PTR(HDX, V32_HDX_BUF_A4)` / 0xa4 pointer | `HDX(modem)->buffer` / 0xa4, pointer 4 | `sysdep_free` |
| 11 | 211 | `FIELD_U16(FP, V32FP_CLEAN_N)` / 0x50d4 unsigned short | `FP(modem)->clean_n` / 0x50d4, unsigned short 2 | Compared with clean buffer maximum; same value width |
| 12 | 216 | `FIELD_PTR(FP, V32FP_CLEAN_BUF)` / 0x50d0 pointer | `FP(modem)->clean_buf` / 0x50d0, pointer 4 | Returned as `short *`; same buffer |
| 13 | 254 | `FIELD_S16(fp, V32FP_ENCODER_SEL)` / 0xa4 signed short | `((struct v32_fp *)fp)->encoder_sel` / 0xa4, signed short 2 | Stores selector |
| 14 | 260 | `FIELD_S16(fp, V32FP_SHORT_2C)` / 0x2c signed short | `((struct v32_fp *)fp)->short_2c` / 0x2c, signed short 2 | Stores mode code |
| 15 | 269 | `FIELD_S16(fp, V32FP_ENCODER_SEL)` / 0xa4 signed short | `((struct v32_fp *)fp)->encoder_sel` / 0xa4, signed short 2 | Stores selector |
| 16 | 273 | `FIELD_S16(fp, V32FP_SHORT_2C)` / 0x2c signed short | `((struct v32_fp *)fp)->short_2c` / 0x2c, signed short 2 | Stores mode code |
| 17 | 283 | `FIELD_S16(fp, V32FP_ENCODER_SEL)` / 0xa4 signed short | `((struct v32_fp *)fp)->encoder_sel` / 0xa4, signed short 2 | Stores selector |
| 18 | 287 | `FIELD_S16(fp, V32FP_SHORT_2C)` / 0x2c signed short | `((struct v32_fp *)fp)->short_2c` / 0x2c, signed short 2 | Stores mode code |
| 19 | 296 | `FIELD_S16(fp, V32FP_ENCODER_SEL)` / 0xa4 signed short | `((struct v32_fp *)fp)->encoder_sel` / 0xa4, signed short 2 | Stores selector |
| 20 | 299 | `FIELD_S16(fp, V32FP_SHORT_2C)` / 0x2c signed short | `((struct v32_fp *)fp)->short_2c` / 0x2c, signed short 2 | Stores mode code |
| 21 | 308 | `FIELD_S16(fp, V32FP_ENCODER_SEL)` / 0xa4 signed short | `((struct v32_fp *)fp)->encoder_sel` / 0xa4, signed short 2 | Stores selector |
| 22 | 311 | `FIELD_S16(fp, V32FP_SHORT_2C)` / 0x2c signed short | `((struct v32_fp *)fp)->short_2c` / 0x2c, signed short 2 | Stores mode code |
| 23 | 321 | `FIELD_S16(fp, V32FP_ENCODER_SEL)` / 0xa4 signed short | `((struct v32_fp *)fp)->encoder_sel` / 0xa4, signed short 2 | Stores selector |
| 24 | 324 | `FIELD_S16(fp, V32FP_SHORT_2C)` / 0x2c signed short | `((struct v32_fp *)fp)->short_2c` / 0x2c, signed short 2 | Stores mode code |
| 25 | 329 | `FIELD_U8(modem, V32_OBJ_STATUS)` / 0x30 unsigned byte | `((struct v32_modem *)modem)->status` / 0x30, unsigned byte 1 | Stores bad-mode status |
| 26 | 330 | `FIELD_U8(modem, V32_OBJ_FLAGS)` / 0x31 unsigned byte | `((struct v32_modem *)modem)->flags` / 0x31, unsigned byte 1 | Sets fault bit |
| 27 | 340 | `FIELD_U16(sdm, V32_SDM_TAP1_POS)` / 0x02 unsigned short | `sdm->tap1_pos` / 0x02, signed short 2 | Subtracts shift, explicitly casts result to short; same low 16-bit result |
| 28 | 341 | `FIELD_U16(sdm, V32_SDM_TAP2_POS)` / 0x04 unsigned short | `sdm->tap2_pos` / 0x04, signed short 2 | Same subtraction and explicit cast |
| 29 | 368 | `FIELD_S16(fp, V32FP_SHORT_509C)` / 0x509c signed short | `((struct v32_fp *)fp)->rx_smc.shift` / 0x509c, signed short 2 | Initializes receive shift |
| 30 | 370 | `FIELD_S16(fp, V32FP_SHORT_5098)` / 0x5098 signed short | `((struct v32_fp *)fp)->rx_smc.mode` / 0x5098, signed short 2 | Initializes receive mode |
| 31 | 371 | `FIELD_S16(dec, V32_DEC_SHORT_04)` / 0x04 signed short | `dec->short_04` / 0x04, signed short 2 | Initializes decoder subfield |
| 32 | 373 | `FIELD_S16(fp, V32FP_SHORT_2E)` / 0x2e signed short | `((struct v32_fp *)fp)->short_2e` / 0x2e, signed short 2 | Stores receive mode code |
| 33 | 388 | `FIELD_S16(fp, V32FP_SHORT_5098)` / 0x5098 signed short | `((struct v32_fp *)fp)->rx_smc.mode` / 0x5098, signed short 2 | Sets receive mode |
| 34 | 389 | `FIELD_S16(fp, V32FP_SHORT_509C)` / 0x509c signed short | `((struct v32_fp *)fp)->rx_smc.shift` / 0x509c, signed short 2 | Sets receive shift |
| 35 | 391 | `FIELD_S16(dec, V32_DEC_SHORT_04)` / 0x04 signed short | `dec->short_04` / 0x04, signed short 2 | Sets decoder subfield |
| 36 | 393 | `FIELD_S16(fp, V32FP_SHORT_2E)` / 0x2e signed short | `((struct v32_fp *)fp)->short_2e` / 0x2e, signed short 2 | Stores receive mode code |
| 37 | 399 | `FIELD_S16(fp, V32FP_SHORT_5098)` / 0x5098 signed short | `((struct v32_fp *)fp)->rx_smc.mode` / 0x5098, signed short 2 | Sets receive mode |
| 38 | 402 | `FIELD_S16(fp, V32FP_SHORT_509C)` / 0x509c signed short | `((struct v32_fp *)fp)->rx_smc.shift` / 0x509c, signed short 2 | Sets receive shift |
| 39 | 404 | `FIELD_S16(dec, V32_DEC_SHORT_04)` / 0x04 signed short | `dec->short_04` / 0x04, signed short 2 | Sets decoder subfield |
| 40 | 406 | `FIELD_S16(fp, V32FP_SHORT_2E)` / 0x2e signed short | `((struct v32_fp *)fp)->short_2e` / 0x2e, signed short 2 | Stores receive mode code |
| 41 | 407 | `dec->vtb` cast to `struct vtb *` | `&dec->vtb` / 0x18, aggregate 56 | `VTBv32_init` receives the same embedded aggregate address |
| 42 | 415 | `FIELD_S16(fp, V32FP_SHORT_2E)` / 0x2e signed short | `((struct v32_fp *)fp)->short_2e` / 0x2e, signed short 2 | Stores receive mode code |
| 43 | 416 | `DEC(fp)->vtb` cast to `struct vtb *` | `&DEC(fp)->vtb` / 0x18, aggregate 56 | `VTBv32_init` receives same aggregate address |
| 44 | 424 | `FIELD_S16(fp, V32FP_SHORT_2E)` / 0x2e signed short | `((struct v32_fp *)fp)->short_2e` / 0x2e, signed short 2 | Stores receive mode code |
| 45 | 425 | `DEC(fp)->vtb` cast to `struct vtb *` | `&DEC(fp)->vtb` / 0x18, aggregate 56 | `VTBv32_init` receives same aggregate address |
| 46 | 433 | `FIELD_S16(fp, V32FP_SHORT_2E)` / 0x2e signed short | `((struct v32_fp *)fp)->short_2e` / 0x2e, signed short 2 | Stores receive mode code |
| 47 | 434 | `DEC(fp)->vtb` cast to `struct vtb *` | `&DEC(fp)->vtb` / 0x18, aggregate 56 | `VTBv32_init` receives same aggregate address |
| 48 | 439 | `FIELD_U8(modem, V32_OBJ_FLAGS)` / 0x31 unsigned byte | `((struct v32_modem *)modem)->flags` / 0x31, unsigned byte 1 | Sets fault bit |
| 49 | 440 | `FIELD_U8(modem, V32_OBJ_STATUS)` / 0x30 unsigned byte | `((struct v32_modem *)modem)->status` / 0x30, unsigned byte 1 | Stores bad-mode status |
| 50 | 449 | `FIELD_U16(sdm, V32_SDM_TAP1_POS)` / 0x02 unsigned short | `sdm->tap1_pos` / 0x02, signed short 2 | Explicit short conversion after subtraction; same low 16-bit result |
| 51 | 450 | `FIELD_U16(sdm, V32_SDM_TAP2_POS)` / 0x04 unsigned short | `sdm->tap2_pos` / 0x04, signed short 2 | Same subtraction and explicit conversion |
| 52 | 470 | `FIELD_U16(modem, V32_OBJ_BPS)` / 0x04 unsigned short | `((struct v32_modem *)modem)->params.rx_rate` / 0x04, signed short 2 | Copied into unsigned-short local before rate comparisons |
| 53 | 477 | `FIELD_INT(modem, V32_OBJ_TRELLIS)` / 0x1c signed int | `((struct v32_modem *)modem)->params.trellis` / 0x1c, signed int 4 | Boolean branch condition |
| 54 | 516 | `FIELD_INT(fp, V32FP_EQ_ADAPT)` / 0x10 signed int | `((struct v32_fp *)fp)->eq_adapt` / 0x10, signed int 4 | Stores 1 |
| 55 | 521 | `FIELD_INT(fp, V32FP_EQ_ADAPT)` / 0x10 signed int | `((struct v32_fp *)fp)->eq_adapt` / 0x10, signed int 4 | Stores 0 |
| 56 | 525 | `FIELD_INT(fp, V32FP_EQ_ADAPT)` / 0x10 signed int | `((struct v32_fp *)fp)->eq_adapt` / 0x10, signed int 4 | Stores 1 |
| 57 | 562 | `FIELD_INT(fp, V32FP_R14)` / 0x14 signed int | `((struct v32_fp *)fp)->int_14` / 0x14, signed int 4 | Disables EC adaptation |
| 58 | 563 | `FIELD_INT(fp, V32FP_R18)` / 0x18 signed int | `((struct v32_fp *)fp)->int_18` / 0x18, signed int 4 | Disables EC adaptation |
| 59 | 571 | `FIELD_INT(fp, V32FP_R14)` / 0x14 signed int | `((struct v32_fp *)fp)->int_14` / 0x14, signed int 4 | Enables near adaptation |
| 60 | 575 | `FIELD_INT(fp, V32FP_R18)` / 0x18 signed int | `((struct v32_fp *)fp)->int_18` / 0x18, signed int 4 | Enables far adaptation |
| 61 | 585 | `FIELD_U16(modem, V32_OBJ_EC_NEAR_DELAY)` / 0x14 unsigned short | `((struct v32_modem *)modem)->params.ec_near_delay` / 0x14, unsigned short 2 | Cast to short for ECC delay input |
| 62 | 615 | `FIELD_INT(fp, V32FP_R08)` / 0x08 signed int | `((struct v32_fp *)fp)->int_08` / 0x08, signed int 4 | Clears receive loop control |
| 63 | 616 | `FIELD_INT(fp, V32FP_R0C)` / 0x0c signed int | `((struct v32_fp *)fp)->int_0c` / 0x0c, signed int 4 | Clears receive loop control |
| 64 | 617 | `FIELD_INT(fp, V32FP_R00)` / 0x00 signed int | `((struct v32_fp *)fp)->int_00` / 0x00, signed int 4 | Clears receive loop control |
| 65 | 618 | `FIELD_INT(fp, V32FP_R04)` / 0x04 signed int | `((struct v32_fp *)fp)->int_04` / 0x04, signed int 4 | Clears receive loop control |
| 66 | 623 | `FIELD_INT(fp, V32FP_R08)` / 0x08 signed int | `((struct v32_fp *)fp)->int_08` / 0x08, signed int 4 | Sets receive loop control |
| 67 | 624 | `FIELD_INT(fp, V32FP_R0C)` / 0x0c signed int | `((struct v32_fp *)fp)->int_0c` / 0x0c, signed int 4 | Sets receive loop control |
| 68 | 625 | `FIELD_INT(fp, V32FP_R00)` / 0x00 signed int | `((struct v32_fp *)fp)->int_00` / 0x00, signed int 4 | Sets receive loop control |
| 69 | 626 | `FIELD_INT(fp, V32FP_R04)` / 0x04 signed int | `((struct v32_fp *)fp)->int_04` / 0x04, signed int 4 | Sets receive loop control |
| 70 | 665 | `FIELD_S16(HDX, V32_HDX_SHORT_9C)` / 0x9c signed short | `HDX(modem)->short_9c` / 0x9c, signed short 2 | Copied to signed-short `lag` |
| 71 | 666 | `FIELD_S16(modem, V32_OBJ_SYMLEN_SEL)` / 0x18 signed short | `((struct v32_modem *)modem)->params.symlen_sel` / 0x18, signed short 2 | Indexes symbol-length table |
| 72 | 740 | first `FIELD_U16(dec, V32_DEC_RETRAIN_N)` / 0x74 unsigned short | `dec->short_74` / 0x74, unsigned short 2 | Read, incremented, and explicitly narrowed |
| 73 | 741 | second `FIELD_U16(dec, V32_DEC_RETRAIN_N)` / 0x74 unsigned short | `dec->short_74` / 0x74, unsigned short 2 | Same expression's write operand |
| 74 | 779 | `FIELD_S16(hdx, V32_HDX_SHORT_9E)` / 0x9e signed short | `((struct v32_hdx *)hdx)->symbol_len` / 0x9e, signed short 2 | Decremented into signed loop counter |
| 75 | 782 | `FIELD_U16(hdx, V32_HDX_SHORT_9E)` / 0x9e unsigned short | `((struct v32_hdx *)hdx)->symbol_len` / 0x9e, signed short 2 | Returned in unsigned-short return type |
| 76 | 803 | `FIELD_PTR(hdx, V32_HDX_TONE0)` / 0x2c pointer | `((struct v32_hdx *)hdx)->tone0` / 0x2c, pointer 4 | Reads tone configuration |
| 77 | 805 | `FIELD_PTR(hdx, V32_HDX_TONE0)` / 0x2c pointer | `((struct v32_hdx *)hdx)->tone0` / 0x2c, pointer 4 | `FPM_TONE_create` target |
| 78 | 826 | `FIELD_U16(hdx, V32_HDX_SHORT_94)` / 0x94 unsigned short | `((struct v32_hdx *)hdx)->turnaround` / 0x94, signed short 2 | Subtraction followed by explicit short conversion |
| 79 | 827 | `FIELD_U16(hdx, V32_HDX_SHORT_9C)` / 0x9c unsigned short | `((struct v32_hdx *)hdx)->short_9c` / 0x9c, signed short 2 | Subtraction operand; same low 16-bit result |
| 80 | 828 | `FIELD_U16(hdx, V32_HDX_SHORT_98)` / 0x98 unsigned short | `((struct v32_hdx *)hdx)->short_98` / 0x98, signed short 2 | Subtraction operand; same low 16-bit result |
| 81 | 829 | `FIELD_U16(hdx, V32_HDX_SHORT_9A)` / 0x9a unsigned short | `((struct v32_hdx *)hdx)->short_9a` / 0x9a, signed short 2 | Subtraction operand; same low 16-bit result |

All 77 raw FIELD body accesses, four additional address-only VTB arguments, and nine changed mapping macros are paired and layout-measured. Unresolved: 0. No address or width mismatch was found in this file. The audit does not infer a functional bug from the signed/unsigned short spelling changes described above. The replay probe covers this complete file and `v32seq.c`; the other 80 selected RXHDX/V32.c sample rows remain covered by their separate batch probes and are outside its 68 ordinary-path denominator.

## Coverage at this checkpoint

The complete ledgers cover 37 sequence-file body references and 77 control-file
raw accesses plus four address-only arguments. Nine changed control-file
mapping macros are checked separately. They find no additional address or
width mismatch and leave no listed role unresolved. These ledgers overlap the
initial twenty-row samples; do not add those samples again when quoting
coverage. The forty selected receiver-state accesses and forty selected
constructor accesses remain samples, not complete-file audits.

The durable `tools/recovery_typed_owner_probe.py` replay now checks 68 ordinary member paths, including the
complete sequence/control ledgers' distinct layout paths and retains all five
known mismatch controls. The two other sampled batches retain their separate
build-artifact probes as recorded above. Layout measurement checks current
member offsets, widths and signedness; the historical statement-role pairing
and consumer analysis remain explicit review evidence, not an automatic proof
that every field in the project is correct. No reconstruction source changed
in this audit extension.


## Historical typed-owner access audit: `V32rxhdx.c`

Scope is the historical file `src/pump/v32/V32rxhdx.c` in commit `0e473a213b6be1dcc75e8fd0394e3f87c05ad66a` and its parent. This file has 143 raw `FIELD*` macro invocations total: 20 occur in 14 accessor/mapping macro definitions, and 123 occur in function bodies. Every body invocation was converted in the audited commit; no raw `FIELD` call remains in the new file. The grouped rows below list all 123 occurrences by historical source line, access constant, and old type. Counts sum to the denominator.

Macro mapping definitions are reviewed separately after the body ledger. Their repeated use sites are not added to the 123 raw FIELD-call denominator. No unchanged raw FIELD body occurrences were found.

### Period measurement

`rxhdx_full_probe.c` was compiled with `run_rxhdx_full_probe.py` using `build/tc_repro/.build-config` and `tools/experiment_toolchain.py`; the helper appended exactly one `-DDSPLIB_REPRODUCE_BUGS`. Compiler is Gentoo GCC 3.4.2-r2, target `i386-pc-linux-gnu`; assembler is GNU as 2.15.92.0.2. The full flags and command are recorded in `rxhdx_full_probe.log`.

The probe reports every typed member used in this file, with offset, size, and signedness for scalar fields. It measures `v32_modem.hdx` at 0x64 and `.fp` at 0x68; `v32_hdx` timer/limit at 0x7c/0x80, symbol length at 0x9e, mode/state at 0x76/0x74, state-left at 0x78, `short_48` at 0x48, `int_90` at 0x90, RTD at 0x96, short accumulators at 0xa8/0xaa/0xac, and regs at 0x3c with signed-short elements. It also measures the root config/status fields and `v32_fp.rx_len`/`rx_buf` at 0x50c8/0x50cc.

### Function-body accesses

Each row represents the listed number of separate old FIELD invocations. Where a row includes multiple old access types for one member, each type has its own line list and count. All current offsets, widths, and signs below are from the period probe; old offsets/types are from the historical local constants and FIELD macro types.

| # | Count | Historical old lines | Old field / offset / type | New typed path / period layout | Consumer check |
|---:|---:|---|---|---|---|
| 1 | 23 | 309, 329, 346, 358, 363, 388, 403, 462, 480, 498, 516, 526, 544, 577, 596, 612, 620, 641, 668, 684, 693, 723, 726 | `V32HDX_TIMER`, +0x7c, unsigned int | `hdx->timer`, +0x7c, unsigned int (4) | Added to unsigned-cast symbol length; compared to limit with unsigned branch semantics |
| 2 | 10 | 329, 363, 462, 498, 526, 578, 620, 668, 693, 726 | `V32HDX_LIMIT`, +0x80, unsigned int | `hdx->limit`, +0x80, unsigned int (4) | Right side of timer bound checks; same unsigned comparison |
| 3 | 12 | 310, 347, 389, 392, 481, 517, 545, 597, 613, 642, 685, 724 | `V32HDX_SYMBOL_LEN`, +0x9e, signed short | `hdx->symbol_len`, +0x9e, signed short (2) | Read into timer update via explicit unsigned-int cast, or short arithmetic |
| 4 | 3 | 350, 420, 555 | `V32HDX_SYMBOL_LEN`, +0x9e, unsigned short | `hdx->symbol_len`, +0x9e, signed short (2) | Accumulator/rtd arithmetic is narrowed to 16 bits; signed-vs-unsigned promotion changes the intermediate by a multiple of 65536, preserving the stored low 16 bits |
| 5 | 1 | 316 | `V32_OBJ_PROTOCOL`, modem +0x00, signed short | `modem->params.protocol`, +0x00, signed short (2) | Compared with 1 |
| 6 | 1 | 317 | `V32_OBJ_OPTIONS`, modem +0x10, signed int | `modem->params.options`, +0x10, unsigned int (4) | Only masked with `V32_OPT_0400` and compared to zero; the selected bit is unchanged by signedness |
| 7 | 1 | 318 | `V32HDX_INT_78`, hdx +0x78, signed int | `hdx->state_left`, +0x78, signed int (4) | Compared signed against 180 |
| 8 | 12 | 330, 364, 463, 499, 527, 557, 579, 621, 669, 694, 713, 727 | `V32_OBJ_FLAGS`, modem +0x31, unsigned byte | `modem->flags`, +0x31, unsigned byte (1) | Fault/bit flags set with OR; same byte and mask |
| 9 | 10 | 331, 365, 464, 500, 528, 580, 622, 670, 695, 728 | `V32_OBJ_STATUS`, modem +0x30, unsigned byte | `modem->status`, +0x30, unsigned byte (1) | Stores the same status code |
| 10 | 10 | 334, 368, 467, 503, 531, 583, 625, 673, 698, 731 | `V32HDX_STATE`, hdx +0x74, signed short | `hdx->state`, +0x74, signed short (2) | Stores error state |
| 11 | 9 | 324, 358, 413, 435, 495, 523, 617, 665, 690 | `V32HDX_MODE`, hdx +0x76, signed short | `hdx->mode`, +0x76, signed short (2) | Indexes/dispatches through `V32NextState` |
| 12 | 2 | 348, 349 | `V32HDX_SHORT_AA`, hdx +0xaa, unsigned short | `hdx->short_aa`, +0xaa, signed short (2) | Unsigned-short accumulation is narrowed to 16 bits; low-16-bit store preserved |
| 13 | 5 | 552, 553, 554, 573, 574 | `V32HDX_SHORT_48`, hdx +0x48, unsigned short | `hdx->short_48`, +0x48, signed short (2) | Counter test/update; writes narrow to the same 16 bits |
| 14 | 1 | 556 | `V32HDX_SHORT_48`, hdx +0x48, signed short | `hdx->short_48`, +0x48, signed short (2) | Signed comparison `> 0x17`; source and typed member signedness agree |
| 15 | 3 | 394, 446, 457 | `V32HDX_INT_90`, hdx +0x90, signed int | `hdx->int_90`, +0x90, signed int (4) | Latch read/write as boolean; same width and sign |
| 16 | 5 | 414, 418, 427, 432, 433 | `V32HDX_RTD`, hdx +0x96, signed short | `hdx->rtd`, +0x96, signed short (2) | Stored, passed to integer calculation, checked/clamped as signed short |
| 17 | 1 | 415 | `V32_HDX_SHORT_94`, hdx +0x94, signed short | `hdx->turnaround`, +0x94, signed short (2) | Doubled and subtracted on the originate branch before narrowing to RTD |
| 18 | 1 | 421 | `V32_HDX_SHORT_94`, hdx +0x94, unsigned short | `hdx->turnaround`, +0x94, signed short (2) | Subtraction is cast to short for RTD; changed promotion differs by a multiple of 65536 and preserves low 16 bits |
| 19 | 1 | 422 | `V32_HDX_SHORT_9C`, hdx +0x9c, unsigned short | `hdx->short_9c`, +0x9c, signed short (2) | RTD subtraction/narrowing; same low 16-bit result |
| 20 | 1 | 423 | `V32_HDX_SHORT_98`, hdx +0x98, unsigned short | `hdx->short_98`, +0x98, signed short (2) | RTD subtraction/narrowing; same low 16-bit result |
| 21 | 1 | 424 | `V32_HDX_SHORT_9A`, hdx +0x9a, unsigned short | `hdx->short_9a`, +0x9a, signed short (2) | RTD subtraction/narrowing; same low 16-bit result |
| 22 | 2 | 390, 391 | `V32HDX_SHORT_A8`, hdx +0xa8, unsigned short | `hdx->short_a8`, +0xa8, signed short (2) | Accumulation explicitly narrows to unsigned short; low 16-bit result preserved |
| 23 | 1 | 449 | `V32HDX_SHORT_AC`, hdx +0xac, unsigned short | `hdx->short_ac`, +0xac, signed short (2) | Read then explicitly cast to short for miss count; same 16-bit value |
| 24 | 3 | 455, 456, 458 | `V32HDX_SHORT_AC`, hdx +0xac, signed short | `hdx->short_ac`, +0xac, signed short (2) | Signed counter store, compare, and reset |
| 25 | 2 | 562, 566 | `V32HDX_REGS`, hdx +0x3c, address of signed-short array | `hdx->regs`, +0x3c, 10-byte array; element 0 is signed short (2) | Preserves pointer base; same index 4 element operation |
| 26 | 1 | 658 | `V32FP_RXBUF`, fp +0x50cc, pointer | `fp->rx_buf`, +0x50cc, pointer (4) | Passed as detector sample-buffer argument |
| 27 | 1 | 659 | `V32FP_RXLEN`, fp +0x50c8, signed short | `fp->rx_len`, +0x50c8, unsigned short (2) | `FPM_MTD_detect` takes `short count`; conversion to that parameter preserves the low 16-bit argument representation |

Counts sum to 123/123. Unresolved field-role pairings: 0. No offset or width mismatch was found. Signedness deviations at rows 4, 6, 18–23, and 27 are recorded with their consuming operations; no functional bug is established. Row 14 was checked against the object's native signed branch below.

### Changed access-mapping macros (separate denominator)

The seven raw mapping definitions changed; the low-level `FIELD*` accessor definitions themselves are excluded from body access counting. Every mapping below has an old constant/source definition and an independently measured period member offset.

| Macro | Old raw mapping | New typed mapping | Period layout |
|---|---|---|---|
| `HDX(m)` | modem +0x64 pointer | `((struct v32_modem *)m)->hdx` | `v32_modem.hdx` +0x64, pointer 4 |
| `FP(m)` | modem +0x68 pointer | `((struct v32_modem *)m)->fp` | `v32_modem.fp` +0x68, pointer 4 |
| `AGC_OF(hdx)` | hdx +0x00 aggregate address | `&hdx->agc` | `v32_hdx.agc` +0x00, size 44 |
| `TONE_OF(hdx, off)` | pointer load at `off`; used for tone0 +0x2c, tone1 +0x30, tone2 +0x34 | `hdx->tone0`, `hdx->tone1`, `hdx->tone2` | +0x2c/+0x30/+0x34, each pointer 4 |
| `MTD_OF(hdx)` | pointer load at +0x38 | `hdx->mtd` | `v32_hdx.mtd` +0x38, pointer 4 |
| `TXSTATE(hdx)` | function pointer at +0x6c | `hdx->tx_state` | +0x6c, function pointer 4 |
| `RXSTATE(hdx)` | function pointer at +0x70 | `hdx->rx_state` | +0x70, function pointer 4 |

No same-file reference to these changed macros was left textually in raw FIELD form after the conversion. The macro definitions, their uses, and the body FIELD accesses are separate denominators; do not add macro expansions to the 123.

### Native check and known controls

Native disassembly of the historical object (`ref/slmodemd/dsplibs.o`, `RxHdxSequenceE`, around 0x84013) shows `cmp $0x17,%bx` followed by signed `jle`; this is consistent with the signed-short `short_48` member used by row 14. In `RxHdxSTone` around 0x84362, the old `rx_len` access is loaded with sign extension (`movswl 0x50c8(%ecx),%edx`). The current `rx_len` member is unsigned short, then passed to `FPM_MTD_detect`, whose `short count` parameter converts it back to signed short. This preserves the 16-bit argument representation on the measured i386 ABI; no behavioral mismatch is established from the type spelling alone.

The period control probe was rerun through `tools/recovery_typed_owner_probe.py` in the same canonical build configuration: five selected known mismatches reproduced (old/current: 0x44/0x46, 0x182/0x102, 0x188/0x108, 0x100/0xd4, 0x102/0xd6). The output is in `recovery_probe_full.log`. These controls validate the offset detector; they are not part of this file's occurrence denominator.


The complete receiver ledger supersedes its earlier forty-row sample.
Only the constructor's forty-row batch remains sampled coverage at this
checkpoint. The per-file denominators remain separate from distinct probe
member paths; repeated accesses share layout measurements but retain their
individual statement-role review.


The shared durable replay now checks 83 distinct ordinary paths, including the
complete receiver ledger, and retains the same five known mismatch controls.
The receiver's historical 123-access denominator counts uses; the 83-path
probe denominator counts layout locations shared across all three complete
ledgers. The constructor sample still uses its separate probe.


## Historical `V32.c` constructor conversion audit

Scope is `src/pump/v32/V32.c` in historical commit `0e473a213b6be1dcc75e8fd0394e3f87c05ad66a` against its parent. This is beyond the earlier 40-access sample. The old constructor body contains 116 `FIELD*` access invocations; every one was converted in the commit. The 13 owner-mapping macros are counted separately. A direct `int *` store through decoder padding was also replaced and is recorded separately. The function still accepts `void *modem`, so owner identity is considered separately from byte offset.

### Period evidence

`v32c_probe.c` was compiled and run with `run_v32c_probe.py`, using `tools/experiment_toolchain.py`, canonical `build/tc_repro/.build-config`, Gentoo GCC 3.4.2-r2 and GNU as 2.15.92.0.2. The helper appended one `-DDSPLIB_REPRODUCE_BUGS`. Full command, flags, tool identity and output are in `v32c_probe.log`.

The measured allocation sizes agree with the historical allocator constants: `v32_modem` 0x6c, `v32_fp` 0x50dc, and `v32_hdx` 0xb0. Their allocated child/sample buffers retain their original explicit sizes.

| Historical mapping | Old owner and offset | New owner/path | Period layout |
|---|---|---|---|
| `HDX` | modem +0x64 pointer load | `v32_modem.hdx` | +0x64, pointer/4 |
| `FP` | modem +0x68 pointer load | `v32_modem.fp` | +0x68, pointer/4 |
| `SDM_TX` | fp +0x30 aggregate address | `v32_fp.scrambler` | +0x30, size 24 |
| `SDM_RX` | fp +0x50b0 aggregate address | `v32_fp.descrambler` | +0x50b0, size 24 |
| `SMC_TX` | fp +0x48 aggregate address | `v32_fp.tx_smc` | +0x48, size 22 |
| `PPS` | fp +0x60 aggregate address | `v32_fp.pps` | +0x60, size 56 |
| `RING` | fp +0xb0 aggregate address | `v32_fp.symout` | +0xb0, size 20 |
| `MRF` | fp +0xc4 aggregate address | `v32_fp.mrf` | +0xc4, size 28 |
| `ECC` | fp +0xe0 aggregate address | `v32_fp.ecc` | +0xe0, size 104 |
| `SRE` | fp +0x148 aggregate address | `v32_fp.sre` | +0x148, size 144 |
| `FSE` | fp +0x204 aggregate address | `v32_fp.fse` | +0x204, size 19992 |
| `SMC_RX` | fp +0x5098 aggregate address | `v32_fp.rx_smc` | +0x5098, size 22 |
| `DEC` | fp +0x5020 aggregate address | `v32_fp.decoder` | +0x5020, size 120 |

Pointer-valued mappings above are loads; aggregate mappings are addresses. They were paired by owner and operation, then compared with independently measured period layout.

### Grouped occurrence ledger

The old constructor body has 116 FIELD invocations grouped by identical historical owner/type/offset operands. The table below lists each group count, exact historical source lines, raw owner plus independently resolved literal offset and access type, current expression, period offset/width/signedness, and the member's use. Counts sum to 116. It excludes changed accessor/mapping macro definitions (listed above) and the separate direct padding write (below).

| Count | Historical lines | Old owner + literal offset / access type | New expression | Period layout and consumer check |
|---:|---|---|---|---|
| 1 | 339 | `modem + 0x68` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->fp` | 0x68/4/n/a; statement role at these historical lines; store |
| 1 | 341 | `modem + 0x64` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->hdx` | 0x64/4/n/a; statement role at these historical lines; store |
| 2 | 342,617 | `hdx + 0x2c` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->tone0` | 0x2c/4/n/a; tone pointer null/init/create/load sequence |
| 2 | 343,704 | `hdx + 0x38` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->mtd` | 0x38/4/n/a; detector pointer null/init/create/load sequence |
| 1 | 344 | `hdx + 0xa4` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->buffer` | 0xa4/4/n/a; statement role at these historical lines; store |
| 1 | 346 | `FP(modem) + 0x50cc` via `FIELD_PTR` (pointer load/store) | `FP(modem)->rx_buf` | 0x50cc/4/n/a; statement role at these historical lines; store |
| 1 | 348 | `FP(modem) + 0x50d0` via `FIELD_PTR` (pointer load/store) | `FP(modem)->clean_buf` | 0x50d0/4/n/a; statement role at these historical lines; store |
| 1 | 373 | `fp + 0x1c` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_1c` | 0x1c/4/1; statement role at these historical lines; store |
| 1 | 374 | `fp + 0x20` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_20` | 0x20/4/1; statement role at these historical lines; store |
| 1 | 375 | `fp + 0x0` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_00` | 0x0/4/1; statement role at these historical lines; store |
| 1 | 376 | `fp + 0x4` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_04` | 0x4/4/1; statement role at these historical lines; store |
| 1 | 377 | `fp + 0x8` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_08` | 0x8/4/1; statement role at these historical lines; store |
| 1 | 378 | `fp + 0x24` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_24` | 0x24/4/1; statement role at these historical lines; store |
| 1 | 379 | `fp + 0xc` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_0c` | 0xc/4/1; statement role at these historical lines; store |
| 1 | 380 | `fp + 0x10` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->eq_adapt` | 0x10/4/1; statement role at these historical lines; store |
| 1 | 381 | `fp + 0x14` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_14` | 0x14/4/1; statement role at these historical lines; store |
| 1 | 382 | `fp + 0x18` via `FIELD_INT` (signed int) | `((struct v32_fp *)fp)->int_18` | 0x18/4/1; statement role at these historical lines; store |
| 7 | 391,393,395,398,400,434,437 | `fp + 0x28` via `FIELD_S16` (signed short) | `((struct v32_fp *)fp)->tx_rate_index` | 0x28/2/1; rate ladder writes plus scrambler/coder mode reads |
| 6 | 404,406,408,411,413,529 | `fp + 0x2a` via `FIELD_S16` (signed short) | `((struct v32_fp *)fp)->rx_rate_index` | 0x2a/2/1; rate ladder writes and decoder selector read |
| 6 | 419,421,423,436,622,660 | `hdx + 0x76` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->mode` | 0x76/2/1; mode initialization and mode-index/configuration reads |
| 4 | 441,445,563,566 | `sdm + 0x2` via `FIELD_U16` (unsigned short) | `sdm->tap1_pos` | 0x2/2/1; absolute tap storage/read feeding explicitly narrowed tap1 |
| 4 | 443,446,564,567 | `sdm + 0x4` via `FIELD_U16` (unsigned short) | `sdm->tap2_pos` | 0x4/2/1; absolute tap storage/read feeding explicitly narrowed tap2 |
| 1 | 469 | `HDX(modem) + 0x76` via `FIELD_S16` (signed short) | `HDX(modem)->mode` | 0x76/2/1; mode initialization and mode-index/configuration reads |
| 1 | 493 | `FP(modem) + 0x1d8` via `FIELD` (address) | `&FP(modem)->agc` | 0x1d8/44/n/a; aggregate address passed to FPM_AGC_init |
| 1 | 524 | `FP(modem) + 0x5020` via `FIELD` (address) | `&FP(modem)->decoder` | 0x5020/120/n/a; aggregate address stored as FSE config owner |
| 1 | 538 | `DEC(fp) + 0x74` via `FIELD_S16` (signed short) | `((struct v32_dec *)FSE(fp)->cfg.owner)->short_74` | 0x74/2/0; zero initialization through copied FSE owner configuration |
| 1 | 540 | `DEC(fp) + 0x52 + 2*i` via `FIELD_S16` (signed short) | `DEC(fp)->sym[i]` | 0x52/2/1; statement role at these historical lines; store |
| 1 | 544 | `DEC(fp) + 0x4` via `FIELD_S16` (signed short) | `DEC(fp)->short_04` | 0x4/2/1; statement role at these historical lines; store |
| 1 | 545 | `DEC(fp) + 0x6` via `FIELD_S16` (signed short) | `DEC(fp)->short_06` | 0x6/2/1; statement role at these historical lines; store |
| 3 | 582,584,586 | `fp + 0x98` via `FIELD` (address) | `((struct v32_fp *)fp)->encoders[0]` | 0x98/4/0; statement role at these historical lines; address formation |
| 1 | 588 | `fp + 0xa4` via `FIELD_S16` (signed short) | `((struct v32_fp *)fp)->encoder_sel` | 0xa4/2/1; statement role at these historical lines; store |
| 1 | 589 | `fp + 0xa8` via `FIELD_PTR` (pointer load/store) | `((struct v32_fp *)fp)->imap` | 0xa8/4/n/a; statement role at these historical lines; store |
| 1 | 590 | `fp + 0xac` via `FIELD_PTR` (pointer load/store) | `((struct v32_fp *)fp)->qmap` | 0xac/4/n/a; statement role at these historical lines; store |
| 3 | 615,618,748 | `HDX(modem) + 0x2c` via `FIELD_PTR` (pointer load/store) | `HDX(modem)->tone0` | 0x2c/4/n/a; tone pointer null/init/create/load sequence |
| 2 | 624,625 | `hdx + 0x30` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->tone1` | 0x30/4/n/a; tone pointer null/init/create/load sequence |
| 1 | 626 | `HDX(modem) + 0x30` via `FIELD_PTR` (pointer load/store) | `HDX(modem)->tone1` | 0x30/4/n/a; tone pointer null/init/create/load sequence |
| 2 | 631,634 | `HDX(modem) + 0x34` via `FIELD_PTR` (pointer load/store) | `HDX(modem)->tone2` | 0x34/4/n/a; tone pointer null/init/create/load sequence |
| 1 | 633 | `hdx + 0x34` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->tone2` | 0x34/4/n/a; tone pointer null/init/create/load sequence |
| 3 | 644,746,762 | `hdx + 0x74` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->state` | 0x74/2/1; statement role at these historical lines; store |
| 2 | 645,757 | `hdx + 0x7c` via `FIELD_INT` (signed int) | `((struct v32_hdx *)hdx)->timer` | 0x7c/4/0; statement role at these historical lines; store |
| 2 | 646,756 | `hdx + 0x80` via `FIELD_INT` (signed int) | `((struct v32_hdx *)hdx)->limit` | 0x80/4/0; statement role at these historical lines; store |
| 2 | 647,754 | `hdx + 0x70` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->rx_state` | 0x70/4/n/a; statement role at these historical lines; store |
| 1 | 648 | `hdx + 0x84` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->block_charge` | 0x84/2/1; statement role at these historical lines; store |
| 1 | 649 | `hdx + 0x9e` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->symbol_len` | 0x9e/2/1; statement role at these historical lines; store |
| 1 | 650 | `hdx + 0xa0` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->sample_len` | 0xa0/2/1; statement role at these historical lines; store |
| 1 | 651 | `hdx + 0x94` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->turnaround` | 0x94/2/1; statement role at these historical lines; store |
| 2 | 652,655 | `hdx + 0x9a` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->short_9a` | 0x9a/2/1; statement role at these historical lines; store |
| 1 | 653 | `hdx + 0x98` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->short_98` | 0x98/2/1; statement role at these historical lines; store |
| 1 | 657 | `hdx + 0x9c` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->short_9c` | 0x9c/2/1; statement role at these historical lines; store |
| 3 | 661,664,753 | `hdx + 0x6c` via `FIELD_PTR` (pointer load/store) | `((struct v32_hdx *)hdx)->tx_state` | 0x6c/4/n/a; statement role at these historical lines; store |
| 4 | 662,665,755,770 | `hdx + 0x78` via `FIELD_INT` (signed int) | `((struct v32_hdx *)hdx)->state_left` | 0x78/4/1; statement role at these historical lines; store |
| 1 | 667 | `hdx + 0xae` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->loss_blocks` | 0xae/2/1; statement role at these historical lines; store |
| 1 | 668 | `hdx + 0xac` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->short_ac` | 0xac/2/1; statement role at these historical lines; store |
| 1 | 689 | `hdx + 0x3c + 2*i` via `FIELD_S16` (signed short) | `((struct v32_hdx *)hdx)->regs[i]` | historical loop i=0..6 addresses +0x3c through +0x48; current `regs` declaration has five elements, so the current expression indexes beyond that declared member for i=5,6. The native constructor stores all seven words. Native API accessors accept only indexes 0..4, but that API bound does not establish the historical storage declaration or extent. See the extent note below. |
| 1 | 690 | `hdx + 0x88` via `FIELD_INT` (signed int) | `((struct v32_hdx *)hdx)->int_88` | 0x88/4/1; statement role at these historical lines; store |
| 1 | 705 | `HDX(modem) + 0x38` via `FIELD_PTR` (pointer load/store) | `HDX(modem)->mtd` | 0x38/4/n/a; detector pointer null/init/create/load sequence |
| 1 | 711 | `fp + 0x50d4` via `FIELD_U16` (unsigned short) | `((struct v32_fp *)fp)->clean_n` | 0x50d4/2/0; statement role at these historical lines; store |
| 1 | 712 | `fp + 0x50d6` via `FIELD_S16` (signed short) | `((struct v32_fp *)fp)->decision_error` | 0x50d6/2/1; statement role at these historical lines; store |
| 1 | 713 | `fp + 0x50d8` via `FIELD_S16` (signed short) | `((struct v32_fp *)fp)->rate_fallback` | 0x50d8/2/0; statement role at these historical lines; store |
| 1 | 716 | `modem + 0x30` via `FIELD_INT` (signed int) | `((struct v32_modem *)modem)->status_word` | 0x30/4/0; four-byte status/flags zeroing before byte status write |
| 1 | 717 | `modem + 0x31` via `FIELD_U8` (unsigned byte) | `((struct v32_modem *)modem)->flags` | 0x31/1/0; statement role at these historical lines; store |
| 1 | 718 | `modem + 0x30` via `FIELD_U8` (unsigned byte) | `((struct v32_modem *)modem)->status` | 0x30/1/0; statement role at these historical lines; store |
| 1 | 720 | `modem + 0x34` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_out_i` | 0x34/4/n/a; statement role at these historical lines; store |
| 1 | 721 | `modem + 0x38` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_out_q` | 0x38/4/n/a; statement role at these historical lines; store |
| 1 | 722 | `modem + 0x40` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_icoeff` | 0x40/4/n/a; statement role at these historical lines; store |
| 1 | 723 | `modem + 0x44` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_qcoeff` | 0x44/4/n/a; statement role at these historical lines; store |
| 1 | 727 | `modem + 0x4c` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_near_i` | 0x4c/4/n/a; statement role at these historical lines; store |
| 1 | 728 | `modem + 0x50` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_near_q` | 0x50/4/n/a; statement role at these historical lines; store |
| 1 | 729 | `modem + 0x48` via `FIELD_S16` (signed short) | `((struct v32_modem *)modem)->diag_fse_taps` | 0x48/2/1; statement role at these historical lines; store |
| 1 | 730 | `modem + 0x58` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_far_i` | 0x58/4/n/a; statement role at these historical lines; store |
| 1 | 731 | `modem + 0x3c` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_n_out` | 0x3c/4/n/a; statement role at these historical lines; store |
| 1 | 732 | `modem + 0x54` via `FIELD_U16` (unsigned short) | `((struct v32_modem *)modem)->diag_near_n` | 0x54/2/0; statement role at these historical lines; store |
| 1 | 734 | `modem + 0x5c` via `FIELD_PTR` (pointer load/store) | `((struct v32_modem *)modem)->diag_far_q` | 0x5c/4/n/a; statement role at these historical lines; store |
| 1 | 735 | `modem + 0x60` via `FIELD_U16` (unsigned short) | `((struct v32_modem *)modem)->diag_far_n` | 0x60/2/0; statement role at these historical lines; store |
| 1 | 752 | `hdx + 0x90` via `FIELD_INT` (signed int) | `((struct v32_hdx *)hdx)->int_90` | 0x90/4/1; statement role at these historical lines; store |


Separate changed direct padding access: historical line 537 writes `*(int *)(void *)DEC(fp)->pad68 = 0`, i.e. decoder owner +0x68, signed 32-bit zero store; the new expression is `DEC(fp)->int_68 = 0`, measured at decoder +0x68, size 4, signed int. This is address/width/type-aligned and remains a zero initialization.


The particularly opaque decoder path was checked independently: constructor config sets `fsecfg.owner = &FP(modem)->decoder`; `FPM_FSE_init` copies with `state->cfg = *cfg`; the later `FSE(fp)->cfg.owner` cast therefore resolves to that same decoder allocation. `short_74` is decoder +0x74 and is zeroed, so the new unsigned-short member store and old signed-short zero store have identical effect.

No scalar address or width mismatch was found. One dynamic path, `regs[i]`, exceeds its declared array bounds for two loop iterations as noted in its ledger row. Old `FIELD_U16` tap accesses now use signed-short member declarations; their values come from `SDMv32_CFG={4,5,23}` and `SDMv32_GPA/GPC` entries 5 or 18, and the derived tap is explicitly narrowed to `short`. This signedness difference does not establish a functional mismatch. The old five selected positive controls were rerun through `tools/recovery_typed_owner_probe.py`; all still mismatch as expected. The canonical probe also checks 125 unique ordinary paths and five `sizeof` values. Full replay is in `v32c_durable_probe.log`.

The occurrence ledger accounts for all 116 raw FIELD invocations, with 75 distinct old owner/type/offset groups and a separately recorded direct padding write. All mapped paths were checked against the period probe. Native `V32FP_recreate` disassembly corroborates seven contiguous word-zero stores at offsets +0x3c through +0x48; evidence is in `v32c_native.txt` around 0x7f256–0x7f27a. The period layout probe measures the current declarations, not the original author's C member boundaries.


The constructor occurrence ledger supersedes its forty-row sample. All four
historical files now have counted access coverage. The shared durable period
probe checks 125 ordinary member paths, five size expectations and the five
known offset-mismatch controls. This closes the sampled-coverage gap, not the
open question about the original register-bank storage model.

### V32 hdx +0x3c through +0x48: storage extent remains unresolved

The historical constructor loop writes seven consecutive 16-bit words,
starting at hdx +0x3c. The native `LoadReg` at 0x838b0 and `StoreReg` at
0x838d0 each sign-extend the index, compare it with 4, and use unsigned
`ja` to reject larger indexes. Their only indexed memory operation is based
at +0x3c with a scale of two. Specifically, `LoadReg` compares `%dx` with
4 at 0x838b7, branches `ja` at 0x838bb, and loads `0x3c(%eax,%edx,2)` at
0x838c4; `StoreReg` has the same compare and branch at 0x838da/0x838de and
stores through `0x3c(%eax,%edx,2)` at 0x838e7. This establishes a five-index
public accessor range; it does **not** establish that the underlying storage
stopped after five words.

The other two words have independent roles in the native state machine.
`ref_V32OrgNextState` stores a narrowed difference to +0x46 at 0x82f21 and
reads +0x46 with sign extension at 0x82eb6 for the alignment calculation.
`ref_RxHdxSequenceE` reads +0x48 at 0x84000, updates it at 0x84017, and
increments/stores it at 0x840b1 after a detected sequence; the threshold
comparison against 0x17 is at 0x84013. `ref_V32RngInitNextState` and
`ref_V32RngRespNextState` read +0x48 at 0x85128 and 0x857ce respectively,
then subtract the sign-extended value from 0x18 for a next-state countdown.
The reconstructed source has corresponding roles in `V32org.c` (current
lines 582, 618), `V32rxhdx.c` (579–600), and `V32RNG.c` (427, 623). The native
constructor also clears both words before handing control to these states.

These observations support two compatible physical models: a seven-word
array whose last two elements are private to those state-machine roles, or a
five-word register array followed by separate `short_46` and `short_48`
members. Both layouts place the same two-byte values at the same addresses;
the API bounds and uses do not distinguish them. The historical seven-loop
form indexed beyond the five elements in the current declaration, but the
audit has not established that the original object used that declaration or
that physical storage stopped after five words. Do not treat the API bound
alone as proof for either model.

The constructor now uses seven explicit zero stores through the five declared
register elements and the two adjacent members, preserving the current layout
and accessor API without asserting that this was the original C declaration.
The independent four-cell replay found that the explicit five-plus-two and
seven-element source forms emitted identical objects across all 14 transitive
consumers; the separate 7-element loop cell stayed byte-identical to the
pinned baseline but retained a runtime loop. The `V32.c.o` audit covers six
function bodies; the five non-constructor bodies stay unchanged.
[The storage-model audit](v32-register-storage-audit.md) records the replay
and actual-header selection controls.

For the adopted constructor, `V32FP_recreate` grew by 16 bytes and `V32.c.o`
by 32 bytes relative to the pinned loop baseline. The two constructor-backed
period fixtures passed 689,769 checks in total: `t_v32fprecr` 583,756 and
`t_v32fpdisp` 106,013. The full period gate passed 392 fixtures with zero
failures. This resolves the current out-of-bounds indexing while leaving the
original physical extent and historical member declaration explicitly
unresolved.
