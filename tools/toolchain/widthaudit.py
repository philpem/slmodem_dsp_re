#!/usr/bin/env python3
"""Bidirectional width/extension audit across the whole object.

For every symbol both sides define, key each MEMORY ACCESS by its owning base
register and displacement, and compare the width/extension the two compilers
chose:

    stores   movb / movw / movl to memory        -> field or local width
    loads    movzbl/movsbl/movzwl/movswl mem,reg -> declared type of what loaded
    cwtl     count only (16-bit temporary / short return)

A STORE conflict at one base+displacement (the object spells `movw`, ours
`movl`, or the reverse) is a width fact about the owner.  A LOAD-extension
conflict (object `movzwl`, ours `movswl`, or the reverse) is a signedness fact
when the value is live; `extcheck.py` decides liveness, this tool reports the
multiset and its location.

TWO CLASSES ARE SUPPRESSED, because a measurement established each:

  * a `movzwl mem,%eax` followed within two instructions by
    `movswl %ax,%eax`/`cwtl` is a TWO-STEP EXTENSION -- the net value is
    signed in both builds, so the first instruction carries no type
    information.  Four of the first five "defects" in this report were this.
  * `disp(%esp)` accesses are locals whose frame slot differs between builds;
    a stack offset is not comparable, so they are excluded from store/load
    pairing (their widths are still visible to `--why`-style reading).

CALIBRATION (measured at fc174e5fc, GCC 3.4.2-r2): with `--exact-only`,
**2 of 1,118 EXACT symbols** report a conflict.  Both are the documented
displacement-collision class the key cannot avoid: the object loads BOTH
extensions at that displacement (two different members), ours only one.  A
conflict on an EXACT symbol therefore means the displacement paired unrelated
sites, not that the EXACT verdict is wrong.  Any other reading of this tool
must be treated as uncalibrated.

REFINEMENT, MEASURED: keying by displacement alone reported 73 symbols / 97
sites at fc174e5fc; adding the owning base and the two suppressions above
reports 56 / 77.  Eleven real type defects were found in the first full pass
over the 73-symbol list; the rest were two-step extensions, dead extensions,
displacement collisions and 16-bit-only uses.

    python3 tools/toolchain/widthaudit.py [--top N] [--sym SYM]
                                         [--strong] [--exact-only]
                                         [--exact-list FILE]

`--strong` prints only single-kind mismatches (object one width, ours another)
plus the `cwtl` delta ranking.  `--exact-only` is the calibration check.
Set `ROOT` to the worktree (default: this script's repository root) and
`TC_OUT` to the object tree (default: `build/tc_out`).  It refuses to report
when the tree is empty, because "0 conflicts" over 0 symbols is not a clean
tree.

The tool is a triage aid, never a gate: trace every hit in `tools/dis.py`,
answer the three questions in AGENTS.md, and judge a change by run length and
LCS.
"""
import collections
import glob
import os
import re
import subprocess
import sys

ROOT = os.environ.get('ROOT', os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
BLOB = os.environ.get('BLOB', os.path.join(ROOT, 'ref/slmodemd/dsplibs.o'))
OURS = os.environ.get('TC_OUT', os.path.join(ROOT, 'build/tc_out'))

LOAD_EXT = {
    'movzbl': 'z8', 'movsbl': 's8',
    'movzwl': 'z16', 'movswl': 's16',
    'movzbw': 'z8w', 'movsww': 's16w',
}


def nm_ranges(path):
    out = subprocess.run(['nm', '-S', '--defined-only', '-f', 'posix', path],
                         capture_output=True, text=True).stdout
    d = collections.defaultdict(list)
    for line in out.splitlines():
        p = line.split()
        if len(p) < 4:
            continue
        name, typ, addr, size = p[0], p[1], p[2], p[3]
        if typ.lower() not in ('t', 'w'):
            continue
        try:
            a, s = int(addr, 16), int(size, 16)
        except ValueError:
            continue
        if s:
            d[name].append((a, s))
    return d


LINE = re.compile(r'^\s*([0-9a-f]+):\t(.*)$')
MEM = re.compile(r'(-?0x[0-9a-f]+)?\(%([a-z0-9]+)\)')


def disasm(path, ranges):
    out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', path],
                         capture_output=True, text=True).stdout
    per = {}
    for line in out.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        txt = m.group(2).strip()
        txt = re.sub(r'\s*#.*$', '', txt)
        per[int(m.group(1), 16)] = txt
    addrs = sorted(per)
    res = {}
    for name, rl in ranges.items():
        buf = []
        for a, s in rl:
            for x in addrs:
                if a <= x < a + s:
                    buf.append(per[x])
        res[name] = buf
    return res


def classify(txt):
    """-> list of (kind, key) for one instruction."""
    parts = txt.split(None, 1)
    if not parts:
        return []
    mn = parts[0]
    ops = parts[1] if len(parts) > 1 else ''
    m = MEM.search(ops)
    disp = 0
    base = ''
    if m:
        if m.group(1):
            disp = int(m.group(1), 16) & 0xffffffff
            if disp >= 0x80000000:
                disp -= 0x100000000
        base = m.group(2)
    if mn in ('movb', 'movw', 'movl') and m:
        # destination is memory iff the memory operand is the last one
        last = ops.rsplit(',', 1)[-1].strip()
        if MEM.search(last):
            return [('store', (disp, base, mn[3:]))]
        return []
    if mn in LOAD_EXT:
        return [('load', (disp, base, LOAD_EXT[mn]))]
    if mn in ('cwtl', 'cltd', 'cwtd', 'cltq'):
        return [('cwtl', (0, '', mn))]
    return []


R16 = re.compile(r'%(ax|bx|cx|dx|si|di|bp)\b')
R32 = {'ax': 'eax', 'bx': 'ebx', 'cx': 'ecx', 'dx': 'edx',
       'si': 'esi', 'di': 'edi', 'bp': 'ebp'}


def repaired(body, i):
    """True if load i is immediately re-sign-extended -- the extension is
    unobservable (the net value is signed in both builds)."""
    txt = body[i]
    m = re.match(r'movz[wb]l\s+\S+\s+%(e[a-z]{2})\b', txt)
    if not m:
        return False
    full = m.group(1)
    low = [k for k, v in R32.items() if v == full]
    if not low:
        return False
    r16 = low[0]
    for j in (i + 1, i + 2):
        if j >= len(body):
            break
        nxt = body[j]
        if re.match(r'movs[wb]l\s+%%%s\s*,\s*%%%s\b' % (r16, full), nxt):
            return True
        if nxt.split()[0] in ('cwtl', 'cwtd') and r16 == 'ax':
            return True
    return False


def gather(path, ranges, names):
    res = {}
    for name, body in disasm(path, ranges).items():
        if name not in names:
            continue
        acc = collections.Counter()
        for i, txt in enumerate(body):
            if txt.startswith('movz') and repaired(body, i):
                continue
            for kind, key in classify(txt):
                if key[1] == 'esp' and kind in ('store', 'load'):
                    continue
                acc[(kind, key)] += 1
        res[name] = acc
    return res


def main():
    argv = sys.argv[1:]
    top = 30
    only = None
    exact_only = '--exact-only' in argv
    if '--top' in argv:
        top = int(argv[argv.index('--top') + 1])
    if '--sym' in argv:
        only = argv[argv.index('--sym') + 1]

    blob_r = nm_ranges(BLOB)
    ours_r = collections.defaultdict(list)
    for f in sorted(glob.glob(OURS + '/*.o')):
        for k, v in nm_ranges(f).items():
            ours_r[k].extend(v)
    names = set(blob_r) & set(ours_r)
    if only:
        names = {n for n in names if n == only}
    if not names:
        print('no shared symbols -- refusing to report a clean tree '
              '(check ROOT=%s)' % ROOT, file=sys.stderr)
        sys.exit(2)

    bidx = gather(BLOB, {n: blob_r[n] for n in names}, names)
    oidx = {}
    for f in sorted(glob.glob(OURS + '/*.o')):
        d = nm_ranges(f)
        sel = [n for n in d if n in names]
        if not sel:
            continue
        got = gather(f, {n: d[n] for n in sel}, set(sel))
        for k, v in got.items():
            if k in oidx:
                oidx[k] = oidx[k] + v
            else:
                oidx[k] = v

    exact = set()
    exfile = None
    if '--exact-list' in argv:
        exfile = argv[argv.index('--exact-list') + 1]
    else:
        exfile = os.environ.get('EXACT_LIST', '')
    if os.path.exists(exfile):
        for line in open(exfile):
            line = line.strip()
            if line:
                exact.add(line.split()[-1])

    rows = []
    for n in sorted(names):
        b = bidx.get(n, collections.Counter())
        o = oidx.get(n, collections.Counter())
        # group by (kind, disp) -> set of widths
        bg = collections.defaultdict(set)
        og = collections.defaultdict(set)
        for (kind, key), c in b.items():
            if c:
                bg[(kind, key[0], key[1])].add(key[2])
        for (kind, key), c in o.items():
            if c:
                og[(kind, key[0], key[1])].add(key[2])
        conflicts = []
        for k in set(bg) & set(og):
            if bg[k] != og[k] and k[0] in ('store', 'load'):
                strong = len(bg[k]) == 1 and len(og[k]) == 1
                conflicts.append((k, sorted(bg[k]), sorted(og[k]), strong))
        bc = sum(c for (kind, _), c in b.items() if kind == 'cwtl')
        oc = sum(c for (kind, _), c in o.items() if kind == 'cwtl')
        if conflicts or bc != oc:
            ns = len([c for c in conflicts if c[3]])
            rows.append((ns, abs(bc - oc), n, conflicts, bc, oc))

    if '--strong' in argv:
        rows = [r for r in rows if r[0]]
        rows.sort(key=lambda r: (-r[0], -r[1], r[2]))
        print('STRONG single-kind mismatches (blob one width, ours another): %d symbol(s)'
              % len(rows))
        for nc, dc, n, conf, bc, oc in rows:
            print('%-52s %d  cwtl %d/%d' % (n, nc, bc, oc))
            for k, bw, ow, strong in conf:
                if not strong:
                    continue
                d = k[1]
                print('    %-6s disp %-9s blob %-4s ours %s'
                      % (k[0], hex(d) if d >= 0 else '-' + hex(-d),
                         ','.join(bw), ','.join(ow)))
        print('--- cwtl delta, ranked')
        cr = [r for r in rows]
        cr.sort(key=lambda r: -r[1])
        for nc, dc, n, conf, bc, oc in cr[:40]:
            if dc:
                print('%-52s cwtl blob/ours %d/%d' % (n, bc, oc))
        return
    if exact_only:
        bad = [r for r in rows if r[2] in exact]
        print('EXACT symbols with a reported conflict: %d of %d checked '
              '(denominator %d shared symbols)' % (len(bad), len(exact & names), len(names)))
        for r in bad[:top]:
            print('  %s conflicts=%d cwtl %d/%d %s' % (r[2], r[0], r[4], r[5], r[3]))
        return

    print('shared symbols: %d, with a width/extension delta: %d'
          % (len(names), len(rows)))
    rows.sort(key=lambda r: (-r[0], -r[1], r[2]))
    for nc, dc, n, conf, bc, oc in rows[:top]:
        print('%-52s store/load-conflicts %d  cwtl blob/ours %d/%d' % (n, nc, bc, oc))
        for k, bw, ow in conf[:6]:
            print('    %-6s disp %-8s blob %-10s ours %s'
                  % (k[0], hex(k[1]) if k[1] >= 0 else '-' + hex(-k[1]), ','.join(bw), ','.join(ow)))


if __name__ == '__main__':
    main()
