# V90 P3 reset: duplicated source call merges after reload

At base `276d30b5`, the independently bounded arm-local DIL reset control yields **no byte-exact gain**. Two complete translation-unit controls give 56 emitted-body comparisons, 18/28 exact in each. The unchanged raw baseline reproduces the archived production object. Both experimental objects are themselves raw byte-identical, including all bodies, symbol bindings, imports/exports, allocated named/anonymous data, BSS, nontext relocation identities and text positions. Reset remains 427 bytes against the original 479 bytes (SIZE 52). No production source/header change was adopted; no runtime, mutation or fuzzing was executed.

The full original graph corrects the screening hypothesis. The mode-zero arm obtains/stores `jdBits` and calls DIL reset at `0x2c3ac`; the other arm obtains/stores `jdV92Bits` and `jdV92PhaseBits`, including null handling, and calls it at `0x2c45d`. Both arms reach **the same warmup loop** at `0x2c46a`. The loop reloads `sessionFlag` at `0x2c47f` on each iteration, selecting `generateV92Symbol` at `0x2c473` or `generateV90Symbol` at `0x2c488`. Thus fixed-mode or captured-mode warmup is not supported by the original. Different argument setup ordering around the two DIL calls is observable, but does not alone identify the original source spelling.

The control moves the shared DIL reset into both mode arms, after the same vector stores, while retaining the shared dynamically dispatched warmup. Its initial RTL has two reset calls (UIDs 261/328 versus baseline 316). Both remain distinct through `.26.postreload` (260/327 versus baseline 315). **`.27.flow2` is the first measured merge**, retaining UID327; one call remains through `.35.mach`. This is a late compiler CFG-sharing boundary, after register allocation/reload, rather than evidence that source lacks a reset statement. We do not infer the exact responsible internal routine or a unique original optimization profile from the dump name.

The stage detector fires on the known source duplication before testing the clean final result: 58 single instruction streams checked (29 per control). `.00.cgraph` is not RTL and `.08.gcse` embeds earlier snapshots with duplicated instruction IDs; both are explicitly excluded from the single-stream parser. Full-object byte equality independently covers the final assembly even if a diagnostic stage is excluded.

Reproduce from the isolated checkout, using the retained Gentoo profile and compiler-selected assembler:

```sh
python3 tools/batch_cpp_p3_reset_reproduce.py \
  --domain docs/batch-cpp-p3-reset-domain.md \
  --baseline-dir build/production-before
python3 tools/batch_cpp_p3_reset_audit.py
```

Artifacts are under `build/batch-cpp-p3-reset`: `results.json` records complete commands, configuration, source/header/object hashes and body verdicts; `audit.json` records stage call IDs and the full-object audit; `original-reset.dis` preserves the complete original function. The compiler is Gentoo GCC 3.4.2-r2 and its executed assembler is GNU 2.15.92.0.2. The reproduction define is last in each actual compile command.

Close this two-cell domain. A further source experiment needs an independent original operand/owner/CFG discriminator; call multiplicity alone is insufficient. This result does not establish that all reconstruction source is correct, that all remaining mismatches are register allocation, or that issue #22 has a unique solution.
