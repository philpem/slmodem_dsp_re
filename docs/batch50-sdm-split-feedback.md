# SDM split compound feedback

Four cells: raw902, compound-feedback plus postincrement-mask prior control,
and two sequential compound-XOR stores with separate or postincrement mask.
Blob XORs input with first tap before second tap; grouped compound feedback
XORs taps first and reloads input twice. Blob's final mask reload is retained
where grouped candidate forwards output. Split ordinary `*data ^= reg>>s1;
*data ^= reg>>s2;` expresses the observed intermediate ownership; no widths,
declaration permutations or volatile. Bound these cells before compiling and
close if no independent witness appears. FullTU raw/bystander/metadata audit.

Measured result: 4cells splitfeedback SIZE21/16, original compoundpost SIZE1; no gain. All thirteen SDM TUs pass complete data/nontext/relocation/metadata/bystander audit; only descrambler changes, zero exact losses. No adoption.
