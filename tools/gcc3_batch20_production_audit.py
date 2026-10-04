#!/usr/bin/env python3
"""Check all production inputs against the batch baseline and retained replays."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as driver

root = driver.ROOT
before = root / 'build/production-before'
after = root / 'build/tc_out'
keys = sorted(p.name for p in before.glob('*.o'))
assert len(keys) == 300 and set(keys) == {p.name for p in after.glob('*.o')}
assert (before / '.build-config').read_bytes() == (after / '.build-config').read_bytes()
lookup = {}
for parent in (root / 'build').iterdir():
    if parent.is_dir() and 'batch20' in parent.name:
        for p in parent.rglob('candidate.o'):
            digest = hashlib.sha256(p.read_bytes()).hexdigest()
            lookup.setdefault(digest, []).append(str(p.relative_to(root)))
changed = {}
for name in keys:
    if (before / name).read_bytes() == (after / name).read_bytes():
        continue
    digest = hashlib.sha256((after / name).read_bytes()).hexdigest()
    assert digest in lookup, (name, 'no raw retained replay match')
    changed[name] = {'sha256': digest, 'replay_matches': lookup[digest]}
assert (before / 'src_dsp_fpm_mrf.c.o').read_bytes() == (after / 'src_dsp_fpm_mrf.c.o').read_bytes()
base = json.loads((root / 'build/before-byteident.json').read_text())
candidate = json.loads((root / 'build/after-byteident.json').read_text())
gains = sorted(set(candidate['exact_symbols']) - set(base['exact_symbols']))
losses = sorted(set(base['exact_symbols']) - set(candidate['exact_symbols']))
assert not losses and len(gains) >= 20, (gains, losses)
report = {'objects': len(keys), 'raw_unchanged': len(keys) - len(changed),
          'config_identical': True, 'mrf_callee_raw_unchanged': True,
          'candidate_matches': changed, 'gains': gains, 'losses': losses,
          'exact_bytes_gain': candidate['exact_bytes'] - base['exact_bytes']}
(root / 'build/batch20-production-proof.json').write_text(json.dumps(report, indent=2) + '\n')
print(f"{len(keys)}/{len(keys)} objects: {len(keys)-len(changed)} raw unchanged, "
      f"{len(changed)} match retained replays; configuration and MRF callee raw unchanged")
print(f"{len(gains)} exact gains, {len(losses)} losses, +{report['exact_bytes_gain']} exact bytes")
