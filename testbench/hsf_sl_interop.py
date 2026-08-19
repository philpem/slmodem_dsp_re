#!/usr/bin/env python3
"""Byte-verified V.34 call between a Conexant HSF and SmartLink endpoint.

The SmartLink parent launches the HSF process through slmodemd-shim.sh.  This
keeps both applications at their normal DTE/PTYS and makes a mixed-carrier
test reproducible without a physical line.
"""
import os
import random
import re
import select
import signal
import hashlib
import subprocess
import sys
import tempfile
import time


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SL = os.path.join(ROOT, "build", "hybrid-fit", "slmodemd-fit")
HSF = os.environ.get("HSFUSER", "/home/philpem/dev/softmodems/conexant/hsfuser/build/hsfuser")
SHIM = os.environ.get("HSF_SLMODEMD_SHIM",
                      "/home/philpem/dev/softmodems/conexant/hsfuser/tools/slmodemd-shim.sh")


def drain(fd, timeout=0.1):
    chunks = []
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        ready, _, _ = select.select([fd], [], [], max(0, end - time.monotonic()))
        if not ready:
            break
        try:
            b = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not b:
            break
        chunks.append(b)
    return b"".join(chunks)


def wait_for(fd, token, timeout):
    got = b""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        got += drain(fd, 0.2)
        if token in got:
            return True, got
    return False, got


def random_payload(seed, nbytes):
    # The PTYs are an emulation of a serial terminal, not an octet-transparent
    # wire protocol.  Keep stress data printable (and omit '+') so terminal
    # control characters and the Hayes escape are not mistaken for a carrier
    # failure.  A uniform 94-symbol stream is still not V.42bis-friendly.
    alphabet = bytes(x for x in range(0x20, 0x7f) if x != ord("+"))
    rng = random.Random(seed)
    return bytes(alphabet[rng.randrange(len(alphabet))] for _ in range(nbytes))


def main():
    run = tempfile.mkdtemp(prefix="hsf-sl-interop.", dir="/tmp")
    os.makedirs(os.path.join(run, ".config", "slmodem"))
    env = os.environ.copy()
    env.update({"HOME": run, "HSF_ROLE": "originate", "HSFUSER": HSF})
    log = open(os.path.join(run, "sl.log"), "wb")
    p = subprocess.Popen([SL, "-d9", "-n", "-e", SHIM, "/dev/slamr8"],
                         stdout=log, stderr=subprocess.STDOUT, env=env,
                         start_new_session=True)
    try:
        pty = hsf_pty = None
        end = time.monotonic() + 20
        logname = os.path.join(run, "sl.log")
        while time.monotonic() < end and (not pty or not hsf_pty):
            with open(logname, "rb") as f:
                text = f.read()
            m = re.search(rb"TTY is `(/dev/pts/\d+)'", text)
            hm = re.search(rb"modem\[originate\].*-> (/dev/pts/\d+)", text)
            if m:
                pty = m.group(1).decode()
            if hm:
                hsf_pty = hm.group(1).decode()
            time.sleep(.1)
        if not pty or not hsf_pty:
            print("FAIL: endpoint PTYs not created", file=sys.stderr)
            return 1
        fd = os.open(pty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        hfd = os.open(hsf_pty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            # The HSF originator places the call automatically.  Answer only
            # after it has sent SR and the SmartLink DTE has received RING.
            ok, pre = wait_for(fd, b"RING", 15)
            if not ok:
                print("FAIL: no RING", pre[-200:], file=sys.stderr)
                return 1
            for cmd in (b"ATZ\r", b"AT+MS=34,1\r", b"ATA\r"):
                os.write(fd, cmd)
                time.sleep(.4)
            ok, result = wait_for(fd, b"CONNECT", 45)
            if not ok:
                print("FAIL: no CONNECT", result[-400:], file=sys.stderr)
                return 1
            nbytes = int(os.environ.get("HSF_SL_BULK", "128"))
            if nbytes < 1:
                raise ValueError("HSF_SL_BULK must be positive")
            # Do not measure V.42bis.  Distinct deterministic random streams
            # make a loopback and a one-way failure visible too.
            # Exclude '+' so a quiet tail can never accidentally satisfy the
            # Hayes escape sequence while exercising the data path.
            to_hsf = random_payload(0x534c0001, nbytes)
            to_sl = random_payload(0x48534602, nbytes)
            drain(fd, .5)
            drain(hfd, .5)
            got_sl = got_hsf = b""
            sent_sl = sent_hsf = 0
            # 128 bytes is a smoke test.  HSF_SL_BULK=32768 makes this a
            # sustained two-way soak at 33.6 kbit/s and allows
            # time for retrain/renegotiation behaviour to surface.
            end = time.monotonic() + max(30, nbytes * 16 / 33600 + 60)
            while time.monotonic() < end and (len(got_sl) < nbytes or len(got_hsf) < nbytes):
                if sent_sl < nbytes:
                    try:
                        sent_sl += os.write(fd, to_hsf[sent_sl:sent_sl + 4096])
                    except BlockingIOError:
                        pass
                if sent_hsf < nbytes:
                    try:
                        sent_hsf += os.write(hfd, to_sl[sent_hsf:sent_hsf + 4096])
                    except BlockingIOError:
                        pass
                got_sl += drain(fd, .2)
                got_hsf += drain(hfd, .2)
            if got_hsf[:nbytes] != to_hsf:
                print("FAIL: SmartLink -> HSF payload mismatch "
                      f"({len(got_hsf)}/{nbytes}, got "
                      f"{hashlib.sha256(got_hsf[:nbytes]).hexdigest()[:16]}, want "
                      f"{hashlib.sha256(to_hsf).hexdigest()[:16]})", file=sys.stderr)
                return 1
            if got_sl[:nbytes] != to_sl:
                print("FAIL: HSF -> SmartLink payload mismatch "
                      f"({len(got_sl)}/{nbytes}, got "
                      f"{hashlib.sha256(got_sl[:nbytes]).hexdigest()[:16]}, want "
                      f"{hashlib.sha256(to_sl).hexdigest()[:16]})", file=sys.stderr)
                return 1
            print("PASS: mixed HSF-originated / SmartLink-answered V.34 CONNECT; "
                  f"{nbytes} bytes verified in each direction", flush=True)
            # A real terminal program normally has a quiet interval before
            # its +++/ATH escape.  Hold one here so the test does not blur
            # tail delivery into intentional teardown.
            time.sleep(float(os.environ.get("HSF_SL_TAIL_GUARD", "1.2")))
            print("log:", logname)
            return 0
        finally:
            os.close(fd)
            os.close(hfd)
    finally:
        log.close()
        os.killpg(p.pid, signal.SIGTERM)
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()


if __name__ == "__main__":
    sys.exit(main())
