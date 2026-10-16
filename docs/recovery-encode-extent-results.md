# Faithful diagnostic scratch extent

The opening two-cell domain is recorded in
[recovery-encode-extent-domain.md](recovery-encode-extent-domain.md). Both
complete encode.c compilations use the retained Gentoo source profile and
DSPLIB_REPRODUCE_BUGS; the unchanged control reproduces its production object
raw. The original cEncodedTemp.1 extent is270 bytes, and the candidate
faithful extent restores that local symbol size from271 to270.

Both canonical function bodies remain unchanged: cEncodeChar stays EXACT;
edprintf stays SIZE9. There are no function gains/losses, added definitions
or changes to other allocated bytes. Alignment absorbs the extent correction
without changing the total BSS size. The fixed-build preprocessor arm keeps
271 bytes; the original length guard and all encoded indices remain unchanged.
D39 records why the faithful131-character message writes its final NUL into
original alignment padding.

Existing Gentoo `make period T=t_encode J=3` passes1/0, including the
128..135-character boundary and captured diagnostic output. No new fixture or
tolerance is used. This is a data-symbol recovery, not an exact-function gain.
The D40 readable-output extension remains separate code-generation debt.

Replay the pinned domain with `tools/recovery_encode_extent.py --domain
docs/recovery-encode-extent-domain.md --baseline-dir ARCHIVED_TC_REPRO
--historical-headers`. Artifacts under build/recovery-encode-extent retain
complete commands, config/header/source/object hashes and both function
verdicts. Full branch gates and strict/raw partial-link results are recorded
in the campaign implementation report.
