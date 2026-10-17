"""Observe selected installed-compiler reload choices; no inferior calls or state edits."""
import json, os, gdb
from pathlib import Path
TARGET=os.environ.get('GCC3_RELOAD_TARGET','EchoCanceler')
records=[];alternatives=[];errors=[];pending=[];counts={}

def num(x):return int(gdb.parse_and_eval(x))
def name():
 try:return gdb.parse_and_eval('cfun->decl->decl.name->identifier.id.str').string()
 except:return ''
def rtl(x,depth=0):
 if int(x)==0:return None
 t=x.dereference();code=str(t['code']);mode=str(t['mode']);r={'code':code,'mode':mode}
 if code=='REG':
  rn=int(t['u']['fld'][0]['rtint']);r['regno']=rn
  if rn>=53:r['renumber']=num('reg_renumber[%d]'%rn)
 elif code=='CONST_INT':r['value']=int(t['u']['hwint'][0])
 elif code in ('MEM','PLUS','MULT','SET') and depth<4:
  r['operands']=[rtl(t['u']['fld'][i]['rtx'],depth+1) for i in range(1 if code=='MEM' else 2)]
 return r
class Done(gdb.FinishBreakpoint):
 def __init__(self,row):super().__init__(gdb.newest_frame(),internal=True);self.row=row
 def stop(self):
  try:
   self.row['returned']=int(self.return_value);self.row['n_reloads']=num('n_reloads')
   self.row['reloads']=[]
   for i in range(num('n_reloads')):
    v=gdb.parse_and_eval('rld[%d]'%i)
    self.row['reloads'].append({'in':rtl(v['in']),'out':rtl(v['out']),'class':str(v['class']),'regno':int(v['regno']),'opnum':int(v['opnum']),'optional':int(v['optional']),'when_needed':str(v['when_needed'])})
   pending.remove(self.row)
  except Exception as e:errors.append(str(e))
  return False
class Finish(gdb.Breakpoint):
 def stop(self):
  if name()!=TARGET:return False
  try:
   uid=num('insn->u.fld[0].rtint')
   counts[uid]=counts.get(uid,0)+1
   if uid not in (56,187,189):return False
   row={'uid':uid,'replace':num('replace'),'code':num('insn_code_number'),'n_operands':num('recog_data.n_operands')}
   for v in ('best','live_known','commutative','swapped','goal_alternative_number','goal_alternative_swapped','n_alternatives'):
    row[v]=num(v)
   n=row['n_operands'];row['operands']=[rtl(gdb.parse_and_eval('recog_data.operand[%d]'%i)) for i in range(n)]
   row['constraints']=[gdb.parse_and_eval('recog_data.constraints[%d]'%i).string() for i in range(n)]
   for array in ('goal_alternative','goal_alternative_win','goal_alternative_match_win','goal_alternative_matches'):
    row[array]=[num('%s[%d]'%(array,i)) for i in range(n)]
   records.append(row);pending.append(row);Done(row)
  except Exception as e:errors.append(str(e))
  return False
class Alternative(gdb.Breakpoint):
 def stop(self):
  if name()!=TARGET:return False
  try:
   uid=num('insn->u.fld[0].rtint')
   if uid not in (56,187,189):return False
   row={'uid':uid,'replace':num('replace')}
   for v in ('losers','reject','bad','swapped','this_alternative_number','best','commutative'):
    row[v]=num(v)
   row['score']=6*row['losers']+row['reject']
   row['this_win']=[num('this_alternative_win[%d]'%i) for i in range(num('recog_data.n_operands'))]
   row['this_match_win']=[num('this_alternative_match_win[%d]'%i) for i in range(num('recog_data.n_operands'))]
   row['matches']=[num('this_alternative_matches[%d]'%i) for i in range(num('recog_data.n_operands'))]
   alternatives.append(row)
  except Exception as e:errors.append(str(e))
  return False
assert gdb.lookup_type('long').sizeof==4, 'unsupported compiler ABI'
start=num('(unsigned long)find_reloads')
for offset,expected in ((2099,'8b9d50f2ffff85db'),
                        (2151,'39856cf2ffff0f8eb3000000'),
                        (2759,'c78588f2ffff00000000')):
 assert bytes(gdb.selected_inferior().read_memory(start+offset,len(bytes.fromhex(expected)))).hex()==expected, 'unsupported reload hook'
Alternative('*find_reloads+2099',internal=True)
Finish('*find_reloads+2759',internal=True)
exits=[];gdb.events.exited.connect(lambda e:exits.append(getattr(e,'exit_code',None)))
gdb.execute('run')
Path('reload-observe.json').write_text(json.dumps({'records':records,'alternatives':alternatives,'counts':counts,'errors':errors,'exits':exits,'pending':len(pending)},indent=2)+'\n')
assert exits==[0] and records and not errors and not pending,(exits,len(records),errors,len(pending))
print('observed',len(records),'reload choices')
