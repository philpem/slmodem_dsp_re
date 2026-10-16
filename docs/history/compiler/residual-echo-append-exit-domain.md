# Echo append shared exit and guard-owned fast count

Follow-up to the closed initial four-cell cursor/preincrement cross. Both
properties recover distinct original instruction shapes; combined245B still
misses original244B and adds an instruction. No near-score adoption.
Independent original path witnesses remain: the range guard jumps into fast
path while slow path falls through; both paths join one cleanup. Fast count
is copied to its remaining register only after the zero guard, and original
count remains available for +=. Current combined control declares/copies
remaining before that guard and retains an explicit fast return.

Five full-TU cells: unchanged production baseline; combined cursor/preincrement
control repeated; three further controls crossing fast early-return versus
common if/else exit and pre-guard versus guard-owned remaining initialization.
Late form guards original count then initializes unsigned remaining inside;
no declaration-order, register, width, literal or flag permutations. All
existing calls, unsigned wrap, equality-only compaction and unguarded copy
remain identical. Original machine store timing is not proof of unique source.

Raw unchanged and repeated controls required, all bystanders/data/metadata
reviewed. Inspect complete control flow and early RTL, not size. Complete cross
closes on misses, no further nearby spellings. No source adoption absent exact
supported output and final period gate. No mutation/fuzzing execution.
