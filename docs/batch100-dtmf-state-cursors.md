# DTMF per-tone state consumption

Reference initializes each low/high group state pointer after its pre-notch
callback, hands current pointer to each tone callback, advances by4bytes
(two shorts), retains independent short j for coefficient/energy index.
Retained rx->tone_state[j] recomputes address from indexed member each time.
Four-cell domain low-state cursor ×high-state cursor, sourceinit before each
corresponding tone loop and after prefilter call. Preserve source/state read
ordering, j widths/energy products/return/table data; samples stayindexed as
object shows. No call-count/profile/store reorder changes; complete TU audit.

Four full controlled TUs negative: baseline1074B vs947B; high-only1090B;
low-only and combined1074B. All three cursor controls change canonical bodies; equal size of low-only
and combined does not imply raw equality. No complete preimage. All16
named coefficient objects/data targets/nontext/metadata unchanged, no exact
gains/losses. No source adoption; close cursor group family without synonyms.
