# V.22 permutation counter: lifetime and consumed index

Original `V22_MRF_init` initializes EBX to zero at 0x8d069, before the
allocation branch and history clear. It uses that counter in the coefficient
permutation: load the source coefficient at 0x8d0e7, compute the successor,
store through the old destination index at 0x8d0ee, then publish the narrowed
successor at 0x8d0f6. Retained source initializes `k` only immediately before
the permutation and writes `work[k++]`, whose compiled counter publication
precedes the source load and destination store.

Cross two ordinary source explanations, four cells including baseline:
initialize the existing short counter at declaration; separate the
coefficient assignment from the following increment. Preserve types,
array bounds, allocation, arithmetic and observable memory order. Prediction:
these source lifetime/use boundaries reproduce the original allocation and
store schedule. Falsifier: complete function remains nonexact. No arbitrary
declaration permutations, register constraints or alternate loop arithmetic
are in this domain.

Compile complete translation units under the unchanged Gentoo configuration
with bug reproduction enabled. Require unchanged raw reproduction; review all
shared bodies, allocated data, symbol metadata and relocations. Adopt only
supported strict gains without unexplained losses.

The first four cells restored the early initialization and delayed
publication separately, but their combination remained SIZE by one byte,
with different register lifetimes. The original load-before-successor witness
also supports capturing `short value = coeff[i]` before `work[k++] = value`.
Add that ordinary source-load boundary with late/early initialization (two
cells, six total). This is a consumed-value witness, not a new index/type or
register permutation. The complete original is still the acceptance test;
near-size is not grounds for adopting any of these controls.
