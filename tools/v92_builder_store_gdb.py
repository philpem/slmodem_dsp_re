"""Read-only six-site builder store trace for body-pinned period fixtures."""
import gdb,json
from pathlib import Path
S=json.loads(Path('store-settings.json').read_text());records={};active={};errors=[];pending=[];bps=[];exits=[]
REGS=['eax','ecx','edx','ebx','esp','ebp','esi','edi']
def n(s):return int(gdb.parse_and_eval(s))
class Finish(gdb.FinishBreakpoint):
 def __init__(self,key):super().__init__(gdb.newest_frame(),internal=True);self.key=key;pending.append(key)
 def stop(self):
  try:
   active.pop(self.key[0]);pending.remove(self.key);records[self.key]['finished']=True
   if len(records)==14 and not pending:
    for b in bps:b.enabled=False
  except Exception as e:errors.append(str(e))
  return False
class Entry(gdb.Breakpoint):
 def __init__(self,kind,address):super().__init__('*'+hex(address),internal=True);self.kind=kind
 def stop(self):
  try:
   obj=n('*(unsigned int*)($esp+4)');mode=n('*(int*)'+hex(obj));key=(self.kind,mode)
   if mode not in [0,1,2,3,-1,7,65536]or key in records:return False
   assert self.kind not in active
   records[key]={'kind':self.kind,'mode':mode,'events':[],'finished':False};active[self.kind]=(key,obj);Finish(key)
  except Exception as e:errors.append(str(e))
  return False
class Store(gdb.Breakpoint):
 def __init__(self,kind,address,field):super().__init__('*'+hex(address),internal=True);self.kind=kind;self.field=field
 def stop(self):
  if self.kind not in active:return False
  try:
   key,obj=active[self.kind];raw=bytes(gdb.selected_inferior().read_memory(n('$pc'),7));assert raw[0]==0x89
   modrm,sib=raw[1:3];mod=modrm>>6;assert mod in [1,2]and modrm&7==4
   data=(modrm>>3)&7;scale=1<<(sib>>6);index=(sib>>3)&7;base=sib&7;assert index!=4 and base!=4
   disp=int.from_bytes(raw[3:3+(1 if mod==1 else 4)],'little',signed=True)
   address=(n('$'+REGS[base])+n('$'+REGS[index])*scale+disp)&0xffffffff
   offset=8 if self.field=='output'else 0x1008
   assert (address-obj-offset)%4==0
   i=(address-obj-offset)//4;assert 0<=i<1024
   records[key]['events'].append([self.field,i,n('$'+REGS[data])&0xffffffff])
  except Exception as e:errors.append(str(e))
  return False
for kind,config in S.items():
 bps.append(Entry(kind,config['entry']))
 for site,field in config['sites']:bps.append(Store(kind,config['entry']+site,field))
gdb.events.exited.connect(lambda e:exits.append(getattr(e,'exit_code',None)))
gdb.execute('run')
r={'records':list(records.values()),'errors':errors,'pending':pending,'exit_codes':exits}
Path('store-observe.json').write_text(json.dumps(r,indent=2)+'\n')
assert exits==[0]and not errors and not pending and len(records)==14,(exits,errors,len(records))
print('observed 14 builder calls',sum(len(v['events'])for v in records.values()),'object-store events')
