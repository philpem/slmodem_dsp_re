# V27 direct owner-member deletion arguments

F11612. At d3abfa0b, [predeclared two-cell controls in two complete TUs](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950997289).
F11610 restored ignored arguments but left RX191B versus193B and TX107B with
BYTES9. Canonical suffixes after RX's third MRF call and TX's first PPS call
already matched. Initial FSE/SRE and PPS setup differed: source assigned the
child local before its call; blob prepared the ignored scalar first, then read
the child owner. C argument evaluation permits direct expressions to lower
otherwise than an explicit preceding pointer assignment.

Replace just RX's initial three embedded-free argument expressions and TX's
initial PPS expression with direct owner-member addresses, removing the
preceding pointer assignments. Keep locals and owner reloads used later,
every other statement/guard/call/argument/header/helper/source order. No register,
stack or declaration permutations, flags, volatile or incompatible casts.

Four valid compilations; each TU two distinct full emissions. Both baseline
objects raw-reproduce retained production. RX193B and TX107B complete EXACT,
two gains/no losses. Seven function symbols total, no named data objects.
Type/binding/visibility/imports/exports/allocated nontext agree. RX decision
changes only under consistent register renaming (alpha comparison passes) at
unchanged298B and stays non-exact; four other unchanged bodies (RX create,
epoch, training and TX create) retain canonical bytes/relocations. Review this
bystander instead of claiming only the edited function's body changes.

The original source may have used macros or another direct expression; do
not claim unique spelling from scheduling. This is supported call factoring,
not an ordinary lifecycle bug. Existing fixed V27 create/delete tests use real
source/reference allocations and verify allocator balance; retain all tests.
No extra synthetic alias fixture is needed for this setup change.

Production adoption rebuilds all300 period objects: exactly the two expected
objects change, and both raw-match promoted complete experiment cells. Census
894/1852 →896/1852, exact bytes89990 →90290, two gains/no losses. Fixed
make phase passes385 tests/0 failures and all structural gates. No fuzzing
or mutation execution. Full same-order partial links remain DIFFERENT;
positioned bytes68614 →68637/943398, allocated bytes914366 →914382,
sections70/92 and symbols394/2907 unchanged, relocations1023 →1021/18317.
Keep this mixed layout measurement; function gains do not establish the
original whole-object profile. Artifacts build/playbook-v27-delete-adoption.
Replay tools/playbook_v27_delete_arguments.py --domain <linked URL>.
Artifacts build/playbook-v27-delete-arguments retain actual complete saved
flags and mandatory DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed
assembler2.15.92.0.2, commands/source/header hashes/full objects/initial RTL,
changed-body disassemblies and per-family complete-object audits.
