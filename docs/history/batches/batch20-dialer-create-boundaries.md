# DialerCreate modem reload and grade comparison

Four complete93d7eee1 TUs cross two object-observed source boundaries:
* SetPulseMakeTime/BreakTime first arg original parameter vsd->modem. Blob
reloads member+d4 before both calls after GetDialerConfig publishes cfg;
ours keeps original incoming modem in EBX across callbacks. Calls can expose
object writes, so these named member reads are genuine source boundaries.
* Grade rejection signed int <=INVALID vsunsigned word comparison. Blob
cmp1/jbe; ours dec/jle. AnalyseDialString produces onlygrades0..3, so unsigned
and signed tests agree over all reachable parser results; no synthetic value
used to infer original type or change field ABI.

All stores and source definition order preserved. No unconditional debug
string computation or arbitrary initialization permutation. If completebody
misses, domain closes; size-only convergence not adopted. Require rawbaseline,
full retainedprofile/bugdefine/executedassembler/RTL/fullTUmetadata/data/relocs.
Finalowner gates adopted batch once.

## Result: closed without adoption

4/4 valid complete TUs retain0/6exact, all Create261vsblob269/SIZE8. Only
Createchanges, each recovery remains incomplete. No source adoption or
arbitrary field-store order search. StructABI/parsergrade outputs untouched.
