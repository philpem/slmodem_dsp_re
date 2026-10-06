# FPM_FSE_init configuration-copy dependency boundary

Declared before compilation on e0052eec, complete fpm_fse.c, unchanged
Gentoo retained flags and headers, bug define last. Original 458 bytes and
retained 458 bytes differ at the configuration publication: original REP MOVSL
follows four enable stores and precedes freq (+0x3c), phase_acc (+0x70).
Retained structure assignment allows both later reset stores to be scheduled
before REP MOVSL. Subsequent allocation/free/short-index loops match.

Four cells cross the ordinary struct assignment versus builtin memcpy of the
same complete configuration, and its source placement before all reset stores
versus after the four enable stores. Original operands establish the boundary;
this is not an arbitrary store permutation or register-fitting experiment.
Preserve all allocation sizes, narrowing, loop bounds, diagnostics, signatures,
data and binding. No forced dependencies, volatile, profile/header edits or
runtime/mutation/fuzz execution. Audit all four bodies and complete metadata,
allocated bytes, BSS and nontext relocation identities; raw baseline must match.
Stop after this crossed domain if no exact gain. A new lead then requires an
independent original operand/source/RTL discriminator.
