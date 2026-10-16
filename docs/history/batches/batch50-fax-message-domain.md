# Fax message and status boundary controls

Revision 902f47fa; completed reconstructed source only. No profile change,
headers, runtime/fuzz/mutation, register constraints or assembly fitting.

The eight 39-byte message reporters have an unsigned full-width guard, valid
arm first, and a full-width AND 255 before indexing in the blob. The current
33-byte forms place the null arm first and convert to unsigned char. Cross
only arm orientation and full-width mask versus narrowing, independently,
keeping the historical inclusive guard (including its invalid-table edge).

The V17/V29 transmitter status reporters show a load/AND/store first flags
update, then source flag load before the adjacent flag clear. Transfer F10231:
explicit first flags value and final value lifetime, independently, retaining
all alias-visible stores and second bitrate reads. The mask admits bit 2;
intervening adjacent clear affects only bit 0, so final sampling is equivalent
for every overlap. All other statements and types fixed. Baseline plus three
message controls and three status controls per TU. Stop on miss.

Complete TU baseline must reproduce raw retained bytes under the complete
Gentoo 3.4.2-r2 flags and bug define; review all bodies, exports, relocations,
named data and allocated nontext before adoption. Root runs final batch gate.
