# HDLC receive original frame-result guard and narrowing at use

902f47fa baseline retained period commands/config/assembler+bug define.
Original _hdlc_receive_state (738B) tests length AX at word width and keeps
nonzero valid-frame body before deferred error body; explicit MOVZWL AX at
cTOOLS output count handoff. Source unsigned-short length is tested at full
register width and has empty/error arm first. Independent two-cell axes:
nonzero valid arm first; signed short local with unsigned-short handoff cast.
Full four-cell cross, all statements/child reads/callback order fixed inside
arms. Whole 16-bit length semantics preserved including high bit. No flags,
headers, register/argument/local permutations or count-signedness speculation.
Complete TU all bodies/export/data/reloc audit, baseline raw reproduction;
stop/reframe after finite miss. No harness/fuzz/mutation execution.
