# SGD symbol count and receiving width

Pinned856c1ecb full Sgd.c TU. Original SGD_symbol_gen loads signed field cfg.sym_bits with MOVZWL at0x9f6b9 and uses its promoted value for k*bits; baseline uses signed-int receiving local and MOVSWL. F11351 review deliberately keeps shared field signed because pattern_det sign-extends it, but does not forbid a local unsigned-short receiver in this distinct function. Predeclare unsigned-short bits local versus baselineint, crossed with while(n--) versus manually seeded sentinelcount; original word countdown ends MOVSWL/INCword/JNE0x9f70f..714. Batch50 count family applied onlycorrelate/pattern, notsymbolgenerator. Preserve datalocal, signedlast/k/next andternary,outputpointer advance, maskwidth/unsignedchar shiftcount,member writes/return. Every lowshiftcount identical; original hardware onlyuseslowCL, valid configuration bits8 remainsunchanged. Noheaderedit, arbitrarycaptureorder,flags orsemanticcleanup. FourcompleteTU cells,rawbaseline,data/metadata/relocs/allbodiesaudit mandatory.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- unsigned-bits: {'SGD_symbol_gen': ['SIZE', 16]}.
- postdecrement: {'SGD_symbol_gen': ['SIZE', 21]}.
- unsigned-bits-postdecrement: {'SGD_symbol_gen': ['SIZE', 14]}.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-sgd-symbol`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
