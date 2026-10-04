# Voice online verdict lifetime across diagnostic and kernel calls

Original voice_online508B assigns the returned1 carrier before the kernel
beep-in-progress call (0xac11b..0xac12d). Source541B writes ret after that
call. Original diagnostic side path returns before these assignments, so
assignment-before-diagnostic is a negative distinguishing timing witness.

Four full-TU cells: baseline; ret1 before diagnostic; after diagnostic before
beep_done store; immediately before kernel call. Change no loops, prototypes,
flags, field types or reachable values. Review all bystanders and data; only
strict full508B recovery is adoptable. If all miss, close timing alone and
reframe on an independently observed loop boundary. No fuzz/mutation.
