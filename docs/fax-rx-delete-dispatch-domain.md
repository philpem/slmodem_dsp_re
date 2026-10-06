# Receive cleanup slot dispatch

Two full-TU cells at e0052eec. _delete_data_rx_modem original reads slot
MOVZWL before comparing its word register to V17RX; retained single equality
if compares directly in memory. This is compiler selection before allocation,
not a register-color-only difference. Natural one-case switch with default
break can account for an expanded controlling expression. Preserve original
V17save-free order, fresh parent/config reads between opaque frees and all
shared cleanup. No local/declaration/definition/order permutations or flags.
Rawbaseline/all functions/metadata/data/BSS/reloc audit; miss closes family.
No adoption absent complete exactness; no runtime/fuzz/mutation execution.
