# Constellation mixed-radix expansion: one complete gain

Base6b4509bd, retained Gentoo GCC3.4.2-r2 profile with reproduction define appended last; all compiler/selected assembler commands and hashes retained. Five complete-TU cells reproduce the raw baseline. Literal-index digit expansion plus literal-index place updates and original sum load roles reproduce **calcModulusParameters EXACT622B**, raising TU6/9→7/9 with no losses.

| Control | calcModulusParameters | getPower |
|---|---|---|
| retained loops |309B SIZE313|978B SIZE404|
| place expansion only |363B SIZE259|574B BYTES10|
| digit expansion only |565B SIZE57|574B BYTES10|
| both expansions |622B BYTES7|574B BYTES10|
| both + observed sum load roles |622B EXACT|574B BYTES10|

Original has5modulo+6division helper relocations and six static place-value stores; retained digit loop exposes1+2call sites. Both source components are required to reconstruct the entire622B body under the retained profile. Five ordinary mixed-radix divisions and special sixth truncation preserve original signed64 arithmetic and unsigned32 member reloads. Place product remains double, written without introducing extra rounds or caching member inputs across calls.

The final7bytes were solely shaperSR/word0 load roles entering the codeword-count exponent. Reversing the two commutative source operands recovers original ECX←shaperSR and EAX←word0; subsequent592bytes were already exact. InitialRTL records left operand in pseudo61 and right in pseudo62 for both source spellings, so reversal exchanges their field identities before allocation/scheduling. Final load order reverses the initial operand traversal in both cases. This is an observed operand-role discriminator, not a fabricated local/register/slot constraint.

Expanding either component also changes source complexity enough to stop automatically inlining calcModulusParameters into getPower. Its original CALL boundary and complete574B shape return, without a compiler flag. **getPower remains BYTES10**: its two double local stack homes0x28/0x30 are exchanged. No slot/declaration permutations or scalar-type controls are proposed; do not claim this residual proves original source correctness or a global inline-profile ceiling.

Full audit covers45emitted/common body comparisons. Seven other bodies remain unchanged; symbol metadata/binding/imports/exports, named/allocated nontext and BSS remain fixed. Exactly six nontext relocation destinations move, all entries of getPower's inline six-way switch. Every target is decoded to an instruction boundary in the owning function; final case offsets275,304,336,368,400,42 exactly reproduce the original. Baseline offsets705,629,667,591,553,310 are retained in the audit. Table payload changes are explicitly accounted for, not ignored.

Source winner is retained in src/pump/v90/V90ConstellationPower.cpp. Four literal apparatus anchors in v90cpmembers.json/v90cpowerstd.json are retargeted without executing mutations: exponent+1 fault remains the same sum minus5; shifted place-value fault still changes all five multiplier indices. All finds remain unique. No snapshots or test implementation changes.

Replay tools/batch_cpp_constellation_expand_reproduce.py; audit tools/batch_cpp_constellation_expand_audit.py; domain batch-cpp-constellation-expand-domain.md. Full artifacts are in build/batch-cpp-constellation-expand. Parent owns combined period/structural gate and publication; no commit here. Scalar expansion establishes a recovered family under the retained profile, not unique author spelling versus compiler unrolling under an unknown original profile.
