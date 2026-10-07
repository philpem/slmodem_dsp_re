# Energy callback/result graph: bounded controls close without adoption

Four valid full Fdspkrnl translation-unit controls at75e7ef4b give24 emitted-body comparisons,2/6exact in each, zero gains/losses. Raw unchanged baseline reproduces archived production. Complete binding/import/export, allocated named/anonymous data, BSS and nontext relocation audits are unchanged. Text positions vary as the two nonexact bodies vary; all six emitted functions are reviewed, not only the target.

| Result graph | RMS evaluation | bValidateEnergyValue bytes | Changed bodies |
| --- | --- | ---: | --- |
| Retained | Retained | 596 | None |
| Retained | Sequenced before index | 580 | Target, FDSP_Kernel_Loop |
| Common success | Retained | 614 | Target, FDSP_Kernel_Loop |
| Common success | Sequenced before index | 598 | Target only |
| Original | Original | 296 | — |

The callback value-age detector fires: baseline first index load UID28 precedes RMS call31; sequenced control call31 precedes first index load34. Common-success retained uses load32 beforecall35; combined uses call35 beforeload38. These are initial RTL facts before optimizers can obscure the source-evaluation boundary. Common-success control preserves beep bypass, quiet-success case, countdown, reset/debug callbacks and all arithmetic; no index or result width is changed.

Complete call inventories identify an independent remaining boundary: original calls FDSP_Kernel_InitObj once out-of-line; all four reconstructed target bodies inline it. None recovers the original296B body. Changing local result structure or sequencing an independently witnessed callback read therefore does not suffice for complete byte identity. This report does not infer a unique original optimization profile or classify every remaining instruction as an inlining artifact.

The historical source comment is stale: it describes an external reconstruction with ordinary convention, but the actual definition is `static int`, and original/current ELF symbols are both STB_LOCAL. Actual original entry uses EAX for buf and EDX for n, saving EDX to EBP; remaining hist/idxp/histlen/k are stack arguments. Retained entry likewise preserves EAX into ECX and EDX into EBP, with the same four stack arguments. Thus the observed effective ABI is two register arguments on both sides, consistent with regparm(2), not an ABI mismatch. The source has no explicit regparm attribute; whether original used one or received equivalent local-call optimization is not uniquely recovered. No binding/ABI/profile change was made.

Close the declared2×2 domain. No partial operand/result gain is adopted, no production source/header changes are made, and no further body/flags coercion or neighboring matrix follows without a new witness. No runtime, fuzzing or mutation execution.

Replay tools/energy_callback_result_reproduce.py with docs/energy-callback-result-domain.md and sibling byteexact-x87-scheduling/build/production-before; then tools/energy_callback_result_audit.py. Build artifacts under build/energy-callback-result contain full commands, identity, source/header/object hashes, dumps, all grades and audit.json. Gentoo retained profile and compiler-selected assembler were executed, with DSPLIB_REPRODUCE_BUGS last in each command.
