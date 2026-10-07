# EIA6 scheduling replay domain

Base75e7ef4b (PR279 merged). Two full-TU cells only: untouched production,
then the already measured SF default-math control. No new source family.
Use retained configuration, headers and reproduction define; add scheduler
verbosity solely for diagnostics. Both objects must repeat their previous
raw bytes before interpreting ready lists or dependencies.

Question: does the scale load move across the integer copies at scheduling,
and does the magnitude/fraction conversion order also first diverge there?
Trace each UID through RTL, allocation, block reorder, sched2 and stack.
Compare issue order against dependency edges and final assembly. A movement
already present before scheduling refutes a scheduler origin for that movement.
No source adoption by equal length, instruction forms or register renaming.
Reopen source only on an independently witnessed dependency/lifetime difference.

Run tools/eia6_scheduler_reproduce.py --domain docs/eia6-scheduler-domain.md
--baseline-dir build/production-before. Diagnostics must report their
denominators and demonstrate the movement on this known control.
