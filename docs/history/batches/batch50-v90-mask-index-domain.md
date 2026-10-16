# Mapping getter loop index signedness

Base902f47fa. Both getter zeroing loops use JA/JBE unsigned bounds in the blob, while current `int i` emits JLE. Test unsigned zeroing index, independently and crossed with the already-established explicit setter clamp. No k/table owner or argument load permutations. Full raw control and full TU bystanders required; all loops still span exactly0..7.
