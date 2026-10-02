#!/usr/bin/env python3
"""Bound the remaining decision word-distance and guarded target-read cross."""
import sys
import playbook_small_patterns as driver
import playbook_v34_decision_return as result_cells


def variants(path, source):
    parents = result_cells.variants(path, source)
    cells = {'baseline': source}
    for best in ('int', 'short'):
        parent = parents['distance-' + best + '_points-walk_return-int']
        for distance in ('int', 'short'):
            for guarded in (False, True):
                start, end, fn = driver.function(parent, 'decision')
                assert fn.count('\t\tint dist;') == 1
                if distance == 'short':
                    fn = fn.replace('\t\tint dist;', '\t\tshort dist;')
                if guarded:
                    targets = ('\tint tx = (unsigned short)d->target_re;\n'
                               '\tint ty = (unsigned short)d->target_im;\n')
                    assert fn.count(targets) == 1
                    fn = fn.replace(targets, '')
                    loop = '\tfor (i = 0; i < npts; i++, p++) {'
                    assert fn.count(loop) == 1
                    fn = fn.replace(loop, '\tif (npts > 0) {\n' + targets + '\n' + loop)
                    tail = '\n\t}\n\n\td->best_index'
                    assert fn.count(tail) == 1
                    fn = fn.replace(tail, '\n\t}\n\t}\n\n\td->best_index')
                label = 'best-%s_dist-%s_targets-%s' % (
                    best, distance, 'guarded' if guarded else 'entry')
                cells[label] = parent[:start] + fn + parent[end:]
    assert len(set(cells.values())) == 9
    return cells


def overlays(path, label):
    if label == 'baseline':
        return {}
    return result_cells.overlays(path, 'baseline_return-int')


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '361ef919'
    driver.OUT_NAME = 'playbook-v34-decision-loads'
    driver.SOURCE_PATHS = ('src/pump/v34/V34RX.c',)
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlays
    driver.main()
