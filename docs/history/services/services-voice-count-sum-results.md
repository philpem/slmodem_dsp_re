# Voice final count publication: complete operand recovery

At e0052eec, **voice_modem becomes EXACT338** by spelling the final count
sum as `det_len + saved`. Retained `saved + det_len` is BYTES2. Both locals
are unchanged unsigned shorts, all callback types/count pointers and lifetimes
remain unchanged. The promoted sum is at most131070, so its association and
operand order cannot overflow int; final unsigned-short narrowing is identical
for all65536×65536 value pairs. This changes arithmetic operand ownership,
not declarations or chosen registers.

The independent original witness follows the handler callback at0xac870:
MOVZWL0x2a(esp)→ECX reads detector count, MOVL0x28(esp)→EDX reads handler
count, ADD EDX,ECX publishes their sum. Retained loads these same homes in
same order but into opposite destinations. This heterogeneous read/result
graph isolates the two-cell source domain. F11774's ten earlier formal-width
controls changed detector_progress's API/caller narrowing, a separate axis;
no header or ABI change here. The result does not recover a unique original
source beyond this declared two-spelling domain.

Whole voice TU5→6/9 exact; only voice_modem changes. All metadata/bindings/
imports/exports/named objects/nontext bytes/BSS/canonical data relocations
remain identical. Raw unchanged control reproduces the whole baseline object.
Winner object SHA2566a54b46a5007d8dc1b6d0a347d81590074995a2f0476c127d881029b0936b8d7.
Tools services_voice_count_sum_reproduce.py and services_return_audit.py
preserve actual compiler commands, Gentoo3.4.2-r2/assembler2.15.92.0.2,
complete flags/bug define, source/header/object hashes, every RTL stage and
full bodies. Combined services pass **38 valid TUs/402 function comparisons**,
5 reproduced whole-object baselines,30 changed-body controls; one gain,
zero losses. Root runs deciding period gate on combined production batch.

## Read-only acquisition and transfer screen

Seven paired original/retained bodies saved under build/services-owner-screen:
edprintf, RD_create, CID_process, cid_progress, VOICE_process, voice_modem,
reset_cid. Core pool excluded dp_wrapper and FixedRC; its other methods are
already exact except edprintf, which has no instance callback owner. Existing
RD/Detector/VOICE_create owner ages have earlier dedicated negative controls.
CID's receiver/mode and service modem reads, VOICE_process's converter/modem/
handler reads, and voice_modem's handler acquisition already follow their
external calls. No newly missed owner age is asserted or varied.

A final separately bounded mixed-sum screen inspects33 core/service/voice TUs,
26 nonexact functions of reconstructed size≤350B, excluding dp_wrapper and
FixedRC. It requires an original MOVZWL memory input and a full MOV memory
input feeding the same ADD within6 decoded instructions, with no intervening
branch/call, and fires on voice_modem's known witness. It finds exactly one
candidate, voice_modem; no further source tests. This is syntax triage with a
stated six-instruction window, not whole-object alias analysis or proof that
other sum source families do not exist. Replay tools/services_mixed_sum_screen.py.

Source handoff changes only src/voice/voice.c's sum. No commits, runtime,
fuzzing or mutation execution by this subagent.
