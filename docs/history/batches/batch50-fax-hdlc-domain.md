# Emulated receive diagnostics and member boundaries

902f47fa fixed profile. _hdlc_emulate_receive_state has one authentic absent
level>1 diagnostic immediately before output: original format
"%2d.%02d[sec] Receive buffer OK in _hdlc_emulate_receive_state\n", clock_sec,
clock_frac. Blob reloads superframe_read_idx after diagnostic and after
cTOOLS_handle_hdlc_output; ours retains a local across both calls. Cross only
original diagnostic restoration and member-owned index uses/increment. Four
full-TU cells, all other loops/layouts fixed, no header/profile changes. Stop
if no exact gain, retain measured nonexact bodies and relocation audit.
