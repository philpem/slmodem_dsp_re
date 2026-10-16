# DTMF bank selection arm order

Reference DTMF_MTD_detect tests rate==9600 and branches to9600 cold arm after
main body; fallthrough initializes8000 bank. Retained if==9600 puts9600 first
and8000else. Single source control inverts predicate and swaps both complete
banks. Preserve every table pointer and following loop; no arbitrary local
array initialization or register fitting. Read own source and full reference
relocations: same four filter call sites, positive9600 arm is at+870. Full TU
sixteen tables/metadata/nontext/relocations and bystanders reviewed.

Two controlled full TUs, retained and swapped bank1074B vs947B; inverted
source bank arm changes canonical body without size gain. No exact gains/losses,
all16 coefficient tables/metadata/nontext/relocations unchanged. The emitted
arm order alone does not identify source bank order in this measured negative case; close family, no bank/declaration permutations.
