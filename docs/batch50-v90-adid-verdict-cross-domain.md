# ADID verdict comparison orientation cross

Common entry-owned isThereAnyAltRbsPhase now57B EXACT; isAltRbs146B differs only CMP operand orientation/JGE versus original JLE. Cross common verdict with whole-predicate complement of `d<=threshold`, and explicit reject-zero/accept-one arms. Preserve signed full-int values and existing phase guard; no local/order changes. These are comparison-CFG spellings in the initial RTL, not register-fitting variants. Cross both with exact phase predicate. Six raw-controlled completeTU cells, -dr.
