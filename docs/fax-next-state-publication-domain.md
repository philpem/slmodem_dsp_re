# V21 next-state diagnostic publication

Eight cells, V21receive/transmit fullTUs × baseline/diagnostic-owner-reload.
Original RxNextStateV21 reloads root->hdx after three known-state debugcalls
at a1e55/a1e69/a1e7d; TxNextStateV21 same at a269b/a26af/a26c3. Retained
owner captured before switch/debugcalls. Add only reload on actual diagnostic
arm, preserving all stores/order/predicates/default prints/type/switch/calls.
Default has no hdx write and requires no post-print reload. Complete raw
baseline/all emitted bodies/metadata/data/BSS/nontext relocation audit.
No register/order/declaration alternatives after miss; no runtime/fuzz/mutation.

Before expansion, original switch value begins MOVSWL once and default
argument freshly MOVSWL loads member again. RX control instead MOVZWL then
MOVSWL for switch, CWTL for cacheddefaultHI. TX cachedshortstate similarly
survives defaultread. Cross ordinary int controlling-value capture and fresh
defaultmember read with observed diagnostic-owner reload, yielding4cells/TU.
All low16-bit signed values and source store/call order stay identical.
The hypothesis is emitted-width/source-capture provenance, not the nearSIZE3.
