#!/usr/bin/env python3
"""One original countdown hypothesis preserving the pre-load zero boundary."""
import playbook_small_patterns as driver
import json

def manifest():
    rows = [dict(label=label, directory='build/floatfir-consumed-count/FloatFIR/'+cell,
                 compiler='cc1plus', input='FloatFIR.ii')
            for label, cell in [('before', 'baseline'), ('after', 'consumed-entry-count')]]
    assert all((driver.ROOT/row['directory']/row['input']).is_file() for row in rows)
    (driver.ROOT/'build/floatfir-consumed-count-manifest.json').write_text(json.dumps(rows, indent=2)+'\n')

def variants(path, source):
    start = source.index('FloatFIR::process(const float *in, float *out, unsigned int count)')
    end = source.index('\n}\n', start)+2
    body = source[start:end]
    guard = '\tif (count == 0)\n\t\treturn;'
    tail = '\t} while (--count != 0);'
    assert body.count(guard) == body.count(tail) == 1
    changed = body.replace(guard, '\tif (count-- == 0)\n\t\treturn;')
    changed = changed.replace(tail, '\t} while (count-- != 0);')
    return {'baseline': source, 'consumed-entry-count': source[:start]+changed+source[end:]}

if __name__ == '__main__':
    driver.REV = 'ca84f1cc'
    driver.OUT_NAME = 'floatfir-consumed-count'
    driver.SOURCE_PATHS = ('src/dsp/FloatFIR.cpp',)
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = variants
    driver.main()
    manifest()
