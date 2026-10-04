#!/usr/bin/env python3
"""Assembled fixed-frame proof controls; no reconstructed source changes."""
import argparse, json, subprocess
from pathlib import Path
import byteident as b
import jumptable as p
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/jumptable-batch50-stack-controls'
BASE='''.text
.globl sample
.type sample,@function
sample:
sub $8,%esp
mov %ebx,(%esp)
mov %esi,4(%esp)
mov 12(%esp),%ecx
cmp $1,%ecx
ja .Ldefault
jmp *.Ltable(,%ecx,4)
.Lcase0: mov $1,%ebx
jmp .Ldone
.Lcase1: mov $2,%ebx
jmp .Ldone
.Ldefault: mov $3,%ebx
.Ldone:
mov %ebx,%eax
mov 4(%esp),%esi
mov (%esp),%ebx
add $8,%esp
ret
.size sample,.-sample
.section .rodata,"a",@progbits
.Ltable:
.long .Lcase0,.Lcase1
'''
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument("--candidate",type=Path,default=ROOT/"build/tc_out/src_pump_v90_V90Phase3Modulator.cpp.o",help="complete period TU containing both exact Sd generators")
 args=ap.parse_args()
 OUT.mkdir(parents=True,exist_ok=True)
 cases={'baseline':(BASE,True), 'table-rebased':(BASE.replace('.Ltable:\n','.space 16\n.Ltable:\n'),True),
  'different-destination':(BASE.replace('.long .Lcase0,.Lcase1','.long .Lcase1,.Lcase0'),True)}
 negatives={
  'return-address-write':BASE.replace('mov %ebx,(%esp)','mov %ebx,8(%esp)'),
  'return-address-read':BASE.replace('mov 12(%esp),%ecx','mov 8(%esp),%ecx'),
  'saved-slot-clobber':BASE.replace('mov 12(%esp),%ecx','mov %ecx,(%esp)\nmov 12(%esp),%ecx'),
  'duplicate-save':BASE.replace('mov 12(%esp),%ecx','mov %ebx,(%esp)\nmov 12(%esp),%ecx'),
  'saved-selector-clobber':BASE.replace('cmp $1,%ecx','mov %ecx,4(%esp)\nmov 4(%esp),%ecx\ncmp $1,%ecx'),
  'saved-slot-wrong-owner':BASE.replace('mov 12(%esp),%ecx','mov (%esp),%ecx'),
  'wrong-restore-slot':BASE.replace('mov (%esp),%ebx','mov 4(%esp),%ebx'),
  'selector-after-guard':BASE.replace('ja .Ldefault','ja .Ldefault\nmov (%esp),%ecx'),
  'skip-save':BASE.replace('mov %ebx,(%esp)','jmp .Lafter\nmov %ebx,(%esp)\n.Lafter:'),
  'missing-restore':BASE.replace('mov (%esp),%ebx','nop'),
  'full-clobber-after-restore':BASE.replace('add $8,%esp','mov $4,%ebx\nadd $8,%esp'),
  'partial-clobber-after-restore':BASE.replace('add $8,%esp','mov $4,%bl\nadd $8,%esp'),
  'modified-before-save':BASE.replace('mov %ebx,(%esp)','inc %ebx\nmov %ebx,(%esp)'),
  'unbalanced-return':BASE.replace('add $8,%esp','nop'),
  'wrong-release':BASE.replace('add $8,%esp','add $4,%esp'),
  'release-before-restore':BASE.replace('mov 4(%esp),%esi','add $8,%esp\nmov 4(%esp),%esi').replace('mov (%esp),%ebx\nadd $8,%esp','mov (%esp),%ebx'),
  'second-frame-allocation':BASE.replace('cmp $1,%ecx','sub $8,%esp\ncmp $1,%ecx'),
  'partial-stack-pointer':BASE.replace('cmp $1,%ecx','mov %ax,%sp\ncmp $1,%ecx'),
  'frame-pointer-reassignment':BASE.replace('cmp $1,%ecx','mov %eax,%esp\ncmp $1,%ecx'),
  'frame-address-exposure':BASE.replace('cmp $1,%ecx','mov %esp,%eax\ncmp $1,%ecx'),
  'negative-slot':BASE.replace('mov %ebx,(%esp)','mov %ebx,-4(%esp)'),
  'unaligned-slot':BASE.replace('mov %esi,4(%esp)','mov %esi,2(%esp)'),
  'branch-unbalanced':BASE.replace('.Lcase1: mov $2,%ebx','.Lcase1: add $8,%esp\nmov $2,%ebx'),
  'frame-return-immediate':BASE.replace('\nret','\nret $4'),
  'unsupported-implicit-write':BASE.replace('cmp $1,%ecx','cpuid\ncmp $1,%ecx'),
 }
 cases.update({k:(v,False) for k,v in negatives.items()});results={};objects={}
 for label,(source,expected) in cases.items():
  asm=OUT/(label+'.s');obj=OUT/(label+'.o');asm.write_text(source)
  command=['as','--32','-o',str(obj),str(asm)]
  subprocess.run(command,check=True,capture_output=True)
  objects[label]=obj
  try: proof=p.prove(str(obj),'sample');accepted=True;reason=None
  except p.Refused as e:proof=None;accepted=False;reason=str(e)
  assert accepted==expected,(label,reason)
  results[label]={'proved':accepted,'proof':proof,'refusal':reason,'command':command}
  print(label,'PROVED' if accepted else 'REFUSED: '+reason)
 baseline=b.body(str(objects['baseline']),'sample')
 for label,obj in objects.items():
  grade=b.verdict(*baseline,*b.body(str(obj),'sample'))[0];results[label]['grade']=grade
  expected='EXACT' if label in ('baseline','table-rebased') else 'RELOC' if label=='different-destination' else None
  if expected:assert grade==expected,(label,grade)
  else:assert grade!='EXACT',(label,grade)
 real={}
 for name in ('_ZN18V90Phase3Modulator10generateSdEv','_ZN18V90Phase3Modulator13generateSdNotEv'):
  blob=ROOT/'ref/slmodemd/dsplibs.o';winner=args.candidate
  a=p.prove(str(blob),name);c=p.prove(str(winner),name);assert a['identity']==c['identity']
  assert b.verdict(*b.body(str(blob),name),*b.body(str(winner),name))==('EXACT',0)
  real[name]={'blob':a,'candidate':c,'grade':'EXACT'}
 (OUT/'results.json').write_text(json.dumps({'assembler':subprocess.check_output(['as','--version'],text=True),'synthetic_objects':len(cases),'accepted':3,'refused':len(negatives),'controls':results,'real':real},indent=2)+'\n')
 print(f'{len(cases)} assembled ELF controls: 3 accepted, {len(negatives)} refused; 2 real blob/candidate exact pairs')
if __name__=='__main__':main()
