# Quality monitor use and publication boundary cross

Base a95a6c65, retained Gentoo profile and production1074 strict census.
Only QualityDetectV17/V27/V29 in their full original TUs. Three independent
source properties identified by reading all three original bodies:

- Count publication tails: original averaging path falls through after signed
  <=49, stores averaged error, publishes n+1 and returns; judged-count path
  reloads the member for its increment and joins that publication separately.
  Retained V17/V29 reverse the outer branch, while V27 has nested else-if and
  no averaging return. Test low-count averaging/return followed by distinct
  count50 judgement/member increment. Preserve seed0, frozen >50, signed
  comparisons, coefficient/product/rounding order, latch and all values.
- Count tests directly consume the count member at zero and signed branch
  boundaries, instead of testing a sign-extended captured local at each guard.
  Original has MOVZWL then TEST/CMP word before arithmetic; this is a use
  boundary, not a local-type or register permutation. Retain the entry local
  for averaging n+1 so no unobserved publication freshness is introduced.
- Original low signal word is sign-extended before AND with full-width active
  state. V27/V29 source currently AND full-width operands then narrows result.
  Test explicit ordinary short narrowing before that AND, preserving final
  short conversion. All low16 outputs equal for all bit patterns. V17 already
  uses the narrow union member, so no additional narrowing cell there.

V17/V27 fixed-control cells include the independently original post-diagnostic
owner reread from the previously negative quality-owner domain; V29 already
has that reread. This repeats old owner-only negative controls intentionally
as the fixed context for new interactions, not as a fresh owner-family claim.
No shared headers/API/structure changes. No declaration/slot/register/order
fitting, flags or adjacent carrier edits. Baseline plus4 combinations forV17,
baseline plus8 forV27, baseline plus7 nonbaseline combinations forV29:
22full-TU cells declared before compilation. Unique source emissions counted;
near sizes not adopted. Predictions: count word/branch/tail and signal-load
boundaries may jointly recover full clone bodies; any whole-body nonexact
result falsifies that cell as preimage. Keep all informative negatives.
Raw baseline/control repeat, full flags/assembler/bugdefine-last provenance,
every emitted body and binding/data/BSS/nontext relocation audit required.
No fuzz/mutation/harness execution; parent owns combined final gates/commit.
