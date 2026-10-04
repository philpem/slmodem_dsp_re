# Class1 constructor original NULL-owner failure exit

902f47fa retained full period profile/raw baseline/bug define/actual assembler.
Original fax_class1_create (1532B): post-template cfg-owner tests 0x93315
and0x933cc both branch to shared0x933f9; original diagnostic
"Internal memory allocation error!\n\n" at .rodata.str1.4:0x117d8 and
NULL return. Source currently conditionally skips each child create on NULL
and continues initialization. This is an actual original CFG/behavior missing
arm, not a invented sysdep_malloc failure check: preserve the same owner-read
sites, completed template writes, and avoid adding guards at earlier malloc
returns (original has none). Two independent original vmi_c/vmi_a cfg-owner
error sites, four full-TU cells. Diagnostic return spelling kept identical
across sites so compiler may merge original shared error tail. All other
allocation/copy/read/call/store ordering unchanged. Audit metadata/data,
original string/call relocations, all bystanders/exact sets. No new headers,
flags, runtime/fuzz/mutation or reopening completed fax reconstruction.

Measured four valid cells: constructor SIZE2 baseline, either individual
failure arm SIZE35, both SIZE30. No exact gains/losses, all8exactneighbours
stable. Unchanged-source nonexact fax_class1_progress also changes, explicitly
recorded as a compiler-cursor bystander instead of silently excluded from
completeTU proof. Candidate only; no production adoption. Dedicated audit
checks original error string addition exactlyonce, diagnostic relocation
changes only, metadata/named data/nonstring sections unchanged. Both-site
preimage is a genuine missing original failure branch, but byte closure is
not established; future period behavioral recovery can assess independently.
