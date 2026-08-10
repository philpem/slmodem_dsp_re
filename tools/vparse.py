#!/usr/bin/env python3
"""
vparse.py -- read the (name, offset, reader) triples out of a `loadParams`.

`V90Parameters::loadParams(char *)` and `V92Parameters::loadParams(char *)` are
straight-line sequences of

    Vparser_read_int  (file, "name", &this->field)
    Vparser_read_float(file, "name", &this->field)

and nothing else: 295 calls in the first, 54 in the second, and an `awk` over
`objdump -dr` of the whole of `.text` shows those two members are the ONLY
callers of either stub anywhere in the object.  Both stubs are three bytes --
`xor %eax,%eax; ret` -- so the calls have no effect at run time.  The argument
lists are nevertheless a complete, self-describing field map of the class: the
original author's own name for every field, its offset from `this`, and
whether it is read as an `int` or a `float`.

That makes this a MEASUREMENT, not a decompilation.  Every triple comes from a
relocation and a displacement; nothing is inferred and nothing is guessed.

The only cleverness needed is that GCC 3.4 shuffles the three arguments
through whatever registers are free and writes them into the outgoing area in
whatever order it likes (`-maccumulate-outgoing-args`), so this walks the
instruction stream keeping an abstract value for each register and each
outgoing stack slot:

    ('str', addr)   a .rodata.str1.1 address, from an R_386_32 relocation
    ('this', off)   `this` plus a constant, from `lea off(%base),%reg`
    ('arg', off)    some other incoming stack argument
    None            anything else

Which incoming slot holds `this` is not assumed: every candidate is tried and
the one that resolves the most third arguments wins, which is 295 of 295 for
the V.90 member and 54 of 54 for the V.92 one.  A run that does not resolve
every call exits non-zero, so a future change that defeats the abstract
interpreter cannot be mistaken for a class that has fewer fields.

Usage:
    tools/vparse.py <obj> <mangled-loadParams-symbol> [--json out.json]
"""
import argparse
import json
import re
import subprocess
import sys

LEA_RE = re.compile(r'lea\s+(-?0x[0-9a-f]+)?\(%(e[a-z][a-z])\),%(e[a-z][a-z])$')
MOVIMM_RE = re.compile(r'mov\s+\$0x([0-9a-f]+),%(e[a-z][a-z])$')
MOVLOAD_RE = re.compile(r'mov\s+(0x[0-9a-f]+)?\(%esp\),%(e[a-z][a-z])$')
MOVSTORE_RE = re.compile(r'mov\s+%(e[a-z][a-z]),(0x[0-9a-f]+)?\(%esp\)$')
MOVRR_RE = re.compile(r'mov\s+%(e[a-z][a-z]),%(e[a-z][a-z])$')
MOVIMMSTACK_RE = re.compile(r'movl\s+\$0x([0-9a-f]+),(0x[0-9a-f]+)?\(%esp\)$')
ADDIMM_RE = re.compile(r'(add|sub)\s+\$0x([0-9a-f]+),%(e[a-z][a-z])$')
DEST_RE = re.compile(r'%(e[a-z][a-z])$')
# Instructions that write no general register.  Anything NOT matched by a
# specific handler and NOT in this set clobbers its destination, so a form the
# interpreter does not model becomes `unresolved` and is reported, rather than
# leaving a stale value that reads as a successful resolution.  That is
# gates.md rule 1, and this tool needed it: `add $0x554,%esi` silently turned
# the last of 295 calls into `this + 0`, and the run still said `0 unresolved`.
READONLY = ('push', 'cmp', 'test', 'nop', 'ret', 'j', 'call', 'fld', 'fst',
            'fcom', 'fucom', 'hlt', 'lock')

CALLER_SAVED = ('eax', 'ecx', 'edx')


def strings_of(obj, sect):
    """address -> string, for one of the string-literal sections."""
    out = subprocess.run(['objdump', '-s', '-j', sect, obj],
                         capture_output=True, text=True, check=True).stdout
    byte = {}
    for line in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+)\s((?:[0-9a-f]{2,8}\s)+)', line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        raw = m.group(2).replace(' ', '')
        for i in range(0, len(raw), 2):
            byte[addr + i // 2] = int(raw[i:i + 2], 16)
    res = {}
    for a in sorted(byte):
        if byte.get(a - 1, 0) != 0:
            continue                      # not the start of a string
        s = []
        p = a
        while byte.get(p, 0) != 0:
            s.append(chr(byte[p]))
            p += 1
        res[a] = ''.join(s)
    return res


def body(obj, sym):
    """
    The instructions of one symbol, with each standalone relocation line folded
    onto the instruction it belongs to.  `objdump -dr` prints relocations on
    their own lines, and every convenient way of trimming that output drops
    them -- CLAUDE.md's third trap.  Folding rather than trimming is what keeps
    a table of pointers from reading as a table of small integers.
    """
    out = subprocess.run(['objdump', '-dr', '--section=.text', obj],
                         capture_output=True, text=True, check=True).stdout
    insns = []
    inside = False
    for line in out.splitlines():
        m = re.match(r'^[0-9a-f]+ <(.*)>:$', line)
        if m:
            if inside:
                break
            inside = (m.group(1) == sym)
            continue
        if not inside:
            continue
        m = re.match(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} )+\s*\t(.*)$', line)
        if m:
            t = m.group(1).split('#')[0].split(' <')[0]
            insns.append([re.sub(r'\s+', ' ', t).strip(), None])
            continue
        m = re.match(r'^\s*[0-9a-f]+:\s+(R_386_\w+)\s+(\S+)', line)
        if m and insns:
            insns[-1][1] = (m.group(1), m.group(2))
    if not insns:
        sys.exit('vparse.py: %s not found in %s' % (sym, obj))
    return insns


def step(reg, slot, text, rel):
    m = MOVIMMSTACK_RE.match(text)
    if m:
        off = int(m.group(2), 16) if m.group(2) else 0
        slot[off] = (('str', rel[1], int(m.group(1), 16))
                     if rel and rel[0] == 'R_386_32' else None)
        return
    m = MOVIMM_RE.match(text)
    if m:
        reg[m.group(2)] = (('str', rel[1], int(m.group(1), 16))
                           if rel and rel[0] == 'R_386_32' else None)
        return
    m = LEA_RE.match(text)
    if m:
        off = int(m.group(1), 16) if m.group(1) else 0
        base = reg.get(m.group(2))
        reg[m.group(3)] = ((base[0], base[1] + off)
                           if base and base[0] in ('this', 'arg') else None)
        return
    m = MOVSTORE_RE.match(text)
    if m:
        off = int(m.group(2), 16) if m.group(2) else 0
        slot[off] = reg.get(m.group(1))
        return
    m = MOVRR_RE.match(text)
    if m:
        reg[m.group(2)] = reg.get(m.group(1))
        return
    m = ADDIMM_RE.match(text)
    if m:
        base = reg.get(m.group(3))
        d = int(m.group(2), 16) * (1 if m.group(1) == 'add' else -1)
        reg[m.group(3)] = ((base[0], base[1] + d)
                           if base and base[0] in ('this', 'arg') else None)
        return
    if not text.startswith(READONLY):
        m = DEST_RE.search(text)
        if m:
            reg[m.group(1)] = None


def run(insns, this_slot):
    reg, slot, calls = {}, {}, []
    for text, rel in insns:
        if text.startswith('call') or text.startswith('jmp'):
            if rel and rel[1] in ('Vparser_read_int', 'Vparser_read_float'):
                calls.append((rel[1], dict(slot)))
            for r in CALLER_SAVED:
                reg.pop(r, None)
            continue
        m = MOVLOAD_RE.match(text)
        if m:
            off = int(m.group(1), 16) if m.group(1) else 0
            if off in slot:
                # a value this function spilled earlier, reloaded
                reg[m.group(2)] = slot[off]
            else:
                reg[m.group(2)] = (('this', 0) if off == this_slot
                                   else ('arg', off))
            continue
        step(reg, slot, text, rel)
    return calls


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('obj')
    ap.add_argument('sym')
    ap.add_argument('--json')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    strs = {sect: strings_of(args.obj, sect)
            for sect in ('.rodata.str1.1', '.rodata.str1.4')}
    insns = body(args.obj, args.sym)

    best = None
    for cand in range(0x08, 0x60, 4):
        calls = run(insns, cand)
        n = sum(1 for _, s in calls if s.get(8) and s.get(8)[0] == 'this')
        if best is None or n > best[0]:
            best = (n, cand, calls)
    _, this_slot, calls = best

    rows = []
    for kind, slots in calls:
        nm, ptr = slots.get(4), slots.get(8)
        rows.append({
            'reader': 'float' if kind.endswith('float') else 'int',
            'name': (strs.get(nm[1], {}).get(nm[2])
                     if nm and nm[0] == 'str' else None),
            'offset': ptr[1] if ptr and ptr[0] == 'this' else None,
        })

    bad = [r for r in rows if r['name'] is None or r['offset'] is None]
    print('%s: %d calls, %d resolved, %d unresolved (this at +0x%x)'
          % (args.sym, len(rows), len(rows) - len(bad), len(bad), this_slot))
    if not args.quiet:
        for r in rows:
            print('  %-5s +0x%-5s %s'
                  % (r['reader'],
                     ('%x' % r['offset']) if r['offset'] is not None else '??',
                     r['name'] or '<unresolved>'))
    if args.json:
        with open(args.json, 'w') as f:
            json.dump(rows, f, indent=1)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
