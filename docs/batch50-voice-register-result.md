# Voice S-register common result lifetime

Raw902f47fa src/service/voice.c fullTU. Blob vce_get_sreg clears ECX immediately after modem_get_param and all switch arms assign that one default-zero result; reconstruction uses early returns. The sensitivity case shifts into the same result and conditionally sets1 with a branch when source sensitivity is nonzero, instead of SETNE boolean return.

Declare three cells: retained; common unsigned default-zero result with original sensitivity boolean selection; common result with explicit result-zero/source-nonzero branch and upper-bound clamp. Allcase values, types, getter call/read counts and unsupported default remain. No source ordering beyond actual common result evidence. Raw baseline and fullTUmetadata/nontext/nameddata/canonicalreloc/bystander audits, zero exact losses.

First three cells miss:162B baseline and212B common-result forms (SIZE24). Initial RTL shows source result initializer before external call creates a live-through-call zero in a callee-save register, while blob clears its result only after call. This independent lifetime discriminator justifies three new controls: raw baseline and both common sensitivity forms with result initialization after modem_get_param. Keep declared zero initialization, not uninitialized default; no scope synonyms.

Guarded common result initialized after call EXACT188B,4/8→5/8 without losses; boolean-after-call misses. Fullrootaudit51TUcontrols preserves all seven neighbours and all data/metadata/relocations. Adopt only guarded post-call winner; period gate pending.
