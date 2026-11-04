#!/usr/bin/env python3
"""Assembler-built controls for diagnostic vectors, independent of grades."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
SPEC=importlib.util.spec_from_file_location('diagnostic_diffvector',ROOT/'tools/toolchain/diffvector.py')
v=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(v)


class DiagnosticVector(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='diffvector-')
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.number=0

    def assemble(self,body,prefix='',suffix='',symbol='sample'):
        self.number+=1
        src=self.root/('case%d.s'%self.number);obj=src.with_suffix('.o')
        src.write_text('.text\n'+prefix+'\n.globl '+symbol+'\n.type '+symbol+',@function\n'+symbol+':\n'+body+'\n.size '+symbol+',.-'+symbol+'\n'+suffix)
        subprocess.run(['as','--32',str(src),'-o',str(obj)],check=True,capture_output=True)
        return obj

    def pair(self,a,c,**kwargs):
        return v.report(a,[c],'sample',**kwargs)['copies'][0]

    def test_identity_and_count_invariants(self):
        a=self.assemble('mov 4(%esp),%eax\nadd $3,%eax\nret')
        r=self.pair(a,a)
        self.assertEqual(r['strict_identity']['grade'],'EXACT')
        for axis in r['axes'].values():
            self.assertEqual(axis['mismatched'],0)
            self.assertEqual(axis['reference_count'],axis['matched']+axis['mismatched']+axis['unknown']+axis['reference_only'])
            self.assertEqual(axis['candidate_count'],axis['matched']+axis['mismatched']+axis['unknown']+axis['candidate_only'])
        self.assertEqual(r['axes']['x87_registers']['reference_count'],0)
        self.assertEqual(r['axes']['x87_registers']['status'],'not_applicable')
        self.assertEqual(r['axes']['immediates']['reference_count'],1)
        self.assertEqual(r['axes']['addressing']['reference_count'],1)

    def test_placement_shift_is_separate(self):
        a=self.assemble('mov $3,%eax\nret')
        c=self.assemble('mov $3,%eax\nret',prefix='.type preceding,@function\npreceding: .space 19,0x90\n.size preceding,.-preceding')
        r=self.pair(a,c)
        self.assertEqual(r['strict_identity']['grade'],'EXACT')
        self.assertEqual(r['axes']['encoding']['mismatched'],0)
        self.assertEqual(r['axes']['placement']['mismatched'],1)

    def test_short_and_near_branch_same_successor(self):
        a=self.assemble('test %eax,%eax\nje end\nmov $3,%ecx\nend: ret')
        c=self.assemble('test %eax,%eax\n.byte 0x0f,0x84\n.long end-.-4\nmov $3,%ecx\nend: ret')
        r=self.pair(a,c)
        self.assertEqual(r['axes']['cfg_edges']['mismatched'],0)
        self.assertEqual(r['axes']['cfg_edges']['unknown'],0)
        self.assertGreater(r['axes']['encoding']['mismatched'],0)
        self.assertEqual(r['axes']['cfg_blocks']['mismatched'],0)

    def test_changed_successor_and_condition(self):
        template='test %eax,%eax\n{op} {target}\nmov $3,%ecx\nfirst: inc %ecx\nsecond: ret'
        a=self.assemble(template.format(op='je',target='first'))
        c=self.assemble(template.format(op='je',target='second'))
        r=self.pair(a,c)
        self.assertGreater(r['axes']['cfg_edges']['mismatched'],0)
        self.assertEqual(r['axes']['mnemonics']['mismatched'],0)
        c=self.assemble(template.format(op='jne',target='first'))
        self.assertGreater(self.pair(a,c)['axes']['cfg_edges']['mismatched'],0)

    def test_mid_instruction_target_is_unknown(self):
        a=self.assemble('.byte 0xeb,1\nmov $3,%eax\nret')
        r=self.pair(a,a)
        self.assertFalse(r['cfg']['reference']['complete'])
        self.assertGreater(r['axes']['cfg_edges']['unknown'],0)

    def test_indirect_pc32_relocation_is_not_direct_callee(self):
        a=self.assemble('call *(foo-.)\nret')
        r=self.pair(a,a)
        self.assertEqual(r['axes']['relocation_destinations']['matched'],1)
        self.assertEqual(r['axes']['cfg_edges']['unknown'],1)
        self.assertFalse(r['cfg']['reference']['complete'])

    def test_link_resolution_context_is_restored(self):
        a=self.assemble('ret')
        previous=v.b.LINK_RESOLUTION
        try:
            v.b.LINK_RESOLUTION=('external.o','manifest.txt')
            self.pair(a,a)
            self.assertEqual(v.b.LINK_RESOLUTION,('external.o','manifest.txt'))
        finally:
            v.b.LINK_RESOLUTION=previous

    def test_indirect_and_unsupported_control_unknown(self):
        for body in ['jmp *%eax','ud2','int $0x80\nret']:
            with self.subTest(body=body):
                a=self.assemble(body);r=self.pair(a,a)
                self.assertGreater(r['axes']['cfg_edges']['unknown'],0)
                self.assertTrue(r['unknown_evidence'])
                self.assertEqual(r['strict_identity']['grade'],'EXACT')

    def test_relocation_target_addend_and_site(self):
        a=self.assemble('mov foo,%eax\nret')
        for operand in ('bar','foo+4'):
            c=self.assemble('mov '+operand+',%eax\nret')
            r=self.pair(a,c)
            self.assertEqual(r['strict_identity']['diagnostic'],'RELOC')
            self.assertEqual(r['strict_identity']['grade'],'NONMATCH')
            self.assertEqual(r['axes']['relocation_destinations']['mismatched'],1)
        c=self.assemble('inc %ecx\nmov foo,%eax\nret')
        r=self.pair(a,c)
        self.assertEqual(r['axes']['relocation_destinations']['matched'],1)
        self.assertEqual(r['axes']['relocation_sites']['mismatched'],1)

    def test_unresolved_section_identity_never_matched(self):
        a=self.assemble('mov .data+4,%eax\nret',suffix='.data\n.space 12')
        r=self.pair(a,a)
        self.assertEqual(r['strict_identity']['grade'],'UNRESOLVED')
        self.assertEqual(r['axes']['relocation_destinations']['matched'],0)
        self.assertEqual(r['axes']['relocation_destinations']['unknown'],1)

    def test_displacement_scale_width_and_sign(self):
        cases=[('mov 4(%eax),%ecx','mov 8(%eax),%ecx','addressing'),
               ('mov (%eax,%edx,2),%ecx','mov (%eax,%edx,4),%ecx','addressing'),
               ('mov %ax,(%ebx)','mov %eax,(%ebx)','typed_operations'),
               ('movswl (%eax),%ecx','movzwl (%eax),%ecx','typed_operations'),
               ('mov (%eax,%eax,2),%edx','mov (%ebx,%ecx,2),%edx','addressing')]
        for left,right,axis in cases:
            with self.subTest(left=left,right=right):
                r=self.pair(self.assemble(left+'\nret'),self.assemble(right+'\nret'))
                self.assertGreater(r['axes'][axis]['mismatched'],0)

    def test_relocated_immediate_does_not_hide_field_offset(self):
        a=self.assemble('movl $foo,4(%eax)\nret')
        c=self.assemble('movl $foo,8(%eax)\nret')
        r=self.pair(a,c)
        self.assertEqual(r['axes']['addressing']['mismatched'],1)
        self.assertEqual(r['axes']['relocation_destinations']['matched'],1)
        self.assertEqual(r['axes']['immediates']['unknown'],1)

    def test_partial_register_lane_and_x87_literals(self):
        r=self.pair(self.assemble('mov %al,(%ebx)\nret'),self.assemble('mov %ah,(%ebx)\nret'))
        self.assertEqual(r['axes']['gp_registers']['mismatched'],1)
        self.assertEqual(r['axes']['typed_operations']['mismatched'],1)
        r=self.pair(self.assemble('fadd %st(1),%st\nret'),self.assemble('fadd %st(2),%st\nret'))
        self.assertEqual(r['axes']['x87_registers']['mismatched'],1)
        self.assertEqual(r['axes']['typed_operations']['unknown'],1)

    def test_insertion_and_repeated_anchor_ambiguity(self):
        a=self.assemble('mov $1,%eax\nmov $2,%edx\nret')
        c=self.assemble('mov $1,%eax\ninc %ecx\nmov $2,%edx\nret')
        r=self.pair(a,c)
        self.assertEqual(r['axes']['mnemonics']['candidate_only'],1)
        self.assertTrue(r['mapping']['ambiguous'])
        self.assertEqual(r['mapping']['uniqueness'],'not established')

    def test_padding_target_retained(self):
        a=self.assemble('jmp pad\nmov $3,%eax\npad: nop\nret')
        r=self.pair(a,a)
        self.assertEqual(r['reference']['padding_rows'],1)
        self.assertEqual(len(r['mapping']['pairs']),4)
        self.assertEqual(r['axes']['cfg_edges']['unknown'],0)

    def test_explicit_copy_scope_and_missing_input(self):
        a=self.assemble('ret');missing=self.root/'missing.o'
        result=v.report(a,[a,missing],'sample')
        self.assertEqual(result['provided_candidate_copies'],2)
        self.assertEqual(result['copies'][0]['strict_identity']['grade'],'EXACT')
        self.assertIsNone(result['copies'][1]['axes'])
        self.assertEqual(result['copies'][1]['strict_identity']['grade'],'NODATA')
        self.assertNotIn('grade',result)

    def test_malformed_or_missing_symbol(self):
        a=self.assemble('ret');bad=self.root/'bad.o';bad.write_bytes(b'not ELF')
        self.assertIsNone(self.pair(a,bad)['axes'])
        wrong=self.assemble('ret',symbol='other')
        self.assertIsNone(self.pair(a,wrong)['axes'])

    def test_json_output_cannot_alias_inputs(self):
        a=self.assemble('ret');before=a.read_bytes()
        for output in (a,self.root/'hardlink.o'):
            if output!=a:output.hardlink_to(a)
            run=subprocess.run([sys.executable,str(ROOT/'tools/toolchain/diffvector.py'),
                '--reference',str(a),'--candidate',str(a),'--symbol','sample','--json',str(output)],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0)
            self.assertIn('must not alias',run.stderr)
            self.assertEqual(a.read_bytes(),before)

    def test_cli_reports_every_copy_and_versions(self):
        a=self.assemble('ret');output=self.root/'report.json'
        run=subprocess.run([sys.executable,str(ROOT/'tools/toolchain/diffvector.py'),
            '--reference',str(a),'--candidate',str(a),'--candidate',str(a),
            '--symbol','sample','--json',str(output)],check=True,capture_output=True,text=True)
        result=json.loads(output.read_text())
        self.assertEqual(len(result['copies']),2)
        self.assertIn('objdump',result['tool_versions'])
        self.assertIn('2 supplied copies',run.stdout)
        self.assertIsNone(result['linked_resolution_artifacts'])


if __name__=='__main__':
    unittest.main(verbosity=2)
