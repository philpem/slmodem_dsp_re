#!/usr/bin/env python3
"""Replay INFO1a's SI Uinfo definition and allocated low-word consumers.

Use --domain uinfo-lifetime --baseline-dir PATH with the retained production
objects/config at 4d2efa2d. This is an allocation audit, not a source variant.
Original binary anchors are 8dc1 CWDE, 8dc4 full EAX stack store, and 8fdb
MOVZWL. They support a sign-extended SI value with a low-word consumer.
"""
import json
import re
import playbook_small_patterns as d

d.REV = '4d2efa2d'
d.OUT_NAME = 'info1a-uinfo-lifetime-replay'
d.SOURCE_PATHS = ('src/pump/v34/v34info1a.cpp',)
d.DUMP_FLAGS = ('-da', '-fdump-tree-original')
d.variants = lambda path, source: {'baseline': source}


def inspect(initial, combine, greg):
    return {
        'SI_definition': bool(re.search(r'reg/v:SI 65 \[ uinfo \].*?sign_extend:SI', initial, re.S)),
        'low_word_consumers': len(re.findall(r'zero_extend:SI \(subreg:HI \(reg/v:SI 65', combine)) == 2,
        'ESI_allocation': bool(re.search(r'reg/v:SI 4 si \[orig:65 uinfo \]', greg)),
        'allocated_low_word_consumers': len(re.findall(r'zero_extend:SI \(reg:HI 4 si \[orig:65 uinfo', greg)) == 2,
    }


if __name__ == '__main__':
    d.main()  # Includes complete-TU raw reproduction against the supplied object.
    folder = d.ROOT/'build'/d.OUT_NAME/'v34info1a'/'baseline'
    dumps = [(folder/('v34info1a.cpp'+suffix)).read_text()
             for suffix in ('.01.rtl', '.20.combine', '.25.greg')]
    checks = inspect(*dumps)
    # Known wrong producer: prove the detector rejects a zero-extension claim.
    negative = inspect(dumps[0].replace('sign_extend:SI', 'zero_extend:SI'), *dumps[1:])
    assert not negative['SI_definition'], 'producer negative control did not fire'
    report = {'checks': checks, 'negative_control_rejected': True,
              'inference_boundary': 'Original RTL unavailable; current ESI allocation does not prove why original spilled SI.',
              'next_discriminator': 'Establish an actual aggregate owner at obj+4 from independent layout/callee evidence before any owner-expression control.'}
    (folder.parent.parent/'lifetime-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Uinfo lifetime: %d/%d checks; wrong-producer negative rejected' %
          (sum(checks.values()), len(checks)))
    assert all(checks.values()), checks
