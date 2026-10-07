# Fax binary timing shortlist

Base fa941457; retained production objects and census from sibling
byteexact-x87-scheduling/build/tc_out and byteident-reload-batch.json.
Coarse binary event screen covers all shared fax functions, retains every
nonexact original function <=350 bytes, and reports its denominator. Register
bases are not equated across instructions or objects. ESP references excluded;
EBP can be a data cursor under omitted frame pointers, so remains included.
Memory/call/branch events only rank manual operand/path inspection.

Two independently new witnesses, two cells per complete TU (four compilations):

- SDM.c / SDM_descrambler: original 9f26e stores unmasked word; 9f273 advances
  cursor; 9f276 reads the old word via preserved old cursor; 9f279 masks and
  9f27b stores it. Current source advances only after its masking assignment;
  current binary folds two stores into one. Test ordinary `*data++ &= mask`
  instead of `*data &= mask; data++;`, preserving the existing first store,
  feedback update and captured mask. Pointer advancement has no external
  publication or volatile access, so abstract-machine outputs remain equal.
  Prediction: the original pointer lifetime may preserve its second load/store
  boundary. Falsifier: initial CSE folds this form too, or whole body not exact.
  Prior output width/feedback order cells do not test cursor lifetime.
- V17rxdec.c / FSE_decision_eqtrn: original has two complete final angle,
  magnitude and return publications (continuing TRN and completing TRN),
  whereas reconstructed source has one common final publication. Place the
  existing publication at each branch end with its return; all state updates,
  helper uses and arithmetic unchanged. Prediction: branch-local publication
  may reproduce original distinct continuations. Falsifier: optimizer merges
  them or whole body remains nonexact. Prior tick/absolute/owner domains are
  distinct from this tail boundary. No arbitrary state-store reordering.

No header, API, type, profile, slot/register/order permutations. Previous
TxHdxEQCondV27 domain closed; DemodDataV21 direct AGC consumer already tested
under F11614; SMCv17 traversal/wrap already tested and not reopened.
Require raw baseline reproduction, complete flags/assembler/bugdefine-last,
all emitted body/binding/data/nontext relocation audits. No adoption by size.
No fuzz/mutation/runtime harness execution; parent owns final gates/commit.
