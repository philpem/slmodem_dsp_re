# V22 FSE whole config sampling boundary

Baseline93d7eee1, only src/pump/v22/v22_fse.c; no headers/profilechanges.
Original initializer loads both cfgpointers (qthen i) before assigning either
state pointer; current scalar sequence assigns icoff before reading qcoff.
Original working-coefficient loop reloads copiedstate.icoff afterallocations;
current compiler carries originalcfg.icoff longer. Two sourcecells: retained
scalar assignments versus ordinary structconfigvalue sampled first, then both
scalar stores. Same values/types/storeorder/loops/allocations; no memcpy or
pointercasts, syntheticspills or registerfitting. FullTUmetadata/data/canonical
relocations and all4functions (including exactfree/getdiag controls) audited.
Close onnegative; root batchhas20validatedgains and no furtherexpansion.

Result: whole-config sampling reaches full372B, BYTES5, grade1alpha exact
(register names apart); baseline SIZE7. Only init changes,2/4strict exact
unchanged. CompleteTU metadata/data/allocatednontext/canonicalrelocs agree.
No byte-exact gain or source/header adoption. The independently justified
config-value boundary resolves instruction structure, leaving a register-only
residual; preserve as a bounded preimage for future original allocation-profile
work, rather than extend arbitrary temporaries or reorder other declarations.
No further expansion in this batch, as instructed byparent.
