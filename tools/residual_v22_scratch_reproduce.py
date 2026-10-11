#!/usr/bin/env python3
"""Replay the same three V22 controls through existing read-only scratch tracing."""
import json
import subprocess
import sys
import playbook_small_patterns as d


def main():
    entries = [{'label': label, 'directory': 'build/residual-v22-ack/v22prc/' + source,
                'compiler': 'cc1', 'input': 'v22prc.i'}
               for label, source in (('baseline', 'baseline'), ('combined', 'square-1-late-1'),
                                     ('baseline-repeat', 'baseline'))]
    manifest = d.ROOT / 'build/residual-v22-scratch-manifest.json'
    manifest.write_text(json.dumps(entries, indent=2) + '\n')
    subprocess.run([sys.executable, str(d.ROOT / 'tools/gentoo_peep2_search_reproduce.py'),
                    '--manifest', str(manifest), '--output', str(d.ROOT / 'build/residual-v22-scratch')], check=True)


if __name__ == '__main__':
    main()
