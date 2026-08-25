#!/usr/bin/env python3
"""
instrcount.py -- instructions per symbol, ours against the blob's.

WHY THIS EXISTS, and finding 7480 is the whole argument.  `exitPhase3`'s first
version had every branch, every store and all thirteen other calls matching
the object, and it was still missing a call: `enterPhase4`'s idempotence test
took its taken edge into `equalizer->enterPhase4()`, so the source made TWO
calls where the inlining showed one.  Nothing in the differential tier could
see it -- the fixture did not exist yet -- and neither `compare.py` nor
`samesize.py` reports the number that did: 174 instructions from GCC 3.4.2
against the blob's 186, where the accounted difference should have been nine.

`compare.py` compares MNEMONIC SEQUENCES and buckets a symbol as identical,
same-size or different; `samesize.py` slices the same-size bucket.  Both are
about the SHAPE of what we emit.  This one reports the SIZE of it, per symbol,
in instructions rather than bytes -- because bytes move with register
allocation and addressing modes and instructions do not, and because a missing
statement is a missing instruction whatever it encodes to.

READ A GAP AS A MISSING CALL UNTIL PROVEN OTHERWISE.  7480's residual, once
the call was added, was nine instructions and every one was accounted: seven
for the callee-saved registers (the blob spills four to stack slots where ours
pushes two) and two for an x87 schedule.  A gap of tens is a statement; a gap
of hundreds is an inlining boundary (finding 605) and means one of the callees
we CALL is being inlined into us, or the reverse.

    tools/instrcount.py                       # every symbol we have built
    tools/instrcount.py --sym _ZN14V90...     # one symbol
    tools/instrcount.py --min 200             # only the big ones
    tools/instrcount.py --worst 20            # the twenty widest gaps

It counts what objdump prints as an instruction inside the symbol's extent:
one line with an address, a byte column and a mnemonic.  Alignment padding
between symbols is outside every symbol's `st_size` and is therefore not
counted on either side -- which is the point of taking the extent from the
symbol table rather than from the next label.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

INSN = re.compile(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} )+\s*([a-z][\w.]*)')


def blob_path():
    env = os.environ.get('BLOB')
    if env:
        return env
    try:
        common = subprocess.check_output(
            ['git', 'rev-parse', '--git-common-dir'],
            cwd=ROOT).decode().strip()
    except Exception:
        common = os.path.join(ROOT, '.git')
    if not os.path.isabs(common):
        common = os.path.join(ROOT, common)
    return os.path.join(os.path.dirname(common), 'ref', 'slmodemd', 'dsplibs.o')


def sectionmap(obj):
    """section index -> name.  See `count` for why this is not optional."""
    out = {}
    txt = subprocess.check_output(['readelf', '-SW', obj],
                                  stderr=subprocess.DEVNULL).decode()
    for m in re.finditer(r'^\s*\[\s*(\d+)\]\s+(\S+)', txt, re.M):
        out[int(m.group(1))] = m.group(2)
    return out


def symbols(obj):
    """name -> (section, value, size) for every FUNC in one object."""
    out = {}
    try:
        txt = subprocess.check_output(['readelf', '-sW', obj],
                                      stderr=subprocess.DEVNULL).decode()
    except Exception:
        return out
    secs = sectionmap(obj)
    for ln in txt.split('\n'):
        f = ln.split()
        if len(f) < 8 or f[3] != 'FUNC':
            continue
        try:
            value = int(f[1], 16)
            size = int(f[2], 0)
            ndx = int(f[6])
        except ValueError:
            continue
        out[f[7]] = (secs.get(ndx, '.text'), value, size)
    return out


def count(obj, name, section, value, size):
    """
    Instructions in [value, value+size) of the SECTION holding `name`.

    `-j` IS NOT DECORATION AND ITS ABSENCE FAILS SILENTLY UPWARDS.  In an
    unlinked object every section has its own address space, so a template
    instantiation in `.gnu.linkonce.t.*` occupies the same VMA range as
    something in `.text`; `--start-address` filters by ADDRESS across every
    section, so without `-j` the count is the sum over all of them.  It read
    `Agc<float>::process` as 2,091 instructions in 206 bytes before this
    argument was added -- a number impossible on its face, which is the only
    reason it was noticed.  A tool that over-counts the BLOB makes our side
    look complete, which is the failure direction that matters here.
    """
    if size == 0:
        return 0
    cmd = ['objdump', '-d', '-j', section,
           '--start-address=0x%x' % value,
           '--stop-address=0x%x' % (value + size), obj]
    txt = subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode()
    n = 0
    for ln in txt.split('\n'):
        m = INSN.match(ln)
        if m and not _padding(ln, m.group(1)):
            n += 1
    return n


#
# ALIGNMENT PADDING IS NOT CODE, AND THIS TOOL USED TO COUNT IT.  The
# docstring above says padding between symbols is outside `st_size` and so is
# not counted, which is true and was read as the whole story.  It is not:
# GCC aligns a LOOP HEAD by emitting `nop`, `lea 0x0(%esi,%eiz,1),%esi` or
# `mov %esi,%esi` INSIDE the function, where `st_size` covers them.  How many
# it emits depends on where the loop happens to land, so two functions with
# identical code can differ by ten in this count for no reason at all.
#
# That is not a cosmetic imprecision -- it is the tool reporting the opposite
# of the truth.  `V90Demapper::printErrorHistogramAndReset` read +12 and is
# EQUAL on code; `V90SpectralShaper::process` read -1 and the blob has THREE
# code instructions we do not; `V90Demodulator::getAT_UD` read +10 and the
# blob has one MORE than us.  Finding 7774's whole lever is "same bytes,
# different instruction count means a missing statement", and it was careful
# to say "padding stripped" -- a reader who took that count from HERE would
# have chased three phantoms and missed three real absences.  Finding 7805.
#
# The predicate is byteident.py's, imported rather than copied, because two
# spellings of "is this padding" is exactly the drift CLAUDE.md's one-home
# rule exists to stop.  `mov %r,%r` is NOT in byteident's version -- it is
# added here and only here, and it is listed in `--pad` so it can never be
# folded in silently.
#
sys.path.insert(0, os.path.join(HERE, 'toolchain'))
import byteident as _bi                                  # noqa: E402

_SELFMOV = re.compile(r'\bmov\s+(%\w+),\1\s*$')


def _padding(line, mnemonic):
    """Is this disassembly line alignment padding rather than code?"""
    ops = line.split(mnemonic, 1)[1].split('#')[0].strip() if mnemonic else ''
    if _bi._padding(mnemonic, ops):
        return True
    return bool(_SELFMOV.search(line.split('\t')[-1]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sym', help='one symbol, exact name')
    ap.add_argument('--min', type=int, default=0,
                    help='skip symbols under this many blob instructions')
    ap.add_argument('--worst', type=int, default=0,
                    help='print only the N widest gaps')
    ap.add_argument('--tcout', default=os.environ.get('TC_OUT') or
                    os.path.join(ROOT, 'build', 'tc_out'))
    args = ap.parse_args()

    blob = blob_path()
    if not os.path.exists(blob):
        sys.exit('instrcount.py: no blob at %s' % blob)

    objs = []
    for dirpath, _, names in os.walk(args.tcout):
        for nm in names:
            if nm.endswith('.o'):
                objs.append(os.path.join(dirpath, nm))
    if not objs:
        sys.exit('instrcount.py: TC_OUT (%s) holds no objects.  Run\n'
                 '  tools/toolchain/build.sh\n'
                 'first; a zero denominator is finding 2400.' % args.tcout)

    bsyms = symbols(blob)
    rows = []
    seen = 0
    for obj in objs:
        for name, (sec, value, size) in symbols(obj).items():
            if args.sym and name != args.sym:
                continue
            if name not in bsyms:
                continue
            seen += 1
            bsec, bvalue, bsize = bsyms[name]
            bn = count(blob, name, bsec, bvalue, bsize)
            if bn < args.min:
                continue
            on = count(obj, name, sec, value, size)
            rows.append((bn - on, on, bn, size, bsize, name,
                         os.path.basename(obj)))

    if not rows:
        sys.exit('instrcount.py: 0 symbols matched of %d compared -- '
                 'nothing was measured.' % seen)

    rows.sort(key=lambda r: -abs(r[0]))
    if args.worst:
        rows = rows[:args.worst]
    else:
        rows.sort(key=lambda r: r[5])

    print('%-9s %-9s %-7s  %-9s %-9s  %s' %
          ('ours', 'blob', 'delta', 'ourbytes', 'blobbytes', 'symbol'))
    for delta, on, bn, osz, bsz, name, where in rows:
        print('%-9d %-9d %+-7d  %-9d %-9d  %s  [%s]' %
              (on, bn, -delta, osz, bsz, name, where))
    print()
    print('instrcount.py: %d symbols compared against %s' % (seen, blob))
    print('instrcount.py: %d printed, %d exact' %
          (len(rows), sum(1 for r in rows if r[0] == 0)))


if __name__ == '__main__':
    main()
