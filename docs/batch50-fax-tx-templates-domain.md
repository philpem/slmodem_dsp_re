# V17 transmitter original whole-template boundaries

Base902f47fa, retained complete period profile/config, original object witness.
V17TX_create original copies FIFO_CFG dword+word before size/fill overwrites,
SDM_CFG dword before all three field patches, SMCv17_CFG four-byte array copy.
Current source spells all three field-by-field and removes original wide
reads. Original source comments already describe those full template copies.
Three independent whole-copy boundaries, full eight-cell factorial domain:
FIFO struct initializer, SDM struct initializer, SMC array memcpy. Every
config patch/local/call/statement order fixed, no new headers/types/flags.
Semantic values passed to callees identical; original template read width
and load lifetime are the proposed mechanism. FullTU/raw baseline/bystander/
exports/data/relocation audit. No permutation expansion after a miss; no
register/spill fitting. Root owns final period and integrated byte census.
