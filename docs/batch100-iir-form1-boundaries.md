# IIR form-I counter and sample width

Reference FPM_iir_filt_II outer sample counter narrows at each increment and
compares AX against count word; retained int index does neither. Reference
inner section latch decrements word and tests old-count via INC/DX flags;
retained explicit sentinel int j needs CMP32. Reference intermediate sample
is a signed word carried between sections, whereas retained int acc holds an
explicit cast. Eight-cell domain: short outer index, short postdecrement
section counter, short sample carrier. All coefficient/state read and write
sequence preserved, no source reorder/option/header changes. Inner zero
sections passes current sample unchanged; negative section count remains
modulo65536 under both written sentinel and postdecrement. Full TU and exact
bystanders compared. Separate from form-II undefined output boundary.

Eight valid complete TUs; no exact gain/loss. Outer-short only and outer-short
+short sample217B vs reference232B. Postdecrement216B; baseline213B; sample
carrier alone raw-inert. Full data/metadata/nontext/relocation controls agree;
only formI changes. Close counter/sample carrier family without synonyms.
