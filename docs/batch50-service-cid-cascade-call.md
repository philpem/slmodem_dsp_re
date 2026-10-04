# CID nested cascade call source boundary

Threecells raw902, ratio-first named predicate/cursor seed, nested cascade
call (no intervening y assignment). Blob two sequential IIRcalls narrow EAX
at firstcall result before secondcall; both statements and nested expression
are source candidates. Current separate y assignment extends intermediate
owner, while nesting binds firstreturn directly to secondformal. Filter
only writes its two short states at+7e/+80 and+82/+84; coefficient locals and
stateaddresses do not depend on intermediate writes. Preserve energies,
shortreturn/counter, order of actualtwoIIRcalls, no option/register controls.
FullTU raw902 and allmetadata/data/nontextrelocs/bystanders required.

Measured result: 3cells nestedcascade body-inert at259B/BYTES34; no gain. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
