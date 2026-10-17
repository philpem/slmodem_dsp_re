#!/usr/bin/env python3
"""Snapshot the retained complete manifest and replace only bounded FPM TUs."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
import playbook_small_patterns as d

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline-dir', type=Path, required=True)
p.add_argument('--run-dir', type=Path, default=Path('build/stage-c-fpm-extents'))
a = p.parse_args()
manifest = (a.baseline_dir / 'tc_link_manifest.txt').read_text()
rows = [line.split() for line in manifest.splitlines()]
assert len(rows) == 300 and all(len(row) == 2 for row in rows)
image = (a.baseline_dir / '.build-config').read_text().splitlines()[0].split(' ', 1)[1]
replacements = {'src_dsp_fpm_tables.c.o': ('fpm_tables', 'original-extents'),
                'src_dsp_fpm_log10.c.o': ('fpm_log10', 'original-extent'),
                'src_dsp_fpm_sqrt.c.o': ('fpm_sqrt', 'original-source')}
report = {'manifest_entries': len(rows), 'input_objects': {}, 'replacements': replacements}
for label in ('baseline', 'original', 'retained'):
    dst = a.run_dir / ('full-link-' + label)
    dst.mkdir(exist_ok=True)
    (dst / 'manifest.txt').write_text(manifest)
    for obj, source in rows:
        src = a.baseline_dir / obj
        report['input_objects'][obj] = hashlib.sha256(src.read_bytes()).hexdigest()
        if label != 'baseline' and obj in replacements and not (label == 'retained' and obj == 'src_dsp_fpm_log10.c.o'):
            family, cell = replacements[obj]
            src = a.run_dir / family / cell / 'candidate.o'
        shutil.copyfile(src, dst / obj)
    command = ['tools/toolchain/partiallink.sh', str(dst / 'dsplibs.o'), str(dst / 'manifest.txt')]
    report[label + '_command'] = command
    with (dst / 'link.log').open('w') as log:
        subprocess.run(command, env=dict(os.environ, TC_IMAGE=image), stdout=log, stderr=subprocess.STDOUT, check=True)
    report[label + '_hash'] = hashlib.sha256((dst / 'dsplibs.o').read_bytes()).hexdigest()
(a.run_dir / 'full-link.json').write_text(json.dumps(report, indent=2) + '\n')
print('3 complete partial links / 300 objects each; diagnostic3 and retained2 explicit replacements')
