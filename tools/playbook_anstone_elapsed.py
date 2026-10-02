#!/usr/bin/env python3
"""Two staged full-TU answer-tone elapsed-field factoring controls."""
from pathlib import Path
import playbook_small_patterns as driver
from playbook_anstone_loop import variants as loop_variants


def variants(path, source):
    source = loop_variants(path, source)['cursor']
    start, end, fn = driver.function(source, 'GenerateAnsTone')
    changed = fn.replace('\tint elapsed;\n', '')
    assert changed.count('elapsed = ans->elapsed + count;') == 2
    changed = changed.replace('elapsed = ans->elapsed + count;', 'ans->elapsed += count;')
    changed = changed.replace('if (elapsed >=', 'if (ans->elapsed >=').replace('if (elapsed >', 'if (ans->elapsed >')
    changed = changed.replace('\tans->elapsed = elapsed;\n\n', '')
    return {'baseline': source,
            'field-update': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver.REV = '9a16b620'
    driver.OUT_NAME = 'playbook-anstone-elapsed'
    driver.SOURCE_PATHS = ('src/pump/v32/v32anstone.c',)
    driver.variants = variants
    cell = driver.ROOT/'build'/driver.OUT_NAME/'v32anstone'
    cell.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT/'build/playbook-anstone-loop/v32anstone/cursor/candidate.o'
    saved = cell/'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
