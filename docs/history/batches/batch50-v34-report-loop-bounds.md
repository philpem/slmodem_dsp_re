# V34 diagnostic loop entry and exclusive bound

New independent instruction witnesses on found-edge preimage: candidate223B
has only four different bytes; first row11cmp k,n differs from blobtest n,n
at scan entry, and row-loop compare138 differs from blob143. Declare raw902
baseline plus four crossed cells on found-edge seed: explicit n>0 guarding
scan, and row upper bound i<144 instead of i<=138. The latter is exactly
equivalent for i=0;+6. The former follows blob signed zero/negative guard and
leaves scan body/empty report unchanged. No data/layout/register permutations.
Audit complete TU and initial RTL; close if miss. Parent finalperiod batch gate,
no runtime harness/mutation/fuzz here.
