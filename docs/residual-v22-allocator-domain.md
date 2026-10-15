# Unchanged V.22 detector allocation/reload observation

Three read-only controls: saved complete-TU ACK baseline, saved combined
square/late-verdict control, and independent baseline repeat. No source, RTL,
profile or inferior state mutation. Target is unchanged Detect_1s, whose
instruction patterns agree through24.lreg but differ at25.greg.

Observe all pseudo assignments at global_alloc entry, reload entry and reload
return using the installed hash-pinned Gentoo cc1. Also report register order,
ever-live/fixed flags and available reload/stack state, refusing missing core
mapping data. Compare actual snapshots before attributing the difference to
global allocation or reload. Require saved/plain/traced objects identical raw
for all three controls and exact baseline event repeat. This is compiler
diagnosis, not source recovery or original compiler-state inference. No
fuzzing/mutation execution. A new source experiment requires a separate witness.

Follow-through within the same three inputs: observe individual reload choices,
check a bijection of printed memory alias IDs (retaining addresses, registers,
sizes, alignments and every alias relationship), then observe the existing
peephole scratch events if the earlier interpretation is refuted. No compiler
state interventions or additional source spellings.
