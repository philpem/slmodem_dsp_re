# Broad helper/caller screen and V34 controls

Source baseline 7edfb734; retained Gentoo GCC 3.4.2-r2, executed assembler
2.15.92.0.2, complete build-config flags with DSPLIB_REPRODUCE_BUGS. Production
remains **1079/1852 EXACT**. No src/, include/ or profile changes are adopted.
[Screen domain](helper-caller-screen-domain.md).

The screen covers **299 C/C++ TUs, 1934 emitted body copies**: 1105 exact
copies excluded, 781 non-exact copies eligible, 48 without original symbols.
The assembly TU is explicitly excluded. These copy denominators are different
from the distinct-symbol production census. It recognizes315 of1367 lexical
for headers, leaves1052 unsupported, and maps every recognized header to an
owner. There are83 eligible fixed-loop bodies. Canonical-call counting excludes
155 retained and93 original call sites, including indirect callbacks.

Three detector controls pass: live closed V92Precoder6/12-loop positive,
historical ConstellationPower recorded-count positive (mod5→1 qualifies;
div6→2 does not satisfy the <=1 rule), and live exact equal-call negative.
The historical positive is not an archived object replay. Initial incorrect
TU300 and control expectations were preserved as invalid logs and excluded.

There are **24 nominations, nine outside previous screening scopes**:

| Body | New-screen observation | Disposition |
|---|---|---|
| DialerAbort | diagnostic2/1 | Prior terminal/capture domains already closed; no retry |
| TimingV34 | setTimingStateParameters2/0 | Known helper/profile lead; no profile changes |
| probeselect | chkForceBaudRate2/1; fewer backedges | Known #22 lead; aggregate counts do not recover source |
| v34handshak | many missing calls;194/58 backedges | Known inline divergence; no global inference |
| v8handshak | cosread3/1, mpyint3/0 | Previously recorded leads; not reopened |
| receiver | fixed three-tap loop;24/26 backedges | Original adaptive loop remains at5c7b0..5c81b; no expansion |
| VPcmV34Progress | fixed16-bit packing loop;33/39 backedges | Original packing loop remains atb4a0..b4af |
| modulatevector | seven halvings/four groups;13/14 backedges | Both original loops remain (59f79,5a2c0); no expansion |
| putFrame | four-group loop;1/2 aggregate backedges | Original sixteen fixed-offset group callback sites; tested below |

The fifteen old-scope nominations include bodies not previously nominated at
an earlier source baseline; `prior scope` means the filtering domain was
covered, not that every current nominee was individually tested. No new
source controls were opened for those from counts alone.

This screen omits included-header loops and macro/variable bounds. Manual V34
header inspection found the snapshot's two six-entry loops retained in the
original; the width witness below is independent of expansion. The other two
recognized header loops are not declared exhaustively classified by this pass.
Neither an empty new-source adoption nor these nominations prove a byte-exact
ceiling or establish that the remaining reconstruction is correct.

## putFrame: explicit groups recover operations, but not complete identity

[Predeclared finite domain and follow-ups](v34-putframe-expansion-domain.md).
Original584B at57fb0 has sixteen group callbacks with fixed offsets2..17;
retained376B rolls four callback statements over four groups. Initial RTL has
7 calls versus19 after expansion; final code has6 versus18, matching the
original18 total sites. The high-width head shares one call between paths,
so static call counts are not per-path invocation counts. All call/read order
is preserved; no frame values are captured across callbacks.

Eight width/expansion cells:

| Expanded groups | Short nb | Short small/small_last | Bytes | Verdict |
|---|---|---|---:|---|
| no | no | no |376| SIZE208 |
| no | no | yes |375| SIZE209 |
| no | yes | no |377| SIZE207 |
| no | yes | yes |377| SIZE207 |
| yes | no | no |577| SIZE7 |
| yes | no | yes |576| SIZE8 |
| yes | yes | no |578| SIZE6 |
| yes | yes | yes |578| SIZE6 |

Four additional expanded/short-nb controls cross the original main full-width
path with initializing width defaults before callback/frame captures. All are
578B/SIZE6; individual bodies differ despite equal size. A separate four-cell
cross tests a typed frame-parameter subobject at+a00 (original base formed at
57fe4), preserving the existing flat view through an experimental header union.
Without the subobject,578B/SIZE6; with it,565B/SIZE19. It reproduces the base
access but does not establish the original structure. The generator's initial
prefix-replacement collision stopped before compilation; corrected controls
were rerun, and the failed generator log is excluded.

Expansion changes unchanged decodeDepth as well as putFrame. The header-owner
controls additionally change getFrame and modulatevector. All three matrices
preserve their exact3/12 common bodies, complete symbol metadata, named data,
allocated nontext sizes and relocation-cleared data. No exact losses or gains.
**18 full-TU cells /216 common body verdicts** (includes three raw baselines).
This bounded domain is closed without adoption; closer size is not identity.

## Snapshot: genuine short-counter witness, still no exact body

[Four-cell domain](v34-snapshot-width-domain.md). Original first index update
sign-extends a word at672ed; the second uses cwtl at67473. Both compare words
with five. Original error-difference narrowing is explicit at67316/6732a.
The helper declares int i,t and casts t at its assignments.

Short-i, short-t and both header overlays all leave v34handshak12867B against
original61541B (SIZE48674), with exact9/29 common bodies preserved. Short i
changes the target; short t alone is target-raw-inert. All three controls also
change unchanged v34tx1_trnseg4a, another TU-history effect. No uniqueness or
complete byte-exact recovery follows. **4 cells /116 common body verdicts**.

## Replay and stopping point

    python3 tools/helper_caller_screen.py
    python3 tools/gcc3_v34_putframe_expansion.py --domain docs/v34-putframe-expansion-domain.md
    python3 tools/gcc3_v34_putframe_capture.py --domain docs/v34-putframe-expansion-domain.md
    python3 tools/gcc3_v34_putframe_owner.py --domain docs/v34-putframe-expansion-domain.md
    python3 tools/gcc3_v34_snapshot_width.py --domain docs/v34-snapshot-width-domain.md
    python3 tools/helper_caller_audit.py

Artifacts live under build/helper-caller-screen.json and the four named
build/gcc3-v34-* experiment directories. Commands, compiler identities,
source/header/config/object hashes and complete verdicts are recorded there.
All raw full-TU baselines reproduce retained objects. Final audit covers
**22 cells /332 common body verdicts**, symbol metadata, named data,
relocation-cleared allocated nontext and putFrame initial/final call controls.
No mutation/fuzzing execution or additional period run is claimed: production
source is unchanged, and the earlier388/0 production gate remains the result.

Next widening should explicitly classify indirect callback targets and
included-header/macro-bounded loops, then verify iteration graphs and callback
read ages. A new putFrame experiment needs an independent remaining operand,
load-age or type witness, rather than further initialization permutations.
These findings do not close issue22 or identify the original flag profile.
