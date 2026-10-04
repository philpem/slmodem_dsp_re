# V34 shell table initialization

Reference initG248 caches count as word on stack, promotes unsigned for first
loops, and reloads word for final xyz index. Reference loads xyz[count] before
writing t1/t2 and carries cursor through final loop. The reverse convolution
pointer uses MOVZWL BP then scales2, narrowing unsigned j to16 bits; retained
pointer uses fullunsigned j. Eight-cell domain: unsigned-short count carrier;
xyz cursor initialized before first loop; narrowed unsigned-short convolution
pointer index. Other counters/bounds/products/store orders unchanged, no header
or compiler changes. Narrowed index differs when j exceeds65535, observable
for malformed large counts, not claimed modem reachable. Constant xyz capture
is safe for valid inputs; writes into its storage through aliased contexts
are already undefined. No source adoption unless strict complete-object gain.

First cube no exact; early-cursor controls leave SIZE2 versus reference269B.
Fresh CFG discriminator: reference
first table has positive n guard before loop body with first iteration body
entry; retained for jumps to bottom predicate. Reference second convolution
loop has no entry guard (unsigned top>=0), runs first body before bottom bound.
Four controls cross explicitly guarded do first-table loop with do convolution
loop on early-cursor+word-count+narrow-index control. These preserve unsigned
loop wrap/nontermination behavior, no padding or register forcing.

Initial CFG attempt rejected compile: generator sliced baseline tail with
overlay function offsets. Preserved invalid-overlay-slice, no emission
interpreted; corrected to consistent overlay slices and reran whole domain.

CFG cross leaves all four bodies at SIZE2. Second-loop do is canonical-raw
inert; guarded first-loop do changes body at the same size. New independent arithmetic owner witness:
reference keeps count-1 in EDI, derives top by2*base for first loop, and4*base
by SHL2 for mirror destination in second loop. Retained code derives mirror
from2*top, reusing top rather than original base. Two-cell source-graph
control: introduce unsigned base=n-1, top=2*base, mirror index4*base-j on
word/early/index source. Pure unsigned arithmetic equivalence holds all inputs;
no shift spelling or pseudo/register permutations. Stop if this graph fails.

Fifteen valid complete TUs across8+5+2 stages; no exact gains/losses.
Early cursor yields267B vs269B (SIZE2); count carrier and narrowed index
controls produce no further size gain. Four CFG controls form two canonical bodies separated by first-loop guard;
second-loop spelling is inert. Shared count-minus-one base stays267B.
Only initG248 changes; all twelve TU symbols and16 named data objects
(number verified by complete audit output) have unchanged metadata/data/
allocated nontext/relocation targets. Closed all declared families; do not
relabel near size as a preimage or permute source declarations for registers.
