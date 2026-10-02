# ECC initializer retained locals

F11596. Nine predeclared full-TU cells at 5a54307b cross uncached/signed-short/
promoted-int near-delay with uncached/signed-short/promoted-int fill.
[Domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949412015).
Blob early signed loads/spills at 0xa7646/0xa764a precede allocation; final
fill at 0xa77a0 reads the spill. Retained source reads near-delay later and
fill in each final-loop iteration. Cache after config copy, before arithmetic
and allocation; no public type or allocation/count/store changes.

Nine source cells, four complete emissions. Short/int cache choices raw-merge
for each presence pair, so local width is not uniquely recovered. Baseline
raw-reproduces production; delay-only and fill-only each reach603B/SIZE4,
combined607B is exact. All four combined width cells raw-agree. Retain signed
short locals, matching the stored fields, with ordinary C promotion at uses.

Complete three-function review: cancel unchanged (1873B, SIZE178 vs2051B),
init599→607B exact, free98B becomes exact with no source change: its dead
stack discard POP switches EAX→ECX. This is a preceding-TU codegen effect,
not recovered free source. Delay-only already changes that POP. All9 cells
preserve all three functions, global/type/binding/visibility and ECC_CFG24B;
all allocated nontext section bytes and canonical data relocations agree.
Exact set0/3→2/3 with no losses. Neither complete TU nor original flag profile
is claimed recovered; the cancel residual remains.

Replay tools/playbook_ecc_init_cache.py --domain <linked URL>.
Artifacts build/playbook-ecc-init-cache retain full actual saved commands,
mandatory DSPLIB_REPRODUCE_BUGS, header/source hashes, nine complete objects,
initial RTL and complete-object-audit.json. Gentoo GCC3.4.2-r2; executed selected
assembler2.15.92.0.2. No fuzzing or mutation execution.

The new fixed borrowed-buffer alias control uses fresh=0, near_taps1/far_taps0,
far_lag2 and near_i pointing to cfg.fill. Earlier clear overwrites that member;
blob still fills two line elements with the saved input. Four original fills
{-32768,-1,16,0x0103} give24 checks, including reference nonvacuity controls.
This is synthetic component alias fidelity, not a reachable modem lifecycle.
Borrowed arrays are never passed to ECC_free. Usual existing tests separately
cover fresh/reuse, NULL config, near/far-only, small lines and signal cancellation.

The initial fixture invocation had an uninitialized aggregate status later
replaced by the next block's result, so its green wrapper result is INVALID
and excluded (/tmp/ecc-init-before-fixture-invalid-return.log). Corrected
initialized/accumulated status was rerun before source adoption and fails as
required. Do not interpret the invalid run as a passing baseline.

Retained comparison build300/300, zero failures; only src_dsp_fpm_ecc.c.o
changes and raw-reproduces combined short-cache winner. Whole tree882/1852
→884/1852 exact,87,605→88,310 exact bytes: init607B and free98B gains, no losses.
Same-order complete300-object partial links remain DIFFERENT (strict exit1):
positioned68,258→68,254 /943,398, allocated914,142 unchanged; exact section70/92,
symbol394/2907, relocation1020/18317 unchanged. No layout score fitting or
complete-object identity claim.

Fixed Gentoo make phase385 passed/0 failed; all9438 ECC checks pass including
24 alias checks (8/24 failed on corrected baseline fixture). Structural14240
references/2725 finding headings and285 suites/10038 static anchors clean.
No fuzzing/mutation execution or modern portability claim.
