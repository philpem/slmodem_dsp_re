import pathlib,sys,tempfile,subprocess,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import effect_graph as eg

class RealELFControls(unittest.TestCase):
    def pair(self,left,right):
        with tempfile.TemporaryDirectory() as td:
            results=[]
            for i,text in enumerate((left,right)):
                p=pathlib.Path(td)/str(i); p.with_suffix('.s').write_text('.text\n.globl probe\n.type probe,@function\nprobe:\n'+text+'\n.size probe,.-probe\n')
                subprocess.run(['as','--32','-o',str(p.with_suffix('.o')),str(p.with_suffix('.s'))],check=True,capture_output=True)
                results.append(eg.extract(p.with_suffix('.o'),'probe'))
            return eg.compare(*results),results
    def test_identity_and_register_rename(self):
        a='mov 4(%esp),%eax\nmov 8(%esp),%edx\nmovl $0,4(%eax)\ntest %edx,%edx\nje done\nmovl $1,8(%eax)\ndone: ret'
        b=a.replace('%eax','%ecx')
        for text in (a,b):
            c,r=self.pair(a,text); self.assertEqual(c['status'],'bounded'); self.assertGreater(c['relations_compared'],0); self.assertFalse(c['findings']); self.assertFalse(c['unpaired'])
    def test_branch_arm_store_positive(self):
        a='mov 4(%esp),%eax\nmov 8(%esp),%edx\nmovl $0,4(%eax)\ntest %edx,%edx\nje done\nmovl $1,8(%eax)\ndone: ret'
        b=a.replace('movl $0,4(%eax)\n','').replace('je done\n','je done\nmovl $0,4(%eax)\n')
        c,r=self.pair(a,b); self.assertTrue(any(f['kind']=='happens_before_or_branch_arm' for f in c['findings']))
    def test_call_store_positive(self):
        a='push %ebx\nmov 8(%esp),%ebx\nmovl $0,4(%ebx)\ncall external\npop %ebx\nret'
        b=a.replace('movl $0,4(%ebx)\ncall external','call external\nmovl $0,4(%ebx)')
        c,r=self.pair(a,b); self.assertTrue(c['findings'])
    def test_width_positive(self):
        a='mov 4(%esp),%eax\nmovw $0,4(%eax)\nret'
        c,r=self.pair(a,a.replace('movw','movl')); self.assertEqual(len(c['unpaired']),2)
    def test_owner_positive(self):
        a='mov 4(%esp),%eax\nmovl $0,4(%eax)\nret'
        c,r=self.pair(a,a.replace('0,4(%eax)','0,8(%eax)')); self.assertEqual(len(c['unpaired']),2)
    def test_independent_store_order_negative(self):
        a='mov 4(%esp),%eax\nmovl $1,4(%eax)\nmovl $2,8(%eax)\nret'
        b=a.replace('movl $1,4(%eax)\nmovl $2,8(%eax)','movl $2,8(%eax)\nmovl $1,4(%eax)')
        c,r=self.pair(a,b); self.assertFalse(c['findings']); self.assertFalse(c['unpaired'])
    def test_inverted_layout_negative(self):
        a='mov 4(%esp),%eax\nmov 8(%esp),%edx\ntest %edx,%edx\nje done\nmovl $1,8(%eax)\ndone: ret'
        b='mov 4(%esp),%ecx\nmov 8(%esp),%edx\ntest %edx,%edx\njne yes\nret\nyes: movl $1,8(%ecx)\nret'
        c,r=self.pair(a,b); self.assertFalse(c['findings']); self.assertFalse(c['unpaired'])
    def test_predicate_positive(self):
        a='mov 4(%esp),%eax\nmov 8(%esp),%edx\ncmp $4,%edx\njl done\nmovl $1,8(%eax)\ndone: ret'
        c,r=self.pair(a,a.replace('jl done','jb done')); self.assertTrue(any(f['kind']=='predicate' for f in c['findings']))
    def test_ambiguous_occurrences_refuse_pairing(self):
        a='mov 4(%esp),%eax\nmovl $0,4(%eax)\nmovl $0,4(%eax)\nret'
        c,r=self.pair(a,a); self.assertEqual(c['matched_anchors'],0); self.assertEqual(c['ambiguous_anchor_count'],1)
    def test_indirect_cfg_refusal(self):
        c,r=self.pair('mov 4(%esp),%eax\njmp *%eax','mov 4(%esp),%eax\njmp *%eax'); self.assertEqual(c['status'],'refused')
    def test_flag_join_not_paired_from_textual_predecessor(self):
        a='mov 4(%esp),%eax\nmov 8(%esp),%edx\ntest %edx,%edx\nje alternate\ncmp $1,%edx\njmp join\nalternate: cmp $2,%edx\njoin: jne done\nmovl $1,8(%eax)\ndone: ret'
        c,r=self.pair(a,a); self.assertEqual(r[0]['unknown']['branch_predicate'],1)
    def test_wrong_architecture_API_and_CLI_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            p=pathlib.Path(td);src=p/'wrong.s';obj=p/'wrong.o';output=p/'report.json';src.write_text('.text\n.globl probe\n.type probe,@function\nprobe: ret\n.size probe,.-probe\n');subprocess.run(['as','--64','-o',str(obj),str(src)],check=True,capture_output=True)
            with self.assertRaisesRegex(ValueError,'ELF32'):eg.extract(obj,'probe')
            for script in ('effect_graph.py','stack_compare.py'):
                completed=subprocess.run([sys.executable]+(['-O'] if sys.flags.optimize else [])+[str(eg.HERE/script),'--native',str(obj),'--candidate',str(obj),'--symbol','probe','--output',str(output)],capture_output=True,text=True)
                self.assertNotEqual(completed.returncode,0);self.assertIn('ELF32',completed.stderr);self.assertFalse(output.exists())
if __name__=='__main__': unittest.main()
