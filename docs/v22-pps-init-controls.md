# V22 pulse-shaper work-index and rail controls

F11599. [Four-cell predeclared cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949682755)
at43ef6841 tests early k initialization × work_i/work_q declaration order.
Blob k zero precedes config/allocation at0x8d6aa; work_i+0x100/work_q+0x10
versus retained late zero and opposite stack arrangement. All statements,
types, allocations and external data unchanged.

Four sources/four complete emissions: baseline305B/SIZE15, rails305B/SIZE15,
early289B/SIZE1, both289B/SIZE1 vsblob290B. No exact gain/loss,1/3 unchanged;
only init changes. Combined76vs78 instructions after padding removal; not a
register-only mismatch or exact recovery. All3 functions/2 globals/data owners,
type/binding/visibility and allocated nontext preserved. Baseline raw-reproduces
production. Replay tools/playbook_v22_pps_init.py --domain <linked URL>;
artifacts build/playbook-v22-pps-init preserve full actualcommands/hashes/RTL
and complete-object-audit.json under Gentoo3.4.2-r2, executed assembler2.15.92.0.2,
full savedflags and mandatorybugdefine.

Close this index/array-position family without source adoption. No candidate
differential/census/partial gates; existing fixtures are not candidate results.
The neighboring MRF near-misses and this result suggest local index/stack/order
families remain underdetermined; do not add saved registers or permute more
source to fit them. Select a fresh operand/use discriminator in another function.
No global ceiling or original-profile conclusion; no fuzzing/mutation execution.
