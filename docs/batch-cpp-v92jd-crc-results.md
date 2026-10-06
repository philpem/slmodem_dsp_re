# V92 CRC ownership/expansion transfer: bounded negative

Base6b4509bd, four complete V92Jd TU controls cross constant-index CRC shifts with direct same-owner input/CRC member expressions. Original packJdData665B/packJdPhaseData681B promotes sixteen CRC ints into a64byte portion of a0x48frame; retained pointer-helper/rolled shifts emit237/253B. Same-owner/component references plus constant shifts reproduce original665/681B lengths and loop promotion, but remainBYTES255/259. This transfer recovers a mechanism, not complete functions.

Pointer-helper expansion alone reaches525/541B; direct members with rolled shift reach259/255B and lose two previously exact unpack-reset functions. Crossed expanded/direct restores both exact resets, leaving zero losses and zero complete gains. Get-vector wrappers return to original22B CALL+LEA boundaries as packer source complexity grows; they remainBYTES1/BYTES4. No production V92 source edit or declaration/register/slot followup is retained.

Rawbaseline reproduced,21emitted/shared bodies per cell (84comparisons). Full symbol metadata/bindings/imports/exports, named/allocated nontext, BSS and nontext relocations fixed. Losing control explicitly reports its two exact losses; it is not hidden behind the final zero net count. Expanded/direct changes exactly two packet bodies and their two wrappers; all17other bodies unchanged. Runtime/mutation/fuzzing not executed.

Replay tools/batch_cpp_v92jd_crc_reproduce.py; audit tools/batch_cpp_v92jd_crc_audit.py; declared domain batch-cpp-v92jd-crc-domain.md. Commands, hashes, fullRTL and changed-body disassembly reside under build/batch-cpp-v92jd-crc. The family is closed absent an independent new source discriminator, rather than further register/stack permutations.
