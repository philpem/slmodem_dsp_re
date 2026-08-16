#!/bin/bash
#
# Is the 33600/14400 split an asymmetric LINE, or a fault in our receiver?
#
# Nothing had established which direction either end's number describes, and
# transmit rate / negotiated maximum / achieved receive rate are three different
# claims.  This reads both directions at BOTH ends, per call:
#
#   slmodemd  CONNECT <n>  = m->rx_rate   OUR RECEIVE   (modem.c:317)
#             TxRate: <n>  = m->tx_rate   OUR TRANSMIT  (modem.c:306), printed
#                            only when S70 bit 2 is set
#   SupraExpress  AT&V1    = LAST TX rate and LAST RX rate, separately
#
# The consistency check: our RX should equal their TX, and our TX their RX.  If
# the pairs agree then both ends are right and the link is simply asymmetric --
# no defect involved, and the ranked list in deviations.md Appendix C does not
# apply to this symptom at all.
#
# FIVE CALLS.  This bench connects 4 times in 5 and rates vary threefold
# between successes, so one call cannot say what the rates typically are.  One
# call IS enough to fix the semantics -- which number names which direction is
# structural and does not vary -- and that is the part the whole question turns
# on.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
. "$BENCH/modems.sh"
LOG=${1:-$BENCH/captures/asym}
IOD=${2:-240}
N=${3:-5}
MODEM=${MODEM:-supra}

# PRE-FLIGHT ONCE, HERE, BEFORE THE LOOP.  The previous version of this script
# made five calls into an empty line and tabulated the result.  row.sh now
# pre-flights too, but doing it here as well means the run refuses to START
# rather than aborting on call 1 of 5 -- and it resolves the path that
# read_farend needs anyway.
TTY=$(modem_require "$MODEM") || {
	echo "REFUSING to run: no modem behind '$MODEM'." >&2
	echo "  $BENCH/modems.sh list   -- what is attached" >&2
	exit 3
}
echo

# AT&V1 must be read BEFORE the next call's ATZ, which clears it.
read_farend() {
	python3 - "$TTY" <<'PY'
import os, select, sys, termios, time
try:
    fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
except OSError as e:
    print("ERR|ERR|%s" % e)
    raise SystemExit
a = termios.tcgetattr(fd)
a[0] = 0
a[1] = 0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[3] = 0
a[4] = a[5] = termios.B115200
a[6][termios.VMIN] = 0
a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
termios.tcflush(fd, termios.TCIOFLUSH)
os.write(fd, b"AT&V1\r")
out = b""
end = time.time() + 3
while time.time() < end:
    if select.select([fd], [], [], 0.3)[0]:
        try:
            d = os.read(fd, 4096)
        except OSError:
            break
        if d:
            out += d
            end = time.time() + 0.5
os.close(fd)
tx = rx = term = "-"
for line in out.decode(errors="replace").replace("\r", "\n").split("\n"):
    s = line.strip()
    if s.startswith("LAST TX rate"):
        tx = s.split(".")[-1].strip()
    elif s.startswith("LAST RX rate"):
        rx = s.split(".")[-1].strip()
    elif s.startswith("TERMINATION"):
        term = s.split(".")[-1].strip()
print("%s|%s|%s" % (tx, rx, term))
PY
}

# WHICH MODEM.  A result table that does not name its modem is not
# reproducible: this bench has two, on different extensions, and a replug has
# already renumbered them once.
echo "MODEM UNDER TEST: $(modem_provenance "$MODEM")"
echo "IODELAY: $IOD   calls: $N"
echo

printf '%-4s %-9s %-12s %-12s %-14s %-14s %s\n' \
	RUN MODEM 'our RX' 'our TX' 'their TX' 'their RX' TERMINATION
printf '%-4s %-9s %-12s %-12s %-14s %-14s %s\n' \
	---- ----- ------ ------ -------- -------- -----------
for i in $(seq 1 "$N"); do
	L="$LOG-$i"
	SLMODEMD_IODELAY="$IOD" TTY="$TTY" \
	TTY_EXTRA="AT+A8E=1,1;ATS95=47" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		bash "$BENCH/row.sh" "$L" pty "$(modem_ext "$MODEM" 2>/dev/null || echo 1901)" \
		> "$L.run.log" 2>&1

	ourrx=$(grep -a -oE 'pty +CONNECT [0-9]+' "$L.run.log" | head -1 | sed 's/.*CONNECT //')
	ourtx=$(grep -a -oE 'TxRate: *[0-9]+' "$L.run.log" | head -1 | grep -a -oE '[0-9]+$')
	IFS='|' read -r ftx frx term <<< "$(read_farend)"
	printf '%-4s %-9s %-12s %-12s %-14s %-14s %s\n' \
		"$i" "$MODEM" "${ourrx:-no connect}" "${ourtx:--}" \
		"${ftx:--}" "${frx:--}" "${term:--}"
done
