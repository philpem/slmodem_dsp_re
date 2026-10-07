# Multiplied scale capture and publication

Base9025b8d8. Previous eight-cell value/owner/common-return graph closes at
148B BYTES8: all canonical instructions agree except one three-instruction
order. Original computes IMULa1b5d, reads request+10a1b65, stores owner+18
a1b68, then publishes multiplied PPS+50a1b6b. Candidate publishes PPS first.

This independent value-age/store witness supports separating arithmetic
capture from publication, preserving the original alias-visible order. Four
full-TU cells: production; prior compound/whole-owner/common-return graph;
explicit multiplied-scale capture with early store control; same capture with
store after request+10→owner+18. No guessed register or arbitrary declaration
placement; new temporary expresses the observed value lifetime. Do not delay
the multiplication itself past the intervening store. Strict full body plus
metadata/data/bystanders and deciding period gate required before adoption.
