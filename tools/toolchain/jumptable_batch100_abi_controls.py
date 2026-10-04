#!/usr/bin/env python3
"""Static ELF controls for push-save cdecl jump tables; no program execution."""
import json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import playbook_small_patterns as d
import jumptable as p
import byteident as b
OUT=ROOT/'build/jumptable-batch100-abi-controls';OUT.mkdir(parents=True,exist_ok=True)
def fixture(label,change=None,padding=0,saves=('ebx',),frame=8):
 argument=frame+4*len(saves)+4
 prologue='\n'.join('push %'+r for r in saves)+f'\nsub ${frame},%esp'
 epilogue=f'add ${frame},%esp\n'+'\n'.join('pop %'+r for r in reversed(saves))+'\nret'
 body=f'''{prologue}
mov {argument}(%esp),%ebx
mov (%ebx),%eax
cmp $1,%eax
ja .Ldefault
jmp *.Ltable(,%eax,4)
.Lfirst:
mov %ebx,(%esp)
call helper
mov $1,%eax
jmp .Lexit
.Lsecond:
mov $message,(%esp)
mov %ebx,4(%esp)
call helper
mov $2,%eax
jmp .Lexit
.Ldefault:
xor %eax,%eax
.Lexit:
{epilogue}
'''
 if change:body=change(body)
 padding_source=(f'.type prefix,@function\nprefix:\n.space {padding},0x90\n.size prefix,.-prefix\n' if padding else '')
 source=f'''.text
{padding_source}
.globl sample
.type sample,@function
sample:
{body}
.size sample,.-sample
.type helper,@function
.section .rodata
.p2align 2
.Ltable:
.long .Lfirst,.Lsecond
.type message,@object
message:
.asciz "message"
.size message,.-message
.section .note.GNU-stack,"",@progbits
'''
 if label=='different-targets':source=source.replace('.long .Lfirst,.Lsecond','.long .Lsecond,.Lfirst')
 if label=='object-call':source=source.replace('.type helper,@function','.type helper,@object')
 if label=='local-unrelocated-call':source=source.replace('.type helper,@function','.type helper,@function\nhelper:\nret\n.size helper,.-helper')
 src=OUT/(label+'.s');obj=OUT/(label+'.o');src.write_text(source)
 command=['as','--32','-o',str(obj),str(src)]
 subprocess.run(command,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 return obj,command
positive={'baseline':{},'rebased':{'padding':32},'four-saves':{'saves':('ebx','esi','edi','ebp'),'frame':16}}
negative={
 'relocated-allocation':lambda s:s.replace('sub $8,%esp', '.Lallocation:\n.byte 0x81,0xec\n.long 8\n.reloc .Lallocation+2,R_386_32,message'),
 'relocated-release':lambda s:s.replace('add $8,%esp', '.Lrelease:\n.byte 0x81,0xc4\n.long 8\n.reloc .Lrelease+2,R_386_32,message'),
 'relocated-guard-bound':lambda s:s.replace('cmp $1,%eax', '.Lbound:\n.byte 0x81,0xf8\n.long 1\n.reloc .Lbound+2,R_386_32,message'),
 'relocated-argument-offset':lambda s:s.replace('mov 16(%esp),%ebx', '.Largument:\n.byte 0x8b,0x9c,0x24\n.long 16\n.reloc .Largument+3,R_386_32,message'),
 'relocated-stack-store-offset':lambda s:s.replace('mov $message,(%esp)', '.Lstore:\n.byte 0xc7,0x84,0x24\n.long 0\n.long 0\n.reloc .Lstore+3,R_386_32,message'),
 'relocated-opcode':lambda s:s.replace('mov $message,(%esp)', '.Lopcode:\nmov $0,(%esp)\n.reloc .Lopcode,R_386_32,message'),
 'caller-register-save':lambda s:s.replace('push %ebx','push %eax',1),
 'duplicate-save':lambda s:s.replace('push %ebx','push %ebx\npush %ebx',1),
 'unsaved-register-write':lambda s:s.replace('.Lfirst:', '.Lfirst:\nxor %esi,%esi'),
 'unsaved-argument-owner':lambda s:s.replace('mov 16(%esp),%ebx','mov 16(%esp),%edi'),
 'wrong-pop':lambda s:s.replace('pop %ebx','pop %esi'),
 'missing-pop':lambda s:s.replace('pop %ebx\n',''),
 'pop-before-release':lambda s:s.replace('add $8,%esp\npop %ebx','pop %ebx\nadd $8,%esp'),
 'unaligned-frame':lambda s:s.replace('sub $8,%esp','sub $6,%esp'),
 'wrong-release':lambda s:s.replace('add $8,%esp','add $4,%esp'),
 'frame-reallocation':lambda s:s.replace('.Lfirst:', '.Lfirst:\nsub $8,%esp'),
 'save-slot-write':lambda s:s.replace('mov %ebx,(%esp)','mov %ebx,8(%esp)'),
 'save-slot-read':lambda s:s.replace('mov 16(%esp),%ebx','mov 8(%esp),%ebx'),
 'return-address-read':lambda s:s.replace('mov 16(%esp),%ebx','mov 12(%esp),%ebx'),
 'unaligned-argument':lambda s:s.replace('mov 16(%esp),%ebx','mov 17(%esp),%ebx'),
 'frame-address-escape':lambda s:s.replace('.Lfirst:', '.Lfirst:\nlea (%esp),%eax'),
 'partial-argument-store':lambda s:s.replace('mov %ebx,(%esp)','movw %bx,(%esp)'),
 'indexed-stack-read':lambda s:s.replace('mov 16(%esp),%ebx','mov 16(%esp,%eax,4),%ebx'),
 'call-after-release':lambda s:s.replace('pop %ebx\nret','pop %ebx\ncall helper\nret'),
 'callee-save-clobber-after-pop':lambda s:s.replace('pop %ebx\nret','pop %ebx\nxor %ebx,%ebx\nret'),
 'wrong-call-addend':lambda s:s.replace('call helper','call helper+4'),
 'indirect-callback':lambda s:s.replace('call helper','call *(%ebx)',1),
 'object-call':lambda s:s,
 'local-unrelocated-call':lambda s:s,
 'external-tail-jump':lambda s:s.replace('call helper','jmp helper',1),
 'guard-bypass':lambda s:s.replace('.Lfirst:', '.Lfirst:\njmp .Lbypass').replace('jmp *.Ltable','.Lbypass:\njmp *.Ltable'),
 'return-immediate':lambda s:s.replace('\nret','\nret $4'),
}
records={};base=None
for label,options in positive.items():
 obj,command=fixture(label,**options);proof=p.prove(str(obj),'sample')
 if label=='baseline':base=proof
 if label=='rebased':assert proof['identity']==base['identity']
 records[label]={'accepted':True,'proof':proof,'command':command}
for label,change in negative.items():
 obj,command=fixture(label,change=change)
 try:p.prove(str(obj),'sample')
 except p.Refused as e:records[label]={'accepted':False,'reason':str(e),'command':command}
 else:raise AssertionError('accepted invalid ABI fixture: '+label)
assert len(records)==35 and sum(r['accepted'] for r in records.values())==3
baseline=b.body(str(OUT/'baseline.o'),'sample')
rebased=b.body(str(OUT/'rebased.o'),'sample')
assert b.verdict(*baseline,*rebased)[0]=='EXACT'
different,_=fixture('different-targets')
assert b.verdict(*baseline,*b.body(str(different),'sample'))[0]=='RELOC'
for label in negative:
 candidate=b.body(str(OUT/(label+'.o')),'sample')
 assert b.verdict(*candidate,*candidate)[0]=='UNRESOLVED',label
# The real original is the positive example that motivated this extension.
real=p.prove(str(ROOT/'ref/slmodemd/dsplibs.o'),'RxNextStateV29')
assert real['count']==5 and real['targets']==[106,151,207,269,321]
assert real['fixed_stack_frame']['direct_abi_calls']==7
(OUT/'results.json').write_text(json.dumps({'synthetic':records,'real':real,'integration':{'equal_pair':'EXACT','different_targets':'RELOC','invalid_identical_pairs':len(negative)}},indent=2)+'\n')
print('35 static ELF controls: 3 accepted,32 refused; rebased identity equal; real5-slot/7-call ordinary ABI table proved; integration1EXACT/1RELOC/32UNRESOLVED')
