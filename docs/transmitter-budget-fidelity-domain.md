# Transmitter budget fidelity review

Baseline1b0a3097, retained Gentoo profile and bug define. This is a partial
behavioural adoption review of the closed batch50-fax-budget domain, not a new
carrier/store-order search. Six V17 and four V29 countdown handlers compare
MOVSWL remaining against MOVZWL budget in the original; the retained source
sign-extends both. Exactly four full-TU cells: baseline and unsigned-at-use
budget for each of V17t_prc.c/V29t_prc.c. Preserve countdown carrier, member
subtraction, all other calls, signatures, stores, data and flags. Audit every
emitted body, binding, data and nontext relocation; raw baselines mandatory.

Fixed component tests construct the transmitter, invoke the genuine START
transition, then call its installed Silence/Quiet handler with budget words
0x7fff,0x8000,0xffff. No internal history is planted. The installed countdown
is48, so original selects48 for each word. Separate oversized output buffers
cover the retained wrong count as well (up to65535 symbols, under4 output
samples per symbol at the default10/3 resampler). Compare return, remainder,
countdown, complete output including unchanged tail, and state. This proves the
component boundary, not that the public fixed-small-budget modem wrapper sends
high-bit words. Preserve a pre-fix failing run before adopting source.

No fuzzing, mutation execution or tolerance change. Final permitted period and
structural gates decide retention; stop this finite review if unexplained
collateral or a valid differential failure remains.

## Two additional partial-boundary controls

After native review, add baseline plus the already-measured joint byte snapshots
for V29RX_control, and baseline plus the corrected fused-history/delayed-tap/
word-counter form for fskdetect: four additional complete-TU cells. This reviews
partial recovery only; no expansion of those historical products. V29 original
retains each request byte across child stores. FSK original combines third-phase
products with history transfer, loading the coefficient before that transfer,
uses signed-word counters and captures the delayed tap before shifting the line.
Existing fixed period fixtures must pass; full metadata/data/call/bystander
controls required. No arbitrary writable-coefficient alias history or public
reachability claim. CrossDataLinks cursor/count candidates remain withheld:
parameters are stable scalar owners and the remaining indexed-loop/constant
initialization difference supplies no independent new source discriminator.

## V27 transfer after a new native operand witness

A source-wide signed-budget scan finds the same comparison in four V27
handlers (SCR1,EQCond,Alt,Quiet). Original MOVSWL/MOVZWL pairs before CMP distinguish signed countdown from
unsigned budget in all four bodies (for example a3af3/a3af6 in SCR1).
Add exactly baseline and unsigned-at-use comparison for V27t_prc.c, keeping
the selected short carrier and use-site narrowing unchanged: ten cells total
across five TUs; the emitted-symbol ledger supplies the verdict denominator.
For the V27 fixture select4800 bps, seed32 and resampler5/1; use524288-short
output buffers, bounding the incorrect65535-symbol path as well.
The earlier EQCond owner/rate/cursor domain is not reopened. Extend the fixed
constructor/START/Quiet fixture to V27, deriving its positive seed from
V27TX_FRMSIZE[rate]. Buffer capacity follows the default resampler ratio;
confirm that ratio before executing a high-bit negative control. No fixture
or source edits while the earlier whole-period run is reading them.

## V27 selected-count carrier and member subtraction

With the unsigned comparison fixed, Quiet's native output still sign-extends
its chosen budget and re-extends it at the TxNoCarrier call; original keeps a
zero-extended chosen word. Original also rereads the member for subtraction
(a3d98) instead of using the earlier signed comparison copy. These independent
use/read witnesses justify exactly four full-TU Quiet-only controls on the
corrected comparison context: retained short chosen count versus unsigned short,
crossed with cached subtraction versus compound member decrement. Preserve
countdown local, all other handlers, helpers, ordering and flags. The positive
countdown gate bounds any selected count to0..32767, so the carrier change does
not widen an executed value. Complete current raw baseline required. No extra
carrier/declaration/register or nearby syntax controls after this cross.
