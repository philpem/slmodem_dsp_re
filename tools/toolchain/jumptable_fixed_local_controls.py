#!/usr/bin/env python3
"""Real-ELF controls for fixed-frame local reads and original ABI save identity."""
import hashlib
import argparse
import sys
import types
import json
import subprocess
from pathlib import Path
TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/jumptable-fixed-local-controls')
parser.add_argument('--native', type=Path)
parser.add_argument('--candidate', type=Path)
parser.add_argument('--symbol', default='AnalyseDialString')
args = parser.parse_args()
if (args.native is None) != (args.candidate is None):
    parser.error('--native and --candidate must be supplied together')
OUT = args.output_dir.resolve()
OUT.mkdir(parents=True, exist_ok=True)
# Execute the captured tool bytes, and make byteident use this same module.
TOOL_SOURCE = (TOOLS / 'jumptable.py').read_bytes()
p = types.ModuleType('jumptable')
p.__file__ = str(TOOLS / 'jumptable.py')
sys.modules['jumptable'] = p
exec(compile(TOOL_SOURCE, p.__file__, 'exec'), p.__dict__)
BYTEIDENT_SOURCE = (TOOLS / 'byteident.py').read_bytes()
b = types.ModuleType('byteident')
b.__file__ = str(TOOLS / 'byteident.py')
sys.modules['byteident'] = b
exec(compile(BYTEIDENT_SOURCE, b.__file__, 'exec'), b.__dict__)
PARSED_INPUTS = []
class CapturedPath:
    def __init__(self, path): self.path = Path(path)
    def read_bytes(self):
        data = self.path.read_bytes()
        PARSED_INPUTS.append({'path':str(self.path),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)})
        return data
p.Path = CapturedPath
FIX = OUT / 'controls'
FIX.mkdir(exist_ok=True)
SOURCE = '''.text
.globl sample
.type sample,@function
sample:
sub $28,%esp
mov %ebx,12(%esp)
mov %edx,%ebx
mov %esi,16(%esp)
mov %edi,20(%esp)
mov %ebp,24(%esp)
mov $-2,%edx
mov %edx,8(%esp)
mov %ebx,(%esp)
call helper
mov 32(%esp),%ecx
cmp $1,%eax
ja .Ldefault
jmp *.Ltable(,%eax,4)
.Lfirst:
mov 8(%esp),%eax
sete %al
sbb %ecx,%ecx
jmp .Lexit
.Lsecond:
mov %edi,8(%esp)
mov 8(%esp),%eax
jmp .Lexit
.Ldefault:
xor %eax,%eax
.Lexit:
mov 12(%esp),%ebx
mov 16(%esp),%esi
mov 20(%esp),%edi
mov 24(%esp),%ebp
add $28,%esp
ret
.size sample,.-sample
.type helper,@function
.section .rodata
.p2align 2
.Ltable:
.long .Lfirst,.Lsecond
.section .note.GNU-stack,"",@progbits
'''

def assemble(label, source):
    asm = FIX / (label+'.s'); obj = FIX / (label+'.o')
    asm.write_text(source)
    subprocess.run(['as','--32','-o',str(obj),str(asm)],check=True,capture_output=True)
    return obj

positive = {
    'baseline': SOURCE,
    'rebased': SOURCE.replace('.text\n','.text\n.space 16,0x90\n').replace('.Ltable:\n','.space 32\n.Ltable:\n'),
    'local-callee-destination': SOURCE.replace('mov 8(%esp),%eax','mov 8(%esp),%edi'),
    'local-loop': SOURCE.replace('sete %al','test %edx,%edx\njne .Lfirst\nsete %al'),
    'higher-block-before-save-without-clobber': SOURCE.replace('mov %ebx,12(%esp)','jmp .Llate\n.Lsave:\nmov %ebx,12(%esp)').replace('.Ldefault:','.Llate:\nnop\njmp .Lsave\n.Ldefault:'),
    'target-swap': SOURCE.replace('.long .Lfirst,.Lsecond','.long .Lsecond,.Lfirst'),
}
for condition in ('o','no','b','ae','e','ne','be','a','s','ns','p','np','l','ge','le','g'):
    for register in ('al','cl','dl','bl','ah','ch','dh','bh'):
        positive['setcc-'+condition+'-'+register] = SOURCE.replace('sete %al','set'+condition+' %'+register)

negative = {
    'duplicate-save': SOURCE.replace('mov %ebx,12(%esp)','mov %ebx,12(%esp)\nmov %ebx,12(%esp)'),
    'save-after-register-write': SOURCE.replace('mov %ebx,12(%esp)','inc %ebx\nmov %ebx,12(%esp)'),
    'higher-block-clobber-before-first-save': SOURCE.replace('mov %ebx,12(%esp)','jmp .Llateclobber\n.Lsave:\nmov %ebx,12(%esp)').replace('.Ldefault:','.Llateclobber:\ninc %ebx\njmp .Lsave\n.Ldefault:'),
    'higher-block-byte-clobber-before-first-save': SOURCE.replace('mov %ebx,12(%esp)','jmp .Llateclobber\n.Lsave:\nmov %ebx,12(%esp)').replace('.Ldefault:','.Llateclobber:\nsete %bl\njmp .Lsave\n.Ldefault:'),
    'higher-block-argument-clobber-before-first-save': SOURCE.replace('mov %ebx,12(%esp)','jmp .Llateclobber\n.Lsave:\nmov %ebx,12(%esp)').replace('.Ldefault:','.Llateclobber:\nmov 32(%esp),%ebx\njmp .Lsave\n.Ldefault:'),
    'outgoing-save-overwrite': SOURCE.replace('mov %ebx,(%esp)','mov %ebx,12(%esp)'),
    'different-register-save-overwrite': SOURCE.replace('mov %ebx,(%esp)','mov %eax,12(%esp)'),
    'immediate-save-overwrite': SOURCE.replace('mov %ebx,(%esp)','movl $1,12(%esp)'),
    'partial-save-overwrite': SOURCE.replace('mov %ebx,(%esp)','movw %bx,12(%esp)'),
    'unaligned-partial-overlap': SOURCE.replace('mov %ebx,(%esp)','mov %ebx,10(%esp)'),
    'save-wrong-owner-read': SOURCE.replace('mov 8(%esp),%eax','mov 12(%esp),%eax'),
    'return-write': SOURCE.replace('mov %ebx,(%esp)','mov %ebx,28(%esp)'),
    'return-read': SOURCE.replace('mov 8(%esp),%eax','mov 28(%esp),%eax'),
    'local-unaligned-read': SOURCE.replace('mov 8(%esp),%eax','mov 7(%esp),%eax'),
    'local-partial-read': SOURCE.replace('mov 8(%esp),%eax','movw 8(%esp),%ax'),
    'local-byte-read': SOURCE.replace('mov 8(%esp),%eax','movb 8(%esp),%al'),
    'local-partial-extension-read': SOURCE.replace('mov 8(%esp),%eax','movzwl 8(%esp),%eax'),
    'local-partial-write': SOURCE.replace('mov %edx,8(%esp)','movw %dx,8(%esp)'),
    'local-uninitialized': SOURCE.replace('mov %edx,8(%esp)','nop'),
    'local-initialization-bypass': SOURCE.replace('mov %edx,8(%esp)','test %edx,%edx\nje .Lskipinit\nmov %edx,8(%esp)\n.Lskipinit:'),
    'local-read-before-initialization': SOURCE.replace('mov %edx,8(%esp)','mov 8(%esp),%eax\nmov %edx,8(%esp)'),
    'local-branch-reads-before-init': SOURCE.replace('mov %edx,8(%esp)','test %edx,%edx\nje .Lbadread\nmov %edx,8(%esp)').replace('.Lfirst:','.Lbadread:\n.Lfirst:'),
    'local-read-only-postdispatch-write': SOURCE.replace('mov %edx,8(%esp)','nop').replace('.Lfirst:','.Lfirst:\nmov %edx,8(%esp)'),
    'alias-esp-base': SOURCE.replace('sete %al','mov %esp,%eax'),
    'alias-frame-address': SOURCE.replace('sete %al','lea 8(%esp),%eax'),
    'alias-indexed-store': SOURCE.replace('sete %al','mov %eax,8(%esp,%ecx,4)'),
    'alias-indexed-read': SOURCE.replace('sete %al','mov 8(%esp,%ecx,4),%eax'),
    'extra-allocation': SOURCE.replace('sete %al','sub $28,%esp'),
    'changed-stack-depth': SOURCE.replace('sete %al','sub $4,%esp'),
    'partial-stack-pointer': SOURCE.replace('sete %al','inc %sp'),
    'missing-restore': SOURCE.replace('mov 12(%esp),%ebx','nop'),
    'scratch-after-restore': SOURCE.replace('add $28,%esp','mov 8(%esp),%ebx\nadd $28,%esp'),
    'scratch-unsaved-destination': SOURCE.replace('mov %edi,20(%esp)','nop').replace('mov 20(%esp),%edi','nop').replace('mov 8(%esp),%eax','mov 8(%esp),%edi'),
    'callee-byte-write-after-restore': SOURCE.replace('add $28,%esp','sete %bl\nadd $28,%esp'),
    'callee-sbb-after-restore': SOURCE.replace('add $28,%esp','sbb %ebx,%ebx\nadd $28,%esp'),
    'save-bypass': SOURCE.replace('mov %ebx,12(%esp)','jmp .Lskipsave\nmov %ebx,12(%esp)\n.Lskipsave:'),
    'early-return-before-saves': SOURCE.replace('mov %ebx,12(%esp)','test %eax,%eax\nje .Learly\nmov %ebx,12(%esp)').replace('.Ldefault:','.Learly:\nret\n.Ldefault:'),
    'early-release-return-before-saves': SOURCE.replace('mov %ebx,12(%esp)','test %eax,%eax\nje .Learly\nmov %ebx,12(%esp)').replace('.Ldefault:','.Learly:\nadd $28,%esp\nret\n.Ldefault:'),
    'early-release': SOURCE.replace('sete %al','add $28,%esp'),
    'partial-guard': SOURCE.replace('cmp $1,%eax','cmp $1,%ax'),
    'signed-guard': SOURCE.replace('ja .Ldefault','jg .Ldefault'),
    'wrong-index': SOURCE.replace('jmp *.Ltable(,%eax,4)','jmp *.Ltable(,%ecx,4)'),
    'selector-clobber': SOURCE.replace('ja .Ldefault','ja .Ldefault\ninc %eax'),
    'guard-bypass': SOURCE.replace('.Lfirst:','.Lfirst:\njmp .Lbypass').replace('jmp *.Ltable','.Lbypass:\njmp *.Ltable'),
    'guard-entry-alias': SOURCE.replace('jmp *.Ltable','.globl bypass\nbypass:\njmp *.Ltable'),
    'setcc-local-store': SOURCE.replace('sete %al','sete 8(%esp)'),
    'setcc-save-store': SOURCE.replace('sete %al','sete 12(%esp)'),
    'setcc-return-store': SOURCE.replace('sete %al','sete 28(%esp)'),
    'setcc-heap-store': SOURCE.replace('sete %al','sete (%ebx)'),
    'setcc-operand-prefix': SOURCE.replace('sete %al','.byte 0x66,0x0f,0x94,0xc0'),
    'setcc-address-prefix': SOURCE.replace('sete %al','.byte 0x67,0x0f,0x94,0xc0'),
    'setcc-bad-extension': SOURCE.replace('sete %al','.byte 0x0f,0x94,0xc8'),
    'unsupported-implicit-callee-clobber': SOURCE.replace('sete %al','cpuid'),
    'return-immediate': SOURCE.replace('\nret\n','\nret $4\n'),
    'indirect-call': SOURCE.replace('call helper','call *%eax'),
}

records = {}; objects = {}
for label, source in {**positive, **negative}.items():
    obj = assemble(label,source); objects[label] = obj
    try:
        proof = p.prove(str(obj),'sample'); accepted=True; reason=None
    except p.Refused as error:
        proof=None; accepted=False; reason=str(error)
    expected = label in positive
    if accepted != expected: raise AssertionError((label,accepted,reason))
    body = b.body(str(obj),'sample')
    grade = b.verdict(*body,*body)[0]
    if grade != ('EXACT' if expected else 'UNRESOLVED'): raise AssertionError((label,grade))
    data=obj.read_bytes()
    records[label]={'accepted':accepted,'reason':reason,'self_grade':grade,'object_sha256':hashlib.sha256(data).hexdigest(),'proof':proof}
baseline=b.body(str(objects['baseline']),'sample')
comparisons={}
for label, expected in [('rebased','EXACT'),('target-swap','RELOC')]:
    grade=b.verdict(*baseline,*b.body(str(objects[label]),'sample'))[0]
    if grade!=expected:raise AssertionError((label,grade,expected))
    comparisons[label]=grade

real = None
if args.native is not None:
    # Retain exactly the immutable object bytes supplied to the parser.
    for label, path in [('native', args.native), ('candidate', args.candidate)]:
        (OUT / (label + '.o')).write_bytes(path.read_bytes())
    native = p.prove(str(OUT / 'native.o'), args.symbol)
    candidate = p.prove(str(OUT / 'candidate.o'), args.symbol)
    if native['identity'] != candidate['identity']:
        raise AssertionError('real table identity mismatch')
    real_grade = b.verdict(*b.body(str(OUT / 'native.o'), args.symbol),
                           *b.body(str(OUT / 'candidate.o'), args.symbol))
    if real_grade != ('EXACT', 0):
        raise AssertionError(real_grade)
    real = {'native': native, 'candidate': candidate, 'verdict': real_grade}
result = {'positive_count': len(positive), 'refusal_count': len(negative),
          'controls': records, 'comparisons': comparisons, 'real': real,
          'parsed_inputs': PARSED_INPUTS,
          'tool_sha256': hashlib.sha256(TOOL_SOURCE).hexdigest(),
          'byteident_sha256': hashlib.sha256(BYTEIDENT_SOURCE).hexdigest(),
          'assembler': subprocess.check_output(['as', '--version'], text=True)}
(OUT / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'positive_count': len(positive), 'refusal_count': len(negative),
                  'comparisons': comparisons,
                  'real_verdict': real['verdict'] if real else None,
                  'real_table_entries': real['native']['count'] if real else None,
                  'tool_sha256': result['tool_sha256']}))
