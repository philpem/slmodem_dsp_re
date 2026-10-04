# Designer quiet NaN source initialization

Pinnedf2cdb66a, immutable300 period objects in build/production-before,
1045/1852 strict exact. Precompile four complete
V90ConstellationDesigner.cpp overlays independently crossing the high-rate
and low-rate NaN initializer nanf("")→NAN only. The original target loads quiet
NaN0x7fc00000 twice; direct-math rewriteF11201 introduced nanf source text,
whose period compilation adds a call. Existing math.h supplies NAN. No
initialization order, types, arithmetic, layouts, headers or flags change.
Both float initializers remain quiet NaN sentinels; preserve bug define after
complete retained Gentoo flags and execute selected assembler. Use -da first;
if GCC diagnostic dump ICEs, preserve invalid attempt and rerun all four with
-dr, requiring raw baseline object reproduction. Stop after this source domain.
Audit every bystander, metadata, named data/nontext/canonical relocation and
imports, explicitly recording new NaN constant/data changes. No production
source adoption, central findings or Playbook edit, runtimes/fuzz/mutation.

All four full-da compilations succeed. Each retains13/24 strict exact functions,
zero gains/losses. Original setConstellationToNoise3641B versus retained3623B,
each one-NAN overlay3614B and both-NAN3598B. Neither source boundary yields
an exact function; stop without further statement/type/flag variants.

Retained two source nanf calls CSE into one emitted call. Either isolated NAN
keeps that call/import/empty argument, adding exactly one SF quiet-NaN pool
entry0000c07f. Both NAN removes nanf and its empty-string reference/import;
it reuses the same quiet-NaN pool entry. The canonical target relocation
multiset differs only by that NaN entry and, in the both-NAN cell, the removed
nanf call/empty string. Metadata/export/function inventories unchanged except
the expected missing undefined nanf symbol; zero named data symbols. All other
allocated constants/data values and nontext relocations preserved. Diagnostic
string pool reorders, preserving every string and multiplicity; both-NAN
removes precisely the extra empty .rodata.str1.1 string. No other data changes.

Five raw bystander bodies change when section-relative constant/string pool
addends move. Every one of the23 canonical bystander bodies is EXACT against
the raw-reproduced retained baseline, with equal canonical relocation maps and
identical nonpadding decoded instruction operands. This is pool placement,
not a bystander source or instruction change. Complete4/4 TU audit passes:
`python3 tools/gcc3_uref_designer_nan_audit.py`. Artifacts include actual complete
compiler/selected assembler identities, commands, source/object hashes,
full-da RTL and complete-tu-audit.json. No audited byte-exact winner exists.
Production source/header and central findings/Playbook remain untouched.
