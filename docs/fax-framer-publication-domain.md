# Post-callback framer publication

Two full-TU cells at e0052eec. FAXVMI_process captures vmi->framer at entry,
but original loads it only at95523 after packing, modem, unpacking and both
possible reversal callbacks. That changes behavior if a callback publishes a
new framer. Candidate retains original calls/status operations and moves only
capture to before the first framer-dependent FULL guard after UNDERRUN guard.
Require rawbaseline, complete bodies/binding/data/BSS/relocation audit, no
exact losses. If miss, close local owner capture rather than reorder unrelated
status/local/register declarations or fit scores. No runtime/fuzz/mutation.
