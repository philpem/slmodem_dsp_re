# Last original unsigned MRF handoffs

Baseline93d7eee1; src/service/Rxcid.c and src/pump/v23/v23rx.c only. Ownership
checked against root exclusions; neither is external PR245 scope. No shared
header edit or unrelated count/loop/AGC/FSD/IIR rewrite. Adopted MRF consistent
int formal is fixed for every cell via candidate-only header overlay; its
explicit signed-short-at-use callee has prior complete raw-object proof.

CID: two cells retain signedshortncount versus pass originalunsignedshortcount
only atMRF. OriginalMOVZWL argumentcount→ESI is preserved into stack+0xc atMRF.
V23: two source cells retain `(short)count` versus `(unsignedshort)count` atMRF,
as blob saves MOVZWL DI→stack+0x18 thenreloads that forstack+0xc atcall. The full
inputcount remains int elsewhere; do not blindly pass full32bits. Each matches
callee signed16 interpretation while recovering original outgoing slot bits.

Cross V23's two cells with previously adopted ctor/ratio boundaries toprove
581Bconstructor gain retained (fourV23cells,total6). Rawbaseline reproduced
under fixedconsistentint formal (previousallconsumerproofpredictsmerge).
All fullTUbody/nontext/data/metadata/canonicalreloc controls required. Closed
family result honest; do not demand branch/flag/registerfit afternegative.

Both final handoff domains negative: cid_modem SIZE54unchanged (bodymoves),
v23FP_rx_progress SIZE126→107, noexact. SixfullTUcells auditmetadata/data/
nontext/canonicalrelocs, noadditionalgains/losses; V23ctor581Bexactpreserved.
No production edits. Initial import lacked retained constructor generator's
debug dependency and failed before compiler; preserved firstlog as invalid
preflight, corrected dependency run valid. Domainclosed.
