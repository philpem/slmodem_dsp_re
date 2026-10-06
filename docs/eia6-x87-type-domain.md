# EIA6 producer mode, reopened by a backend lifetime witness

Declared2ca02aec before compilation. Original void edprintf boundary is
followed by FILD45365, FMUL45371 and nonpopping SI FIST45384, with x still
live and stack occupancy1. Corroborated GCC3 reg-stack live-XF rule duplicates
and pops with spare capacity; existing four actual Gentoo controls do exactly
that at34.stack. Stock backend source constrains the rule to XF, not SF/DF;
Gentoo patch sources are not proven identical. This is a new independent mode/
lifetime witness, not permission for arbitrary type permutations.

Six complete TUs cross producer XF/SF/DF with retained graph and already
witnessed literal18copy/single-zero graph. XF controls reproduce the closed
raw objects. For SF/DF use coherent x initializer cast, integer whole lift,
zero sign literal and matching fabsf/fabs spelling at that producer precision;
keep original SF-valued0.001f/10000.0f constants, integer truncations, separate
float xf publication and all diagnostic/field/callback/induction semantics.
The original floating instructions constrain SF constants and delayed xf
memory rounding, but do not uniquely choose SF versus DF source.

Predict SF/DF eliminate live-XF clone/pop and may preserve register multiply/
zero operands. Falsifier: wrong original operations, extra narrowing or any
period differential disagreement; raw full-body EXACT required for adoption.
Whole-TU metadata/data/bystanders and diagnostic tree/RTL provenance required.
No other type/slot/register/declaration/statement/flag search. Stop after6cells
unless a new independent witness appears. No fuzz/mutation, V34/PR263/shared
header/issue22 edits. Existing copy/predicate-only family remains closed.
