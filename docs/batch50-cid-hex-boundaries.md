# CID hexadecimal renderer read and byte-predicate boundaries

902f47fa, retained full period profile. Eight-cell cross (including unchanged)
over low-nibble read before/after the high-digit store, signed versus unsigned
byte temporary, and explicit branch assignments versus ternaries. Same complete
Data.c TU covers exported data_raw, its inlined unformatted wrapper and formatted
renderer's MESG path. No flags, constants, type layouts or pointer annotations.

Blob reads buf[i] twice: the second load follows the high-digit output store.
Current source computes both nibbles before that store. With overlapping input
and output this is observably different; no no-alias contract is declared.
Blob nibble comparisons use JG, current unsigned-byte comparisons use JBE.
Nibbles are0..15, so signed-char temporary agrees for every extracted nibble;
byte predicate spelling is a distinct source-width control, not undefined data.
Blob low-valued decimal arm falls through while letter arm is cold. Explicit
branch assignments test that CFG form independently of read/type boundaries.

Prediction: all three axes recover source operations, with a combined strict
candidate possible. Falsifier: no strict hit or collateral exact loss closes
this declared family; a same-size near miss is not adopted. Audit all three
bodies, metadata, nontext/relocations and helper inlining before source retention.

## New discriminator after the declared eight-cell domain closes

All eight controls complete; no strict hits. Late read alone recovers both
117-byte bodies and every register/operand except six branch/arm bytes.
Signedness and explicit-versus-ternary forms each compile identically within
that late-read class, so those axes are closed. The remaining blob arms put
the <=9 decimal case first, while all tested source controls put >9 letters
first. This observed mirrored CFG is a new discriminator, also corroborated by
the fax reporter's valid-arm-first recovery. Declare a separate four-cell
late-read × decimal-first ternary cross, preserving all integer predicates.
No new source type or carrier is introduced. Require both full bodies exact
and independent controls; no nearest-score adoption.

Decimal-first + late-read recovers both complete bodies except two JA-versus-JG
branch opcodes. This is a new predicate-dependent signedness discriminator:
under >9-first the signed/unsigned controls were raw equivalent, while under
<=9-first the integer branch condition can distinguish them. Declare the
remaining signedness × decimal-first cross on the late-read seed (five full-TU
cells including the original raw baseline). This finishes the interaction;
no additional source axis is opened by byte scores alone.
