# Fax default-argument recovery

The earlier F11872 experiment left all source unchanged because its target
was a complete strict match. The owner subsequently authorized improvements
as well as byte-exact gains. Adopt only its independently proven argument:

```c
int extra = 0;
/* FTM alone assigns extra = 0x50. */
```

Original FAX_class1_command clears EBP at0x174f, passes EBP as the fourth
argument at0x17e0, and FTM assigns80 at0x1940. The other EBP writes in the
body restore its saved caller value during epilogues; they are not dispatch
assignments. This gives zero for other successful commands and80 for FTM.
The old uninitialized source compiled to80 on every successful command.

The new production object is raw identical to the already audited extra-only
complete-TU control. Only FAX_class1_command changes; three bystanders stay
identical. Its length gap changes SIZE8 toSIZE5. No strict function gain or
loss follows. The prior full audit explicitly explains the unchanged-string
pool's reordered offsets and checks data/BSS/bindings/nontext jump tables.
The new argument audit fires on original, defective baseline and production.

This is source/argument fidelity, not a measured public operational failure:
the actual callee consumes arg4 only for TM, where the old caller already
passed80. Its remaining source/CFG/profile families stay closed pending new
evidence; no constructor, dispatch, local-type or register fitting is adopted.

Validation: `make phase J=8`,388 passed/0 failed, all structural checks green.
`make tc`:300 sources/300 objects/0 failures. The complete strict census remains
1074/1852,118087 original bytes, with zero exact-set gains or losses versus the
preceding checkpoint.

Reproduction and measurement:

```
python3 tools/fax_service_values_reproduce.py --domain docs/fax-service-values-domain.md
python3 tools/fax_service_values_audit.py
python3 tools/fax_extra_argument_audit.py
```

Artifacts: `build/fax-extra-production-audit.json`,
`build/phase-peep2-fax-extra.log`, `build/tc-fax-extra.log`.
