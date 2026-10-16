# Fax CRC arithmetic boundary

Four full-TU cells at240481e6: unchanged baseline, pre-OR subtotal narrowing,
postdecrement countdown, crossed control. Original967b5 narrows BX into EDX
before OR with t>>12; retained macro narrows after OR. This independently
reopens the batch50 countdown-only negative. The same operator placement is
observed for the second nibble at967d4. The final OR cannot introduce bits
above bit15 because t is masked0xf000 and t>>12 is at most15, so pre-OR
narrowing preserves the16-bit map. All helper calls and API types unchanged.
Raw baseline reproduction, every emitted body and binding/data/nontext
relocation audit required. A miss closes this finite family; no near-score
adoption or register/declaration permutation.
