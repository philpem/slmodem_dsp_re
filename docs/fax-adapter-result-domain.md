# Fax virtual-modem status forwarding

Eight full-TU cells, four Vmi_v17/21/27/29 TUs each baseline and modem-status
candidate. Original all eight process adapters preserve wrapped int modem
return in EAX across count/result stores; FAXVMI_process consumes the function
pointer return as status (954d9..9550f). Retained adapters declared void and
clobber EAX for an output address. Therefore test int-returning adapter with
named wrapped status, original count stores then return status. Candidate-only
faxadapt header overlays reconcile declarations; no production headers edited.
Other functions/data/bindings unchanged; all4 raw baselines must reproduce.
Audit all emitted bodies and exports/data/nontext relocs before any adoption.
No register/declaration/cursor fitting. Existing casts in the dispatch table
are reconstruction compensations, not evidence the author intended void API.
