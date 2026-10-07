# Two transmitter control transfers

Base9025b8d8. Four cells each in V27t_stc and V29t_stc, untouched production
first. All original operands and callbacks were read independently.

V27 original148B first stores request scale at block+64, then multiplies
that captured value, writes owner+18, and finally publishes scale. Source
has only one scale store and a derived PPS pointer. Transfer the independently
verified V17 scale capture/publication graph to the whole block owner. Cross
original flag freshness: original reloads request+0d after conditional
source+8=1, while source carries flags across that store. Remove the snapshot
only at those two witnessed uses. Preserve layout and reinit callback.

V29 original126B similarly uses stored scale after its first store, rather
than re-reading request. Its one loaded request flag byte feeds both bit4
verdict and bit1 call guard across source+8 publication. Cross compound scale
with captured flag byte and ordinary zero/conditional-one local verdict,
then publish that verdict before callback. Original SETNE supports that
integer use graph; no arbitrary register/type/permutation controls.

Strict EXACT full bodies, all metadata/data/nontext and bystander review,
then deciding period differential required before adoption. No source-profile
exceptions, headers, undefined masks, mutation or fuzzing.
