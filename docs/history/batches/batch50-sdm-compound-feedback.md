# SDM compound feed-forward boundary

Declare raw902 baseline and one source compound-XOR control only. Blob
descrambler shifts both feedback taps before XORing the original input, writes
the resulting word, updates its local register from the saved original word,
then reloads and masks the stored word. Current left-associated expression
XORs the input between shifts and GCC forwards the mask into the first store.
Test ordinary `*data ^= (reg >> shift1) ^ (reg >> shift2)` only, keeping the
original saved unsigned-short input and the second mask statement. This is an
expression/owner boundary, not emission-order permutations (F7827 closed).
No volatile, flags, fabricated spills, harness or mutation. Complete raw TU,
all bystanders, data/nontext/relocations and actual period RTL required.

Measured result: 2cells compound reduces SIZE21→SIZE6, no gain. All thirteen SDM TUs pass complete data/nontext/relocation/metadata/bystander audit; only descrambler changes, zero exact losses. No adoption.
