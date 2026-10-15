# ADID first-study scheduling dependency domain

Baseline90b7f0d1, unchanged complete TU/period profile. Add only diagnostic
-fsched-verbose=5 to the already raw-equal selected pass dump switches.
Require unchanged complete object before interpreting dependency/ready/issue
tables. No optimization flag control or source alteration in this first stage.

Prediction: middle clearing sum,count,square-sum are independent by typed
memory dependencies; scheduling can place sum ahead of the count xor/store
pair. Inspect priorities, incoming/outgoing edges and issued order at the loop
exit. Original count-first is a witness, not proof of original alias metadata.
Find independent address/lifetime/API evidence before proposing a source
control; don't cast, force registers or permute stores to manufacture a graph.

New original load-placement witness: at0x4147a the reference code is loaded into
edx BEFORE the middle count/xor and float stores. Retained reload912 follows the
stores and shares dx with the count scratch, creating xor/store→load edges.
Test two source cells: retained form and k=ucode before middle clearing, with
high-loop initialization incrementing that captured k. No store reordering.
Prediction: earlier k lifetime recovers original load age and changes scratch
eligibility/packet scheduling. Full equality is not required for a supported
partial result, but audit complete TU and all data/exports before adoption.

For six phases and byte reference codes, all middle accumulator write addresses
are below+a96b (max sum+1dff,count+29ff,square-sum+9f17 inclusive); none writes
the ucode field. Thus capturing across these stores preserves the reference
even at the blob's out-of-row byte values. Preserve wrap at0xff and all existing
low/high loop behavior. Do not silently narrow the input domain.

The high-only capture emits an additional independent middle-index load and
loses the original maintained-index operand. Cross its ownership once: use
captured k for all three middle accesses as well as the high-loop start. This
tests one reference-code owner across the contiguous split, not another order
permutation. Original addresses at the middle stores use one shared ecx index;
all three arrays must consume the same reference byte at this boundary.

One remaining ownership cross separates low/high loop counters: declare the
high counter at its captured-reference boundary while leaving the low counter
alone. Original lower-loop countdown and upper-loop sign-test are different
counter uses. The shared-k controls add a redundant middle-index load; a distinct
counter tests whether that coupling, rather than capture age alone, explains it.
No declaration-order search. Close this four-cell domain if it cannot recover
the original maintained address plus early reload together.
