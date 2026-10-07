#!/usr/bin/env python3
"""Bounded read-only explicit long-double / SI FIST transfer inventory."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]

def source_functions(text):
    # Preserve offsets while excluding strings and comments from type evidence.
    clean=re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),text,flags=re.S)
    result=[]
    pattern=r'\n([A-Za-z_]\w*(?:::\w+)*)\([^;{}]*\)\s*(?:const\s*)?\{'
    for m in re.finditer(pattern,clean):
        depth=1;i=m.end()
        while depth and i<len(clean):
            depth+=(clean[i]=='{')-(clean[i]=='}');i+=1
        assert depth==0
        if re.search(r'\blong\s+double\b',clean[m.end():i]):
            result.append((m[1],text.count('\n',0,m.start())+2,clean[m.end():i]))
    return result

def assembly(path):
    text=subprocess.check_output(['objdump','-dw',str(path)],text=True)
    functions={};current=None
    for line in text.splitlines():
        m=re.match(r'^([0-9a-f]+) <([^>]+)>:$',line)
        if m:current=m[2];functions[current]=[]
        elif current:
            row=re.match(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(\S+)\s*(.*)$',line)
            if row:functions[current].append([int(row[1],16),row[2],row[3]])
    return functions

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects',type=Path,required=True);ap.add_argument('--census',type=Path,required=True)
    ap.add_argument('--output',type=Path,default=ROOT/'build/gcc-x87-transfer-screen.json');args=ap.parse_args()
    paths=sorted(p for p in (ROOT/'src').rglob('*') if p.suffix in ('.c','.cpp') and 'v34' not in p.parts and not p.name.startswith(('V34','vpcm')))
    exact=set(json.loads(args.census.read_text())['exact_symbols']);blob=assembly(d.b.BLOB)
    candidates=[];counts={'source_TUs':len(paths),'explicit_long_double_function_bodies':0,'mapped_emitted_bodies':0,'mapping_ambiguous':0,'unmapped_source_bodies':0,'nonexact_original_le800':0,'original_si_nonpop':0,'eligible':0}
    all_mappings=[]
    for p in paths:
        source=p.read_text();functions=source_functions(source)
        obj=args.objects/(str(p.relative_to(ROOT)).replace('/','_')+'.o')
        assert obj.exists(),('missing scoped object',str(p))
        if not functions:continue
        counts['explicit_long_double_function_bodies']+=len(functions)
        obj=args.objects/(str(p.relative_to(ROOT)).replace('/','_')+'.o')
        retained=assembly(obj)
        names=list(retained);demangled=subprocess.check_output(['c++filt']+names,text=True).splitlines()
        for name,line,body in functions:
            mapped=[n for n,dm in zip(names,demangled) if dm.startswith(name+'(')]
            if len(mapped)!=1:
                counts['mapping_ambiguous' if mapped else 'unmapped_source_bodies']+=1;all_mappings.append({'source':str(p.relative_to(ROOT)),'name':name,'symbols':mapped});continue
            symbol=mapped[0];counts['mapped_emitted_bodies']+=1
            if symbol in exact or symbol not in blob:continue
            size=d.b.sizes(d.b.BLOB).get(symbol,0)
            if not 0<size<=800:continue
            counts['nonexact_original_le800']+=1
            orig=blob[symbol];ours=retained[symbol]
            nonpop=[r for r in orig if r[1]=='fistl'];pop=[r for r in orig if r[1]=='fistpl']
            if not nonpop:continue
            counts['original_si_nonpop']+=1
            ours_nonpop=[r for r in ours if r[1]=='fistl'];ours_pop=[r for r in ours if r[1]=='fistpl']
            if len(ours_nonpop)>=len(nonpop) and len(ours_pop)<=len(pop):continue
            counts['eligible']+=1
            candidates.append({'source':str(p.relative_to(ROOT)),'line':line,'name':name,'symbol':symbol,'original_bytes':size,
                               'original_fistl':nonpop,'original_fistpl':pop,'retained_fistl':ours_nonpop,'retained_fistpl':ours_pop,
                               'original_instructions':orig,'retained_instructions':ours,
                               'calls_before_first_original_fistl':[r for r in orig if r[0]<nonpop[0][0] and r[1]=='call']})
    assert any(c['name']=='V90PreFilter::setParamEia6' for c in candidates),'known positive missed'
    args.output.write_text(json.dumps({'build_config':(args.objects/'.build-config').read_text(),'baseline_sha256':hashlib.sha256(args.census.read_bytes()).hexdigest(),'counts':counts,'candidates':candidates,'unmapped_or_ambiguous_source_mapping':all_mappings,
                                     'explicitly_excluded_source_paths':sorted(str(p.relative_to(ROOT)) for p in (ROOT/'src').rglob('*') if p.suffix in ('.c','.cpp') and p not in paths)},indent=2)+'\n')
    print(counts)
    for c in candidates:print(c['name'],c['original_bytes'],c['source'],c['line'],len(c['original_fistl']),len(c['retained_fistl']))
if __name__=='__main__':main()
