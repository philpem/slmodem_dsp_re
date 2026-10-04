# Receive setter query ownership across callbacks

Original305B voice_set_rx reads cfg.get_sreg after detector_set_enable for
the silence_create argument (0xaf1c8), then each gain call uses CALL *4(EBX)
with a new owner load after prior calls. Retained source snapshots query at
entry, keeping ESI through all callbacks and calling that register. Five
complete TUs: raw baseline; late snapshot×direct gain owner calls (three
nonbaseline crosses); direct owner at every use (no snapshot). Existing
unsigned-result interpretation stays at the use via an explicit uint cast;
headers/ABI unchanged. All compiler flags/bug define and full data/bystander
checks mandatory; positive requires strict full305B identity. Callback-visible
pointer changes are meaningful; do not call this universally behavior-inert.
