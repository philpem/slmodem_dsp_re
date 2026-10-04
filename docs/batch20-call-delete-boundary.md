# Original call deletion diagnostic

Baseline93d7eee1. Reference call_delete158B versus110B source. The missing
entry block compares unsigned dsplibs_debug_level>1 and prints the original
.rodata.str1.1+0x369 string `call: delete...\n` before CALLPROG_Delete.
The source contains no corresponding hook, although it already includes
debug.h. This is original observable diagnostic behavior, not instrumentation.

Before compilation declare retained and restored entry-diagnostic complete
call.c TUs only. Expect call_delete to recover its guard/call/tail duplication;
other functions must remain canonical identical. Preserve data/import/string
changes explicitly. Do not vary optimizer flags or incidental declarations.
