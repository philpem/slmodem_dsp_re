"""Conservative fixed callee-register permutation for frame-free i386 leaves.

This is an additional REGALLOC certificate, not a general equivalence checker.
Decoded row offsets must come from byteident.insns; plain tuple lists refuse.
The boundary assumes the flat i386 ABI (DS and SS share a linear base).
All unsupported instructions, stack forms and control-flow entries refuse.
"""
import itertools
import re

CALLEE = ('ebp', 'edi', 'esi', 'ebx')
PREFIX = tuple(('push', '%' + r) for r in CALLEE)
SUFFIX = tuple(('pop', '%' + r) for r in reversed(CALLEE)) + (('ret', ''),)
JCC = frozenset(('ja', 'jae', 'jb', 'jbe', 'je', 'jne', 'jg', 'jge', 'jl',
                 'jle', 'jo', 'jno', 'js', 'jns', 'jp', 'jnp'))
# No calls, string operations, multiply/divide implicit GP operands, environment
# stores, conditional definitions, or alternate returns. x87 effects stay literal.
X87 = frozenset(('fld', 'flds', 'fldl', 'fld1', 'fldz', 'fst', 'fstp', 'fsts',
                 'fstps', 'fstl', 'fstpl', 'fildl', 'fildll', 'fadd', 'faddp',
                 'fadds', 'faddl', 'fsub', 'fsubp', 'fsubr', 'fsubrp', 'fsubs',
                 'fsubl', 'fdiv', 'fdivp', 'fdivr', 'fdivrp', 'fdivs', 'fdivl',
                 'fmul', 'fmulp', 'fmuls', 'fmull', 'fxch', 'fsqrt', 'fcom',
                 'fcomp', 'fcompp', 'fabs', 'fchs', 'fnstsw'))
INTEGER = frozenset(('mov', 'movzbl', 'movzwl', 'movsbl', 'movswl', 'lea',
                     'add', 'sub', 'and', 'or', 'xor', 'cmp', 'test', 'inc',
                     'dec', 'push', 'sahf'))
WRITES = frozenset(('mov', 'movzbl', 'movzwl', 'movsbl', 'movswl', 'lea'))
RMW = frozenset(('add', 'sub', 'and', 'or', 'xor', 'inc', 'dec'))


def certificate(x, y, *, fields, regtok, reg32, padding):
    """Return a fixed permutation on proof, otherwise None.

    Ordinary changed-register uses require a dominating full-width definition
    on *every* CFG path. Prefix/suffix saves are literal and outside that proof.
    ESP may occur only as a memory base, a push's implicit destination, or the
    destination of immediate add/sub. Stack cells may only be read, so stores
    cannot overwrite saved ABI state. Stack depth agrees at all CFG joins.
    """
    def prepare(rows):
        offsets = getattr(rows, 'offsets', None)
        if offsets is None or len(offsets) != len(rows):
            return None
        active = [(off, tuple(row)) for off, row in zip(offsets, rows)
                  if not padding(*row)]
        if len(active) < 10 or len({a for a, _ in active}) != len(active):
            return None
        code = [r for _, r in active]
        if tuple(code[:4]) != PREFIX or tuple(code[-5:]) != SUFFIX:
            return None
        end = len(code) - 5
        positions = {off: i for i, (off, _) in enumerate(active)}
        graph, uses, defs, delta, normalized = {}, {}, {}, {}, []
        for i, (mn, ops) in enumerate(code):
            if i < 4 or i >= end:
                normalized.append((mn, ops))
                continue
            f = [p.strip() for p in fields(ops)]
            regs = regtok.findall(ops)
            if any(r not in reg32 and r != 'st' for r in regs):
                return None
            if re.search(r'%[a-z]+:', ops):
                return None
            d = f[-1] if f else ''
            write = mn in WRITES or mn in RMW
            if mn in JCC or mn == 'jmp':
                m = re.fullmatch(r'\.([+-]\d+)', ops)
                if not m or int(m[1]) not in positions:
                    return None
                target = positions[int(m[1])]
                # No entry into saves or into the middle of the restoring tail.
                if not 4 <= target <= end:
                    return None
                graph[i] = [target] + ([] if mn == 'jmp' else [i + 1])
                normalized.append((mn, 'row:%d' % target))
            else:
                if mn not in INTEGER | X87:
                    return None
                # Fixed supported operand counts; malformed/new decoder forms
                # cannot silently become a read or a definition.
                if mn == 'sahf':
                    if f:
                        return None
                elif mn in INTEGER:
                    expected = 1 if mn in ('inc', 'dec', 'push') else 2
                    if len(f) != expected:
                        return None
                if mn == 'push' and (len(f) != 1 or not re.fullmatch(
                        r'%(?:eax|ebx|ecx|edx|ebp|edi|esi)', f[0])):
                    return None
                if mn == 'fnstsw' and ops != '%ax':
                    return None
                if mn in X87 and any('%' + r == d for r in CALLEE):
                    return None
                if mn == 'lea' and '%esp' in ops:
                    return None
                # Never copy or export ESP; its sole explicit write is a known
                # immediate stack adjustment. No stack-memory stores permitted.
                adjustment = (mn in ('add', 'sub') and d == '%esp'
                              and re.fullmatch(r'\$0x[0-9a-f]+', f[0]))
                if adjustment and int(f[0][1:], 16) > 0x100000:
                    return None
                for operand in f:
                    if '%esp' in operand and operand == '%esp' and not adjustment:
                        return None
                    if '%esp' in operand and operand != '%esp':
                        if not re.fullmatch(r'(?:-?0x[0-9a-f]+)?\(%esp\)', operand):
                            return None
                        if (write or mn.startswith('fst')) and operand == d:
                            return None
                if 'sp' in regs:
                    return None
                graph[i] = [i + 1]
                normalized.append((mn, ','.join(re.sub(
                    r'^(?:0x0|0)(\([^)]*\))$', r'\1', p) for p in f)))
            idiom = (mn in ('xor', 'sub') and len(f) == 2
                     and f[0] == d and d[1:] in reg32 and d.startswith('%')
                     and reg32[d[1:]] == d[1:])
            full_def = mn in WRITES and d.startswith('%') and d[1:] in CALLEE
            uses[i] = set(reg32[r] for r in regtok.findall(
                '' if idiom else ','.join(f[:-1] if full_def else f)) if r in reg32)
            defs[i] = {d[1:]} if (full_def or idiom) else set()
            delta[i] = (4 if mn == 'push' else
                        (int(f[0][1:], 16) * (1 if mn == 'sub' else -1)
                         if mn in ('add', 'sub') and d == '%esp' else 0))
        # Forward dataflow: intersect initialized registers, require equal stack
        # depths at joins. Revisit loops until the must-defined sets stabilize.
        states, pending = {4: (frozenset(), 16)}, [4]
        while pending:
            i = pending.pop()
            known, depth = states[i]
            if i == end:
                if depth != 16:
                    return None
                continue
            after = known | defs[i]
            depth += delta[i]
            if not 16 <= depth <= 0x100000:
                return None
            for nxt in graph[i]:
                if nxt in states:
                    old, old_depth = states[nxt]
                    if depth != old_depth:
                        return None
                    merged = old & after
                    if merged == old:
                        continue
                    states[nxt] = (merged, depth)
                else:
                    states[nxt] = (frozenset(after), depth)
                pending.append(nxt)
        # Unreachable executable rows have no entry-boundary proof. Refuse them.
        if set(states) != set(range(4, end + 1)):
            return None
        return normalized, uses, states, end

    a, b = prepare(x), prepare(y)
    if a is None or b is None or a[3] != b[3]:
        return None
    for perm in itertools.permutations(CALLEE):
        mapping = dict(zip(CALLEE, perm))
        changed = {r for r in CALLEE if mapping[r] != r}
        if not changed:
            continue
        if any((a[1].get(i, set()) & changed) - state[0]
               for i, state in a[2].items()):
            continue
        changed_y = {mapping[r] for r in changed}
        if any((b[1].get(i, set()) & changed_y) - state[0]
               for i, state in b[2].items()):
            continue
        def rename(m):
            r = m[1]
            family = reg32.get(r)
            if family not in changed:
                return m[0]
            # Only full-width callee tokens are supported. A narrow alias may
            # preserve unknown high bits and cannot be renamed by this proof.
            return '%' + mapping[r] if r in mapping else '%UNSUPPORTED'
        transformed = [(mn, regtok.sub(rename, op) if 4 <= i < a[3] else op)
                       for i, (mn, op) in enumerate(a[0])]
        if transformed == b[0]:
            return mapping
    return None
