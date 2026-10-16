# Imported modem bit-count contract

Baseline9fabd432. Vendored ref/slmodemd/modem_dp.h:82–83 and modem.c:594/612
declare and define modem_get_bits/put_bits with an int count. V22/V32/VPCM
already use int; B103 and V23 redeclare these imports with unsigned short.
Original v23_process passes full local int n_rx/n_tx to the imports; retained
source adds word casts and MOVZWL before both calls. This is independent of
the already recovered V23ModemMain return declaration.

Cross imported count int versus retained ushort with caller explicit word
casts retained versus omitted, across both complete B103 and V23 TUs:8cells.
Prediction: declaration-only and cast-only changes are inert in V23, whereas
their combination removes both original-absent narrowings. B103's short
in/out counters have independent original unsigned normalization; keep its
call casts unless original full-body argument tracing justifies removing them.
Do not change the host implementation, core count fields, callbacks or loops.

Audit raw baselines, all bodies, operations tables, imports and data. The
upstream header settles the count type, not pointer constness or recovered
parameter names. No claim that oversized blocks are reachable, and no fuzzing
or synthetic huge-block fixture. Full period gate remains mandatory.
