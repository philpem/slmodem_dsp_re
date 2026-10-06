# Fax publication batch

Base e0052eec, retained Gentoo3.4.2-r2 profile with complete recorded commands,
DSPLIB_REPRODUCE_BUGS last, actual selected assembler2.15.92.0.2 identity.
All full-TU unchanged baselines raw-reproduced before interpretation.

## Adopted: FAXVMI_process, EXACT327

The source captured vmi->framer at entry, retaining it across indirect
pack/modem/unpack/reverse callbacks. Original loads framer at95523, after
all callbacks and the underrun guard. Moving this one owner capture to that
boundary reproduces the complete327-byte original body. This is a pointer
publication/read boundary supported by object operands; it does not infer
that modem lifecycle callbacks normally replace the framer. No interface,
export, type, store or callback-table change. All other five emitted TU
bodies unchanged; exact3→4/6, zero losses. Original member loads/calls and
full canonical relocation targets agree, not just instruction mnemonics.

## Closed: three receive-wrapper accumulator/count boundaries

Twelve cells, four each V17r_prc/V21r_prc/V27r_prc. int accumulator retaining
existing explicit(short) per-add cast crossed with final caller-memory count
predicate. Original CWDE-after-ADD and HI loop predicate separately bound the
family; narrower final totals are not full-width arithmetic evidence alone.
No complete exact gain/loss. V21/V27 memory predicate reaches original127B,
BYTES17; int accumulator neither closes that nor changes those emissions.
V17 original127B remains SIZE6/7/8/9 across controls. No adoption or expansion
into arbitrary declarations, stack slots or register fits. Next independent
question is declaration/use coalescing before global allocation; size alone
cannot reopen this family.

## Closed: receive cleanup slot switch

Two full class1rx TUs. One-case switch controls original MOVZWL slot then
registerCMP versus retained single if memoryCMP. Complete output raw-identical
between candidate and baseline; source-CFG spelling cannot account for that
load boundary under these controls. No adoption or neighboring synonyms.

`python3 tools/fax_publication_batch_audit.py`:26validcells,174complete body
comparisons, unchanged records/binding/visibility/type, named data/BSS and
symbolic nontext relocation inventories; one measured negative merge-string
pool reorder described below, all nontarget bodies unchanged outside the next-state negative family,
zero exact losses. Known winner causes one changed-body and one exact-gain
report, so a clean detector is not silently empty. Tools/domains and full
saved objects/RTL/source/header/hash ledgers remain under build/fax-*.

No AGC-return or priorCRC/V21flag domain rerun; AGC family already closed
F11614 across12TUs/36cells and needs new independent evidence. No fuzzing,
mutation or per-iteration runtime execution. Root owns aggregate census and
one final combined makephase for adoption.

## Larger callback-owner inventory, bounded negatives

V21 next-state original receive/transmit owners reload after each of three
known-state diagnostic callbacks. Eight crossed controls preserve all field
stores/debugstrings and compare post-diagnostic reload with ordinary int
switch-state capture/fresh default member read. All rawbaselines pass,
no gains/losses. Receive callback reload reduces SIZE20 to3; int controlling
carrier/fresh default is raw-inert on receive at both owner controls. On
transmit, reload restores the original caller-saved owner and root-only frame
but remains SIZE27; cross remains SIZE26. The smaller retained SIZE3 hid
wrong pointer lifetime/callee-save frame, so not a sound reason to retain it
as an original-source claim. None of these controls is adopted.

HDLC unframe original reads current parent after unknown CRC/debug calls
(960e0,96116,96154), while retained source caches entry framer throughout.
The earlier closed HDLC FRAME reload control concerns the other function.
Two fullTU unframe controls preserve initial scalar snapshots and use current
parent members in later accesses; SIZE209 becomes58, no exactgain/loss. Only
unframe body changes. Remaining CFG/debugcall duplication has not been
explained; reproducing reloads alone does not recover complete source.

After two further batches with no complete closure, scope review is explicit:
owner publication remains a fruitful detector (FAXVMI_process exact), but
not a promise every owner repair settles scheduling/CFG effects. Reopening
these negatives requires independent RTL/source/operand evidence rather
than near-size improvements or declaration/register permutations. Three
receive wrapper and AGC families remain closed.

### Full-TU collateral in negative next-state controls

Each owner-reload control also changes three source-unchanged Hdx callers per
TU through the inlined next-state body. These are not register-only fits:
complete baseline-to-candidate sizes change. RX callers Data/Wait/Start and
TX callers Data/Idle/Start were individually checked: sourcechunks and direct
canonical call-target multisets remain identical, while inlined owner reads
and resulting code layout change. No exact loss; no caller candidate adopted.
The durable audit records each changed caller/verdict and its call inventory,
instead of silently assuming all nonedited bodies remain unchanged. Every
saved object/source hash and complete live original canonical verdict is
recomputed against results.json before deriving exact gains/losses.

### Unframe negative control: named merge-string pool reorder

The current-parent negative control changes only .rodata.str1.1 allocation
order, not named objects or BSS/nontext relocations. Both pools are197bytes,
SHF_ALLOC|SHF_MERGE|SHF_STRINGS(flags0x32), alignment/entrysize1, with identical
complete NUL-terminated string multisets. Two literals swap their positions:
"CRC is now OK!\n" moves0x97→0xb5, and "Replacing the second byte...\n"
moves0xa7→0x97. All other literal offsets unchanged. byteident's merge-string
relocation canonicalization identifies actual literal bytes, so this layout
change does not erase a target distinction. Unedited HDLC frame's complete
canonical body stays identical despite relocated pool entries. The audit
records this precise negative-control collateral; it does not weaken the
winner's unchanged-data requirement or regard arbitrary pools as equivalent.
