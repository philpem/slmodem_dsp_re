# V8 receiver/transmitter initialization carriers

Declared nine-TU cells: raw902 plus unsigned charFlip input seed crossed with
short rx zero-loop counter, short tx zero-loop counter, and named receive
component pointer captured before loops. Blob v8_rxinit uses cwtl/inc and
cmpw for both zero loops, and captures &v->rx (+0x1c) before scratch clearing;
source int counter and full-object member bases differ. v8_txinit likewise
uses word loop comparisons. Fixed ranges are within signed short; no added
state/store reorder. Captured rx pointer only replaces v->rx member uses,
not scratch or staging base. No headers/signatures changed. Audit full TU,
all initial RTL counters/owner references and bystanders/nontext/relocs/data.
No fuzz/mutation/harness, root batch finalperiod; close nine-cell family onmiss.

Result: receiver229→228/231B vs226, transmitter163→165 vs181. All nine
valid complete TUs retain charFlip33B gain but add none. Metadata/data/nontext/
relocations unchanged; only requested init bodies change. No init source
adoption; this bounded width/owner family is closed.
