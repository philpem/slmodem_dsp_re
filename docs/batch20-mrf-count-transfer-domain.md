# MRF count-width transfer after the V21 transmit recovery

Baseline93d7eee1. Six original caller boundaries: DemodDataV32, DemodDataB103,
DemodDataV17/V21/V27/V29. Only remove each function's MRF `(short)count` cast;
retain all earlier AGC/FSD/IIR/loops/counters/types. Four cells per TU cross
source cast retained/omitted with public MRF short/int formal. Callee's int
formal/explicit-short-at-use spelling already has full raw baseline proof.

Blob count evidence: V32 MOVZWL argument→EBX→stack+0xc; B103 initially MOVZWL
count→EBX and keeps it for MRF while separate signed copies serve earlier AGC
and loops; V17/V29 argumentMOVZWL→EDI→MRFslot while separate signed copies
serve other callees; V21/V27 argumentMOVZWL→EBX→MRFslot with separate signed
copies for AGC. Current MRF-only cast forces signed copy into the slot instead.
High bits therefore differ for countbit15, even though original callee narrows
at use. This is a real observed source handoff, not an arbitrary type fit.
Whole-TU exact/bystander/data/import/binding controls required before adoption;
close each bounded negative. No reopen of completed fax reconstruction.

Results: zero exact gains across24cells. Int-count differences: V32 target
stillnonexact and SetAdaptEqV32 exact→BYTES7 (threeadditional scratchbystanders);
B103 SIZE70→86; V17 SIZE32→10; V21 SIZE10→9; V27 SIZE26→BYTES198;
V29 SIZE18→3. All source families nowclosed, no production adoption.
Complete-TU audit preserves the V32loss explicitly and passes metadata,
nameddata/allocatednontext/canonicalreloccontrols. No unrelatedsource rewrites.

Other caller evidence: cid_modem's original count arrives zero-extended inESI
and stays there forMRF; ours passes signed-short ncount from an earlier narrowing.
v23FP_rx_progress takes fullint count but explicitly zero-extends DI to a
savedstackslot forMRF; ours uses signed BP→EDI forMRF. Thus v23's matching
boundary would need unsigned16-at-use, not blindly removing its countcast.
These are new candidate-only caller preimages, not compiled or adopted here.
