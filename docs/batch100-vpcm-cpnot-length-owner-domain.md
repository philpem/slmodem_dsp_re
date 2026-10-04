# VPcm CPnot copy length after diagnostic

Pinned856c1ecb. In getV90CpBits the source snapshots short live=nofBits before the CPnot transition diagnostic. Original0xd3dc debug gate branches to call0xd470 then rejoins0xd3e9, where nofBits is first loaded with MOVZWL. It is therefore a member read AFTER the external call, unlike the current snapshot carried through it. This is a concrete callback/member age witness, not a register schedule or declaration permutation. Earlier findings type connectionEvaluator/copy fields but do not test this copy-length age.

Predeclare two completeTU cells: baseline and declare live uninitialized at its existing site, assign live=nofBits immediately after the diagnostic before copy loop. Keep copied elements, signed short loop bound, bitmask/shift semantics, counters/allfieldstores/printarguments, connectionEvaluator owner and return result fixed. No other source/header/flags changes. Raw completeTU baseline plus metadata/nameddata/nontext canonical relocs/allbodyaudit mandatory. Strict targetexactness; nonexact member-age change held for separate lifecycle evidence, no adoption solely by score.

Measured: both valid cells raw reproduce baseline; late length read changes SIZE2→6, zero gains/losses. No production adoption. Source memory age is a behavioral lead held separately; no byte-fit continuation without another independent original witness.

Audit correction: this negative changes anonymous .rodata.str1.4 ordering; unchanged-data audit correctly refuses. Dedicated CPnot ledger checks unchanged metadata/named objects and exact string multiplicities and records complete canonical nontext. No data content/call-target change; source remains unadopted.
