# Phase3 dispatcher result narrowing

Base902f47fa. Original generateSymbol CALLs either child then CW(T)L sign-extends AX; ours tail-jumps with no narrowing. Child bodies and declarations correctly return int, but their values are all sign-extended shorts. Test wrapper casts on each return, a common short result, and cast of conditional dispatch. Cross each with confirmed cycle+Jd source. Explicit cast has no reachable value effect and encodes original instruction, no shared API change. Complete TU raw controls and consumers mandatory.
