# FCS nibble conversion and FIFO value selection controls

902f47fa fixed complete compiler flags. FCS blob narrows polynomial step to
16 bits before OR-ing the low nibble; ours narrows after OR. The lower nibble
cannot overlap bits outside16, so one explicit unsigned-short conversion
before OR is equivalent. Cross independently with narrow postdecrement
condition already bounded; this is a new arithmetic use boundary witness.

FIFO_write blob seeds accepted count with available space then selects the
requested count when it fits; source seeds requested count and clamps it.
Cross only this two-arm min value owner and postdecrement condition seen in
blob. No cursor types, local permutations, state layout or profile edits.
All complete TU bodies/exports/data/relocations audit; stop after four cells.
