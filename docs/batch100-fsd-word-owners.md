# FSD cached-word and running-state owner controls

Precompile856c1ecb. Original index update uses CWTL then CMPW against cached
fir_taps and SETL/NEG/AND. Retained compares against promoted int taps and emits
full CMP/branch. Original also initializes output count before configuration
reads; retained initializes it after captures. Eight full-TU controls cross
short configuration carriers (taps/sections/delay/slice/bit pair), short running
state/count carriers (idx/nbits/i), and early count initialization. All values
still derive from the same original short fields; don't narrow formal/header.
Original word-sentinel count loop and output bounds remain intact. No predicate
order or arbitrary declaration permutation. Review complete bodies, bystanders,
named config/data and actual GCC initial/if-conversion modes before adoption.

Eight controls634/657B vs745B, no exact. A distinct source CFG discriminator
is required for original SETL/NEG/AND: current ternary's true value is a newly
computed increment, while noce's destination-identity mask requires in-place
idx update then conditional zero. Declare sixteen-cell previous owner cross ×
ordinary increment/conditional-clear factoring. Do not widen profile or permute
predicates. Original output counter has MOVZWL at output/index increment but
MOVSWL at signed cap check; simple short-all is diagnostic, not unique source.

Sixteen clear controls621–652B vs745B, no exact. Ordinary in-place clearing
shrinks output and recovers a mask family rather than reference full layout.
Final independent four-use family: unsigned-short output owner with explicit
signed cap use (original MOVZWL output increments/indexes, MOVSWL at cap) ×
unsigned-short bit_samples owner (original CMPW5 with unsigned half shift),
for retained source and the combined cfg/state/early/clear source. Eight cells,
not a return ABI change. This fixes a genuine typed-use distinction; source
interpretation must still include wrapping high-count output indices and
signed cap, rather than calling all locals simply short.

Results: eight width/init controls634 or657B; sixteen conditional-clear crosses
621–657B; eight mixed output/period-owner controls634–662B. Reference745B.
No exact; all32 controls preserve exact init/free and the complete default
configuration object, nontext bytes/relocations, bindings and metadata.

`gcc3_batch100_numeric_boundary_trace.py` records four stages for baseline and
combined clear source, alongside independently firing VTB controls (six source
cases/24 stage cases total). Ordinary clear changes generated predicate history;
reference word/mask fragments don't establish complete allocation/source regime.
Close these cached-width, counter-owner and clear families. No source adoption.

Original unsigned output indexing versus retained signed running carrier is an
out-of-domain distinction at large wrapping counters; ordinary Bell103 blocks
are smaller, and no lifecycle reachability or new runtime-equivalence result
is claimed. Public ABI retained; no short/int return prototype guessing.
