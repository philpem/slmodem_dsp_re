# CrossDataLinks second-phase ownership

Original123B keeps first phase indexed, loads the second input/output owners
only after that phase (0xae215..ae21b), then advances both pointers and
consumes EDX count by decrement (0xae240..ae254). Retained123B keeps the
second phase indexed with an independent increasing index and loads all
owners at entry. Four complete TUs cross output/input pointer postincrement
and consuming count on the second loop alone. No callback/alias-boundary
changes, headers or flag controls. Valid raw master baseline mandatory;
original store/read order and complete nonexact bystanders/data/relocations
reviewed. Decline all nonexact candidates even if live ranges improve.

Result: no exact gain/loss. Count consumption alone124B/SIZE1; cursors alone121B/SIZE2; both119B/SIZE4 versus123B. Complete original ownership still needs a separate first-loop/constant initialization discriminator; not another nearby counter synonym. Candidate changes only CrossDataLinks.
