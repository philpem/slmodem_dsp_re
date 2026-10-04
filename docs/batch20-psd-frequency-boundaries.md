# Psd frequency loop boundary controls

Baseline93d7eee1. Getter reference77B versus78B source. The reference loads
sampleRate on x87 before its zero-bin guard, compares zero to bins, and closes
the loop with bins>i. The current for-loop loads sampleRate after its guard
and compares i<bins. Products and integer-to-x87 widths already agree.

Before compilation declare four complete-TU cells: retained source; capture
sampleRate as a long double before the loop; explicit nonzero guarded do-loop
with bins>++i; both. Conversion from float to long double is exact and adds
no arithmetic or narrowing. No reciprocal is evaluated on the empty path.
Prediction: captured x87 ownership may reproduce unconditional load, while
explicit loop boundary may recover initial/backedge comparisons. Falsifier:
canonicalization leaves those boundaries unchanged or other bodies regress.
Do not enumerate register/declaration synonyms if this product misses.
