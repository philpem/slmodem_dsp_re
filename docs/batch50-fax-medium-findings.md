# Medium completed-fax bounded source negatives

Following the stable eighteen-gain package, fresh 651..1500-byte screen
measured nineteen nonexact completed fax bodies. Four independent domains
produce twenty-two valid complete-TU cells and no exact gains or losses.
`tools/batch50_fax_medium_audit.py` proves all unselected bodies unchanged,
metadata/export/ABI unchanged except the deliberately restored original
undefined SDM_CFG reference, named data/nonstring sections unchanged, text
relocation targets equal except the explicitly recovered template reads.
String pool emission order changes in some negative cells: exact string
values/counts and section sizes are audited separately, not called raw-equal.
No production source/header changes from these controls.

* V17TX_create (1043B, baseline SIZE3): original FIFO struct template,
  SDM struct template and SMCv17_CFG array wide-copy boundaries versus
  field initialization. Full eight-cell cross, no closure. These restore
  real named reads/offsets but do not establish original whole-function
  source or profile; do not adopt for local resemblance or smaller size.
* _hdlc_receive_state (738B, baseline SIZE8): nonempty-frame valid body
  first and signed word length carrier with unsigned narrowing at output
  handoff. Full four-cell cross; combined SIZE2, no closure. Whole 16-bit
  length semantics preserved, no register/local argument permutations.
* _tx_scrambled_ones_state (681B, baseline SIZE20): original *tx_count
  read occurs immediately before FAXVMI_process, after callbacks/scratch
  stores; reconstruction captured it at entry. Independent original signed
  readiness compare differs for negative tx_bytes_per_block from current
  unsigned compare. Four cells; late-result with/without signed-ready
  reaches SIZE3, no closure. Preserve callback-alias and negative-count
  discrepancies explicitly; no assumption they are unreachable inputs.
* faxvmi_hdlc_frame (928B, baseline SIZE37): original parent framer reloads
  after debug/name callbacks; source snapshot versus consistent rereads.
  Two cells; rereads SIZE99, no closure. Original scalar bitpacking
  snapshots remain fixed. No artificial owner/count/register fitting.

Domains: docs/batch50-fax-{tx-templates,hdlc-frame-guard,medium-reads}-domain.md.
Tools: corresponding batch50_fax_*.py plus medium_audit.py. Actual commands,
compiler/executed-assembler identity, source/hash/object/RTL and all body
verdicts retained under build/batch50-fax-*. Root owns final period gate;
no runtime, mutation or fuzzing execution was added here.

Fresh V17 constructor mode/store domain adds four cells: original partitioned
mode decision tree via switch, exact abs/dif/tcm store order, independently
crossed. Encoder-only SIZE3, switch with/without stores SIZE13; no gain or
loss. FullTU audit changes only constructor and preserves metadata/data/
reloc targets. Prior template-copy domain stays closed and independent.
