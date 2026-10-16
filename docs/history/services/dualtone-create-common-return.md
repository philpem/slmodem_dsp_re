# Dual_TONE_create: allocation outcomes share the returned pointer

Baseline2be7a9e8. The blob tests the allocator result in EAX, saves it in
EBX, and branches over initialization on failure. Both paths return EBX
through the same epilogue. Retained source early-returned literal zero;
Gentoo materialized a separate zero result and emitted66 bytes against64.

Two predeclared full-TU cells test unchanged source and guarded successful
initialization followed by unconditional `return st`. The candidate reproduces
all64 bytes and both canonical sysdep_malloc/sysdep_memset relocations. All
three functions and strong globals survive; only Dual_TONE_create changes.
TU exactness1/3 ->2/3, zero losses. The complete unchanged object raw-reproduces.
Dual_TONE_detect stays SIZE17 and is not inferred to be recovered.

Allocation size, clear size, ratio226, minimum energy1 and call order are
unchanged. When allocation fails, st is null and the common return returns
null without touching memory. When it succeeds, the same initialization runs.
The existing fixed t_dualtone checks allocation size/accounting, the complete
initialized object, field values and delete behavior. It does not force
allocation failure; that path's equivalence is established structurally and
by complete function-byte identity, not claimed as runtime fixture coverage.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945100150).
Replay `python3 tools/playbook_dualtone_create.py --domain URL`. Artifacts in
`build/playbook-dualtone-create/`: shared complete `.build-config` commands,
source/header hashes, inventories/bindings, all changed canonical bodies and
relocation-bearing disassembly. Gentoo GCC3.4.2-r2 and selected assembler
2.15.92.0.2 are executed; mandatory bug reproduction is enabled. No flag or
initialization permutation; the two-cell domain is closed.

## Validation

Complete comparison build300/300, zero failures. Only the DualTone_Detector
object changes; its complete retained bytes raw-reproduce the tested winner.
Whole-tree862/1852 ->863/1852, exact bytes83,784 ->83,848; only
Dual_TONE_create gained, zero losses. Saved before/after inventories and
exact-name delta live in `build/playbook-dualtone-adoption/` with all300
baseline objects. Notch remains untouched.

Same-order complete300-object partial links remain DIFFERENT(strict exit1).
Positioned equality68,477 ->68,340 /943,398:137 fewer matching bytes as
later code shifts. Candidate allocated bytes914,190 ->914,174. Exact section
records70/92 and symbol records394/2,907 unchanged; exact relocation records
1,019 ->1,022 /18,317. These measured positional effects do not contradict
the canonical function gain, and that gain does not imply whole-object identity.

Fixed Gentoo make phase passes385/0; structural checks clean. Reference census
14,224 refs/2,685 finding headings; static anchors285 suites/10,038, all unique.
Both new replay tools compile; whitespace checks pass. No mutation execution,
snapshot refresh, fixture change or modern portability claim.
