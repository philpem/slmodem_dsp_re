#!/usr/bin/env python3
"""Preserve seven existing MP mutation effects across recovered helper calls."""
import json
import subprocess
from pathlib import Path
import playbook_small_patterns as d


def main():
    source = Path('src/pump/v90/V90MP.cpp').read_text()
    baseline = subprocess.check_output(['git', 'show', 'd9995470:test/mutations/v90mp.json'], text=True)
    mutations = json.loads(baseline)
    audit = []

    def edit(index, member, change):
        _, _, fn = d.function(source, 'V90MP::' + member)
        altered = change(fn)
        assert altered != fn
        first = 0
        while fn[first] == altered[first]:
            first += 1
        suffix = 0
        while fn[len(fn) - suffix - 1] == altered[len(altered) - suffix - 1]:
            suffix += 1
        a = fn.rfind('\n', 0, first) + 1
        b = fn.find('\n', len(fn) - suffix)
        b = len(fn) if b < 0 else b + 1
        while source.count(fn[a:b]) != 1:
            if a:
                a = fn.rfind('\n', 0, a - 1) + 1
            elif b < len(fn):
                end = fn.find('\n', b)
                b = len(fn) if end < 0 else end + 1
            else:
                raise AssertionError('no unique member anchor')
        find = fn[a:b]
        replace = altered[a:len(altered) - (len(fn) - b)]
        assert fn.replace(find, replace, 1) == altered
        mutations[index]['find'] = find
        mutations[index]['replace'] = replace
        mutations[index]['why'] = (mutations[index].get('why', '') +
            ' MP producer recovery: preserve the original mutation in its named consumer; helper-body substitutions in this mutant do not change the shared producer or the other consumer.').strip()
        audit.append({'index': index, 'label': mutations[index]['label'],
                      'member': member, 'unique': source.count(find) == 1})

    edit(44, 'infoToBits', lambda fn: fn.replace('rateMask >> (i - 0x24)', 'rateMask >> (0x31 - i)'))
    _, _, helper = d.function(source, 'V90MP::calcSequenceLength')
    body = helper[helper.index('{'):]
    edit(47, 'infoToBits', lambda fn: fn.replace('calcSequenceLength();',
         body.replace('if (q * groupSize != w)', 'if (1)'), 1))
    edit(48, 'infoToBits', lambda fn: fn.replace('calcSequenceLength();',
         body.replace('((q + 1) * groupSize)', '(q * groupSize)'), 1))
    edit(70, 'bitsToInfo', lambda fn: fn.replace('bitIndex++;\n\t\tbits[i]', 'bits[i]', 1))
    edit(95, 'bitsToInfo', lambda fn: fn.replace('PrintBase2(str, (unsigned long)rateMask, 14);',
         'PrintBase2(str, (unsigned long)rateMask, 16);', 1))
    old = 'PrintBase2(str, (unsigned long)rateMask, 14);'
    low = "{\n\t\t\t\t\tchar *q = str;\n\t\t\t\t\tunsigned int mask;\n\t\t\t\t\tfor (mask = 1; mask <= 0x2000; mask <<= 1)\n\t\t\t\t\t\t*q++ = (rateMask & mask) ? '1' : '0';\n\t\t\t\t\t*q = 0;\n\t\t\t\t}"
    def second(fn):
        first = fn.index(old)
        at = fn.index(old, first + len(old))
        return fn[:at] + low + fn[at + len(old):]
    edit(96, 'bitsToInfo', second)
    invert = "{\n\t\t\t\t\tchar *p = str;\n\t\t\t\t\tunsigned int mask;\n\t\t\t\t\tfor (mask = 0x2000; mask != 0; mask >>= 1)\n\t\t\t\t\t\t*p++ = (rateMask & mask) ? '0' : '1';\n\t\t\t\t\t*p = 0;\n\t\t\t\t}"
    edit(97, 'bitsToInfo', lambda fn: fn.replace(old, invert, 1))
    for mutation in mutations:
        if 'find' in mutation:
            assert source.count(mutation['find']) == 1, mutation['label']
            assert mutation['replace'] != mutation['find']
    Path('test/mutations/v90mp.json').write_text(json.dumps(mutations, indent=1) + '\n')
    out = Path('build/recovery-mp-next-integration')
    out.mkdir(exist_ok=True)
    (out / 'locator-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    print('7 existing consumer mutation effects preserved; no mutant executed')


if __name__ == '__main__':
    main()
