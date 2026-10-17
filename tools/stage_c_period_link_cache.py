#!/usr/bin/env python3
"""Exercise link cache hits, mode switches, restored-mtime flags and object content."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile

root = d.ROOT / 'build/stage-c-period-link-cache'
root.mkdir(exist_ok=True)
report = {}

def snapshot(test):
    p = d.ROOT / 'build/period' / test
    return {'mtime_ns':p.stat().st_mtime_ns,'hash':hashlib.sha256(p.read_bytes()).hexdigest()}


def run(label, test='t_fpm_log10', mode='fixture', expected=0):
    command = ['make','period','T='+test]
    with (root/(label+'.log')).open('w') as log:
        result = subprocess.run(command, cwd=d.ROOT, env=dict(os.environ, PERIOD_LINK_MODE=mode), stdout=log, stderr=subprocess.STDOUT)
    assert (result.returncode == 0) == (expected == 0), (label, result.returncode)
    record = {'command':command,'mode':mode,'exit':result.returncode,'binary':snapshot(test)}
    if mode != 'individual': record['aggregate_key'] = (d.ROOT/'build/period/aggregate-inputs.key').read_text().strip()
    report[label] = record
    (root/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    return record

first = run('cache-baseline')
second = run('cache-hit')
assert first['binary'] == second['binary']
individual = run('mode-individual-negative', mode='individual', expected=1)
assert individual['binary']['mtime_ns'] != second['binary']['mtime_ns']
shutil.copy2(d.ROOT/'build/period/t_fpm_log10', root/'individual-negative.exe')
restored = run('mode-aggregate-restored')
assert restored['binary']['mtime_ns'] != individual['binary']['mtime_ns']
shutil.copy2(d.ROOT/'build/period/t_fpm_log10', root/'aggregate-positive.exe')

flags = d.ROOT/'test/unit/t_v90loadparams.ldflags'
original, stat = flags.read_bytes(), flags.stat()
try:
    baseline = run('wrapper-baseline', 't_v90loadparams', 'aggregate')
    flags.write_bytes(original.rstrip() + b' -Wl,--defsym,stage_c_cache_probe=1\n')
    os.utime(flags, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    changed = run('flags-restored-mtime', 't_v90loadparams', 'aggregate')
    assert changed['binary']['mtime_ns'] != baseline['binary']['mtime_ns']
    nm = subprocess.check_output(['nm',str(d.ROOT/'build/period/t_v90loadparams')],text=True)
    assert 'stage_c_cache_probe' in nm
finally:
    flags.write_bytes(original)
    os.utime(flags, ns=(stat.st_atime_ns, stat.st_mtime_ns))
run('flags-restored', 't_v90loadparams', 'aggregate')

obj = d.ROOT/'build/period/src_dsp_fpm_log10.o'
original, stat = obj.read_bytes(), obj.stat()
try:
    baseline = run('content-baseline')
    with obj.open('rb') as stream:
        section = ELFFile(stream).get_section_by_name('.comment')
        position = section['sh_offset'] + section.data().index(b'GCC')
    changed = bytearray(original)
    changed[position] = ord('H')  # Metadata-only diagnostic, no source code change.
    obj.write_bytes(changed)
    os.utime(obj, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    mutated = run('object-restored-mtime')
    assert mutated['aggregate_key'] != baseline['aggregate_key']
    assert mutated['binary']['mtime_ns'] != baseline['binary']['mtime_ns']
finally:
    obj.write_bytes(original)
    os.utime(obj, ns=(stat.st_atime_ns, stat.st_mtime_ns))
restored = run('object-restored')
assert restored['aggregate_key'] == baseline['aggregate_key']
assert restored['binary']['hash'] == baseline['binary']['hash']
report['checks'] = {'cache_hit_keeps_binary':True,'mode_switch_relinks_and_fails_layout':True,
                    'flags_content_with_old_mtime_relinks':True,'object_content_with_old_mtime_rebuilds_aggregate':True,
                    'restoration_reproduces_binary':True}
(root/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print('5/5 cache controls passed; source, flags and period object restored')
