#!/usr/bin/env python3
"""Retrieve hash-pinned official GCC3.4.2 scheduler sources for the trace."""
import hashlib
import json
from pathlib import Path
import urllib.request

HASHES={
 'haifa-sched.c':'7893bd18d3a60659cc2e6391c57984ed26f436425b565da4f583f01fc2508f30',
 'sched-rgn.c':'71fad91fa5fa86cc0ac4252f2c1e105d47f0a52965a2ef262e4021817f39f7b1',
 'sched-ebb.c':'52f7c34fbdfe0aef67b5df16927ad981879a9f1abbd53190cabfc0e83558f09c'}


def main():
 root=Path(__file__).resolve().parents[1]/'build/scheduler-source'
 root.mkdir(parents=True,exist_ok=True)
 records={}
 for name, expected in HASHES.items():
  url='https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-3.4.2/gcc/'+name
  with urllib.request.urlopen(url,timeout=30) as response:
   data=response.read()
  actual=hashlib.sha256(data).hexdigest()
  if actual!=expected:raise ValueError((name,'source hash mismatch',actual,expected))
  (root/name).write_bytes(data);records[name]={'url':url,'sha256':actual}
 (root/'provenance.json').write_text(json.dumps({'version':'GNU GCC3.4.2 release source, not verified Gentoo patch reconstruction','sources':records},indent=2)+'\n')
 print('Official GCC3.4.2 scheduler source: 3/3 hash-pinned files verified')


if __name__=='__main__':main()
