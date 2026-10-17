#!/usr/bin/env python3
"""Replay coefficient-accumulator / imaginary-history owner controls.

Frozen c11823619 baseline is the retained 317-byte split-span implementation.
The four cells test an explicit coefficient initialization boundary crossed
with the original EDI=q+0xa0 imaginary-history acquisition. The statement
boundary folds away; only the witnessed base acquisition is retained.
This does not reopen the earlier nine-cell spelling domain.

  python3 tools/recovery_v34_center_owners.py --domain center-owners-four-cells \
      --baseline-dir build/next293-baseline
  python3 tools/recovery_v34_center_owners.py --audit

Use a frozen Gentoo canonical baseline. The shared driver requires raw full-TU
reproduction, bug reproduction and header identity, and records every body.
"""
import sys
import json
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'V34EqualizerCenterAdapt')
 staged=fn
 for part in ['re','im']:
  old='\t\ttap = (int)((unsigned)((unsigned)*'+part+' << 16)'
  new='\t\ttap = (int)((unsigned)*'+part+' << 16);\n\t\ttap = (int)((unsigned)tap'
  assert staged.count(old)==2
  staged=staged.replace(old,new)
 cells={'baseline':source,'coefficient-accumulator-owner':source[:a]+staged+source[z:]}
 for label,text in list(cells.items()):
  a,z,fn=d.function(text,'V34EqualizerCenterAdapt')
  old='\tshort *im = q->im + V34_EQ_CENTRE_FIRST;'
  assert fn.count(old)==1
  fn=fn.replace(old,old+'\n\tshort *dly_im = q->dly_im;').replace('q->dly_im[j]','dly_im[j]')
  cells[label+'-imaginary-history-owner']=text[:a]+fn+text[z:]
 assert len(cells)==len(set(cells.values()))==4
 return cells

def function_bytes(path):
 from elftools.elf.elffile import ELFFile
 with path.open('rb') as stream:
  elf=ELFFile(stream)
  symbol=next(s for s in elf.get_section_by_name('.symtab').iter_symbols()
              if s.name=='V34EqualizerCenterAdapt')
  section=elf.get_section(symbol['st_shndx'])
  return section.data()[symbol['st_value']:symbol['st_value']+symbol['st_size']]

def audit():
 from gcc3_value_carriers_audit import inspect
 root=d.ROOT/'build/center-owners-293-cross'
 result=json.loads((root/'results.json').read_text())
 assert result['revision']=='c118236196b1e6a6bd4f7aeafd6458cea1ad544c'
 assert 'DSPLIB_REPRODUCE_BUGS' in result['config']
 cells=result['families']['v34filters']['cells']
 expected={'baseline','coefficient-accumulator-owner',
           'baseline-imaginary-history-owner',
           'coefficient-accumulator-owner-imaginary-history-owner'}
 assert set(cells)==expected
 base=root/'v34filters/baseline/candidate.o'
 metadata=inspect(base);metadata.pop('text_positions')
 assert cells['baseline']['baseline_reproduced']
 changed={'V34EqualizerCenterAdapt','V34EchoPreFilter',
          'V34ModulatorProcess','V34SetupModulator'}
 rows={}
 for label,cell in cells.items():
  path=root/'v34filters'/label/'candidate.o'
  observed=inspect(path);observed.pop('text_positions')
  assert observed==metadata,(label,'metadata/data/relocation difference')
  assert len(cell['functions'])==len(cell['verdicts'])==26
  owner=label.endswith('imaginary-history-owner')
  want=changed if owner else set()
  assert set(cell.get('changed_bodies',[]))==want,label
  assert not cell.get('gains',[]) and not cell.get('losses',[]),label
  assert cell['verdicts']['V34EqualizerCenterAdapt']==['SIZE',11 if owner else 30],label
  actual={name for name in cell['functions']
          if d.b.body(str(path),name)!=d.b.body(str(base),name)}
  assert actual==want,label
  rows[label]={'changed':sorted(want),'gains':[],'losses':[]}
 for left,right in [('baseline','coefficient-accumulator-owner'),
                    ('baseline-imaginary-history-owner',
                     'coefficient-accumulator-owner-imaginary-history-owner')]:
  assert (root/'v34filters'/left/'candidate.o').read_bytes()==(root/'v34filters'/right/'candidate.o').read_bytes()
 original=function_bytes(Path(d.b.BLOB))
 candidate=function_bytes(root/'v34filters/baseline-imaginary-history-owner/candidate.o')
 assert (len(original),len(candidate))==(347,336)
 # A bounded original operand/instruction proof, separate from byteident score:
 # both loop bodies agree literally except the original's three-byte dead store.
 for a,b in [(0x64,0x64),(0xd0,0xc8)]:
  old=original[a:a+93];new=candidate[b:b+90]
  assert old[56:59]==bytes.fromhex('890424')
  assert old[:56]+old[59:]==new
  bad=bytearray(new);bad[0]^=1
  assert old[:56]+old[59:]!=bad,'changed instruction escaped loop control'
 assert original[0xcb:0xd0]==bytes.fromhex('908d742600')
 report={'cells':rows,'body_verdicts':104,
         'metadata_data_nontext_relocations_equal':True,
         'loop_bodies_equal_except_original_dead_store':2,
         'changed_loop_controls_detected':2,
         'statement_boundary_objects_raw_identical':True}
 (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('4 cells / 104 body verdicts: no exact gains/losses; metadata/data/relocations equal; 2/2 original loop proofs; 2/2 changed-loop controls detected')

if __name__=='__main__':
 if sys.argv[1:]==['--audit']:
  audit();sys.exit(0)
 d.REV='c118236196b1e6a6bd4f7aeafd6458cea1ad544c'
 d.OUT_NAME='center-owners-293-cross'
 d.SOURCE_PATHS=('src/pump/v34/v34filters.c',)
 d.DUMP_FLAGS=('-v','-save-temps','-da')
 d.variants=variants
 d.main()
