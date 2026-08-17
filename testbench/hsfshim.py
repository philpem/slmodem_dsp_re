#!/usr/bin/env python3
"""hsfshim.py -- put the Conexant HSF datapump on the far end of chanshim.py.

    hsfshim.py <answer|originate>

`hsfcall.sh` runs this; it is not usually run by hand.  It exists because bash
cannot make a socketpair, and one is exactly what is missing from the HSF half
of the rig:

    slmodemd -e chanshim.py            chanshim.py            hsfuser
      (CHAN_ROLE=server)  <-loopback->  (CHAN_ROLE=client) <->  modem ... dmframe
        socketpair made by                  socketpair made HERE
        slmodemd itself

slmodemd makes the socketpair for its own `-e` child (`socket_start`, and there
is no other arrangement -- it is always the parent).  Nothing makes the one on
the HSF side, so this does: `socket.socketpair()`, both ends marked
inheritable, one end handed to `chanshim.py` as its "audio fd" and the other to
`hsfuser modem <audio_fd> <ctl_fd> <role> dmframe` with `pass_fds`, which keeps
the fd NUMBERS unchanged in the child.

WHAT EACH SIDE OF THAT SOCKET SPEAKS.  324-byte slmodemd `socket_frame`s in
both directions -- 4-byte type plus a 320-byte union, 160 samples S16LE at
8 kHz.  `chanshim.py` speaks it because slmodemd does; hsfuser speaks it
because `dmframe` selects it (`src/dmframe.h` documents the same three type
codes in the same order).  HSF's engine runs at 16 kHz and `dmframe.c`
resamples internally, so there is no resampler here and there must not be one.
Between the two shims the wire is different again -- 320 bytes of raw PCM, no
header -- and that is chanshim's business, not ours.

THE CONTROL FD IS A SECOND SOCKETPAIR AND WE RING DOWN IT.  `HSF_NOCTL` was the
obvious choice and it does not work -- MEASURED, twice, and this is the single
thing about the wiring worth knowing:

  * With `HSF_NOCTL=1`, `src/main.c` swaps the answering side's `ATS0=1` for a
    bare `ATA`.  Over 70 s the modem stayed `offhook=0 tx_nz=0 rx_nz=0` and
    never emitted a sample.  The DTE got no result code at all.
  * `ATS1=1` then `ATA` on its pty -- the "AnswerStart needs a ring (or S1 !=
    0)" note in main.c's pair-mode comment -- changed the symptom and not the
    outcome: `NO CARRIER` in about a second, still `offhook=0`.

The answering side needs a RING, and a ring is a TIMED STATE MACHINE rather
than an edge: `CRingDetector::RingDetected` accepts only if elapsed/cycles
falls between 14 and 66 ms, so ring must be asserted repeatedly through the
burst (hsfuser's `docs/ring-detection.md`, and `exchange.c` rings every 24 ms
through a 2 s burst every 6 s).  That is exactly what this file now does, on
its own socketpair, in HSF's own newline-delimited control format
(`src/link.h`: `RING`, `OFFHOOK`, `ONHOOK`) -- the mechanism, not a workaround
for it, and the same one `exchange.c` uses to reach `CONNECT 33600`.

NONE OF THIS TOUCHES slmodemd's SIP SOCKET, which stays exactly as unused as
`chanshim.py` leaves it.  The two control channels are different formats --
slmodemd's `SR`/`SH`/`MD` ride 324-byte SIP_INFO datagrams -- and joining them
would feed each side noise.  This one runs between hsfshim.py and hsfuser and
carries line state only.

WHICH IS WHY HSF ANSWERS AND WE ORIGINATE.  HSF's ORIGINATE path wants DIAL
TONE before it will dial: `DialerDialString` arms a DialtoneWaitTime timeout
unconditionally and `ATX3` does not disable it (X only gates which result codes
are displayed -- `pair.c`'s own comment, and measured here: `NO DIALTONE` 4.7 s
after `dialling`, then back on-hook for the rest of the run).  `exchange.c`
generates 350+440 Hz to satisfy it; a channel model carrying slmodemd's silence
does not, and injecting call progress into the audio path is not this rig's
job.  Our side dials instead, which is the bench's arrangement anyway.

WHY IT RELAYS THE CHILDREN'S OUTPUT INSTEAD OF LETTING IT INHERIT.  Two
reasons, and the second is the one that matters.  First, the two streams are
tagged, so `chanshim: ...` and `modem[answer] ...` are told apart in one file.
Second, every line is stamped with seconds since launch, which is where the
TRAINING TIME comes from: HSF prints `modem[<role>] >> ATA` / `dialling` when
it commits and `CONNECT <rate>` when it trains, and the difference between
those two stamps is the number.  Nothing else in the rig has a clock --
slmodemd's -d9 log is unstamped and replaydte.py echoes the pty verbatim.

TEARDOWN IS ORDERED, and that is a measurement decision.  On SIGTERM the shim
is stopped FIRST; hsfuser then sees EOF on the audio fd, leaves its loop, and
prints `exiting after N blocks (M rings, frames in X / out Y)`.  Killing the
group flat loses that line, which is the only place HSF's own frame counters
are reported -- the same mistake chanshim.py's docstring records for CHANLAT.
"""

import os
import re
import select
import signal
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HSF_ROOT = os.environ.get("HSF_ROOT",
                          "/home/philpem/dev/softmodems/conexant/hsfuser")

_stop = False

STAMP = re.compile(r"^\[\s*([0-9.]+)\]\s+(\S+)\s(.*)$")


def summarise(path):
    """Turn the stamped log into `KEY=value` lines for hsfcall.sh to eval.

    EVERY NUMBER HERE HAS A DENOMINATOR AND THE DENOMINATOR IS PRINTED, which
    is finding 2400's rule: a tool that reports nothing is indistinguishable
    from a broken one.  `SUMMARY_LINES` is how many stamped lines were parsed,
    so a zero says "the log was empty" and not "the call did nothing".

    The one derived quantity worth its own paragraph is HSF_RTRATIO -- HSF's
    own audio seconds (blocks x 8 ms) over the wall seconds between its loop
    starting and its last report.  HSF's timers run off the wall clock and its
    audio off its sample count, so a machine that cannot keep up expires V.34
    timeouts on a call that never stalled.  Below ~0.95 a no-connect is VOID,
    not a result.
    """
    out, nlines = {}, 0
    loop_t = commit_t = connect_t = last_t = None
    try:
        text = open(path, "r", errors="replace").read().splitlines()
    except OSError as e:
        print("SUMMARY_LINES=0  # %s" % e)
        return 1
    for raw in text:
        m = STAMP.match(raw)
        if not m:
            continue
        nlines += 1
        t, tag, line = float(m.group(1)), m.group(2), m.group(3)
        if tag != "hsf":
            m2 = re.search(r"chanshim: (\d+) frames, ([\d.]+) s of audio", line)
            if m2:
                out["CHAN_FRAMES"], out["CHAN_AUDIO_S"] = m2.group(1), m2.group(2)
            continue
        if loop_t is None and re.search(r"modem\[\w+\] pid", line):
            loop_t = t
        # ATA / ATD is where HSF commits to the call; CONNECT is where it trains.
        if commit_t is None and (">> ATA" in line or "dialling" in line
                                 or ">> ATD" in line):
            commit_t = t
        m2 = re.search(r"CONNECT[ /]*(\d+)", line)
        if m2 and "HSF_CONNECT" not in out:
            out["HSF_CONNECT"], connect_t = m2.group(1), t
        m2 = re.search(r"\+MRR: *(\d+), *(\d+)", line)
        if m2:
            out["HSF_MRR_RX"], out["HSF_MRR_TX"] = m2.group(1), m2.group(2)
        m2 = re.search(r"\+MCR: *(\w+)", line)
        if m2:
            out["HSF_MCR"] = m2.group(1)
        m2 = (re.search(r"exiting after (\d+) blocks", line)
              or re.search(r"blocks=(\d+)", line))
        if m2:
            out["HSF_BLOCKS"], last_t = m2.group(1), t
        m2 = re.search(r"frames in (\d+) / out (\d+)", line)
        if m2:
            out["HSF_FRAMES_IN"], out["HSF_FRAMES_OUT"] = m2.group(1), m2.group(2)
    for raw in text:                    # the rig's own lines, tagged "rig"
        m = STAMP.match(raw)
        if not m or m.group(2) != "rig":
            continue
        m2 = re.search(r"rings sent (\d+), HSF offhook=(\S+)", m.group(3))
        if m2:
            out["HSF_RINGS"], out["HSF_OFFHOOK"] = m2.group(1), m2.group(2)
        #
        # The ANSWERING side has no `>> ATA` to time from -- it is armed with
        # ATS0=1 and commits when it lifts.  Off-hook is that instant, and it
        # is HSF's own report rather than ours.
        #
        if commit_t is None and "went OFF-HOOK" in m.group(3):
            commit_t = float(m.group(1))
    if connect_t is not None and commit_t is not None:
        out["HSF_TRAIN"] = "%.1f" % (connect_t - commit_t)
    if last_t is not None and loop_t is not None and "HSF_BLOCKS" in out:
        audio = int(out["HSF_BLOCKS"]) * 0.008
        wall = last_t - loop_t
        out["HSF_AUDIO_S"] = "%.1f" % audio
        out["HSF_WALL_S"] = "%.1f" % wall
        if wall > 0:
            out["HSF_RTRATIO"] = "%.2fx" % (audio / wall)
    out["SUMMARY_LINES"] = str(nlines)
    for k in sorted(out):
        print("%s=%s" % (k, out[k]))
    return 0


def _sigterm(_sig, _frm):
    global _stop
    _stop = True


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--summary":
        return summarise(sys.argv[2])
    role = (sys.argv[1] if len(sys.argv) > 1
            else os.environ.get("HSF_ROLE", "answer"))
    if role not in ("answer", "originate"):
        sys.exit("hsfshim: role must be answer or originate, got %r" % role)
    hsf_bin = os.path.join(HSF_ROOT, "build", "hsfuser")
    shim = os.path.join(HERE, "chanshim.py")
    for p in (hsf_bin, shim):
        if not os.access(p, os.X_OK):
            sys.exit("hsfshim: %s is not executable" % p)

    t0 = time.monotonic()

    def say(tag, text):
        sys.stdout.write("[%8.3f] %-4s %s\n" % (time.monotonic() - t0, tag,
                                                text))
        sys.stdout.flush()

    a, b = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    fd_chan, fd_hsf = a.fileno(), b.fileno()
    # HSF's control channel: our end stays here, its end goes to the child.
    c_us, c_them = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    fd_ctl = c_them.fileno()
    for fd in (fd_chan, fd_hsf, fd_ctl):
        os.set_inheritable(fd, True)
    null = os.open("/dev/null", os.O_RDWR)
    os.set_inheritable(null, True)

    say("rig", "hsfuser %s   role=%s   audio fd %d <-> shim fd %d, ctl fd %d"
        % (hsf_bin, role, fd_hsf, fd_chan, fd_ctl))
    if role == "originate":
        say("rig", "WARNING: HSF originating needs DIAL TONE this rig does not "
                   "present; expect NO DIALTONE ~5 s after `dialling`.")

    env = dict(os.environ)
    env.pop("HSF_NOCTL", None)          # we speak its control format; see above
    #
    # chanshim.py wants `<dial> <audio-fd> <sip-fd>` and reads argv[-2] for the
    # audio fd -- NOT argv[-1]; the fork passes a call-info socket last.  The
    # dial string is ignored by it exactly as it is by replay.py, which is what
    # makes this rig structurally incapable of placing a call.
    #
    p_chan = subprocess.Popen(
        [sys.executable, shim, "", str(fd_chan), str(null)],
        pass_fds=(fd_chan, null), stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, env=env)
    #
    # SIGPIPE MUST BE IGNORED IN HSF, or the ordered teardown below buys
    # nothing.  `transport()` writes its frame BEFORE it reads, so the first
    # tick after the shim exits kills hsfuser with SIGPIPE (rc=-13, measured)
    # and its `exiting after N blocks (frames in X / out Y)` line -- the only
    # place its frame counters appear -- is never printed.  Ignored, the same
    # write returns EPIPE, `link_write_all` returns -1, `g_dead` is set, and
    # the loop ends the way it was written to.  A disposition of SIG_IGN
    # survives execve (a HANDLER would not), which is why this works on a
    # binary we do not build.
    #
    p_hsf = subprocess.Popen(
        [hsf_bin, "modem", str(fd_hsf), str(fd_ctl), role, "dmframe"],
        pass_fds=(fd_hsf, fd_ctl), stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, env=env,
        preexec_fn=lambda: signal.signal(signal.SIGPIPE, signal.SIG_IGN))

    # The parent must not keep either audio end open, or EOF never reaches HSF.
    a.close()
    b.close()
    c_them.close()
    os.close(null)
    c_us.setblocking(False)

    signal.signal(signal.SIGTERM, _sigterm)
    signal.signal(signal.SIGINT, _sigterm)

    tags = {p_chan.stdout.fileno(): "chan", p_hsf.stdout.fileno(): "hsf",
            c_us.fileno(): "ctl"}
    bufs = {fd: b"" for fd in tags}
    live = set(tags)
    stopping = False
    #
    # RING CADENCE, copied from exchange.c rather than invented: one RING every
    # 24 ms through a 2 s burst, repeating every 6 s.  The 24 ms is the load-
    # bearing number -- CRingDetector measures elapsed/cycles and rejects
    # anything outside 14..66 ms, so one event per 8 ms tick is BELOW the floor
    # and no cadence can rescue it.  Absolute deadlines, so a slow iteration
    # cannot bunch two rings into one tick and halve the interval.
    #
    RING_EVERY, RING_ON, RING_PERIOD = 0.024, 2.0, 6.0
    ringing = False             # armed once HSF has issued its own ATS0=1
    rings = 0
    t_ring0 = next_ring = None
    hsf_offhook = None

    while live:
        if _stop and not stopping:
            stopping = True
            say("rig", "SIGTERM: stopping the shim first so HSF sees EOF "
                       "and prints its counters")
            p_chan.terminate()
        #
        # Ring only while HSF is on-hook.  `offhook` is HSF's own report on the
        # control channel, so this stops on the modem's state and not on a
        # guess about how long answering takes.
        #
        if ringing and not stopping and hsf_offhook != 1:
            now = time.monotonic()
            while next_ring is not None and now >= next_ring:
                if (next_ring - t_ring0) % RING_PERIOD < RING_ON:
                    try:
                        c_us.sendall(b"RING\n")
                        rings += 1
                        if rings <= 3 or rings % 250 == 0:
                            say("rig", "ring #%d" % rings)
                    except OSError as e:
                        say("rig", "ring send failed: %s" % e)
                        ringing = False
                next_ring += RING_EVERY
        r, _, _ = select.select(list(live), [], [],
                                0.005 if ringing else 0.25)
        for fd in r:
            try:
                d = os.read(fd, 65536)
            except BlockingIOError:
                continue
            except OSError:
                d = b""
            if not d:
                if bufs[fd]:
                    say(tags[fd], bufs[fd].decode("latin-1"))
                    bufs[fd] = b""
                live.discard(fd)
                say("rig", "%s stream closed" % tags[fd])
                continue
            bufs[fd] += d
            while b"\n" in bufs[fd]:
                raw, bufs[fd] = bufs[fd].split(b"\n", 1)
                line = raw.decode("latin-1").rstrip("\r")
                say(tags[fd], line)
                if tags[fd] == "ctl":
                    #
                    # HSF reports its hook transitions here.  OFFHOOK is what
                    # stops the ringing, and it is also the ONLY positive
                    # evidence that the answer actually took: `>> ATS0=1` says
                    # we asked, `offhook` says it happened.
                    #
                    if line == "OFFHOOK":
                        hsf_offhook = 1
                        say("rig", "HSF went OFF-HOOK after %d rings" % rings)
                    elif line == "ONHOOK":
                        hsf_offhook = 0
                elif tags[fd] == "hsf" and not ringing and role == "answer" \
                        and ">> ATS0=1" in line:
                    #
                    # ARM ONLY AFTER ATS0=1.  Rings sent before auto-answer is
                    # configured are counted by the detector and answered by
                    # nothing; the modem issues its init one command per 25
                    # blocks, so ATS0=1 lands ~1.4 s into its loop.
                    #
                    ringing = True
                    t_ring0 = next_ring = time.monotonic()
                    say("rig", "ATS0=1 seen: ringing (24 ms cycles, 2 s in 6)")
        #
        # If HSF dies on its own -- a bad exec, a failed module init -- the shim
        # would sit blocked on a read for ever and the slmodemd side would hang
        # rather than report.  Stop it too, so the run ends and says so.
        #
        if p_hsf.poll() is not None and not stopping:
            stopping = True
            say("rig", "hsfuser exited rc=%s; stopping the shim"
                % p_hsf.returncode)
            p_chan.terminate()

    say("rig", "rings sent %d, HSF offhook=%s" % (rings, hsf_offhook))
    for p, name in ((p_hsf, "hsfuser"), (p_chan, "chanshim")):
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
        say("rig", "%s rc=%s" % (name, p.returncode))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
