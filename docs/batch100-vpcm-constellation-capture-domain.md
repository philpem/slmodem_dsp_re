# VPcm constellation input captures before clamp

Original getConstellation loads nofSymbols0xf3db, lane0xf3e1 and values0xf3eb before branching to max-count clamp0xf3f9. Source captures lane/values only after that branch; baseline emits duplicate loads in the out-of-line clamp arm. Original rejection uses zero result plus common cleanup instead of early return, as independently observed in tap readers. This is input capture across a concrete source control boundary, not a free store/declaration permutation.

Predeclare four completeTU cells: baseline; move existing lane/values captures before maxCount clamp in observed lane-then-values order; accepted branch with common zero result; both. Preserve phase discriminator placement, integer division/remainder widths/trace arithmetic, sweepCounter updates, float1.7 scaling, all output stores and return width. No headers/flags/padding/artificial spills. Raw completeTU baseline and all-body/data/metadata/canonicalreloc audit required; targetexactness only.

Measured: Four valid cells: SIZE58 baseline, capture41, common41, cross25. No gains/losses; no source adoption.
