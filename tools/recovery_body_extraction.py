#!/usr/bin/env python3
"""Real ELF extraction regression and immutable old/new full-census control.

No source compilation or identity-rule extension. Existing period objects are
copied before comparison; overlapping-symbol negatives are actual ELF edits.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from collections import Counter
from types import SimpleNamespace
import playbook_small_patterns as d
from unittest.mock import patch
from elftools.elf.elffile import ELFFile

SYMBOL = '_Z17getSegmentPointer7PcmTypei'
OBJECT = 'src_pump_v90_V90DilDescriptorSettings.cpp.o'
BASE = 'c7cf4f8cd'
IDENTITY_SCOPE = 'extraction-only'
PROVENANCE_PATHS = (Path(__file__),d.ROOT/'tools/toolchain/byteident.py',
                    d.ROOT/'tools/toolchain/readonly_copy.py')


def validate_census(report):
    assert not report['verdict_changes'] and not report['body_changes'], (
        report['verdict_changes'],report['body_changes'])


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def previous(out):
    path = out / 'byteident-before.py'
    path.write_bytes(subprocess.check_output(['git','show',BASE+':tools/toolchain/byteident.py'],cwd=d.ROOT))
    spec = importlib.util.spec_from_file_location('byteident_before',path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def mutations(path, out, old):
    raw = path.read_bytes()
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        symbols = list(tab.iter_symbols())
        target = next(s for s in symbols if s.name == SYMBOL)
        other_index = next(i for i,s in enumerate(symbols) if s.name != SYMBOL and
            s['st_info']['type'] == 'STT_FUNC' and s['st_shndx'] == target['st_shndx'])
        entry = tab['sh_offset'] + other_index * tab['sh_entsize']
        start, size = target['st_value'], target['st_size']
    report = {}
    for label, value, length in [('interior-owner',start+0x32,1),
                                  ('overlapping-owner',start-1,size+2),
                                  ('exact-extent-alias',start,size)]:
        changed = bytearray(raw)
        changed[entry+4:entry+12] = struct.pack('<II',value,length)
        mutant = out / (label+'.o')
        mutant.write_bytes(changed)
        before = old.body(str(mutant),SYMBOL)
        after = d.b.body(str(mutant),SYMBOL)
        row = {'sha256':digest(mutant),'old_same_pair_verdict':old.verdict(*before,*before),
            'new_same_pair_verdict':d.b.verdict(*after,*after),
            'new_body_present':after[0] is not None}
        if label == 'exact-extent-alias':
            assert after == d.b.body(str(path),SYMBOL), row
            assert row['new_same_pair_verdict'] == ('UNRESOLVED',1)
        else:
            assert after == (None,None), row
            assert row['new_same_pair_verdict'] == ('NODATA',0)
        report[label] = row
    assert report['interior-owner']['old_same_pair_verdict'] == ('EXACT',0), report
    # Keep the original demonstration too: moving a full-sized owner into the
    # copy corridor makes objdump select unrelated bytes and omit relocations.
    changed = bytearray(raw)
    changed[entry+4:entry+8] = struct.pack('<I',start+0x32)
    mutant = out / 'original-counterexample.o'
    mutant.write_bytes(changed)
    before, after = old.body(str(mutant),SYMBOL), d.b.body(str(mutant),SYMBOL)
    assert old.verdict(*before,*before) == ('EXACT',0)
    assert after == (None,None) and d.b.verdict(*after,*after) == ('NODATA',0)
    report['original-counterexample'] = {'sha256':digest(mutant),
        'old_same_pair_verdict':['EXACT',0],'new_same_pair_verdict':['NODATA',0]}
    return report


def constructor_alias(path, out):
    names = ['_ZN16V92EchoCancellerC%dEP13V92Parametersjj' % n for n in (1,2)]
    with path.open('rb') as stream:
        tab = ELFFile(stream).get_section_by_name('.symtab')
        symbols = list(tab.iter_symbols())
        first = next(s for s in symbols if s.name == names[0])
        index = next(i for i,s in enumerate(symbols) if s.name == names[1])
        offset = tab['sh_offset'] + index * tab['sh_entsize'] + 4
        value, size = first['st_value'], first['st_size']
    raw = bytearray(path.read_bytes())
    raw[offset:offset+8] = struct.pack('<II',value,size)
    mutant = out/'constructor-exact-extent-alias.o'
    mutant.write_bytes(raw)
    expected = d.b.body(str(path),names[0])
    assert expected[0] is not None
    assert all(d.b.body(str(mutant),name) == expected for name in names)
    return {'sha256':digest(mutant),'names':names,'bytes':len(expected[0]),'accepted_aliases':2}


def extraction_controls(path):
    section, start, raw, _ = d.b.owned_function(str(path),SYMBOL)
    command = ['objdump','-dr','--section='+section,'--start-address='+str(start),
               '--stop-address='+str(start+len(raw)),str(path)]
    text = subprocess.check_output(command,text=True)
    lines = text.splitlines()
    first = next(i for i,line in enumerate(lines) if d.b.INSN.match(line))
    last = max(i for i,line in enumerate(lines) if d.b.INSN.match(line))
    wrong_address = lines[:]
    old_address = d.b.INSN.match(lines[first])[1]
    wrong_address[first] = lines[first].replace(old_address+':',hex(start+1)[2:]+':',1)
    wrong_bytes = lines[:]
    wrong_bytes[first] = lines[first].replace('\tfc ', '\t90 ',1)
    cases = {'wrong-address':'\n'.join(wrong_address),
        'truncated-bytes':'\n'.join(lines[:last]+lines[last+1:]),
        'changed-extracted-byte':'\n'.join(wrong_bytes),
        'missing-relocation':'\n'.join(line for line in lines if not d.b.RELOC.match(line)),
        'extra-relocation':text+'\n%x: R_386_32 .rodata\n' % (start+1)}
    rejected = []
    for label,output in cases.items():
        d.b.body.cache_clear()
        with patch.object(d.b.subprocess,'run',return_value=SimpleNamespace(stdout=output,returncode=0)):
            assert d.b.body(str(path),SYMBOL) == (None,None), label
        rejected.append(label)
    d.b.body.cache_clear()
    assert d.b.body(str(path),SYMBOL)[0] == raw
    return {'real_object':str(path),'rejected':rejected,'valid_output_restored':True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects',type=Path,required=True)
    parser.add_argument('--out',type=Path,default=d.ROOT/'build/body-extraction')
    parser.add_argument('--focused-only',action='store_true')
    args = parser.parse_args()
    if IDENTITY_SCOPE == 'extraction-only':
        import readonly_copy
        d.b.body.cache_clear()
        try:
            with patch.object(readonly_copy,'prove',side_effect=readonly_copy.Refused('extraction-only audit')):
                run(args)
        finally:
            d.b.body.cache_clear()
    else:
        run(args)


def run(args):
    apparatus_before = {str(p.relative_to(d.ROOT)):digest(p) for p in PROVENANCE_PATHS}
    def provenance(report):
        after = {str(p.relative_to(d.ROOT)):digest(p) for p in PROVENANCE_PATHS}
        report['apparatus_hashes_before'] = apparatus_before
        report['apparatus_hashes_after'] = after
        assert apparatus_before == after, 'apparatus changed during observation'
    out = args.out.resolve()
    out.mkdir(parents=True,exist_ok=True)
    frozen = out/'objects'
    frozen.mkdir(exist_ok=True)
    manifest = out/'input-manifest.json'
    if not manifest.exists():
        paths = sorted(args.objects.resolve().glob('*.o'))
        assert len(paths) == 300, ('incomplete canonical input',len(paths))
        inputs = {p.name:digest(p) for p in paths}
        for path in paths:
            shutil.copy2(path,frozen/path.name)
        config = (args.objects/'.build-config').read_text()
        assert 'gcc-3.4.2-gentoo2005' in config and '-DDSPLIB_REPRODUCE_BUGS' in config
        (frozen/'.build-config').write_text(config)
        assert inputs == {p.name:digest(p) for p in paths}, 'canonical changed during snapshot'
        manifest.write_text(json.dumps({'source':str(args.objects.resolve()),'comparator_baseline_revision':BASE,
            'objects':inputs,'config_sha256':digest(frozen/'.build-config'),
            'reference_sha256':digest(Path(d.b.BLOB))},indent=2)+'\n')
    inputs = json.loads(manifest.read_text())
    if 'revision' in inputs:
        inputs['comparator_baseline_revision'] = inputs.pop('revision')
        manifest.write_text(json.dumps(inputs,indent=2)+'\n')
    assert inputs['comparator_baseline_revision'] == BASE
    assert inputs['source'] == str(args.objects.resolve()), 'choose a fresh --out for a different object directory'
    assert inputs['objects'] == {p.name:digest(p) for p in sorted(frozen.glob('*.o'))}
    assert digest(Path(d.b.BLOB)) == inputs['reference_sha256']
    assert digest(frozen/'.build-config') == inputs['config_sha256']
    old = previous(out)
    report = {'mutations':mutations(frozen/OBJECT,out,old),
        'constructor_alias':constructor_alias(frozen/'src_pump_v90_V92EchoCanceller.cpp.o',out),
        'extraction_controls':extraction_controls(frozen/OBJECT),'objects':len(inputs['objects']),
        'production_source_changed':False,'identity_rules_changed':IDENTITY_SCOPE != 'extraction-only',
        'identity_proof_scope':IDENTITY_SCOPE,
        'input_manifest_sha256':digest(manifest),'comparator_baseline_revision':BASE,
        'comparator_after_sha256':digest(d.ROOT/'tools/toolchain/byteident.py')}
    if not args.focused_only:
        originals = d.b.sizes(d.b.BLOB)
        owners = {}
        for path in sorted(frozen.glob('*.o')):
            for name in d.b.sizes(str(path)):
                if name in originals:
                    owners.setdefault(name,[]).append(str(path))
        rank = {'EXACT':0,'UNRESOLVED':1,'REGALLOC':2,'RELOC':3,'BYTES':4,'SIZE':5,'NODATA':6}
        counts = [Counter(),Counter()]
        changes, body_changes, copies, ledgers = [], [], 0, {}
        for i,name in enumerate(sorted(owners)):
            refs = [module.body(d.b.BLOB,name) for module in (old,d.b)]
            if refs[0] != refs[1]:
                body_changes.append(['reference',name,refs[0][0] is not None,refs[1][0] is not None])
            scores = [[],[]]
            rows = []
            for path in owners[name]:
                copies += 1
                candidates = [module.body(path,name) for module in (old,d.b)]
                if candidates[0] != candidates[1]:
                    body_changes.append([Path(path).name,name,candidates[0][0] is not None,candidates[1][0] is not None])
                verdicts = []
                for index,module in enumerate((old,d.b)):
                    value = module.verdict(*refs[index],*candidates[index])
                    if value[0] not in ('EXACT','UNRESOLVED','NODATA','RELOC') and module.alpha_equal(
                            module.insns(d.b.BLOB,name),module.insns(path,name)):
                        value = ('REGALLOC',0)
                    scores[index].append(value)
                    verdicts.append(value)
                rows.append({'object':Path(path).name,'before':verdicts[0],'after':verdicts[1]})
            worst = [max(values,key=lambda v:rank[v[0]]) for values in scores]
            for index,value in enumerate(worst):
                counts[index][value[0]] += 1
            if worst[0] != worst[1]:
                changes.append({'symbol':name,'before':worst[0],'after':worst[1]})
            ledgers[name] = rows
            if i % 200 == 0:
                print('census',i,'of',len(owners),flush=True)
        report.update(shared_symbols=len(owners),candidate_body_copies=copies,
            reference_symbols=len(originals),unrepresented_reference_symbols=sorted(set(originals)-set(owners)),
            before=dict(counts[0]),after=dict(counts[1]),verdict_changes=changes,
            body_changes=body_changes,body_ledger=ledgers)
        provenance(report)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        validate_census(report)
    else:
        provenance(report)
        (out/'focused-results.json').write_text(json.dumps(report,indent=2)+'\n')
    assert inputs['objects'] == {p.name:digest(p) for p in sorted(frozen.glob('*.o'))}
    print('body extraction:',len(report['mutations']),'real ELF controls; objects',report['objects'],
        'shared symbols',report.get('shared_symbols','not run'),
        'body copies',report.get('candidate_body_copies','not run'),flush=True)


if __name__ == '__main__':
    main()
