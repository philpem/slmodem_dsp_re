#!/usr/bin/env python3
"""rtprelay.py -- a SIP back-to-back UA that FORWARDS RTP packets untouched.

    rtprelay.py --dial 1901
    rtprelay.py --dial 1901 --hold 90 --rec captures/relay

WHY NOT THE pjsua RELAY.  `relay.py` bridges two calls in pjsua's conference,
which DECODES both legs to linear, mixes them on the local clock, and
re-encodes.  That means two independent RTP senders, each with its own crystal,
have to be reconciled to one local clock -- and the reconciliation is frame
drops and insertions.  Speech survives that; a V.34 equaliser does not.

Measured, three calls and a control:

    supra -> courier over the pjsua relay   V.8 CM into a 2250 Hz tone, no
                                            common modulation, NO CARRIER
    courier -> supra over the pjsua relay   V.8 completes, V.34 Phase 2
                                            probing runs, training retries
                                            four times and gives up
    the same pairing, hairpinned in the     CONNECT 28800/ARQ/V34/LAPM/V42BIS
    VG204 with no RTP at all                in 35 seconds

So the modems agree with each other and the relay's media path was the
problem.  This one has no media path: a packet that arrives on leg A's socket
is written to leg B's socket byte for byte.  No codec, no jitter buffer, no
mixer, no clock -- the sender's timing IS the timing, exactly as it would be
if the two ends were talking directly.

WHAT THAT COSTS.  We are not a media endpoint any more, so:

  * both legs must negotiate the SAME payload type or the forwarded packets
    are mislabelled.  We offer PCMA alone, which is what the VG204's ports
    already are (`compand-type a-law`), so there is nothing to transcode and
    nothing to renumber.
  * sequence numbers, timestamps and SSRC pass through unchanged.  The far
    end sees one continuous stream from a source that changes address, which
    is ordinary for a relay.
  * we cannot hear the call.  `--rec` writes each direction's payload to its
    own file instead, which is BETTER than the conference recording it
    replaces -- that one was a mix, and a mix cannot be analysed per
    direction.

WHY A HAND-WRITTEN SIP STACK.  Only the media had to change, but pjsua owns
its RTP sockets and will not hand them over.  The signalling needed is small
and completely determined by what this PBX actually sends, which is on record
in captures/relay-*.relay.log: FreePBX 16, digest MD5 with qop=auth, no
Record-Route, Contact <sip:10.0.0.26:5060>.  Every header below was modelled
on a captured message rather than on the RFC, which is why it is this short.

THE DIAL GUARD IS HERE because this is the process that turns a number into a
call and the PBX behind it reaches the PSTN.  Same list as row.sh, relay.py
and dmodem-guard.sh.  Do not make ALLOWED an argument or an environment
lookup.
"""

import argparse
import hashlib
import os
import random
import re
import select
import socket
import sys
import time
import uuid

ALLOWED = ("1901", "1902", "1903", "4242", "4343",
           "01138773693", "01133501928")  # 1903 added 2026-08-13; 4343 and the
                                          # two out-and-back-in DIDs 2026-08-17
SECRET = "/home/philpem/dev/sip-D-modem/asterisk-login-4242.secret"


def read_secret():
    out = {}
    with open(SECRET) as f:
        for line in f:
            if ":" in line:
                k, v = line.split(":", 1)
                out[k.strip()] = v.strip()
    for k in ("Server", "Extension", "Secret"):
        if not out.get(k):
            sys.exit("rtprelay: %s has no '%s'" % (SECRET, k))
    return out["Server"], out["Extension"], out["Secret"]


def local_ip(peer):
    """The address the kernel would actually use to reach the PBX.

    Not hostname resolution: this box has several addresses and the SDP has to
    carry the one the PBX will send RTP to.  A connect() on a UDP socket picks
    the route without sending anything.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((peer, 5060))
        return s.getsockname()[0]
    finally:
        s.close()


def digest(user, passwd, realm, nonce, method, uri, qop=None, opaque=None,
           nc="00000001"):
    ha1 = hashlib.md5(("%s:%s:%s" % (user, realm, passwd)).encode()).hexdigest()
    ha2 = hashlib.md5(("%s:%s" % (method, uri)).encode()).hexdigest()
    parts = ['username="%s"' % user, 'realm="%s"' % realm,
             'nonce="%s"' % nonce, 'uri="%s"' % uri]
    if qop:
        cnonce = uuid.uuid4().hex[:16]
        resp = hashlib.md5(("%s:%s:%s:%s:%s:%s"
                            % (ha1, nonce, nc, cnonce, qop, ha2)).encode()).hexdigest()
        parts += ["qop=%s" % qop, "nc=%s" % nc, 'cnonce="%s"' % cnonce]
    else:
        resp = hashlib.md5(("%s:%s:%s" % (ha1, nonce, ha2)).encode()).hexdigest()
    parts += ['response="%s"' % resp, "algorithm=MD5"]
    if opaque:
        parts.append('opaque="%s"' % opaque)
    return "Digest " + ", ".join(parts)


def parse_auth(header):
    d = dict(re.findall(r'(\w+)="?([^",]+)"?', header))
    return d.get("realm"), d.get("nonce"), d.get("qop"), d.get("opaque")


def hdr(msg, name):
    m = re.search(r"^%s:\s*(.*)$" % re.escape(name), msg, re.M | re.I)
    return m.group(1).strip() if m else None


def sdp_media(msg):
    """(ip, port) the far end wants RTP sent to."""
    body = msg.split("\r\n\r\n", 1)[-1]
    c = re.search(r"^c=IN IP4 (\S+)", body, re.M)
    m = re.search(r"^m=audio (\d+)", body, re.M)
    if not (c and m):
        return None
    return (c.group(1), int(m.group(1)))


def branch():
    return "z9hG4bK" + uuid.uuid4().hex[:16]


class Relay:
    def __init__(self, server, ext, passwd, port, rtp_a, rtp_b, log):
        self.server, self.ext, self.passwd = server, ext, passwd
        self.srv_ip = socket.gethostbyname(server)
        self.ip = local_ip(self.srv_ip)
        self.port = port
        self.log = log
        self.sip = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sip.bind(("0.0.0.0", port))
        self.rtp = {}
        for name, p in (("A", rtp_a), ("B", rtp_b)):
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(("0.0.0.0", p))
            self.rtp[name] = {"sock": s, "port": p, "remote": None, "rx": 0, "tx": 0}
        self.t0 = time.time()

    def say(self, msg):
        print("relay: %7.2f %s" % (time.time() - self.t0, msg), flush=True)

    def send(self, msg):
        if self.log:
            self.log.write("\n>>> TX\n" + msg)
        self.sip.sendto(msg.encode(), (self.srv_ip, 5060))

    def recv(self, timeout):
        r, _, _ = select.select([self.sip], [], [], timeout)
        if not r:
            return None
        data, _ = self.sip.recvfrom(65535)
        msg = data.decode("latin-1")
        if self.log:
            self.log.write("\n<<< RX\n" + msg)
        return msg

    # ---- SDP -------------------------------------------------------------
    def sdp(self, rtp_port):
        # PCMA alone.  Offering a second codec risks the two legs choosing
        # different payload types, and a forwarded packet carries its payload
        # type with it -- there is nobody left in the middle to renumber it.
        return ("v=0\r\n"
                "o=- %d %d IN IP4 %s\r\n"
                "s=rtprelay\r\n"
                "c=IN IP4 %s\r\n"
                "t=0 0\r\n"
                "m=audio %d RTP/AVP 8\r\n"
                "a=rtpmap:8 PCMA/8000\r\n"
                "a=ptime:20\r\n"
                "a=sendrecv\r\n"
                % (int(self.t0), int(self.t0), self.ip, self.ip, rtp_port))

    # ---- registration ----------------------------------------------------
    def register(self):
        cid, tag = str(uuid.uuid4()), uuid.uuid4().hex[:12]
        for cseq, auth in ((1, None), (2, "pending")):
            uri = "sip:%s" % self.server
            m = ["REGISTER %s SIP/2.0" % uri,
                 "Via: SIP/2.0/UDP %s:%d;rport;branch=%s" % (self.ip, self.port, branch()),
                 "Max-Forwards: 70",
                 "From: <sip:%s@%s>;tag=%s" % (self.ext, self.server, tag),
                 "To: <sip:%s@%s>" % (self.ext, self.server),
                 "Call-ID: %s" % cid,
                 "CSeq: %d REGISTER" % cseq,
                 "Contact: <sip:%s@%s:%d>" % (self.ext, self.ip, self.port),
                 "Expires: 300",
                 "User-Agent: rtprelay",
                 "Content-Length: 0", "", ""]
            if auth and self._chal:
                realm, nonce, qop, opaque = self._chal
                m.insert(-3, "Authorization: " + digest(self.ext, self.passwd,
                         realm, nonce, "REGISTER", uri, qop, opaque))
            self.send("\r\n".join(m))
            deadline = time.time() + 5
            while time.time() < deadline:
                r = self.recv(deadline - time.time())
                if not r or not r.startswith("SIP/2.0"):
                    continue
                code = int(r.split()[1])
                if code == 401:
                    self._chal = parse_auth(hdr(r, "WWW-Authenticate") or "")
                    break
                if code == 200:
                    self.say("REGISTERED as %s" % self.ext)
                    return True
                if code >= 400:
                    self.say("registration REFUSED: %s" % r.split("\r\n")[0])
                    return False
        return False

    # ---- inbound leg -----------------------------------------------------
    def wait_inbound(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            r = self.recv(min(1.0, max(0.05, deadline - time.time())))
            if not r:
                continue
            if r.startswith("OPTIONS"):
                self.respond(r, 200)          # the PBX pings; stay reachable
                continue
            if not r.startswith("INVITE"):
                continue
            self.in_msg = r
            self.rtp["A"]["remote"] = sdp_media(r)
            self.say("INBOUND from %s, its RTP at %s:%d"
                     % (hdr(r, "From"), *self.rtp["A"]["remote"]))
            self.respond(r, 100)
            self.in_tag = uuid.uuid4().hex[:12]
            self.respond(r, 200, sdp=self.sdp(self.rtp["A"]["port"]),
                         to_tag=self.in_tag)
            return True
        return False

    def respond(self, req, code, sdp=None, to_tag=None):
        reason = {100: "Trying", 200: "OK", 481: "Call/Transaction Does Not Exist"}[code]
        to = hdr(req, "To")
        if to_tag and ";tag=" not in to:
            to += ";tag=%s" % to_tag
        m = ["SIP/2.0 %d %s" % (code, reason),
             "Via: %s" % hdr(req, "Via"),
             "From: %s" % hdr(req, "From"),
             "To: %s" % to,
             "Call-ID: %s" % hdr(req, "Call-ID"),
             "CSeq: %s" % hdr(req, "CSeq"),
             "Contact: <sip:%s@%s:%d>" % (self.ext, self.ip, self.port),
             "User-Agent: rtprelay"]
        if sdp:
            m += ["Content-Type: application/sdp",
                  "Content-Length: %d" % len(sdp), "", sdp]
        else:
            m += ["Content-Length: 0", "", ""]
        self.send("\r\n".join(m))

    # ---- outbound leg ----------------------------------------------------
    def invite(self, number):
        cid, tag = str(uuid.uuid4()), uuid.uuid4().hex[:12]
        uri = "sip:%s@%s" % (number, self.server)
        body = self.sdp(self.rtp["B"]["port"])
        chal = None
        for cseq in (1, 2):
            m = ["INVITE %s SIP/2.0" % uri,
                 "Via: SIP/2.0/UDP %s:%d;rport;branch=%s" % (self.ip, self.port, branch()),
                 "Max-Forwards: 70",
                 "From: <sip:%s@%s>;tag=%s" % (self.ext, self.server, tag),
                 "To: <%s>" % uri,
                 "Call-ID: %s" % cid,
                 "CSeq: %d INVITE" % cseq,
                 "Contact: <sip:%s@%s:%d>" % (self.ext, self.ip, self.port),
                 "User-Agent: rtprelay",
                 "Content-Type: application/sdp",
                 "Content-Length: %d" % len(body), "", body]
            if chal:
                realm, nonce, qop, opaque = chal
                m.insert(-4, "Authorization: "
                         + digest(self.ext, self.passwd, realm, nonce,
                                  "INVITE", uri, qop, opaque))
            self.send("\r\n".join(m))
            deadline = time.time() + 45
            while time.time() < deadline:
                r = self.recv(min(1.0, max(0.05, deadline - time.time())))
                if not r:
                    continue
                if r.startswith("OPTIONS"):
                    self.respond(r, 200)
                    continue
                if not r.startswith("SIP/2.0"):
                    continue
                code = int(r.split()[1])
                if code in (100, 180, 183):
                    continue
                if code in (401, 407) and not chal:
                    h = hdr(r, "WWW-Authenticate") or hdr(r, "Proxy-Authenticate")
                    chal = parse_auth(h or "")
                    # The failed INVITE still needs an ACK before we retry.
                    self.ack(r, uri, cid, tag, cseq, in_dialog=False)
                    break
                if code == 200:
                    self.rtp["B"]["remote"] = sdp_media(r)
                    self.out = dict(cid=cid, tag=tag, cseq=cseq,
                                    to=hdr(r, "To"),
                                    contact=re.sub(r"[<>]", "",
                                                   hdr(r, "Contact") or uri))
                    self.ack(r, self.out["contact"], cid, tag, cseq,
                             in_dialog=True, to=self.out["to"])
                    self.say("OUTBOUND answered, its RTP at %s:%d"
                             % self.rtp["B"]["remote"])
                    return True
                self.say("outbound FAILED: %s" % r.split("\r\n")[0])
                return False
        return False

    def ack(self, resp, uri, cid, tag, cseq, in_dialog, to=None):
        m = ["ACK %s SIP/2.0" % uri,
             "Via: SIP/2.0/UDP %s:%d;rport;branch=%s"
             % (self.ip, self.port, branch() if in_dialog
                else re.search(r"branch=([^;\s]+)", hdr(resp, "Via")).group(1)),
             "Max-Forwards: 70",
             "From: <sip:%s@%s>;tag=%s" % (self.ext, self.server, tag),
             "To: %s" % (to or hdr(resp, "To")),
             "Call-ID: %s" % cid,
             "CSeq: %d ACK" % cseq,
             "Content-Length: 0", "", ""]
        self.send("\r\n".join(m))

    def bye(self):
        if not hasattr(self, "out"):
            return
        o = self.out
        m = ["BYE %s SIP/2.0" % o["contact"],
             "Via: SIP/2.0/UDP %s:%d;rport;branch=%s" % (self.ip, self.port, branch()),
             "Max-Forwards: 70",
             "From: <sip:%s@%s>;tag=%s" % (self.ext, self.server, o["tag"]),
             "To: %s" % o["to"],
             "Call-ID: %s" % o["cid"],
             "CSeq: %d BYE" % (o["cseq"] + 1),
             "Content-Length: 0", "", ""]
        self.send("\r\n".join(m))

    # ---- the actual job --------------------------------------------------
    def forward(self, hold, rec=None):
        """Copy packets between the two legs and change nothing about them."""
        a, b = self.rtp["A"], self.rtp["B"]
        files = {}
        if rec:
            files = {"A": open(rec + ".legA.alaw", "wb"),
                     "B": open(rec + ".legB.alaw", "wb")}
        socks = {a["sock"]: ("A", a, b), b["sock"]: ("B", b, a)}
        self.say("FORWARDING -- %s:%d <-> %s:%d, payload untouched"
                 % (*a["remote"], *b["remote"]))
        end = time.time() + hold
        first = None
        while time.time() < end:
            r, _, _ = select.select(list(socks) + [self.sip], [], [], 0.2)
            for s in r:
                if s is self.sip:
                    msg = self.recv(0)
                    if msg and msg.startswith("BYE"):
                        self.respond(msg, 200)
                        self.say("far end sent BYE")
                        end = 0
                    elif msg and msg.startswith("OPTIONS"):
                        self.respond(msg, 200)
                    continue
                name, src, dst = socks[s]
                pkt, _ = s.recvfrom(4096)
                src["rx"] += 1
                if first is None:
                    first = time.time()
                    self.say("first RTP packet (leg %s)" % name)
                if dst["remote"]:
                    dst["sock"].sendto(pkt, dst["remote"])
                    dst["tx"] += 1
                if files and len(pkt) > 12:
                    files[name].write(pkt[12:])       # strip the RTP header
        for f in files.values():
            f.close()
        self.say("leg A rx %d tx %d   leg B rx %d tx %d"
                 % (a["rx"], a["tx"], b["rx"], b["tx"]))
        if a["rx"] == 0 or b["rx"] == 0:
            self.say("A LEG SENT NOTHING -- the media never arrived, so any")
            self.say("  conclusion about what the modems did is worthless.")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dial", required=True)
    ap.add_argument("--hold", type=float, default=60.0)
    ap.add_argument("--wait-inbound", type=float, default=60.0)
    ap.add_argument("--port", type=int, default=5080)
    ap.add_argument("--rtp-a", type=int, default=4200)
    ap.add_argument("--rtp-b", type=int, default=4202)
    ap.add_argument("--rec", help="prefix for per-direction a-law recordings")
    ap.add_argument("--log", help="every SIP message, sent and received")
    args = ap.parse_args()

    if args.dial not in ALLOWED:
        sys.exit("rtprelay: REFUSING to dial '%s'; allowed: %s"
                 % (args.dial, " ".join(ALLOWED)))

    server, ext, passwd = read_secret()
    log = open(args.log, "w") if args.log else None
    r = Relay(server, ext, passwd, args.port, args.rtp_a, args.rtp_b, log)
    del passwd
    r._chal = None
    try:
        if not r.register():
            return 2
        if not r.wait_inbound(args.wait_inbound):
            r.say("no inbound call in %.0f s" % args.wait_inbound)
            return 3
        if not r.invite(args.dial):
            return 4
        r.forward(args.hold, args.rec)
        r.bye()
        r.say("done")
        return 0
    finally:
        try:
            r.bye()
        except Exception:
            pass
        if log:
            log.close()


if __name__ == "__main__":
    sys.exit(main())
