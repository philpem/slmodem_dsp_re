#!/usr/bin/env python3
"""Audit the builder's exact ordered stores in an existing Gentoo period fixture.

No compilation or source variants are performed. Whole object hashes are
recorded, while only the two builder bodies are pinned: unrelated process or
static-table changes are allowed. Linked addresses come from ELF symbols;
store offsets are relative to those pinned bodies. Provenance checks require
32-bit static ELF and the recovered Gentoo compiler banner. The fixture and
input objects must remain unchanged throughout observation.
"""
import playbook_small_patterns as d
import argparse
import copy
import hashlib
import json
from itertools import product
from pathlib import Path
import shutil
import subprocess
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
NAME = '_ZN21V92ConvolutionEncoder25makeStateTtransitionTableEv'
GENTOO = 'GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)'
RUNTIME = 'GCC: (GNU) 3.3.5  (Gentoo Linux 3.3.5-r1, ssp-3.3.2-3, pie-8.7.7.1)'
PINS = {
    'original': ('da8c200c44743d6fb405f740d5fad3585c1effb4cc5b1bc77c545e6c4dfd77e2', 1558,
                 [(0x160, 'nextState'), (0x16c, 'output'), (0x3be, 'nextState'),
                  (0x3ca, 'output'), (0x581, 'nextState'), (0x58d, 'output')]),
    'candidate': ('d56e4d07ca1222320d2063c7092ccfd3192c148f5934c5aa8711457a592cf881', 1456,
                  [(0x14a, 'nextState'), (0x156, 'output'), (0x37f, 'nextState'),
                   (0x38b, 'output'), (0x520, 'nextState'), (0x52c, 'output')]),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def elf_info(path, symbol):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 32 and elf.little_endian and elf['e_machine'] == 'EM_386', path
        symbols = [s for s in elf.get_section_by_name('.symtab').iter_symbols()
                   if s.name == symbol and s['st_size']]
        assert len(symbols) == 1, (path, symbol, 'missing/ambiguous symbol')
        sym = symbols[0]
        section = elf.get_section(sym['st_shndx'])
        start = sym['st_value'] - section['sh_addr']
        code = section.data()[start:start + sym['st_size']]
        comments = sorted(set(s.decode() for s in elf.get_section_by_name('.comment').data().split(b'\0') if s))
        return {'address': sym['st_value'], 'code': code, 'comments': comments,
                'kind': elf['e_type'], 'interpreter': elf.get_section_by_name('.interp') is not None}


def expected(mode):
 out=[]
 if mode not in [0,1,2]:return out
 for bits in product(range(2),repeat=[6,8,10][mode]):
  if mode==0:
   s0,s1,s2,s3,i0,i1=bits;st=s0+2*s1+4*s2+8*s3;inp=i0+2*i1;y=(s2+i0)%2;ns=s3+2*((s0+s3+i1)%2+2*((s1+i1)%2+2*y))
  elif mode==1:
   t0,t1,t2,t3,t4,j0,j1,j2=bits;st=t0+2*t1+4*t2+8*t3+16*t4;inp=j0+2*j1+4*j2;y=(t3+j1)%2;ns=t4+2*((t0+j1)%2+2*((t1+j0)%2+2*((t2+j2)%2+2*y)))
  else:
   u0,u1,u2,u3,u4,u5,k0,k1,k2,k3=bits;st=u0+2*u1+4*u2+8*u3+16*u4+32*u5;inp=k0+2*k1+4*k2+8*k3;y=(u4+u2+k1)%2;ns=(u0+u1+u2*((u1+k0)%2)+k3)%2+2*((u0+u1+u3+k2+u2*k1)%2+2*((u1+k0+u2)%2+2*(u2+2*(u5+2*y))))
  assert 0<=ns<[16,32,64][mode]
  out += [['nextState',st*16+inp,ns],['output',ns*16+inp,y]]
 return out

def validate(records):
 assert len(records)==14
 counts={}
 for row in records:
  assert row['finished']
  want=expected(row['mode']);assert row['events']==want,(row['kind'],row['mode'],'store sequence mismatch')
  if row['mode']in[0,1,2]:
   per={name:len({x[1]for x in want if x[0]==name})for name in ['nextState','output']}
   assert set(per.values())=={[64,256,1024][row['mode']]}
   counts[row['mode']]={'events_per_function':len(want),'unique_entries_per_array':per,'max_index':max(x[1]for x in want)}
 assert {(v['kind'],v['mode'])for v in records}=={(kind,m)for kind in ['original','candidate']for m in [0,1,2,3,-1,7,65536]}
 return counts

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('fixture', nargs='?', type=Path, default=ROOT / 'build/period/t_v92convmapper')
    ap.add_argument('--period-object', type=Path,
                    help='defaults to src_pump_v90_V92ConvEncoder.o beside fixture')
    ap.add_argument('--reference-object', type=Path, default=Path(d.b.BLOB))
    ap.add_argument('--out', type=Path, default=ROOT / 'build/v92-builder-store-observed')
    args = ap.parse_args()
    fixture = args.fixture.resolve()
    period_object = (args.period_object or fixture.parent / 'src_pump_v90_V92ConvEncoder.o').resolve()
    reference = args.reference_object.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    before = {str(p): sha(p) for p in [fixture, period_object, reference]}
    snapshot = out / 't_v92convmapper-period'
    assert snapshot != fixture, 'output must not overwrite the supplied fixture'
    shutil.copy2(fixture, snapshot)
    assert sha(snapshot) == before[str(fixture)], 'fixture changed during snapshot'
    settings, evidence = {}, {}
    for kind, obj, symbol in [('original', reference, 'ref_' + NAME),
                              ('candidate', period_object, NAME)]:
        linked, source = elf_info(snapshot, symbol), elf_info(obj, NAME)
        assert linked['kind'] == 'ET_EXEC' and not linked['interpreter'], 'require static period fixture'
        assert GENTOO in linked['comments'] and set(linked['comments']) <= {GENTOO, RUNTIME}
        assert source['comments'] == [GENTOO], (kind, 'not recovered Gentoo object')
        assert linked['code'] == source['code'], (kind, 'fixture/object builder mismatch')
        expected_hash, size, sites = PINS[kind]
        assert len(linked['code']) == size and hashlib.sha256(linked['code']).hexdigest() == expected_hash, (kind, 'unreviewed builder body')
        settings[kind] = {'entry': linked['address'], 'sites': sites}
        evidence[kind] = {'object': str(obj), 'object_sha256': before[str(obj)],
                          'builder_sha256': expected_hash, 'builder_bytes': size,
                          'linked_address': linked['address'], 'object_comments': source['comments'],
                          'fixture_comments': linked['comments']}
    (out / 'store-settings.json').write_text(json.dumps(settings, indent=2) + '\n')
    command = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
               'source ' + str(ROOT / 'tools/v92_builder_store_gdb.py'), '--args', str(snapshot)]
    with (out / 'gdb.log').open('w') as log:
        subprocess.run(command, cwd=out, stdout=log, stderr=subprocess.STDOUT, check=True)
    trace = json.loads((out / 'store-observe.json').read_text())
    assert trace['exit_codes'] == [0] and not trace['errors'] and not trace['pending']
    counts = validate(trace['records'])
    for kind in ['changed-value', 'missing-store', 'swapped-order']:
        bad = copy.deepcopy(trace['records'])
        row = next(x for x in bad if x['mode'] == 2)
        if kind == 'changed-value':
            row['events'][0][2] ^= 1
        elif kind == 'missing-store':
            row['events'].pop()
        else:
            row['events'][0], row['events'][1] = row['events'][1], row['events'][0]
        try:
            validate(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('negative control accepted: ' + kind)
    assert sum(len(r['events']) for r in trace['records']) == 5376
    assert before == {p: sha(Path(p)) for p in before}, 'fixture or input object changed during observation'
    assert sha(snapshot) == before[str(fixture)], 'snapshot changed during observation'
    report = {'fixture': str(fixture), 'fixture_sha256': before[str(fixture)],
              'fixture_unchanged': True, 'input_hashes': before, 'evidence': evidence,
              'command': command, 'completed_calls': 14, 'store_events': 5376,
              'counts': counts, 'negative_controls': 3, 'production_source_changed': False}
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('14 completed calls / 5376 ordered stores match both bodies; 3/3 changed-trace controls rejected; fixture and objects unchanged')


if __name__ == '__main__':
    main()
