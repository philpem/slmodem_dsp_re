# V17 16-point slicer input owners before decoder owner

Original16pt entry0x98431..3b captures out_i,out_q,n_out before cfg.owner, then reads samples0x98442/4a. Current cfg.owner capturedfirst. Previous tick×ABSdomain has noexacthit; ABSnodes produce281B but merelywitnessseparateabsolutevalues. Newdomain3cells: rawbaseline; inputarray/count owners capturedbeforecfg.owner, actualsamples thenreadthroughthose existingmemberarrays; sameownerboundarycrossed withABSnegativecontrol. This is original ownershiplifetime andnot arbitrarylocals permutation: preserveargumentarrayreadorder, tick afterreads, same shortn_out narrowingandallstate accesses. No rawoffsets/aliasedview/spills/registertypes,headers orflags. FullTU data/metadata/relocs/all7functions required; noadoptionbysize.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- input-owners-first: all symbol verdicts unchanged.
- input-owners-first-abs-nodes: {'FAX_FSE_decision_16pt': ['SIZE', 1]}.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-fse16-owner`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
