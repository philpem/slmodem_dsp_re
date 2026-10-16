# V8 received-message ownership

856c1ecb baseline. Reference initializes result -1 before sequence selection,
tests original short wordidx via AX, promotes it only on positive arm,
initializes result0 there, then retains full-width buffer-limited length.
Reference sequence word extraction uses MOVZWL versus retained MOVSWL.
Bounded four-cell cube: shared terminal-result / positive guarded-body with
short initial source length and independent full-int working length; unsigned
short sequence-word extraction at use. Preserve buffer count, byte shift,
charFlip call, index and outputs. No prototype/header edits. Sign extension
before >>1 cannot affect extracted lowbyte; guard preserves every int buffer
length including negative values, unlike a narrowed working count.

Initial cube baseline165B and guarded bodies179B versus blob179B; no exact.
Independent stage: result initialization before rx_sequence rather than after
wordidx; and unsigned word capture with signed-short positivity/promotion at
use. Blob executes result -1 before selecting pointer; initial wordidx MOVZWL
then AX test then CWTL distinguishes capture from promoted working length.
Four controls crossed on guarded+unsigned-word candidate, plus raw baseline.

Capture stage no exact, best13 differing bytes. Object length comparison uses
promoted original received word (EAX) against caller count (EDX), with separate
working length ESI; source compares working length. Object selector tests side
with JNE to seq0 and keeps side0 seq2 as fallthrough. Final four-cell control
crosses received-word comparison/result assignment against side0-first
rx_sequence arm. These are observed value owners and arm order, not arbitrary
statement permutations. Stop this family on no exact result.

Owner stage no exact: best9 bytes. Last direct-field-use control (two cells,
raw baseline and one source): positive guard reads short wordidx directly;
working int length reads it only inside guarded body. This tests original
field-read ownership without invented unsigned capture. No callbacks/stores
intervene, same value for all inputs. Side0-first and early result retained.
Close after this independently observed field-use boundary; no promotion or
register spelling permutations.

Sixteen valid complete TUs across4+5+5+2 stages. No exact gains/losses.
Best owner+side0-first179B BYTES9 versus reference179B; instruction-count
comparison53 vs52 rejects pure register renaming: reference has distinct
CWTL+MOV to working length, candidate directly MOVSWL AX→ESI. Direct-field
guard leaves10 bytes. No adoption/claim of original spelling; close the
declared capture/guard/order family. All seven functions, named tables and
allocated nontext/relocations/metadata audited, exact Create/Delete preserved.
