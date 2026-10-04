# Voice DLE common result

Original196B voice_dle_command initializes EBX=0 before its debug calls,
returns that same carrier on ETX/default paths and changes it to9 on CAN.
Retained146B body returns literal0/9 with tail-shared pops. Three complete
TUs: raw856c1ecb baseline, common initialized result with switch, equivalent
if-chain assignments. Fixed recovered compiler/profile/bug define. Predict
original result ownership adds its callee-save lifetime and matches the
instruction/CFG boundaries; strict exact body and full metadata/data/bystander
review required. A larger size or apparent return-carrier match is not success.
No declarations/flags/padding/permutation fitting; close if this domain misses.

Result: common switch recovers196B EXACT; common if189B/SIZE7 misses. No exact loss and only voice_dle_command changes. Three-TU audit checks all functions/metadata/nameddata/relocations and allocated nontext. Merge/string-flagged byte pool preserves the same literal multiset/size but exchanges CAN/ETX string position; this is recorded, not raw whole-object equality. No other source candidate adopted. Final period gate deferred to batch end.
