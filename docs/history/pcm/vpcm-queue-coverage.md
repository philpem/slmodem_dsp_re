# V.PCM queue coverage

`t_vpcmrun.c` now drives the V.PCM input remainder, queue-backed reads and
output compaction during an independently constructed, connecting V.34 call.
The old `vpcm.c` statement that no test exercises `pending > 0` is retired.

## Fixture boundary

The existing fixture negotiates V.8 between two endpoints and saves the
resulting runtime parameters. Each of four V.34 runs then constructs fresh
endpoints from that snapshot: blob/blob, reconstruction/blob,
blob/reconstruction and reconstruction/reconstruction. Each endpoint has its
own sample arrays, runtime parameters, delayed wire and routed bit shim.
Construction uses the blob as the existing fixture's oracle; processing on a
reconstructed endpoint uses this tree's complete V.PCM/V.34 run path.

The new pass partitions each 48-sample wire block into successive process
calls of 1, 2, 3 and 42 samples. The constructor's four queued zero output
samples provide the initial latency; no queue, status or internal handshake
state is planted. The input queue therefore moves through occupancies
1, 3, 2 and 0. The first two calls process no samples; the last two process
4 and 44 samples. Every count is within the constructor's maximum fragment
of 48, and input occupancy plus output occupancy remains four.

This establishes lifecycle reachability at the datapump's `.process`
component boundary. It does not establish that the stock host driver splits
its normal 48-sample calls, nor does it establish a digital-side V.90/V.92
connection. The unchanged first pass retains the normal host cadence.

## Observers

Every chunk checks input/output occupancy against the quantisation arithmetic.
Every reconstructed or mixed run is compared directly with the same chunk of
the blob/blob run: returned status, returned samples, live input remainder,
live output tail, progress code, mode, next bit request and mute count. These
pointer-free observation objects are zero-initialised before `diff_eq_obj`.

Each endpoint must connect exactly once, avoid link errors and receive more
than 1,000 aligned payload bits with zero bit errors. Receive lengths,
alignment, bit-fetch and bit-delivery counts must match the oracle. The
entire receive-bit capture, including its prefix, is also compared exactly.
The shim's fixed capture capacity is 8,192 bits per endpoint; this assertion
does not claim an unbounded payload transcript.

## Measurements and control

Run the focused period differential with:

```sh
make period T=t_vpcmrun J=8
```

The chunked pass drives four runs, two endpoints, 4,000 blocks and four calls
per block: **128,000 process calls**. Each entry occupancy (0, 1, 2 or 3)
appears **32,000 times**. The pass records **64,000 calls with no processing**,
**176 muted processing calls** and **63,824 active processing calls**. Both
endpoints connect and carry payload in all four runs. The eight endpoint
captures contain **65,536 received bits** in total.

A temporary positive control toggled the low bit of the oldest pending input
sample immediately before queue rebinding. The existing 48-sample fixture
groups remained green; the new chunk group failed **16,000 of 352,087 checks**,
first identifying the pending-sample byte. This control predates the final
six whole-transcript comparisons. The temporary corruption was removed;
no production behaviour or tolerance was changed.

The deciding compiler is Gentoo GCC 3.4.2-r2, through the standard period
runner with `DSPLIB_REPRODUCE_BUGS` enabled. The final chunk group passes
**352,093 checks**. The inherited fixture groups remain unchanged: setup 8,
V.8 2, blob-call oracle 46, mixed calls 144 and stall probes 110 checks
(**310 inherited checks**).

The complete `make phase J=8` passes **388 period binaries, zero failures**,
with all structural checks passing. No binary was added, so the suite's binary
denominator remains 388.
