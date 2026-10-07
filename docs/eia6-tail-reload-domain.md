# EIA6 conditional reload graph

Base75e7ef4b. Early-magnitude control is790B BYTES132. Its first109 canonical
instructions (including relocations and branch targets) exactly equal the
original. Remaining differences are final-copy register colors, conditional
copy issue order, and tail jump. Original jump targets past the shared params
reload; candidate targets that reload, three bytes earlier. Original can reuse
EBX as a third copy register, whereas candidate must keep this live for reload.

This independent original branch/use witness supports a bounded source graph:
the conditional arm already reloads params after its callback; put the later
reload only in the else arm, then reuse p for the final four copies. Preserve
both callbacks and their necessary reloads; do not assume callbacks are inert.
Three full-TU cells: production, previously measured early magnitude, and
conditional else reload. No other declaration/slot/color/profile permutations.
Prediction: bypassing reload releases this and permits the original copy graph.
Complete strict EXACT plus full-TU/data/metadata review and deciding period
differential remain mandatory before any production source adoption.
