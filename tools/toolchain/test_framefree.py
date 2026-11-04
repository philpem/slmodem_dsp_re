#!/usr/bin/env python3
"""ELF-backed positive and adversarial controls for framefree REGALLOC."""
import argparse
import itertools
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

import byteident as b
from framefree import CALLEE

BODY = '''
mov 0x14(%esp),%ebp
mov 0x18(%esp),%edi
xor %esi,%esi
mov 0x1c(%esp),%ebx
test %ebp,%ebp
je done
loop:
mov (%edi),%eax
add %eax,%esi
mov %esi,(%ebx)
add $4,%edi
dec %ebp
jne loop
mov %esi,%eax
done:
'''


class FramefreeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.serial = itertools.count()
        cls.accepted = cls.refused = 0

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()
        print("framefree certificate controls: %d accepted, %d refused, %d total" %
              (cls.accepted, cls.refused, cls.accepted + cls.refused))

    def elf(self, body=BODY, mapping=None, prefix=None, suffix=None):
        if mapping:
            body = re.sub(r'%(ebp|edi|esi|ebx)\b', lambda m: '%' + mapping[m[1]], body)
        prefix = prefix if prefix is not None else '\n'.join('push %' + r for r in CALLEE)
        suffix = suffix if suffix is not None else '\n'.join('pop %' + r for r in reversed(CALLEE)) + '\nret'
        p = Path(self.temp.name) / str(next(self.serial))
        p.with_suffix('.s').write_text('.text\n.globl probe\n.type probe,@function\nprobe:\n' +
                                      prefix + '\n' + body + '\n' + suffix + '\n.size probe,.-probe\n')
        subprocess.run(['as', '--32', '-o', str(p.with_suffix('.o')), str(p.with_suffix('.s'))],
                       check=True, capture_output=True)
        return b.insns(str(p.with_suffix('.o')), 'probe')

    def proof(self, a, c):
        result = b.framefree_certificate(a, c)
        if result is None:
            type(self).refused += 1
        else:
            type(self).accepted += 1
        return result

    def test_all_fixed_permutations(self):
        a = self.elf()
        n = 0
        for perm in itertools.permutations(CALLEE):
            mapping = dict(zip(CALLEE, perm))
            if all(k == v for k, v in mapping.items()):
                continue
            c = self.elf(mapping=mapping)
            self.assertIsNotNone(self.proof(a, c), mapping)
            self.assertIsNone(b.alpha_why(a, c))
            n += 1
        self.assertEqual(n, 23)

    def test_unsupported_and_unsafe_forms(self):
        # Apply the same hazard to BOTH sides: correspondence alone would pass.
        hazards = {
            'frame setup': 'mov %esp,%ebp\n',
            'stack address escape': 'lea 4(%esp),%edi\n',
            'stack copy escape': 'mov %esp,%eax\n',
            'push stack pointer': 'push %esp\nadd $4,%esp\n',
            'pop into stack pointer': 'pop %esp\n',
            'partial stack write': 'mov (%esp),%sp\n',
            'partial push': 'push %ax\nadd $4,%esp\n',
            'stack memory push': 'pushl (%edi)\nadd $4,%esp\n',
            'wrapping stack adjustment': 'sub $0xfffffff0,%esp\npush %edi\nadd $0xfffffff4,%esp\n',
            'saved stack overwrite': 'mov %eax,(%esp)\n',
            'x87 saved stack overwrite': 'fstps (%esp)\n',
            'indirect branch': 'jmp *%eax\n',
            'call': 'call external\n',
            'implicit registers': 'mull %ecx\n',
            'string instruction': 'movsl\n',
            'x87 environment': 'fnstenv (%edi)\n',
            'alternate return': 'ret\n',
            'enter leave': 'enter $0,$0\nleave\n',
            'stack restore bypass': 'add $4,%esp\n',
            'stack index': 'mov (%esp,%eax,4),%edi\n',
            'segment override': 'mov %fs:(%edi),%eax\n',
            'incoming value': 'mov %edi,%eax\n',
            'partial initialization': 'mov $0,%di\nmov %edi,%eax\n',
            'read modify write initialization': 'add $1,%edi\n',
            'self dependent definition': 'lea 4(%edi),%edi\n',
            'definition bypass': 'test %eax,%eax\nje bypass\nmov $1,%edi\nbypass:\nmov %edi,%eax\n',
            'loop only definition': 'jmp use\ninit:\nmov $1,%edi\nuse:\nmov %edi,%eax\njne init\n',
            'unequal stack join': 'test %eax,%eax\nje joined\npush %eax\njoined:\n',
            'save prefix entry': 'jmp probe\n',
            'middle restore entry': 'jmp done+1\n',
            'middle save entry': 'jmp probe+1\n',
            'nonboundary entry': '.byte 0xeb,0x01\nmov $0x12345678,%eax\n',
        }
        mapping = dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1]))
        for name, hazard in hazards.items():
            with self.subTest(name=name):
                a = self.elf(hazard + BODY)
                c = self.elf(hazard + BODY, mapping)
                self.assertIsNone(self.proof(a, c))

    def test_changed_semantics(self):
        mapping = dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1]))
        a = self.elf()
        changes = {
            'branch destination': BODY.replace('je done', 'je loop'),
            'condition': BODY.replace('jne loop', 'je loop'),
            'constant': BODY.replace('add $4', 'add $8'),
            'displacement': BODY.replace('(%edi),%eax', '4(%edi),%eax'),
            'width': BODY.replace('mov (%edi),%eax', 'movw (%edi),%ax'),
            'opcode': BODY.replace('add %eax,%esi', 'sub %eax,%esi'),
            'pinned return': BODY.replace('mov %esi,%eax', 'mov %esi,%edx'),
            'narrow changed register': BODY.replace('dec %ebp', 'dec %bp'),
            'relocation': BODY.replace('mov (%edi),%eax', 'mov external,%eax'),
            'x87 operand': BODY.replace('mov (%edi),%eax', 'fld %st(3)\nmov (%edi),%eax'),
        }
        for name, body in changes.items():
            with self.subTest(name=name):
                self.assertIsNone(self.proof(a, self.elf(body, mapping)))
        for prefix, suffix in [('push %edi\npush %ebp\npush %esi\npush %ebx', None),
                               (None, 'pop %ebx\npop %edi\npop %esi\npop %ebp\nret')]:
            self.assertIsNone(self.proof(a, self.elf(mapping=mapping, prefix=prefix, suffix=suffix)))

    def test_stack_temporaries(self):
        body = BODY.replace('mov %esi,%eax', 'push %ebp\nfildl (%esp)\nadd $4,%esp\nmov %esi,%eax')
        mapping = dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1]))
        self.assertIsNotNone(self.proof(self.elf(body), self.elf(body, mapping)))

    def test_relocation_identity(self):
        mapping = dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1]))
        body = BODY.replace('mov (%edi),%eax', 'mov external,%eax')
        a = self.elf(body)
        self.assertIsNotNone(self.proof(a, self.elf(body, mapping)))
        self.assertIsNone(self.proof(a, self.elf(body.replace('external', 'different'), mapping)))
        self.assertIsNone(self.proof(a, self.elf(body.replace('external', 'external+4'), mapping)))

    def test_metadata_required(self):
        a = self.elf()
        c = self.elf(mapping=dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1])))
        self.assertIsNone(self.proof(list(a), c))
        self.assertIsNone(self.proof(a, list(c)))

    def test_zero_memory_only(self):
        mapping = dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1]))
        a = self.elf(BODY.replace('(%edi),%eax', '0(%edi),%eax'))
        c = self.elf(mapping=mapping)
        self.assertIsNotNone(self.proof(a, c))
        self.assertIsNone(self.proof(a, self.elf(BODY.replace('(%edi),%eax', '0,%eax'), mapping)))

    def test_gp_shape_descriptor_is_complete(self):
        self.assertEqual(set(b.GP_REGISTER_SHAPE), set(b.REG32))
        for name, shape in [('al', (8, 0)), ('ah', (8, 8)), ('ax', (16, 0)),
                            ('eax', (32, 0)), ('sp', (16, 0)), ('esp', (32, 0))]:
            self.assertEqual(b.gp_register_shape(name), shape)
        self.assertIsNone(b.gp_register_shape('st'))
        self.assertIsNone(b.gp_register_shape('xmm0'))

    def test_register_shape_use_destination_and_rmw_controls(self):
        positive = [('al', 'cl'), ('ah', 'ch'), ('ax', 'cx'), ('eax', 'ecx')]
        negative = [('al', 'ah'), ('al', 'ax'), ('ax', 'eax'),
                    ('al', 'ch'), ('ah', 'cl'), ('ax', 'cl'), ('ah', 'cx')]
        templates = [('mov', '%{r},(%esi)'), ('mov', '(%esi),%{r}'),
                     ('add', '$0x1,%{r}')]
        accepted = refused = 0
        for want, pairs in ((True, positive), (False, negative)):
            for left, right in pairs:
                for mnemonic, operands in templates:
                    with self.subTest(want=want, left=left, right=right, mnemonic=mnemonic, operands=operands):
                        a = [(mnemonic, operands.format(r=left))]
                        c = [(mnemonic, operands.format(r=right))]
                        reason = b._alpha_live_why(a, c)
                        self.assertEqual(reason is None, want)
                        self.assertEqual(b.alpha_equal(a, c), want)
                        if want:
                            accepted += 1
                        else:
                            self.assertIn('SHAPE', reason)
                            refused += 1
        self.assertEqual((accepted, refused), (12, 21))
        print('GP shape token controls: 12 accepted, 21 refused, 33 total')

    def test_real_elf_shapes_and_saved_frame_fallback(self):
        # The fixed saves/restores and dominated callee definitions make the
        # unchanged narrow operand eligible for framefree. Caller byte/word
        # tokens must still stay literal; no ABI return exception is involved.
        mapping = dict(zip(CALLEE, CALLEE[1:] + CALLEE[:1]))
        negative = [('al', 'ah'), ('al', 'ax'), ('ax', 'eax'),
                    ('al', 'ch'), ('ah', 'cl'), ('ax', 'cl'), ('ah', 'cx')]
        for token in ('al', 'ah', 'ax', 'eax'):
            body = BODY.replace('xor %esi,%esi', 'mov %'+token+',(%edi)\nxor %esi,%esi')
            a, c = self.elf(body), self.elf(body, mapping)
            self.assertIsNotNone(self.proof(a, c), token)
            self.assertTrue(b.alpha_equal(a, c), token)
        for left, right in negative:
            body = BODY.replace('xor %esi,%esi', 'mov %'+left+',(%edi)\nxor %esi,%esi')
            changed = body.replace('mov %'+left+',(%edi)', 'mov %'+right+',(%edi)')
            a = self.elf(body)
            for allocation in (None, mapping):
                with self.subTest(left=left, right=right, allocation=allocation):
                    c = self.elf(changed, allocation)
                    self.assertIsNotNone(b._alpha_live_why(a, c))
                    self.assertIsNone(self.proof(a, c))
                    self.assertIsNotNone(b.alpha_why(a, c))
                    self.assertFalse(b.alpha_equal(a, c))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--actual', help='optional recovered AGC object')
    args, remaining = parser.parse_known_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(FramefreeTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    actual_ok = True
    if args.actual:
        symbol = '_ZN3AgcIfE7processEPKfPfj'
        proof = b.framefree_certificate(b.insns(b.BLOB, symbol), b.insns(args.actual, symbol))
        print('actual AGC fixed-permutation proof:', proof)
        actual_ok = proof is not None
    raise SystemExit(0 if result.wasSuccessful() and actual_ok else 1)
