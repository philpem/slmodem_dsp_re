# V32 cleaned owner reload depends on alias analysis

Two unchanged-source complete-TU controls at f420cd89. Baseline full retained
Gentoo command raw-reproduces; only diagnostic adds-fno-strict-aliasing, with
the mandatory bug define still last. Source hashes are identical.

Diagnostic recovers V32FP_GetCleanedSamples EXACT53B from47B/SIZE6, including
both post-count-store owner reloads. Its machine owner loads are three versus
baseline one. This is an actual option interaction, not a source spelling fix.
Complete TU exact2/6→3/6; V32FP_create/recreate also change. Binding/named data
and nontext sizes are unchanged; merge-string order changes while its complete
string multiset and canonical references agree. Report this collateral.

Twenty-nine parsed instruction streams per cell count real owner-load SETs,
not REG_EQUIV repetitions. Both have two loads through07.addressof. The first
available reduced stream09.loop has one retained/two diagnostic. Duplicate
08.gcse snapshots are excluded, so the specific eliminating pass inside that
window is not attributed. Later block copies produce the diagnostic's three
machine loads. Original RTL remains unavailable.

```
python3 tools/gcc3_v32_cleaned_alias.py --domain docs/v32-cleaned-alias-domain.md
python3 tools/v32_cleaned_alias_audit.py
```

Twelve complete body verdicts; no production source/profile adoption or candidate
runtime claim. A source correction needs independently supported original
field/prototype/access typing; a profile hypothesis needs full-TU crossed source
controls. Do not insert volatile, alias casts or invented unions to force reload.
The retained typed owner may differ from original access idioms, or the original
profile may differ; this finite option control does not decide between them.
No fuzz/mutation execution or tolerance change.
