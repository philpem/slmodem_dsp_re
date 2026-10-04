# V92Jd phase float result boundary

Pinned856c1ecb fullTU. Original getJdPhase80B accumulates unsigned phase identically to56B baseline but ends FMULS,FSTPS/FLDS through a4B owned local before float return (0x11e99..11eaa). Current return expression has no float result local and leaves product on x87 stack. Predeclare exactly baseline versus named float result initialized by the unchanged phase/65536.0f expression, then returned. Values are exact multiples of2^-16 with16bit phase, so rounding cannot alter a valid result. No volatile/artificialspill/type/literal/change, flags or method order. Prior setter/conversion controls distinct and closed. FullTU raw baseline/all bodies/nontext/canonicalrelocs/export audit, parent batch gate only.

## Measured closure

Two validrawfullTUs; namedfloatresult is rawinert56B/SIZE24. All21methods/TU11exact/metadata/data/nontext unchanged. Ordinarylocalalonecannotrestoreoriginalnarrowing; next discriminant is expression mode documentedseparately.
