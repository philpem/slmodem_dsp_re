# Independent Tone IIR shift array capture

Two complete TU cells on original93d7eee1: unchanged and the prior declared
short tap/four-expanded-section seed plus one explicit shift-array capture
in _iir_filter_progress. Prior section/width domain closed without an exacthit.
This new control is anchored on absent object operations: blob computes
&f->shift[0] once before sample loop, spills pointer to frame+34, uses that
cursor across expanded sections. Seed omits that pointer, directly loading
constant member offsets; frame60vsblob64,206vs213 instructions. No arbitrary
pointer/store reorder or extra lifetimes elsewhere. Capture short*shift=f->shift
before loop, replace only f->shift uses in the one function.

Root approved separate source-boundary discriminator. Rawfullbaseline,
unchangedprofile/bugdefine/assembler/RTL and full TU audits required. Close
if fullbody misses; no broad expansion, batch has20provisional gains already.

## Result: three bytes short, no adoption

2/2 valid complete TUs retain3/8exact, no losses. _iir_filter_progress799B
vsblob802, improved from782B seed; toneiir_progress1126vs1141 staysSIZE15.
Initial RTL shiftcursor detector fires. Six bystanders and metadata/data/
nontext/relocations unchanged. Close cursorcontrol, do not introduce padding
or expand permutations to fit the finalthree bytes. Finalbatch already has
20provisional gains from independent adopted leads.
