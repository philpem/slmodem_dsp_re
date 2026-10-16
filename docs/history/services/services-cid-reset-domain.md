# CID reset final publication

Base240481e6. Original8fd24 compares mode to5 before zeroing samples_fill,
with duplicate clear in the automatic arm preceding its mark_conf_step store.
Retained source clears samples_fill before the mode predicate. Compare raw
baseline, explicit predicate-first duplicate clears, and common after clear.
The last is a diagnostic of lexical order; no pointer-alias behavior is
asserted equivalent if writes overlap. Three complete TUs, retain all data,
exports and neighboring functions. Adopt only complete exact gain and final
period validation.
