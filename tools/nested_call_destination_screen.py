#!/usr/bin/env python3
"""Nominate nested member destinations assigned a call result; no age inference."""
import playbook_small_patterns as d
from anchorcheck import defn_index,enclosing
from pathlib import Path
import argparse,json,hashlib,re,subprocess

PATTERN=re.compile(r'(?:\w+->){2,}\w+\s*=\s*(?:\([^;\n]*\)\s*)?[A-Za-z_]\w*(?:::\w+)?\(')


def sites(source):
    index=defn_index(source)
    return [{'line':source.count('\n',0,m.start())+1,'function':enclosing(index,m.start()),
             'expression':m[0]}for m in PATTERN.finditer(source)]


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,default=d.ROOT/'build/nested-call-destination-screen.json')
    args=ap.parse_args();files={};found=[]
    for path in sorted((d.ROOT/'src').rglob('*')):
        if path.suffix not in ('.c','.cpp'):continue
        source=path.read_text();rel=str(path.relative_to(d.ROOT))
        files[rel]=hashlib.sha256(path.read_bytes()).hexdigest()
        found.extend(dict(row,path=rel)for row in sites(source))
    root=d.ROOT/'build/gcc3-boundary-transfer/V21r_int'
    direct=(root/'result/V21r_int.c').read_text()
    cached=(root/'result-cache/V21r_int.c').read_text()
    a=[r for r in sites(direct)if r['function']=='DemodDataV21']
    b=[r for r in sites(cached)if r['function']=='DemodDataV21']
    assert len(a)==1 and not b
    report={'revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=d.ROOT,text=True).strip(),
            'source_hashes':files,'sites':found,'counts':{'source_files':len(files),'sites':len(found),
            'functions':len({(r['path'],r['function'])for r in found}),'files_with_sites':len({r['path']for r in found})},
            'controls':{'direct_V21_positive':a,'explicit_cache_negative':len(b)},
            'scope':'Text nominations only; original before/after-call pointer age requires native def/use review.'}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['counts'],indent=2));print('Controls: direct V21 fires once; explicit cache clears')
    for r in found:print(r['path'],r['line'],r['function'],r['expression'].replace('\n',' '))


if __name__=='__main__':main()
