#!/bin/bash
#
# holdlong.sh -- hold a call open and watch whether the rate keeps climbing.
#
#   holdlong.sh [calls] [hold_seconds]        # default 6 calls, 90 s each
#
# WHY.  Finding F1900: a second `V34DATARATE ... finally` block lands about
# ELEVEN SECONDS after CONNECT and roughly doubles the receive rate -- 14400
# to 26400, 12000 to 21600 -- and the DTE is never told, because `pty CONNECT
# nnn` is emitted once and never revised.  The far end's `ATI11 Speed` confirms
# the higher rate is really carried.
#
# This bench holds calls about 17.5 s after CONNECT.  Against a renegotiation
# at +11 s that leaves six and a half seconds of margin, and NOTHING IN ANY LOG
# DISTINGUISHES "did not renegotiate" FROM "was hung up before it could".  So
# every rate this bench has recorded is a lower bound of unknown tightness.
#
# V.34 rate renegotiation is not one-shot.  Finding F1474 measured `equerr`
# reaching 74-208 well after CONNECT, far under the object's own
# `renegUpthresh` of 2571, with the rate sitting still.  If the equaliser keeps
# improving there may be further steps nobody has stayed on the line to see.
#
# THIS COSTS NO CODE CHANGE.  `call.py --hold` already exists; the only
# variable is patience.  That makes it the cheapest lever on the rate deficit
# by a wide margin -- cheaper than the pre-emphasis work, and testing something
# the pre-emphasis work cannot reach.
#
# WHAT IT RECORDS.  Every `finally txbitrate/rxbitrate` block WITH ITS
# TIMESTAMP, so the trajectory is visible rather than just the endpoint, plus
# the far end's ATI11 at the end as ground truth for where it finished.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
N=${1:-6}
HOLD=${2:-90}
CSV=$BENCH/captures/holdlong.csv

COURIER=$(. "$BENCH/modems.sh"; modem_resolve courier) || exit 3
echo "call,hold_s,connect_rate,final_rate,far_xmit,steps,first_step_s" > "$CSV"

for i in $(seq 1 "$N"); do
	L=$BENCH/captures/hold-$i
	bash "$BENCH/waitquiet.sh" >/dev/null || { echo "machine never quiet" >&2; exit 3; }

	# --hold writes NOTHING and keeps the carrier up, which is what is wanted:
	# a renegotiation should not have to compete with a DTE probe, and finding
	# 1455's confound -- a disconnect coinciding with the first write -- is
	# exactly the sort of thing a long silent hold rules out.
	SLMODEMD_IODELAY=240 TTY=courier HOLD=$HOLD \
	TTY_EXTRA="AT&F;AT&A3;AT&B1" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		timeout $((HOLD + 140)) bash "$BENCH/row.sh" "$L" pty 1902 \
		> "$L.run.log" 2>&1

	timeout 60 python3 "$BENCH/lastlink.py" "$COURIER" --label courier \
		--only ATI11,ATI6 > "$L.lastlink.log" 2>&1

	python3 - "$L" "$HOLD" "$CSV" <<'PY'
import re, sys
base, hold, csvpath = sys.argv[1], sys.argv[2], sys.argv[3]
def rd(p):
    try: return open(p, "rb").read().decode("latin-1")
    except OSError: return ""
sl, run, ll = rd(base + ".slmodemd.log"), rd(base + ".run.log"), rd(base + ".lastlink.log")

steps = [(float(t), int(r)) for t, r in re.findall(
    r"<\s*([\d.]+)>[^\n]*V34DATARATE, finally txbitrate \d+,rxbitrate (\d+)", sl)]
m = re.search(r"<\s*([\d.]+)>[^\n]*modem report result: 1", sl)
con_t = float(m.group(1)) if m else None
m = re.search(r"pty +CONNECT (\d+)", run)
con_r = m.group(1) if m else ""
m = re.search(r"Speed\s+\d+/(\d+)", ll)
far = m.group(1) if m else ""

after = [(t, r) for t, r in steps if con_t is not None and t > con_t]
first = "%.1f" % (after[0][0] - con_t) if after else ""
name = base.split("/")[-1]
print("  %-10s CONNECT %-7s -> final %-7s  far %-7s  %d step(s) after connect%s"
      % (name, con_r or "none", steps[-1][1] if steps else "-", far or "-",
         len(after), ("  first at +%ss" % first) if first else ""))
if steps:
    print("      trajectory: " + "  ".join(
        "%+.0fs:%d" % ((t - con_t) if con_t else 0, r) for t, r in steps))
open(csvpath, "a").write("%s,%s,%s,%s,%s,%d,%s\n" % (
    name, hold, con_r, steps[-1][1] if steps else "", far, len(after), first))
PY
	sleep 6
done
echo
echo "HOLDLONG done -- $CSV"
