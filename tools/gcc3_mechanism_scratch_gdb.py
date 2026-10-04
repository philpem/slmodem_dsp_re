"""Read-only scratch events in the installed GCC3 cc1; run by scratch_observe."""
import gdb,json
from pathlib import Path
settings=json.loads(Path('observer-settings.json').read_text())
events=[];active=[];errors=[]
def read(expr):
 v=gdb.parse_and_eval(expr)
 if v.is_optimized_out:raise RuntimeError('optimized out: '+expr)
 return int(v)
def bits(expr):
 return [r for r in range(53) if read(expr+'[%d]'%(r//32)) & (1<<(r%32))]
def cursor():return read('*(int*)'+hex(settings['cursor_address']))
def function():return gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string()
class Done(gdb.FinishBreakpoint):
 def __init__(self,e):super().__init__(gdb.newest_frame(),internal=True);self.e=e
 def stop(self):
  try:
   result=self.return_value
   self.e['selected']=int(result.dereference()['u']['fld'][0]['rtint']) if result is not None and int(result) else None
   self.e['cursor_after']=cursor();active.remove(self.e)
  except Exception as exc:errors.append(str(exc))
  return False
class Entry(gdb.Breakpoint):
 def stop(self):
  try:
   assert not active,'nested scratch search'
   stack=read('$esp')
   arg=lambda offset:read('*(unsigned int*)'+hex(stack+offset))
   signed=lambda value:value-(1<<32) if value&(1<<31) else value
   reg_set=arg(20)
   e={'function':function(),'cursor_before':cursor(),'from':signed(arg(4)),'to':signed(arg(8)),
      'mode':str(gdb.Value(arg(16)).cast(gdb.lookup_type('enum machine_mode'))),'constraint':gdb.Value(arg(12)).cast(gdb.lookup_type('char').pointer()).string(),
      'allocation_order':[read('reg_alloc_order[%d]'%r) for r in range(53)],
      'excluded':bits('(*(HARD_REG_SET*)'+hex(reg_set)+')'), 'candidates':[]}
   assert 0<=e['from']<=e['to']<5 and e['constraint'],'invalid entry arguments'
   cur=read('peep2_current');e['insn_uid']=read('peep2_insn_data[%d].insn->u.fld[0].rtint'%((e['from']+cur)%5))
   events.append(e);active.append(e);Done(e)
  except Exception as exc:errors.append(str(exc))
  return False
class Candidate(gdb.Breakpoint):
 def stop(self):
  try:
   if not active:return False
   e=active[-1];reg=read('regno');raw=read('raw_regno');cls=read('class')
   item={'register':reg,'order_index':raw,'live':bits('live'),'fixed':read('fixed_regs[%d]'%reg),
         'class_ok':bool(read('reg_class_contents[%d][%d]'%(cls,reg//32))&(1<<(reg%32))),
         'call_used':read('call_used_regs[%d]'%reg),'ever_live':read('regs_ever_live[%d]'%reg),
         'reload_completed':read('reload_completed'),'frame_pointer_needed':read('frame_pointer_needed')}
   if 'eligibility' not in e:
    e['eligibility']={'class':cls,'live':item['live'],'fixed':[read('fixed_regs[%d]'%r)for r in range(53)],'class_members':bits('reg_class_contents[%d]'%cls),'call_used':[read('call_used_regs[%d]'%r)for r in range(53)],'ever_live':[read('regs_ever_live[%d]'%r)for r in range(53)],'frame_pointer_needed':item['frame_pointer_needed'],'reload_completed':item['reload_completed']}
   e['candidates'].append(item)
  except Exception as exc:errors.append(str(exc))
  return False
Entry('*peep2_find_free_register',internal=True)
Candidate('/build/gcc-3.4.2/gcc/recog.c:2984',internal=True)
exits=[];gdb.events.exited.connect(lambda e:exits.append(getattr(e,'exit_code',None)))
gdb.execute('run')
report={'events':events,'errors':errors,'exit_codes':exits,'pending':len(active)}
Path('scratch-observe.json').write_text(json.dumps(report,indent=2)+'\n')
assert not errors and exits==[0] and not active,report['errors']
assert events and any(e['function']==settings['target'] for e in events),'target detector did not fire'
for a,b in zip(events,events[1:]):assert a['cursor_after']==b['cursor_before'],'cursor discontinuity'
print('observed %d scratch searches; target %s; no inferior calls/writes'%(len(events),settings['target']))
