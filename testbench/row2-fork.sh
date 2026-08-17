#!/bin/bash
#
# MATRIX ROW 2 -- d-modem + BLOB originating, SupraExpress 56e PRO answering.
#
# TEARDOWN IS BY PID, NEVER BY PATTERN.  `pkill -f d-modem` matches the path
# /home/philpem/dev/sip-D-modem and will kill unrelated work in this tree --
# it took out an agent once.  slmodemd is started in its own process group and
# the group is signalled; nothing else is touched.
#
set -u
ROOT=/home/philpem/dev/sip-D-modem/d-modem
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
. "$BENCH/modems.sh"
# Superseded by row.sh (`row.sh <log> pty 1901` does this), but kept runnable.
# It used to hard-code /dev/ttyUSB1, which after the 11 August replug is the
# COURIER -- so it would have driven the wrong modem while every line of its
# own output said SupraExpress.
TTY=$(modem_require "${MODEM:-supra}") || exit 3
LOG=${1:-/tmp/row2}
S0_ORIG=""
SLPGID=""

mdm() {
	python3 - "$TTY" "$1" <<'PY'
import os,sys,termios,time,select
fd=os.open(sys.argv[1], os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
a=termios.tcgetattr(fd); a[0]=0; a[1]=0
a[2]=termios.CS8|termios.CREAD|termios.CLOCAL; a[3]=0
a[4]=termios.B115200; a[5]=termios.B115200
a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
termios.tcsetattr(fd,termios.TCSANOW,a); termios.tcflush(fd,termios.TCIOFLUSH)
os.write(fd,(sys.argv[2]+"\r").encode()); time.sleep(1.0)
out=b""
while select.select([fd],[],[],0.5)[0]:
    try: c=os.read(fd,512)
    except: break
    if not c: break
    out+=c
print(out.decode(errors="replace").strip()); os.close(fd)
PY
}

cleanup() {
	echo "--- teardown"
	mdm "ATH" >/dev/null 2>&1 || true
	[ -n "$S0_ORIG" ] && mdm "ATS0=$S0_ORIG" >/dev/null 2>&1 || true
	if [ -n "$SLPGID" ]; then
		kill -TERM -"$SLPGID" 2>/dev/null || true
		sleep 2
		kill -KILL -"$SLPGID" 2>/dev/null || true
		echo "--- signalled process group $SLPGID only"
	fi
	echo "--- S0 restored to '${S0_ORIG:-?}'"
}
trap cleanup EXIT INT TERM

echo "=== SupraExpress"
mdm "ATI3" | sed -n '2,3p'
S0_ORIG=$(mdm "ATS0?" | sed -n '2p' | tr -dc '0-9')
echo "  S0 was '$S0_ORIG' -- arming auto-answer"
mdm "ATS0=1" >/dev/null; mdm "ATW2" >/dev/null

echo "=== slmodemd (blob) + guarded exec, own process group"
setsid "$ROOT/slmodemd/slmodemd" -d9 -e "/home/philpem/dev/sip-D-modem/claude_re/testbench/dmodem-guard-fork.sh" \
	> "$LOG.slmodemd.log" 2>&1 &
SLPID=$!
sleep 4
SLPGID=$(ps -o pgid= -p $SLPID 2>/dev/null | tr -d ' ')
PTY=$(grep -oE '/dev/pts/[0-9]+' "$LOG.slmodemd.log" | head -1)
echo "  pid $SLPID pgid $SLPGID pty ${PTY:-NONE}"
[ -z "$PTY" ] && { tail -8 "$LOG.slmodemd.log"; exit 1; }

echo "=== dialling 1901 (blob originates)"
python3 - "$PTY" "$LOG" <<'PY'
import os,sys,termios,time,select
pty,log=sys.argv[1],sys.argv[2]
fd=os.open(pty,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
a=termios.tcgetattr(fd); a[0]=0; a[1]=0
a[2]=termios.CS8|termios.CREAD|termios.CLOCAL; a[3]=0
a[4]=termios.B115200; a[5]=termios.B115200
a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
termios.tcsetattr(fd,termios.TCSANOW,a); termios.tcflush(fd,termios.TCIOFLUSH)
def rd(sec):
    end=time.time()+sec; out=b""
    while time.time()<end:
        if select.select([fd],[],[],0.3)[0]:
            try: c=os.read(fd,1024)
            except: break
            if c: out+=c
    return out.decode(errors="replace")
os.write(fd,b"ATZ\r");  rd(2)
os.write(fd,b"ATW2\r"); rd(2)
os.write(fd,b"ATDT1901\r")
res=rd(90)
open(log+".dte.log","w").write(res)
for l in res.splitlines():
    if l.strip(): print("   DTE:",l.strip()[:110])
if "CONNECT" in res:
    print("   *** CONNECTED")
    os.write(fd,b"The quick brown fox 0123456789\r")
    print("   loopback:",repr(rd(8)[:160]))
    time.sleep(0.5); os.write(fd,b"+++"); time.sleep(1.5); os.write(fd,b"ATH\r"); rd(3)
else:
    print("   *** NO CONNECT")
os.close(fd)
PY
# Read whatever the SupraExpress printed (RING, CONNECT <rate>, NO CARRIER).
# This MUST run before cleanup, whose ATH does a TCIOFLUSH and destroys it.
echo "=== SupraExpress said"
python3 - "$TTY" "$LOG" <<'PY'
import os,sys,select,time
try:
    fd=os.open(sys.argv[1], os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
except OSError as e:
    print("   (cannot open:",e,")"); raise SystemExit
out=b""; end=time.time()+2
while time.time()<end:
    if select.select([fd],[],[],0.3)[0]:
        try: c=os.read(fd,1024)
        except OSError: break
        if c: out+=c
os.close(fd)
open(sys.argv[2]+".supra.log","wb").write(out)
t=out.decode(errors="replace").strip()
print("  ", t.replace("\r","|").replace("\n","|") if t else "(nothing buffered)")
PY

echo "=== slmodemd, interesting lines"
grep -iE 'connect|carrier|rate|dp_|v8|v34|b103|error|fail|hangup' "$LOG.slmodemd.log" | tail -20
