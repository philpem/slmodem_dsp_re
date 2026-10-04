#!/usr/bin/env python3
"""Wave-2 dump: row-level byte diff for FSE_decision_CD and generateE2u."""
import sys, glob, os, subprocess
sys.path.insert(0, 'tools/toolchain')
import byteident as bi

SYMS = ('FSE_decision_CD', '_ZN18V92Phase4Modulator11generateE2uEv')
om = {}
for o in glob.glob(os.path.join(bi.OURS, "*.o")):
    for s in bi.sizes(o):
        om.setdefault(s, o)
for sym in SYMS:
    print('=' * 78)
    print(sym, '->', om[sym])
    ours = bi.body(om[sym], sym)
    blob = bi.body(bi.BLOB, sym)
    print('size ours=%d blob=%d' % (len(ours[0]), len(blob[0])))
    # row-level diff of raw body bytes
    a, b = ours[0], blob[0]
    runs = []
    i = 0
    while i < max(len(a), len(b)):
        if i < min(len(a), len(b)) and a[i] == b[i]:
            i += 1
            continue
        j = i
        while j < max(len(a), len(b)) and not (j < min(len(a), len(b)) and a[j] == b[j]):
            j += 1
        runs.append((i, j))
        i = j
    print('differing runs (start,end):', [(hex(s), hex(e)) for s, e in runs])
    for dis_out, path in (('OURS', om[sym]), ('BLOB', bi.BLOB)):
        dis = subprocess.run(["objdump", "-d", "--no-show-raw-insn",
                              "--disassemble=" + sym, path],
                             capture_output=True, text=True).stdout
        name = os.path.join('build', 'wave2-%s.%s.txt' % (sym, dis_out))
        with open(name, 'w') as f:
            f.write(dis)
        print('wrote', name, len(dis.splitlines()), 'lines')
