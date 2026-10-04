# MTK oscillator source boundaries

Reference MTK_phasor converts angle to a signed word and converts it back to
x87 via FILD16; retained int n initialized from(short)y uses FILD32. Reference
quadrant reflection tests bit0 of computed quadrant registerDL; retained
condition n&0x100 tests AH. Reference terminal phase has separate member
store/return arms without round-trip float temporary, unlike retained s=float
cast followed by common member store. Eight-cell domain: short n carrier;
quadrant&1 predicate owner; direct terminal member phase stores under same
comparison. Preserve all interpolation expressions and negative x correction.
Original FP diagnose independently: blob FPRem loop, production C callsfmodf;
no profile change in these controls. Terminal store change removes explicit
intermediate narrowing but member rounding remains, NaN predicate unchanged.
Complete TU single function/all data/reloc profiles compared. No header edits.

Parent explicitly allocated independent -ffast-math diagnostic: four-cell
source/profile cross retained versus all three source boundaries, default
versus extra -ffast-math. Raw production default baseline verified. No profile
adoption from exact-set score; fmodf call vs FPRem is an original pass witness.
Library semantics limit: default fmodf has external library exceptional-input
handling, builtin fast remainder can change NaN/sign/errno/out-of-range rules;
source/API changes cannot be inferred simply from matching ordinary oscillator
values. Preserve complete option command and imports/const pools separately.

Eight default source controls: baseline255B vs271B; short-only256B;
member-only252B; all253B; no exact gain/loss. Four profile controls: baseline
255B, source-only253B, fastmath-original264B and fastmath-source264B. Both
fastmath bodies have one FPRem/no calls versus default one fmodf/no FPRem.
All default8 full-TU audits pass unchanged metadata/nontext/no data. Profile
4/4 complete audits account exactly for dropped fmodf undefined import and
added float6Pi constant (cst4 8→12B), remaining pools/data/nontext unchanged.
Source/profile family does not reproduce complete body. No adoption or claim
of original fast-math command; original no-call FPRem witness is independent
support for reopening profile question, not exact-set evidence alone.

Independent header provenance discriminator, explicitly allocated by parent:
Gentoo bits/mathinline.h449 defines fmod/fmodf inline asm FPRem under
__FAST_MATH__ and GCC<3.5. Four crossed default/macro-only(-D__FAST_MATH__)
× retained/fullsource controls isolate inline header selection from unsafe
compiler optimization flags. No production macros/header/profile changes.
Commands append mandatory bug define last through shared compile helper.
Preprocessed body and actual Gentoo headers retained; inline-header output is
apparatus/provenance diagnostic, not adopter optimization/profile by score.

Parent allocated fresh library/formal width discriminator: macro-only inline
selection crossed with retained/full-source × fmodf(float modulus), fmod
(double formal with float modulus promoted), fmod(double formal,double typed
modulus). Six diagnostic cells plus raw default baseline. Reference remainder
loads float6Pi, rounds result SF; double-formal float-modulus preserves same
exact modulus, double-typed modulus changes it and is a deliberate excluded
semantic control unless object operands support it. Formal library names are
header/prototype evidence; no fmod references in blob because inline wrapper.
Compare pool operands/FPRem lifetime and initial XF→DF/SF truncation paths,
not size/score. Retain explicit double negative correction and short/public
prototype otherwise. No production profile/source adoption from this cross.

Library width rejects double fmod as original preimage: double-formal float
modulus inserts FSTPL/FLDL before SF round, not in blob. Double-typed modulus
also changes original float6Pi operand. Fresh conditional source discriminator:
reference duplicates x87value before negative correction and sends positive
arm via late stack-pop; ternary float/double arms create promoted-double
conditional result before float destination, unlike current float in-place
if. Four macro-only controls cross negative mixed-type ternary and terminal
member mixed-type ternary on short-n/quadrant source, plus raw default baseline.
All type promotions preserve values after unchanged float destinations;
no volatile, asm, local array/register/stack fitting. Stop on negative result.

Mixed-conditional control negative: negative ternary raw-inert; final ternary
271B but82vs81 instructions, not grade1. Close syntax family. Last independent
carrier domain: header fmodf yields SF, normalized phase kept across lookups/
interpolation/final addition; source double local can retain SF API rounding
and explicit negative(float) correction while giving DF value lifetime rather
than SF SSA. Four macro-only float/double x ×float/double s carrier controls,
plus raw default baseline, on short-angle/quadrant/direct-member source.
No public fields/prototypes/modulus/casts/options changed. Both widths preserve
original explicit corrections; excess precision is a measured discriminator,
not assumed universal semantic equivalence. No further width permutations.

Macro-only four-cell whole objects raw-equal fast-math whole objects for both
source states. This establishes inline glibc header mechanism in this TU,
without asserting a unique original compiler option. Preprocessed controls
4/4 record asm only on enabled-header cells; retained Gentoo math.h and
bits/mathinline.h at build/gcc3-batch100-mtk-inline-header. InitialRTL shows
asm_operands/v:XF FPRem followed by SF truncation, no builtin call expansion.
No fno-builtin discriminator needed because the header asm is explicit.

Library7TUs: float-formal264B; double-formal/floatmod268B or265B; doublemod
266B. None exact. The double-formal near size is rejected because extra
FSTPL/FLDL round-trip precedes SF truncation, absent original.

Conditional5TUs: negative ternary raw-inert, terminal ternary271B differs143
bytes/82vs81 instructions. Carrier5TUs: phase double alone264B (constant comparison pool mode differs); double advance
247B, bothdouble237B. No exact gain/loss; no remaining local width/conditional
spelling sweeps justified. Literal zero compare moves cst4→cst8 under double
phase, explicitly audited alongside dropped fmodf import/float6Pi addition.

Combined diagnostic audit25/25 fullTUs preserves every metadata record except
expected fmodf import removal, all nontext/data/relocs and mode-specific
constant changes. No production source/header/options adopted. SoleFPRem
original wrapper provenance is the finding; byte-exact gain remains0.
