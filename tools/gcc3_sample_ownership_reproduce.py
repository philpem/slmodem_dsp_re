#!/usr/bin/env python3
"""Four full-TU sister controls for post-call level ownership and late narrowing."""
import itertools,re,json
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 cells={}
 for v90,v92 in itertools.product((False,True),repeat=2):
  text=source
  for enabled,method,count in [(v90,'generateV90Symbol',3),(v92,'generateV92Symbol',5)]:
   if not enabled:continue
   start,end,fn=d.function(text,'V90Phase3Modulator::'+method)
   pattern=r'\t\tsample = scrambledSymbol\(this,\s*(vectorBit\((\w+), symbolCount\)|0)\);'
   def replace(match):
    arg=match.group(2)+'[(symbolCount - 1u) % 72u]' if match.group(2) else '0'
    return ('\t\t{\n\t\t\tint scrambled = scrambler.process('+arg+');\n'
            '\t\t\tunsigned int level = (unsigned short)codeLevel;\n'
            '\t\t\tpolarity ^= scrambled;\n'
            '\t\t\tif (!polarity)\n\t\t\t\tlevel = -level;\n'
            '\t\t\tsample = (short)level;\n\t\t}')
   fn,n=re.subn(pattern,replace,fn);assert n==count,(method,n)
   text=text[:start]+fn+text[end:]
  label='baseline' if not(v90 or v92) else 'v90-%d-v92-%d'%(v90,v92)
  cells[label]=text
 assert len(set(cells.values()))==4
 return cells

def audit():
 from gcc3_value_carriers_audit import inspect,function_chunk
 from gcc3_alignment_vector_audit import normalized_rtl
 root=d.ROOT/'build'/d.OUT_NAME;family=root/'V90Phase3Modulator'
 cells=json.loads((root/'results.json').read_text())['families']['V90Phase3Modulator']['cells']
 bp=family/'baseline/candidate.o';base=inspect(bp);reports={}
 emitted={n for n,v in base['records'].items() if v[0]=='STT_FUNC' and v[3]!='SHN_UNDEF'}
 data={n for n,v in base['records'].items() if v[0]=='STT_OBJECT' and v[3]!='SHN_UNDEF'}
 assert len(emitted)==28 and len(data)==1
 for label,cell in cells.items():
  path=family/label/'candidate.o';got=inspect(path)
  for key in ('records','allocated','nobits','relocations'):assert got[key]==base[key],(label,key)
  assert set(cell['functions'])==emitted
  changed=[n for n in sorted(emitted) if d.b.body(str(path),n)!=d.b.body(str(bp),n)]
  assert changed==cell.get('changed_bodies',[])
  reports[label]={'changed':changed,'unchanged_emitted_bystanders':len(emitted)-len(changed),'all_ELF_controls_equal':True,
                 'all_emitted_function_count':len(emitted),'common_function_count':len(cell['verdicts']),
                 'data_definitions':len(data),'gains':cell.get('gains',[]),'losses':cell.get('losses',[]),
                 'target_sizes':{n:d.b.sizes(str(path))[n] for n in emitted if 'generateV90Symbol' in n or 'generateV92Symbol' in n}}
 stages={}
 for needle in ('::generateJdNot(','Scrambler<T, I>::Scrambler('):
  stages[needle]={}
  for stage in ('01.rtl','20.combine','25.greg','26.postreload','35.mach'):
   chunks=[function_chunk(family/label/('V90Phase3Modulator.cpp.'+stage),needle) for label in ('baseline','v90-1-v92-1')]
   same=normalized_rtl(chunks[0])==normalized_rtl(chunks[1]);assert same==(stage!='35.mach')
   stages[needle][stage]=same
 report={'full_TUs':len(cells),'all_emitted_body_verdicts':len(cells)*len(emitted),'emitted_functions_each':len(emitted),'data_definitions_each':len(data),
         'reports':reports,'bystander_first_stage':stages,'no_production_adoption':True}
 (root/'complete-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('audit:',report['all_emitted_body_verdicts'],'emitted body verdicts; all ELF controls agree')

if __name__=='__main__':
 d.REV='14769433';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
 d.OUT_NAME='gcc3-sample-ownership';d.DUMP_FLAGS=('-v','-save-temps','-da')
 d.variants=variants;d.main();audit()
