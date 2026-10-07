# V17 control owner/value/return graph

Base9025b8d8. Original148B versus retained177B. Three independent original
uses support eight crossed full-TU cells, production first:

- Original a1b50 loads request scale once, stores PPS+50, then multiplies the
  same value by table[mode]. Retained source re-reads request after first store.
  Test compound multiplication of the stored scale, preserving both stores
  and its alias-visible value age rather than assuming disjoint pointers.
- Original addresses block+50 directly; candidate keeps PPS block+48 address
  and uses member+8. Test whole-owner field expression in place of subobject
  pointer. No field layout/headers are changed.
- Original create callback rejoins common return-one at a1b98. Retained source
  duplicates its epilogue. Test a single return after the conditional call.

No prototype/return ABI/profile change. Null request still returns zero.
Unknown alias overlap is not declared equivalent; complete strict EXACT and
deciding period differential are mandatory for production adoption. Full-TU
binding/data/BSS/nontext/bystanders must be audited. No mutation/fuzzing.
