# AGC shift-count ownership

Pin1ffa3fb9, retained Gentoo profile/DSPLIB_REPRODUCE_BUGS. Original gain
loop loads the cached shift's low byte, then SHL CL without an explicit AND31.
Current source explicitly masks31 and prepares it outside the sample loop.
This independent operand/use witness is distinct from closed return/type/arm
families. Count-shape evidence is not permission to force register allocation.

Four full-callee TU cells: baseline unsigned-value/masked shift; unsigned-value
raw count; unsigned-value byte cast count; unsigned-value byte cast plus mask.
No signed-value shift alternative, local type palette, source order or profile
controls. Predict raw/byte count restores some original load/use behavior;
byte+mask controls whether a defined count spelling alone removes the mask.
Require raw baseline, complete object data/metadata/call/bystander audit and
saved initial RTL/combine/reload/layout/assembly trace.

Audit denominator/count/initialization separately: nonzero positive smoothed
short levels give norm1..15 and shift0..14; top-bit unsigned denominator gives
norm0 and shift-1. Freeze retains a prior shift. Known valid initialized unity
pairs admit a bounded range, but public configuration and changed coefficient
pointers prevent inferring a universal range from initial relocations alone.
D6/D85's reachability statements conflict and must be checked against live code.
No runtime C shift with invalid count to manufacture a proof. Hardware CL low5
mask and C shift requirements are distinct. Any source adoption must preserve
original period behavior and state explicit domain/portability limits; otherwise
record an informative declined control, leaving source unchanged.

Close these four cells without nearby shift spellings absent a new witness.
No fuzz/mutation execution, profile/register/frame forcing or tolerance. Update
findings/Playbook; commit then push existing PR284.
