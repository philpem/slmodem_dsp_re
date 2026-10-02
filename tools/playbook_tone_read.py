#!/usr/bin/env python3
"""Eight complete-TU TONE_read phase/index-width and quadrant-bound controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start=source.index('TONE_read(short phase)');end=source.index('\n}\n',start)+2
    original=source[start:end];cells={}
    for short_phase in [False,True]:
        for short_index in [False,True]:
            for bounded in [False,True]:
                label='baseline' if not (short_phase or short_index or bounded) else 'p'+str(int(short_phase))+'-i'+str(int(short_index))+'-b'+str(int(bounded))
                body=original
                if short_phase:body=body.replace('int p = phase & 0x7ff;', 'short p = phase & 0x7ff;')
                if short_index:
                    for expression in ['0x400 - p','p - 0x400','0x800 - p']:
                        old='FPTONE['+expression+']';assert body.count(old)==1;body=body.replace(old,'FPTONE[(short)('+expression+')]')
                if bounded:body=body.replace('if (p >= 0x601)', 'if (p >= 0x601 && p <= 0x7ff)')
                cells[label]=source[:start]+body+source[end:]
    assert len(cells)==len(set(cells.values()))==8
    return cells


if __name__=='__main__':
    driver.REV='599f0fa4'
    driver.OUT_NAME='playbook-tone-read'
    driver.SOURCE_PATHS=('src/dsp/FP_math.c',)
    driver.variants=variants
    driver.main()
