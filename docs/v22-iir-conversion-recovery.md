# V22 IIR: narrow at uses and reset at the iteration boundary

Baseline5ec53f63,867/1852 exact functions and84,358 exact bytes. Blob
V22_iir_filt_demod232B stores accumulator BX into yhist[0] before sign-extending
BX for the mixer. Retained230B source uses a shared short y, promotes it before
that history store, and clears its accumulator at the loop header rather than
the blob's preheader/latch. Inner arithmetic/history loop graphs otherwise match.

The first two-cell full-TU domain removes y and narrows at the two uses,
preserving the mixer cast and avoiding an alias-sensitive history reread.
It recovers store-before-extension, but remains230B/SIZE2; no adoption alone.

A second four-cell domain crosses shared temporary/use-site narrowing with
header/boundary accumulator reset. Boundary form initializes int acc=0 before
the outer loop and resets it after each output store. Shared temporary with
boundary reset232B/BYTES14; both changes232B/EXACT. Neither independent change
recovers the whole function. The combined body has no relocations and matches
all232 bytes; the already-exact initialization function stays exact.

All6 valid compile cells include2 raw unchanged controls;4 distinct source
forms/emissions, with the two original cells replayed exactly in the cross.
Both functions/all4 globals preserve their binding/inventory; only demod changes.
Final TU1/2 ->2/2, no loss. Signed-short loop indices, arithmetic shifts/product
order, coefficient/history ordering, fixed160-sample block and mixer truncation
remain unchanged. No flag, declaration-order or register-name permutation.

[Use-site domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945740690),
[reset-boundary cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945754503).
Replay tools/playbook_v22_iir_narrow.py then tools/playbook_v22_iir_boundary.py,
each with --domain URL. Corresponding build/ artifacts retain complete Gentoo
GCC3.4.2-r2/selected assembler2.15.92.0.2 identities, saved .build-config commands
with mandatory DSPLIB_REPRODUCE_BUGS, source/header/object hashes, inventories,
canonical verdicts and changed-body disassembly.

Existing fixed t_v22_iir compares coefficient copies, complete output/history
state over repeated blocks, and overdriven filter/mixer truncation. Independent
model guards show filter accumulator and mixer overflow cases actually occur.
These component fixtures are not modem-lifecycle reachability evidence. No
fixture, tolerance or static-anchor change; no fuzzing or mutation execution.

## Retained validation

Complete comparison build300/300, zero failures; only v22_iir.c.o changes and
its entire bytes raw-reproduce the tested combined winner. Coefficient data16
bytes unchanged, no read-only/BSS data introduced. Whole-tree867/1852 ->868/1852,
exact bytes84,358 ->84,590, only V22_iir_filt_demod gained, zero losses.
Fixed Gentoo make phase385 passed/0 failed; t_v22_iir reports6,410 checks with
filter/mixer overflow guards firing. Structural references14,229/2,695 headings
clean;285 suites/10,038 static anchors unique. No fixtures/anchors changed.

Complete same-order300-object partial links remain DIFFERENT(strict exit1).
Positioned equality68,317 ->68,316/943,398; allocated914,142 bytes unchanged.
Exact section70/92, symbol394/2,907 and relocation1,018/18,317 records unchanged.
Canonical function-byte recovery does not establish partial-link completion.
