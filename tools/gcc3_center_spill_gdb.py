"""Frozen installed-cc1 history spill hooks; no inferior calls or writes."""
import gdb,json,os,hashlib
from pathlib import Path
TARGET=os.environ.get('GCC3_CENTER_DELETE_TARGET','V34EqualizerCenterAdapt');events=[];emissions=[];errors=[];pending=[];exits=[]
def val(s):return gdb.parse_and_eval(s)
def num(s):return int(val(s))
def name():
 try:return val('cfun->decl->decl.name->identifier.id.str').string()
 except:return ''
def rtl(p,depth=0):
 if not int(p):return None
 n=p.dereference();c=str(n['code']);o={'code':c,'mode':str(n['mode'])}
 if c=='REG':o['regno']=int(n['u']['fld'][0]['rtint'])
 elif c=='CONST_INT':o['value']=int(n['u']['hwint'][0])
 elif depth<8 and c in ['MEM','SIGN_EXTEND','SUBREG','SET','PLUS','MULT','MINUS','ASHIFT','ASHIFTRT','CLOBBER','USE']:
  o['operands']=[rtl(n['u']['fld'][i]['rtx'],depth+1)for i in range(2 if c in ['SET','PLUS','MULT','MINUS','ASHIFT','ASHIFTRT']else 1)]
 elif depth<8 and c=='PARALLEL':
  v=n['u']['fld'][0]['rtvec'].dereference();o['operands']=[rtl(v['elem'][i],depth+1)for i in range(int(v['num_elem']))]
 return o
def insn(p):
 n=p.dereference();return {'uid':int(n['u']['fld'][0]['rtint']),'code':str(n['code']),'pattern':rtl(n['u']['fld'][5]['rtx'])if str(n['code'])in['INSN','JUMP_INSN','CALL_INSN']else None}
def frames():
 f=gdb.newest_frame();a=[]
 while f and len(a)<8:a.append(f.name());f=f.older()
 return a
def reload_uid():
 f=gdb.newest_frame()
 while f:
  if f.name()=='emit_reload_insns':
   for variable in ['insn','chain']:
    try:
     v=f.read_var(variable)
     if v.is_optimized_out:continue
     if variable=='chain':v=v.dereference()['insn']
     return insn(v)['uid']
    except Exception:pass
  f=f.older()
 return None
class Done(gdb.FinishBreakpoint):
 def __init__(self,e,p):super().__init__(gdb.newest_frame(),internal=True);self.e=e;self.p=p
 def stop(self):
  try:
   self.e['after']=insn(self.p);pending.remove(self.e)
   if self.e['hook']=='delete_output_reload':self.e['reload_after']={k:rtl(val('rld[%d].%s'%(self.e['args'][0],k)))for k in ['in','out']}
  except Exception as x:errors.append(str(x))
  return False
class Delete(gdb.Breakpoint):
 def __init__(self,s,kind):super().__init__(s,internal=True);self.kind=kind
 def stop(self):
  if name()!=TARGET:return False
  try:
   p=val('*(rtx*)($esp+4)');e={'hook':self.kind,'before':insn(p),'callers':frames(),'reload_insn_uid':reload_uid()}
   if self.kind=='delete_output_reload':
    e['args']=[num('*(int*)($esp+8)'),num('*(int*)($esp+12)')]
    e['reload_before']={k:rtl(val('rld[%d].%s'%(e['args'][0],k)))for k in ['in','out']}
    e['history_homes']={str(n):{'memory_loc':rtl(val('reg_equiv_memory_loc[%d]'%n)),'mem':rtl(val('reg_equiv_mem[%d]'%n))}for n in [75,89,104,118]}
   events.append(e);pending.append(e);Done(e,p)
  except Exception as x:errors.append(str(x))
  return False
class EmitDone(gdb.FinishBreakpoint):
 def __init__(self,e):super().__init__(gdb.newest_frame(),internal=True);self.e=e
 def stop(self):
  try:self.e['returned']=insn(self.return_value);pending.remove(self.e)
  except Exception as x:errors.append(str(x))
  return False
class Emit(gdb.Breakpoint):
 def stop(self):
  if name()!=TARGET:return False
  try:
   pattern=rtl(val('*(rtx*)($esp+4)'))
   if not pattern or pattern['code']!='SET':return False
   left,right=pattern['operands']
   if left.get('regno')not in [75,89,104,118]or right.get('regno')!=0:return False
   e={'pattern':pattern,'callers':frames(),'reload_insn_uid':reload_uid()};emissions.append(e);pending.append(e);EmitDone(e)
  except Exception as x:errors.append(str(x))
  return False
assert hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest()=='80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'
assert gdb.lookup_type('long').sizeof==4
assert num('(unsigned long)delete_insn')==0x080a821c
assert num('(unsigned long)delete_output_reload')==0x082101d8
Emit('*emit_insn',internal=True)
Delete('*delete_insn','delete_insn');Delete('*delete_output_reload','delete_output_reload')
gdb.events.exited.connect(lambda e:exits.append(getattr(e,'exit_code',None)))
gdb.execute('run')
r={'events':events,'emissions':emissions,'errors':errors,'exit_codes':exits,'pending':len(pending)}
Path('delete-observe.json').write_text(json.dumps(r,indent=2)+'\n')
assert not errors and exits==[0]and not pending and events,(errors,exits,len(pending),len(events))
print('observed target delete events',len(events))
