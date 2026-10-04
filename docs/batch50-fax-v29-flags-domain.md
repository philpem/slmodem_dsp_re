# V29 receiver flag access width

902f47fa fixed profile, no ABI/header/profile changes. Every original V29
receiver status bit update of CARRIER/LOW_SNR/ERROR/DATA is in byte +0x19;
IDLE is byte +0x1a. Existing source often updates union full int, producing
ORL/ANDL versus original ORB/ANDB. The union already models all four bytes.
Replace only bit updates by corresponding byte lvalue, with same named mask
shifted to that byte. Boolean predicates initially retained. Preserve other
bytes exactly, including high-bit invalid statuses and overlapping state.
One control per original witnessed body: RxHdxData/Error/Idle/Prtcol/EpochDet/
Start and RxNextState; constructor additionally witnesses both ERROR and
CREATE_BITS at +0x19. Full-TU baselines and all-body canonical audits. Final
combined winner cell only after individual results; no local permutations.
