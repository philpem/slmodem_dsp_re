# Original V17/V27/V29 transmitter control boundaries

Base856c1ecb, retained full Gentoo3.4.2-r2 flags/config, mandatory bugdefine,
executed assembler identity, raw baseline fullTU reproductions. Prior batch50
RX/V21 control families remain closed; these three TX controls were untested.
Owned pool:133TUs,510common functions,225exact,285nonexact; first domain3TUs.

Original V17/V27/V29 initial scale store captures request multiplier exactly
once, then multiplies that stored scale by indexed coefficient. Source V17/
V29 reload request after initial store; V27 omits initial store entirely.
Compound scale assignment restores original store/read ownership, with
original initial V27 store retained even when alias visible. No assumptions
about request/child/config separation; preserve whole argument semantics.

V17 source PPS subobject pointer forces offset carrier where original keeps
whole transmit block. Independent block-owner direct member accesses, and
common return after conditional create (source early1 return duplicates tail
where original common return). Fixed factorial2x2x2=8cells.

V27 independent block-owner accesses and request flags reevaluation after
force-int member store, original reload at0xa3e90; source cached byte survives
store incorrectly. Compound × owner × reload=8cells. Cached mask untouched.

V29 original request flags snapshot before boolean child store, reused for
reinit predicate; source rereads. Snapshot byte/int carriers are bounded,
equivalent for entire original byte domain, independently crossed with
compound scale.2x3=6cells. No parameter/header/API/register/statement-order/
flag permutations; all other child reads/configstores fixed. Total22cells.
FullTU metadata/data/nontext/relocations/exports/allbystanders and exactset
review; no arbitrary expansion after misses. Root finalperiodgate, no phase,
fuzz/mutation or runtime-test iteration. Findings unnumbered centrallyassigned.
