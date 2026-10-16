# Receive control source boundaries

902f47fa, fixed complete profile and source ABI. The blob's V17/V29 control
first writes zero to child flag, then conditionally writes one; source has a
boolean assignment. One witnessed clear-then-set control each (V21 included
as transfer with same bool assignment). Preserve byte-request rereads after
stores and self-reinit semantics. V27 already spells clear/set, but caches
flags and child owner across writes where blob reloads: independently remove
request byte snapshots and child pointer caching. No unrelated reordering.
All complete TU baselines raw-match; stop each family after this bounded
boundary. All alias cases remain part of semantics; preserve blob reloads.
