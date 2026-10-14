#!/usr/bin/env python3
"""Cross scalar output return type and final input-type narrowing consistently."""
import re
import subprocess
import sys
import playbook_small_patterns as d

REV = '9fabd432'


def consumers():
    target = (d.ROOT/'include/dsplib/Scrambler.h').resolve()

    def reaches(path, seen):
        if path in seen:
            return False
        seen.add(path)
        if path == target:
            return True
        for name in re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]', path.read_text(), re.M):
            for candidate in (path.parent/name, d.ROOT/'include'/name):
                if candidate.is_file():
                    if reaches(candidate.resolve(), seen):
                        return True
                    break
        return False

    paths = tuple(str(p.relative_to(d.ROOT)) for p in sorted((d.ROOT/'src').rglob('*.cpp'))
                  if reaches(p.resolve(), set()))
    assert len(paths) == 27, paths
    return paths


def variants(path, source):
    return {label: source for label in ('baseline','body','return','both')}


def headers(path, label):
    if label == 'baseline':
        return {}
    text = subprocess.check_output(['git','show',REV+':include/dsplib/Scrambler.h'], cwd=d.ROOT, text=True)
    if label in ('return','both'):
        assert text.count('\tT process(T in);') == 1
        text = text.replace('\tT process(T in);','\tI process(T in);')
        assert text.count('T Descrambler<T, I>::process(T in)') == 1
        text = text.replace('T Descrambler<T, I>::process(T in)','I Descrambler<T, I>::process(T in)')
    if label in ('body','both'):
        start = text.index('Descrambler<T, I>::process(T in)')
        end = text.index('\n}', start)
        old = text[start:end]
        assert old.count('return (T)r;') == 1
        text = text[:start]+old.replace('return (T)r;','return r;')+text[end:]
    return {'dsplib/Scrambler.h':text}


if __name__ == '__main__':
    all_consumers = '--all-consumers' in sys.argv
    if all_consumers:
        sys.argv.remove('--all-consumers')
    d.REV = REV
    d.SOURCE_PATHS = consumers() if all_consumers else ('src/pump/v90/V90Phase4Demodulator.cpp',)
    d.OUT_NAME = 'gcc3-descrambler-output-all' if all_consumers else 'gcc3-descrambler-output'
    d.DUMP_FLAGS = ('-v','-save-temps','-da') if not all_consumers else ('-v',)
    d.variants = variants
    d.HEADER_OVERLAYS = headers
    d.main()
