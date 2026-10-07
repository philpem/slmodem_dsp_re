# Constructor retype loss: first divergence is peephole scratch selection

This finishes the next control proposed by `v90-parameters-constructor-ratchet-history.md`. Actual pre/post2efa source/header commits are replayed with dump-only `-v -save-temps -da -dP` instrumentation. Both complete objects remain **raw byte-identical** to their already audited forensic controls: two raw repeats /18 emitted-body repeat comparisons. No source spelling, flags/profile control, correct field-type reversion or compiler instrumentation was added.

## Stage evidence

Both constructors' instruction-pattern streams agree between commits through **27.flow2**, including24.lreg,25.greg and26.postreload. The first C2 difference is **28.peephole2**. This disproves a first-divergence attribution to local/global register allocation or to final scheduling.

Before peephole2, C2 field stores are immediate assignments: UID28 sets +0x4f8 to0 and UID32 sets +0x4fc to14. The pass creates UID118/119 for zero and UID116/117 for14. Zero first uses EAX in both eras. The14 materialization selects **ECX before retyping, EAX after retyping**.30.rnreg then recolours the chains into their final choices:

| Stage/era | Zero source | Fourteen source |
| --- | --- | --- |
| 28.peephole2, pre | EAX | ECX |
| 28.peephole2, post | EAX | EAX |
| 30.rnreg and final, pre | EDX | ECX |
| 30.rnreg and final, post | ECX | EDX |

Those final colours produce EXACT versus BYTES4. The14 scratch differs first; the final zero colour difference is downstream. The constructor dump labels are duplicated demangled names, so the diagnostic requires exactly two chunks, preserves their order, and checks assembly emits C2 before C1. Final constructor carrier patterns correspond to those emitted body roles. The checker compares instruction patterns only, excluding notes/dependency prose and normalizing nondeterministic large tree-pointer addresses.29 single-stream stages ×two clones ×two eras gives116 checked streams; cgraph and gcse's diagnostic snapshots are explicitly excluded.

## Prior writer explains why equal final bytes do not preserve scratch state

The correct +0x434 recovery has a visible earlier compiler graph change in `setToDefault`:

- Pre-retype: UID1246 assigns MEM:SI the immediate1132068864 (`0x437a0000`). At28.peephole2 it becomes scratch materialization/store (UID2063/2064), using ESI.
- Post-retype: UID1247 assigns MEM:SF from SF pseudo218, allocated into EAX before peephole2. It does not take that SI-immediate scratch split. By final renaming its carrier is also ESI.

Thus the writer can emit the **same final bytes** while traversing different scratch-search opportunities. Its body identity does not imply identical state at the following constructor's peephole pass. This is an instance of the existing Playbook scratch-cursor mechanism, not evidence that +0x434 should remain incorrectly typed.

## Backend attribution and limit

Official GCC3.4.2 `i386.md:17507` matches a long SI immediate-to-memory move with a scratch register, splitting it into register materialization plus store. `recog.c:2932..3025` supplies `peep2_find_free_register`: its `static int search_ofs` distributes searches over allocation order, changes after successful search, resets on failure, and is not reset per function. Earlier writer patterns can therefore alter later constructor scratch choices. [The Playbook](method/refinement.md) already documents the mechanism and independently observed installed-compiler cursor behavior.

These actual Gentoo dumps prove the pass boundary and differing producer/scratch graphs; the hash-pinned stock source supplies the mechanism consistent with them. We did **not** measure this replay's exact dynamic cursor-event sequence, success/failure counts or intermediate values. Do not substitute an assumed fixed-step counter for the live-set-dependent search, or claim the specific preceding field is the only dynamic event that affects C2. No new intrusive compiler observation is needed for the bounded conclusion.

Correct float typing stays in production; the historical floor stays unchanged. The old exact constructor may have compensated for an incorrect earlier field type or a different unrecorded TU/profile regime. This result does not prove the old source more faithful, justify a literal permutation, or recover the original full inline/register-allocation profile.

## Reproduce and artifacts

```sh
python3 tools/v90_parameters_ratchet_history.py --retype-era --dump-era
python3 tools/v90_parameters_constructor_stage_trace.py
```

Requires the existing audited pre/post objects; the replay refuses if dump instrumentation changes either raw object. Updated history tool preserves `raw-before-dumps.o` and writes `retype-era-dumps.json`; the stage tool writes `constructor-stage-trace.json`, including exact UID patterns for constructors and prior writer. All artifacts are under `build/v90-parameters-ratchet-ab`, with complete executed Gentoo commands, selected assembler identity and required reproduction define last. No production source/header edit or new runtime test was needed.

Official source pins under `build/gcc-x87-mechanism-source`:

| File | SHA256 |
| --- | --- |
| [recog.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/recog.c) | `9e1ec6d01c9210a82600263de301021ce7514b84512ab5b7cf21a58a12de0590` |
| [i386.md](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.md) | `2b62f98bc15ccdc268036b57da4afe23f3f9e2c5fcfdda5175758e3dc62719ab` |
