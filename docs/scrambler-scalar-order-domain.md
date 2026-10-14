# Scalar scrambler transfer control

The successful descrambler scalar cross exposes an adjacent independent
original witness: Scrambler<h,i>::process(h) writes pTap2 at+0x18 before
pTap1 at+0x14. Its retained source does the reverse and remains BYTES7.
Scrambler<h,h>::process(h) is already exact and its native writes are in the
opposite order. Thus native order alone cannot prescribe shared template text.

Test only retained versus reversed scalar tap-decrement statements in the
complete V90Phase3Modulator, V92Phase3Modulator and V92Phase4Modulator TUs:
6cells. Preserve the exact HH instantiation as an independent negative/control
for transfer, and inspect every nonexact neighbour and data object. Baseline
9fabd432 with explicit historical header snapshot; these three production
objects are unchanged by the descrambler/count recovery. No XOR/declaration/
store-form permutations or per-instantiation specialization. If the shared
candidate disrupts the original HH body or introduces other unsupported
boundaries, decline and close this transfer even if HI becomes exact.
