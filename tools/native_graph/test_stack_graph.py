import pathlib,sys,tempfile,subprocess,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import stack_graph as sg
class StackControls(unittest.TestCase):
    def extract(self,td,i,text):
        p=pathlib.Path(td)/str(i);p.with_suffix('.s').write_text('.text\n.globl probe\n.type probe,@function\nprobe:\n'+text+'\n.size probe,.-probe\n');subprocess.run(['as','--32','-o',str(p.with_suffix('.o')),str(p.with_suffix('.s'))],check=True,capture_output=True);return sg.extract(p.with_suffix('.o'),'probe')
    def pair(self,a,b):
        with tempfile.TemporaryDirectory() as td:
            x,y=self.extract(td,0,a),self.extract(td,1,b);return sg.compare(x,y),x,y
    def test_layout_register_rename_negative(self):
        a='sub $0x40,%esp\nmovl $0,0x20(%esp)\ncall callee\nmovl $20,0x20(%esp)\ncmpw $0,0x20(%esp)\nadd $0x40,%esp\nret'
        b=a.replace('0x40','0x50').replace('0x20','0x30')
        c,x,y=self.pair(a,b);self.assertEqual(len(x['cells']),1);self.assertEqual(c['matched_anchors'],3);self.assertEqual(c['relations_compared'],3);self.assertFalse(c['findings']);self.assertFalse(c['unpaired'])
    def test_call_order_positive(self):
        a='sub $0x40,%esp\nmovl $0,0x20(%esp)\ncall callee\nmovl $20,0x20(%esp)\ncmpw $0,0x20(%esp)\nadd $0x40,%esp\nret'
        b=a.replace('movl $0,0x20(%esp)\ncall callee','call callee\nmovl $0,0x20(%esp)')
        c,x,y=self.pair(a,b);self.assertTrue(c['findings'])
    def test_store_branch_arm_positive(self):
        prefix='sub $0x40,%esp\ncall callee\ntest %eax,%eax\n'
        a=prefix+'movl $0,0x20(%esp)\nje yes\njmp done\nyes: movl $20,0x20(%esp)\ndone: cmpw $0,0x20(%esp)\nadd $0x40,%esp\nret'
        b=prefix+'je yes\nmovl $0,0x20(%esp)\njmp done\nyes: movl $20,0x20(%esp)\ndone: cmpw $0,0x20(%esp)\nadd $0x40,%esp\nret'
        c,x,y=self.pair(a,b);self.assertTrue(c['findings'])
    def test_ambiguous_cells_not_paired(self):
        a='sub $0x40,%esp\nmovl $0,0x20(%esp)\nmovl $0,0x24(%esp)\nadd $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(len(x['ambiguous_cells']),1);self.assertEqual(c['matched_anchors'],0)
    def test_overlapping_cells_excluded(self):
        a='sub $0x40,%esp\nmovl $0,0x20(%esp)\nmovb $0,0x21(%esp)\nadd $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(len(x['cells']),0)
    def test_dynamic_frame_refused(self):
        a='sub $0x40,%esp\nsub %eax,%esp\nadd $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(c['status'],'refused')
    def test_unbalanced_frame_return_refused(self):
        a='sub $0x40,%esp\nmovl $0,0x20(%esp)\nret'
        c,x,y=self.pair(a,a);self.assertEqual(c['status'],'refused');self.assertEqual(x['reason'],'unbalanced fixed-frame return')
    def test_partial_lane_writes_do_not_create_full_register_values(self):
        for op in ('xor %ax,%ax','xor %al,%al','mov %ax,%bx'):
            a='sub $0x40,%esp\ncall callee\n'+op+'\nmov %eax,0x20(%esp)\ntest %ebx,%ebx\nje done\nmovl $20,0x20(%esp)\ndone: cmpw $0,0x20(%esp)\nadd $0x40,%esp\nret'
            c,x,y=self.pair(a,a);self.assertEqual(x['cells'],[]);self.assertEqual(x['branches_supported'],0)
    def test_flag_join_not_paired_from_textual_predecessor(self):
        a='sub $0x40,%esp\ncall callee\ntest %eax,%eax\nje alternate\ncmp $1,%eax\njmp join\nalternate: cmp $2,%eax\njoin: jne done\nmovl $1,0x20(%esp)\ndone: add $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(x['branches_total'],2);self.assertEqual(x['branches_supported'],1)
    def test_relocated_direct_branch_refused(self):
        a='sub $0x40,%esp\n.byte 0xe9\n.long local-.-4\n.reloc .-4,R_386_PC32,external\nlocal: add $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(c['status'],'refused');self.assertIn('relocated direct',x['reason'])
    def test_restored_esp_access_not_classified_as_local_cell(self):
        a='sub $0x40,%esp\nmovl $0,0x20(%esp)\nadd $0x40,%esp\nmovl $20,0x20(%esp)\nret'
        c,x,y=self.pair(a,a);self.assertEqual(len(x['cells']),1);self.assertEqual(len(x['cells'][0]['events']),1)
    def test_implicit_and_multiple_outputs_invalidate_constants(self):
        for op in ('xchg %eax,%ebx','cpuid','fnstsw %ax'):
            a='sub $0x40,%esp\nmov $20,%eax\n'+op+'\nmov %eax,0x20(%esp)\nadd $0x40,%esp\nret'
            c,x,y=self.pair(a,a);self.assertEqual(x['cells'],[])
    def test_relocated_immediate_does_not_become_constant(self):
        a='sub $0x40,%esp\nmov $external,%ebx\nmov %ebx,0x20(%esp)\nadd $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(x['cells'],[]);self.assertEqual(len(x['relocated_noncall_instruction_sites']),1)
    def test_unknown_implicit_output_invalidates_call_result(self):
        a='sub $0x40,%esp\ncall external\nlahf\ntest %eax,%eax\nje done\nmovl $0,0x20(%esp)\ndone: add $0x40,%esp\nret'
        c,x,y=self.pair(a,a);self.assertEqual(x['branches_supported'],0)
    def test_bypassed_table_guard_refused(self):
        a='sub $0x40,%esp\njmp dispatch\nguard: cmp $1,%eax\nja done\ndispatch: jmp *table(,%eax,4)\ncase0: movl $0,0x20(%esp)\njmp done\ncase1: movl $1,0x20(%esp)\ndone: add $0x40,%esp\nret\n.size probe,.-probe\n.section .rodata\ntable: .long case0,case1\n.text'
        c,x,y=self.pair(a,a);self.assertEqual(c['status'],'refused');self.assertIn('guard bypass',x['reason'])
if __name__=='__main__':unittest.main()
