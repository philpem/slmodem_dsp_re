#!/usr/bin/env python3
"""Audit return, tap-order, consumer and imported-count controls in full TUs."""
import hashlib
import json
import re
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies

HI = '_ZN11DescramblerIhiE7processEh'
II = '_ZN11DescramblerIiiE7processEi'
SHI = '_ZN9ScramblerIhiE7processEh'
V90 = '_ZN20V90Phase4Demodulator14getV90DecisionEs'
V92 = '_ZN20V90Phase4Demodulator14getV92DecisionEs'
ROOTS = ('gcc3-descrambler-output','gcc3-descrambler-output-all',
         'gcc3-descrambler-scalar-taps','gcc3-descrambler-scalar-all','gcc3-modem-bit-count',
         'gcc3-scrambler-scalar-order','gcc3-scrambler-scalar-all')


def noncode(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        names = {s.name for s in elf.iter_sections() if s['sh_flags'] & 2 and not s['sh_flags'] & 4}
    return {k:v for k,v in allocated(path).items() if k in names}


def call_edges(path, name):
    return [r['target'] for r in bodies(path)[name] if 'target' in r]


def caller_extensions(path):
    rows = bodies(path)[V90]
    result = []
    for i, row in enumerate(rows):
        if row.get('target') == HI:
            following = rows[i+1:]
            end = next(j for j,r in enumerate(following) if r['mnemonic'] == 'call')
            if following[end].get('target') == '_ZN5V90MP10bitsToInfoEi':
                result.append(any(r['mnemonic'] == 'movzbl' and r['operands'].startswith('%al,')
                                  for r in following[:end]))
    assert len(result) == 2
    return result


def main():
    report = {}; total = verdicts = 0
    for rootname in ROOTS:
        root = d.ROOT/'build'/rootname
        ledger = json.loads((root/'results.json').read_text())
        rows = {}
        for family, rec in ledger['families'].items():
            baseline = root/family/'baseline/candidate.o'
            assert baseline.read_bytes() == (root/family/'retained.o').read_bytes()
            base = inspect(baseline); data = noncode(baseline)
            for label, cell in rec['cells'].items():
                path = root/family/label/'candidate.o'
                assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
                current = inspect(path)
                assert current['records'] == base['records'], (rootname,family,label,'metadata')
                assert current['objects'] == base['objects'], (rootname,family,label,'named data')
                assert noncode(path) == data, (rootname,family,label,'allocated data')
                changed = [name for name in cell['functions'] if masked(path,name) != masked(baseline,name)]
                allowed = {HI,II,SHI,V90,V92} if rootname != 'gcc3-modem-bit-count' else {family+'_process'}
                assert set(changed) <= allowed, (rootname,family,label,changed)
                assert set(cell.get('gains',[])) <= {HI,II,SHI} and not cell.get('losses')
                for name in changed:
                    assert call_edges(path,name) == call_edges(baseline,name)
                if family == 'V90Phase4Demodulator':
                    full = label in ('return','both','output') or (rootname=='gcc3-scrambler-scalar-all' and label!='baseline')
                    assert caller_extensions(path) == [not full,not full]
                rows[family+'/'+label] = {'changed_bodies':changed,'gains':cell.get('gains',[]),
                                         'verdicts':cell['verdicts'],'metadata_data_calls_bystanders':'pass'}
                total += 1; verdicts += len(cell['verdicts'])
        report[rootname] = rows
        print(rootname, len(rows),'complete-TU cells audited',flush=True)
    taps = d.ROOT/'build/gcc3-descrambler-scalar-taps'
    for family,name in [('V90Phase3Demodulator',II),('V90Phase4Demodulator',HI)]:
        path = taps/family/'both/candidate.o'
        assert d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(path),name)) == ('EXACT',0)
        for label, expected in [('baseline',['pTap1','pTap2']),('both',['pTap2','pTap1'])]:
            text = (taps/family/label/(family+'.cpp.01.rtl')).read_text()
            blocks = [block for block in text.split(';; Function ')[1:]
                      if 'Descrambler<T, I>::process(T)' in block.splitlines()[0]]
            assert len(blocks) == 1
            stores = []
            for node in blocks[0].split('\n\n'):
                if '(set (mem/s:SI' in node:
                    stores += re.findall(r'<variable>\.(pTap[12])\+0',node)
            assert stores == expected, (family,label,stores)
    # Raw-inert controls distinguish use-only and declaration-only changes.
    first = d.ROOT/'build/gcc3-descrambler-output/V90Phase4Demodulator'
    assert (first/'baseline/candidate.o').read_bytes() == (first/'body/candidate.o').read_bytes()
    for family in ('b103','v23'):
        root = d.ROOT/'build/gcc3-modem-bit-count'/family
        assert (root/'baseline/candidate.o').read_bytes() == (root/'calls/candidate.o').read_bytes()
        assert (root/'baseline/candidate.o').read_bytes() == (root/'declaration/candidate.o').read_bytes()
    transfer=d.ROOT/'build/gcc3-scrambler-scalar-order'
    for family in ('V90Phase3Modulator','V92Phase3Modulator'):
        path=transfer/family/'tap2-first/candidate.o'
        assert d.b.verdict(*d.b.body(d.b.BLOB,SHI),*d.b.body(str(path),SHI)) == ('EXACT',0)
    control=transfer/'V92Phase4Modulator'
    assert (control/'baseline/candidate.o').read_bytes()==(control/'tap2-first/candidate.o').read_bytes()
    assert (total,verdicts) == (242,4820), (total,verdicts)
    report['denominator'] = {'full_TU_cells':total,'shared_verdicts':verdicts,'exact_functions':3}
    (d.ROOT/'build/descrambler-boundary-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(total,'cells /',verdicts,'shared verdicts; all controls pass; three exact scalar functions')


if __name__ == '__main__':
    main()
