#!/usr/bin/env python3
"""relay.py -- a SIP back-to-back user agent, so two hardware modems can talk
over the path our calls actually take.

    relay.py --dial 1902                # answer an inbound call, bridge to 1902
    relay.py --dial 1902 --hold 90      # give the handshake 90 s

WHY THIS EXISTS.  `hw2hw.sh` was supposed to be the control experiment -- two
real modems, no slmodemd, no blob -- and finding 1467 showed it never left the
VG204.  Both modems hang off the same gateway and 1902 matches `dial-peer
voice 2 pots` (port 0/1), far more specific than the `.T` voip peer that
reaches Asterisk, so the Cisco bridges port 0/0 to port 0/1 internally.  No
RTP, no jitter buffer, no codec round trip.  A result there is about the ATA's
analogue stages and nothing else.

The only way to force the media out of the gateway and back is a third SIP
endpoint.  A modem dials OUR registration, we answer, we place a second call
to the other modem, and the two legs are bridged in our conference.  Now the
audio really does traverse Asterisk twice and RTP four times.

WHICH MAKES THIS A ONE-SIDED TEST, and it has to be read that way.  The relay
path is HARDER than the one slmodemd uses -- two RTP leg pairs instead of one,
plus this process's own jitter buffer and conference bridge.  So:

    two modems reach their ceiling here  ->  the path is clean, conclusive
    two modems show the same deficit     ->  ambiguous; could be the extra hop

Only the first outcome settles finding 1466.  Say so in whatever it produces.

THE MEDIA SETTINGS ARE NOT OPTIONAL.  A relay with VAD or an echo canceller in
it would destroy the very signals it is carrying, and would do so silently --
the calls would connect and the rates would be wrong.  These four are copied
from d-modem.c, which is the configuration this bench already knows carries a
V.34 handshake:

    no_vad          VAD would gate the low-energy parts of a handshake
    ec_tail_len 0   an echo canceller subtracts what it thinks is echo, and a
                    modem's own signal correlates with itself
    null audio      no sound card anywhere near this
    PCMA only       the VG204's ports are `compand-type a-law`; anything else
                    means a transcode

CREDENTIALS.  Written to a mode-600 config file rather than passed as
`--password=`, which would put the secret in argv where `ps` shows it to every
user on the box for the life of the call.  The file is removed on the way out,
including on a signal.

THE DIAL GUARD IS HERE because this is the process that turns a number into a
call, and the PBX behind it reaches the PSTN.  Same list as row.sh and
dmodem-guard.sh.  Do not make ALLOWED an argument or an environment lookup.
"""

import argparse
import atexit
import os
import re
import signal
import subprocess
import sys
import tempfile
import time

ALLOWED = ("1901", "1902", "1903", "4242")   # 1903 = Oli'Net, added 2026-08-13

ROOT = "/home/philpem/dev/sip-D-modem"
# WAS pjproject-2.11.1, which belonged to the older D-Modem fork and went when
# the two were consolidated into d-modem/.  This is 2.15.1's pjsua, the only
# one left in the tree.  UNTESTED against this script: relay.py runs only when
# relaycall.sh is asked for RELAY=pjsua, and rtprelay.py is the default, so no
# call taken since the consolidation has exercised it.  If the pjsua leg
# misbehaves, the version change is the first thing to suspect.
PJSUA = ROOT + "/d-modem/pjproject-2.15.1/pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu"
SECRET = ROOT + "/asterisk-login-4242.secret"


def read_secret():
    """Server, Extension, Secret -- read at point of use, never printed."""
    out = {}
    with open(SECRET) as f:
        for line in f:
            if ":" in line:
                k, v = line.split(":", 1)
                out[k.strip()] = v.strip()
    for k in ("Server", "Extension", "Secret"):
        if not out.get(k):
            sys.exit("relay: %s has no '%s' field" % (SECRET, k))
    return out["Server"], out["Extension"], out["Secret"]


def write_config(server, ext, secret, local_port, rtp_port, rec=None):
    fd, path = tempfile.mkstemp(prefix="relay-", suffix=".cfg",
                                dir=os.environ.get("CLAUDE_JOB_DIR", "/tmp"))
    os.fchmod(fd, 0o600)
    # One option per line, exactly as they would appear on the command line.
    # Everything that touches the audio is here rather than in argv so that the
    # whole media configuration is visible in one place when something is wrong.
    cfg = [
        "--registrar=sip:%s" % server,
        "--id=sip:%s@%s" % (ext, server),
        "--realm=*",
        "--username=%s" % ext,
        "--password=%s" % secret,
        "--local-port=%d" % local_port,
        "--rtp-port=%d" % rtp_port,
        "--auto-answer=200",
        "--auto-conf",              # bridge every call to every other
        "--max-calls=4",
        "--null-audio",
        "--no-tones",
        "--no-vad",
        "--ec-tail=0",
        "--clock-rate=8000",
        "--snd-clock-rate=8000",
        "--quality=10",
        "--dis-codec=speex",
        "--dis-codec=iLBC",
        "--dis-codec=GSM",
        "--dis-codec=G722",
        "--dis-codec=G7221",
        "--dis-codec=L16",
        "--dis-codec=opus",
        "--dis-codec=PCMU",         # the VG204's ports are a-law
        "--add-codec=PCMA",
    ]
    if rec:
        cfg += ["--rec-file=%s" % rec, "--auto-rec"]
    os.write(fd, ("\n".join(cfg) + "\n").encode())
    os.close(fd)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dial", required=True,
                    help="number to call once an inbound call arrives")
    ap.add_argument("--hold", type=float, default=90.0,
                    help="seconds to keep the bridge up (default 90)")
    ap.add_argument("--wait-inbound", type=float, default=60.0,
                    help="seconds to wait for the first call (default 60)")
    ap.add_argument("--local-port", type=int, default=5080)
    ap.add_argument("--rtp-port", type=int, default=4100)
    ap.add_argument("--log", help="write pjsua's output here as well")
    ap.add_argument("--rec", metavar="WAV",
                    help="record the conference mix -- the only way to tell "
                         "'no audio arrived' from 'audio arrived and the "
                         "modems still could not hear each other'")
    args = ap.parse_args()

    if args.dial not in ALLOWED:
        sys.exit("relay: REFUSING to dial '%s'; allowed: %s"
                 % (args.dial, " ".join(ALLOWED)))

    server, ext, secret = read_secret()
    cfg = write_config(server, ext, secret, args.local_port, args.rtp_port,
                       args.rec)
    del secret

    def cleanup():
        try:
            os.unlink(cfg)
        except OSError:
            pass
    atexit.register(cleanup)
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: sys.exit(130))

    print("relay: registering as %s@%s, will bridge to %s" % (ext, server, args.dial))
    p = subprocess.Popen([PJSUA, "--config-file", cfg],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, bufsize=1)

    logf = open(args.log, "w") if args.log else None
    t0 = time.time()
    registered = False
    inbound = None
    outbound_sent = False
    confirmed = set()
    bridge_at = None

    def say(msg):
        print("relay: %7.2f %s" % (time.time() - t0, msg), flush=True)

    def cmd(s):
        p.stdin.write(s + "\n")
        p.stdin.flush()

    try:
        for line in p.stdout:
            if logf:
                logf.write(line)
            # Registration.  Anything else is pointless until this succeeds,
            # and a silent failure here looks exactly like "nobody called".
            # THE STRINGS PJSUA ACTUALLY EMITS.  The first version of this
            # watched for "Incoming call for account" and "Call N state changed
            # to CONFIRMED", neither of which this build logs -- so a call that
            # arrived, was answered, and negotiated PCMA perfectly went
            # entirely unnoticed and the second leg was never placed.  These
            # three are taken from a real log, not from the documentation.
            if not registered and "registration success" in line:
                registered = True
                say("REGISTERED")
            m = re.search(r"Call (\d+) media \d+ \[type=audio\], status is Active",
                          line)
            if m:
                call = int(m.group(1))
                if call not in confirmed:
                    confirmed.add(call)
                    say("leg %d has live media" % call)
                    if len(confirmed) == 2 and bridge_at is None:
                        bridge_at = time.time()
                        say("BOTH LEGS UP -- handshake begins")
            # Conference wiring, straight from pjsua's own conference bridge.
            # Worth printing: if the two legs are never wired TO EACH OTHER the
            # call still connects and the modems hear silence, which looks
            # exactly like a path that cannot carry V.34.
            m = re.search(r"Port (\d+) \(([^)]*)\) transmitting to port (\d+) \(([^)]*)\)",
                          line)
            if m and "sound" not in m.group(2) and "sound" not in m.group(4):
                say("bridged: %s -> %s" % (m.group(2), m.group(4)))
            if re.search(r"Request msg BYE", line) and bridge_at is not None:
                say("a leg sent BYE after %.2f s of bridge"
                    % (time.time() - bridge_at))
                break

            # Place the second leg the moment the first INVITE lands.  We
            # auto-answer, so the originating modem starts its V.8 calling
            # tones immediately and every second before the far end picks up is
            # a second of that burnt against its S7 carrier timer.
            if re.search(r"Incoming Request msg INVITE", line) and inbound is None:
                inbound = time.time()
                say("INBOUND CALL -- answering, dialling %s now" % args.dial)
            if inbound and not outbound_sent:
                outbound_sent = True
                cmd("m")
                cmd("sip:%s@%s" % (args.dial, server))
                say("dialled %s" % args.dial)

            if bridge_at and time.time() - bridge_at > args.hold:
                say("hold expired -- hanging up")
                break
            if not inbound and time.time() - t0 > args.wait_inbound:
                say("no inbound call in %.0f s -- giving up" % args.wait_inbound)
                say("  is anything registered as %s?  did the modem dial it?" % ext)
                break
    finally:
        # `dq` dumps per-call media statistics -- packets and bytes sent and
        # received on each leg.  It is the only way to tell "the bridge was
        # wired but no audio ever arrived" from "audio arrived and the modems
        # still could not hear each other", and those have completely
        # different causes.  Ask for it BEFORE hanging up: after `ha` the
        # streams are gone and there is nothing left to dump.
        try:
            cmd("dq")
            time.sleep(1.5)
        except Exception:
            pass
        try:
            cmd("ha")
            time.sleep(0.5)
            cmd("q")
            p.wait(timeout=5)
        except Exception:
            p.kill()
        if logf:
            logf.close()
        cleanup()

    if bridge_at:
        say("bridge held %.2f s" % (time.time() - bridge_at))
        return 0
    say("NO BRIDGE -- the two legs were never both up")
    return 1


if __name__ == "__main__":
    sys.exit(main())
