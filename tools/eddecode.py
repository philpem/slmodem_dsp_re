#!/usr/bin/env python3
"""
Decode the diagnostic channel `edprintf` writes.

WHY THIS EXISTS

Almost every diagnostic the V.90/V.92 half of dsplibs.o emits goes through
`edprintf` -- 145 functions call it -- and `edprintf` does not print its
argument. It formats the message, then emits each byte as two characters,
high nibble then low, each with a rotating offset added, framed by `$!$ ` and
`????`:

    edprintf("Hi")   ->   $!$ 8>8@????

So a log from a modem running this code is unreadable, and the part of the
object that is hardest to reason about is exactly the part whose diagnostics
say nothing. That is presumably deliberate -- the frame characters are what
the manufacturer's own tool looks for -- but it is not useful to anyone
maintaining this reconstruction. See docs/findings.md, 151.

There are two ways out and this is one of them. The other is to build with
the obfuscation switched off (`dsplib_encode_plain`, include/dsplib/encode.h),
which needs a rebuild; this decodes logs that have already been captured, or
that came from the original binary, which cannot be rebuilt at all.

HOW IT DECODES

`offsetarr` is ten bytes, `4 6 2 7 1 9 3 5 8 7`, and `iEncodeOffset` steps
through it once per emitted character. Each encoded character is

    ch = nibble + offsetarr[k] + ord('0')

so the nibble is recovered by subtracting. `edprintf` ZEROES the counter
before it encodes, so every frame starts at k = 0 and each is independently
decodable -- which matters, because `cEncodeChar` shares the same counter and
moves it between frames.

THE HIGH NIBBLE IS SIGNED. The object takes it with an arithmetic shift
(`sar $0x4` on a byte), so a source byte of 0x80 or more contributes -8..-1
rather than 8..15. Masking the reassembled byte to 8 bits recovers it; not
masking gives a negative number for every non-ASCII byte.

Anything that is not a frame is passed through unchanged, so a log with both
plain and encoded lines in it -- which is what a mixed V.34/V.90 session
produces -- comes out readable throughout.

Usage:
    eddecode.py < modem.log
    eddecode.py modem.log another.log
    eddecode.py --selftest
"""

import argparse
import sys

# offsetarr, .data+0x94a0.  See src/core/encode.c.
OFFSETS = (4, 6, 2, 7, 1, 9, 3, 5, 8, 7)

PREFIX = "$!$ "
SUFFIX = "????"

# '?' OCCURS INSIDE PAYLOADS, BUT `????` NEVER DOES -- and that is provable
# rather than hopeful, which is what makes this parse exact.
#
# An encoded character is `nibble + offsetarr[k] + '0'`, and '?' is 63, so a
# '?' at payload index i needs `nibble == 15 - offsetarr[i % 10]`.  An EVEN i
# is a high nibble, taken with an arithmetic shift, so it can only be -8..7 --
# which needs `offsetarr[i % 10] >= 8`, and the only such entries are index 5
# (9) and index 8 (8).  Of those only 8 is even.  An ODD i is a low nibble,
# 0..15, and can always be '?'.
#
# So a high-nibble '?' can only fall at i congruent to 8 mod 10, and the
# longest possible run is THREE: starting at i = 7 gives odd, even(8), odd,
# and then i = 10 is even with offsetarr[0] = 4, needing a high nibble of 11.
# `selftest` checks that exhaustively rather than trusting the argument.
#
# A frame's payload can therefore END in up to three '?', which merge with
# the suffix into a run of up to seven -- so the FIRST `????` in that run is
# not the terminator and the LAST one is.  Hence: find the prefix, then try
# each `????` after it from the last backwards, and take the first whose
# payload decodes.  A payload that swallowed a following frame fails on its
# '$' -- 36 - '0' is below the lowest nibble any offset can produce -- and
# backs off, so a line holding two frames splits correctly.

# What edprintf emits instead of a frame when the message will not fit.
TOO_LONG = "too long print string"


def encode(payload):
    """The object's encoding, for the self-test to check the decoder against."""
    out = []
    k = 0
    for b in payload:
        hi = (b - 256 if b >= 128 else b) >> 4
        out.append(chr(hi + OFFSETS[k] + ord("0")))
        k = (k + 1) % len(OFFSETS)
        out.append(chr((b & 0xF) + OFFSETS[k] + ord("0")))
        k = (k + 1) % len(OFFSETS)
    return "".join(out)


def decode_body(body):
    """Decode one frame's payload.  Returns (text, complaint-or-None)."""
    if len(body) % 2:
        return None, "odd number of encoded characters (%d)" % len(body)

    out = bytearray()
    for i in range(0, len(body), 2):
        hi = ord(body[i]) - ord("0") - OFFSETS[i % len(OFFSETS)]
        lo = ord(body[i + 1]) - ord("0") - OFFSETS[(i + 1) % len(OFFSETS)]
        if not -8 <= hi <= 15 or not 0 <= lo <= 15:
            return None, "character %d is out of range" % i
        out.append(((hi << 4) | lo) & 0xFF)

    return out.decode("latin1"), None


def _ends(line, start):
    """Offsets of every SUFFIX at or after `start`, last first."""
    out = []
    at = line.find(SUFFIX, start)
    while at >= 0:
        out.append(at)
        at = line.find(SUFFIX, at + 1)
    out.reverse()
    return out


def decode_line(line, keep_frame=False):
    """Replace every frame in `line` with its plaintext."""
    out = []
    i = 0

    while True:
        at = line.find(PREFIX, i)
        if at < 0:
            out.append(line[i:])
            return "".join(out)

        body_at = at + len(PREFIX)
        text = None
        end = None
        for e in _ends(line, body_at):
            text, why = decode_body(line[body_at:e])
            if text is not None:
                end = e + len(SUFFIX)
                break

        if text is None:
            # No suffix after this prefix produced a decodable payload, so
            # this is not a frame.  Pass the prefix through and carry on
            # looking -- saying nothing is better than saying something
            # that looks decoded and is not.
            out.append(line[i:body_at])
            i = body_at
            continue

        out.append(line[i:at])
        out.append("$!$ %s ????" % text if keep_frame else text)
        i = end


def selftest():
    """Round-trip the encoder's own worked example and a few awkward ones."""
    cases = [
        ("$!$ ????", ""),
        ("$!$ 8>8@????", "Hi"),
    ]
    failures = 0

    for encoded, want in cases:
        got = decode_line(encoded)
        if got != want:
            print("FAIL %r -> %r, wanted %r" % (encoded, got, want))
            failures += 1

    # Round trip over every byte.  Frames always start at k = 0 because
    # edprintf zeroes the counter, so that is the only start to test.
    payload = bytes(range(256))
    if decode_line(PREFIX + encode(payload) + SUFFIX) != payload.decode("latin1"):
        print("FAIL round trip over 0..255")
        failures += 1

    # The claim the parse rests on: no payload can contain `????`, and the
    # longest run of '?' one can contain is three.  Checked by construction
    # over every position rather than argued.
    best = run = 0
    for i in range(len(OFFSETS) * 4):
        need = 15 - OFFSETS[i % len(OFFSETS)]
        ok = (-8 <= need <= 7) if i % 2 == 0 else (0 <= need <= 15)
        run = run + 1 if ok else 0
        best = max(best, run)
    if best != 3:
        print("FAIL longest possible run of '?' is %d, expected 3" % best)
        failures += 1

    # A payload that ENDS in '?' merges with the suffix.  b'\x99' encodes to
    # two characters at k = 0,1; the case that matters is a message whose
    # last characters are '?', so build one directly at the position where a
    # high-nibble '?' is possible.
    tail = bytes([0x41] * 3 + [0x76])          # ends at index 6..7 of the key
    enc = encode(tail)
    if "?" in enc[-2:]:
        got = decode_line(PREFIX + enc + SUFFIX)
        if got.encode("latin1") != tail:
            print("FAIL payload ending in '?'")
            failures += 1

    # Two frames on one line must split, not merge.
    two = PREFIX + encode(b"ab") + SUFFIX + PREFIX + encode(b"cd") + SUFFIX
    if decode_line(two) != "abcd":
        print("FAIL two frames on one line -> %r" % decode_line(two))
        failures += 1

    # A line that is not a frame must come back untouched.
    for plain in ("V34HSHAK: Freeze EC", TOO_LONG, "", "$!$ but no suffix"):
        if decode_line(plain) != plain:
            print("FAIL passthrough of %r" % plain)
            failures += 1

    print("selftest: %s" % ("OK" if failures == 0 else "%d failed" % failures))
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="logs to decode; stdin if none")
    ap.add_argument("--keep-frame", action="store_true",
                    help="leave the $!$ / ???? markers around the plaintext")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if args.files:
        streams = [open(f, "r", errors="replace") for f in args.files]
    else:
        streams = [sys.stdin]

    for s in streams:
        for line in s:
            sys.stdout.write(decode_line(line.rstrip("\n"), args.keep_frame)
                             + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
