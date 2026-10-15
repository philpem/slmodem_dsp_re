#!/usr/bin/env python3
"""Replay the five declared constructor scratch-history controls."""
import json
from pathlib import Path
import subprocess
import sys
import playbook_small_patterns as d


def main():
    audit=json.loads((d.ROOT/'build/residual-stage-audit.json').read_text())
    folders={r['source']:r['baseline_directory'] for r in audit['targets']}
    entries=[]
    rows=[('queue-baseline','build/residual-queue-context/V92Modulator/baseline','V92Modulator.cpp'),
          ('queue-isfull','build/residual-queue-context/V92Modulator/isfull','V92Modulator.cpp'),
          ('queue-baseline-repeat','build/residual-queue-context/V92Modulator/baseline','V92Modulator.cpp'),
          ('jd',folders['src/pump/v90/V90Jd.cpp'],'V90Jd.cpp'),
          ('parameters',folders['src/pump/v90/V90Parameters.cpp'],'V90Parameters.cpp')]
    for label,folder,name in rows:
        entries.append({'label':label,'directory':folder,'compiler':'cc1plus','input':Path(name).stem+'.ii'})
    manifest=d.ROOT/'build/residual-constructor-scratch-manifest.json'
    manifest.write_text(json.dumps(entries,indent=2)+'\n')
    subprocess.run([sys.executable,str(d.ROOT/'tools/gentoo_peep2_search_reproduce.py'),
                    '--manifest',str(manifest),'--output',str(d.ROOT/'build/residual-constructor-scratch')],check=True)


if __name__=='__main__':main()
