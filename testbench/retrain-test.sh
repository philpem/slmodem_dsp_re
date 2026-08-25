#!/bin/bash
# Task #109.  PAIRED test: the rate at first CONNECT vs the rate after a
# host-forced retrain (+++ ATO1), within the same call.
#
# Each call is its own control -- same path, same modem, seconds apart -- so
# every between-call confounder this bench has been burned by is held constant.
# Finding F1209 predicts the post-retrain rate should beat the pre-retrain one
# if a second Phase 3 pass is what the good calls had.
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
. "$BENCH/modems.sh"
PREFIX=${1:-$BENCH/captures/rt}
N=${2:-8}
echo "=== pre-flight"; TTY=$(modem_require supra) || exit 3
echo; echo "MODEM: $(modem_provenance supra)"
printf '%-5s %-14s %-16s %s\n' RUN 'our RX at connect' 'our RX at hangup' 'our TX at hangup'
printf '%-5s %-14s %-16s %s\n' ----- ----------------- ---------------- ----------------
for i in $(seq 1 "$N"); do
	[ "$i" -gt 1 ] && sleep 6
	L="$PREFIX-$i"
	TTY_RETRAIN=1 SLMODEMD_IODELAY=240 TTY="supra" \
	TTY_EXTRA="AT+A8E=1,1;ATS95=47" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		bash "$BENCH/row.sh" "$L" pty 1901 > "$L.run.log" 2>&1
	ourrx=$(grep -a -oE 'pty +CONNECT [0-9]+' "$L.run.log" | head -1 | sed 's/.*CONNECT //')
	# AT&V1 is the far end's OWN register pair and survives until the next
	# ATZ, so read it before the next call starts.  LAST TX rate is its
	# transmit, i.e. OUR RECEIVE -- the quantity this whole investigation is
	# about, measured at the other end of the link.
	fin=$(python3 - "$TTY" <<'PY2'
import os, select, sys, termios, time
fd=os.open(sys.argv[1], os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
a=termios.tcgetattr(fd); a[0]=a[1]=a[3]=0
a[2]=termios.CS8|termios.CREAD|termios.CLOCAL; a[4]=a[5]=termios.B115200
a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIOFLUSH)
os.write(fd, b"AT&V1\r"); out=b""; end=time.time()+3
while time.time()<end:
    if select.select([fd],[],[],0.3)[0]:
        d=os.read(fd,4096)
        if d: out+=d; end=time.time()+0.5
os.close(fd)
tx=rx="-"
for l in out.decode(errors="replace").replace("\r","\n").split("\n"):
    s=l.strip()
    if s.startswith("LAST TX rate"): tx=s.split(".")[-1].strip().split()[0]
    elif s.startswith("LAST RX rate"): rx=s.split(".")[-1].strip().split()[0]
print("%s|%s"%(tx,rx))
PY2
)
	IFS='|' read -r ftx frx <<< "$fin"
	mapfile -t far < <(grep -a -oE 'tty +CONNECT [0-9]+' "$L.run.log" | sed 's/.*CONNECT //')
	drop=$(grep -a -c 'NO CARRIER' "$L.run.log")
	printf '%-5s %-14s %-16s %s\n' "$i" "${ourrx:-none}" "${ftx:--}" "${frx:--}"
done
