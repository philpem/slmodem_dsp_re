#!/usr/bin/env python3
"""Census debug relocation anchors; gaps are triage, not proof of missing source."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

def inventory(path):
    result={}
    with path.open('rb') as stream:
        elf=ELFFile(stream);table=elf.get_section_by_name('.symtab')
        functions=[s for s in table.iter_symbols() if s['st_info']['type']=='STT_FUNC' and isinstance(s['st_shndx'],int)]
        for symbol in functions:
            result[symbol.name]={'size':symbol['st_size'],'calls':0,'tail_jumps':0,'other_anchors':0}
        for section in elf.iter_sections():
            if not isinstance(section,RelocationSection):continue
            code=elf.get_section(section['sh_info']).data()
            for relocation in section.iter_relocations():
                if table.get_symbol(relocation['r_info_sym']).name!='dsplibs_debug_printf':continue
                off=relocation['r_offset']
                kind='calls' if off and code[off-1]==0xe8 else 'tail_jumps' if off and code[off-1]==0xe9 else 'other_anchors'
                for symbol in functions:
                    if symbol['st_shndx']==section['sh_info'] and symbol['st_value']<=off<symbol['st_value']+symbol['st_size']:
                        result[symbol.name][kind]+=1
    return result

def total(record):return sum(record[k] for k in ('calls','tail_jumps','other_anchors'))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects',type=Path,default=driver.ROOT/'build/tc_out')
    parser.add_argument('--blob',type=Path,default=Path(driver.b.BLOB))
    parser.add_argument('--exclude',type=Path,help='JSON list of source/header paths; source objects excluded from triage')
    parser.add_argument('--min-size',type=int,default=1)
    parser.add_argument('--max-size',type=int,default=2500)
    parser.add_argument('--json-out',type=Path)
    parser.add_argument('--known-before',type=Path,help='Run call_delete missing-hook positive control against this archived object')
    parser.add_argument('--known-after',type=Path,help='Paired original-hook restoration object for the positive control')
    args=parser.parse_args()
    assert args.min_size>0 and args.max_size>=args.min_size
    before=inventory(args.blob);exclude=set()
    if args.exclude:exclude={p.replace('/','_')+'.o' for p in json.loads(args.exclude.read_text())}
    rows=[];objects=sorted(args.objects.glob('*.o'));compared=0;eligible=0
    assert objects,'empty object denominator'
    for path in objects:
        after=inventory(path)
        for name,q in after.items():
            if name not in before:continue
            compared+=1;p=before[name]
            if path.name in exclude or not args.min_size<=p['size']<=args.max_size:continue
            eligible+=1
            if total(p)!=total(q):rows.append({'object':path.name,'symbol':name,'blob':p,'ours':q})
    report={'objects':len(objects),'compared_object_symbols':compared,'eligible_object_symbols':eligible,'mismatching_counts':len(rows),'counts_are_source_hypotheses':False,'rows':rows}
    if args.known_before or args.known_after:
        assert args.known_before and args.known_after,'positive control needs both archived objects'
        p=inventory(args.known_before)['call_delete'];q=inventory(args.known_after)['call_delete']
        assert total(p)==0 and total(q)==total(before['call_delete'])==1
        report['known_hook_control']={'before':p,'after':q,'blob':before['call_delete'],'passed':True}
        print('Known call_delete hook control:0→1, blob1; detector fires')
    for row in rows:print(row['object'],row['symbol'],'anchors',total(row['blob']),'/',total(row['ours']))
    print('Debug anchors:',len(objects),'objects;',compared,'compared object-symbol entries;',eligible,'size/ownership-eligible;',len(rows),'count mismatches')
    print('Inlining, duplicated CFG arms and shared tail jumps can change counts without changing source statements.')
    if args.json_out:args.json_out.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
