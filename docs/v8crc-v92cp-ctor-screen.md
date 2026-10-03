# v8_crc msb and V92CP constructor screens

Two bounded data-mode domains posted before compilation
([#22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5965750934)),
run on the same worktree and profile as F11671. Both families closed with
measured negatives; nothing adopted. Artifacts:
`build/playbook-v8crc-v92cp-ctor/results.json`,
`build/playbook-v8crc-msb-v92cp-deforder/results.json`.

## v8_crc sign-bit extraction (40/41, SIZE 1, closed)

Blob: `movzwl 0x1e(%ecx),%eax; movswl %ax,%edx; shr $0x1f,%edx` -- unsigned
load primary, sign-extend the loaded register, logical shift 31. Eight
spellings of the extraction, all value-identical for 16-bit inputs, none
reaches it: baseline `((int)(short)crc) < 0 ? 1 : 0` (cast-free bit-15
shift), `hs->crc < 0 ? 1 : 0` and `(hs->crc >> 15) & 1` (both make the
SIGNED load primary: `movswl 0x1e(%ecx),%eax; movzwl %ax,%edx; ...` --
roles reversed), `((short)crc >> 15) & 1`, `((int)(short)crc) < 0`,
`(crc >> 15) & 1`, `crc >> 15`, `(((int)(short)crc) >> 31) & 1`. The blob's
shape needs an unsigned primary load whose register is later re-narrowed
signed -- not produced by any spelling of this extraction; the family is
closed without a source adoption. The residual one byte is the movswl
register encoding.

## V92CP constructor statement order (61/61, decoded order, not adopted)

Blob emission: [0x11c=0x12, 0x120=0, 0x114=0 (fresh xor), 0x119, 0x11a,
0x4, 0x914]. Six insertion points for `byte_04` in resetDetector's statement
order (bitIndex, stateBitCount, rxState, onesRun, zerosRun): position 5
(byte_04 after zerosRun) emits the blob's instruction-for-instruction body
with registers renamed -- BYTES(8), grade-1 ACCEPT, down from 34 differing
bytes at the transcribed offset order. The author's ctor body order is
decoded: resetDetector's order plus byte_04 after zerosRun. The remaining
8 bytes are one register-pair assignment (0x12 and -1 swap ecx/edx).

Reordering V92CP.cpp's thirteen definitions to the blob's own address order
(D2, resetCRC, calcCRC, resetDetector, reset, ctor, setSUV, evaluateCRC,
getBitVector, float2Bits, infoToBits, evaluateInfo, bitsToInfo) keeps all
seven exact functions exact but changes four bodies and moves the ctor the
WRONG way (BYTES 29). The ctor's register carrier is not the TU definition
order at this granularity; both cells declined, nothing adopted. The
statement-order fact stands as the strongest near-miss for a future
adoption if the register carrier is found.

## Scope

No differential or fuzz change: the msb spellings are value-identical and
the reorders are behavior-preserving. No fuzzing or mutation execution;
static anchor checks before any commit. Canonical verdicts via
byteident.py only; changed-bodies lists per cell in the results.json
artifacts.
