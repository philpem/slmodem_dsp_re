#!/usr/bin/env python3
import playbook_small_patterns as d
import batch20_v8_interface_reproduce as previous
d.REV='93d7eee1';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.OUT_NAME='batch20-v8-getword'
def variants(path,source):
 old=previous.variants(path,source);out={'baseline':source}
 for name,text in old.items():
  a,b,f=d.function(text,'V8GetMessage');needle='(seq->word[i] >> 1)';assert f.count(needle)==1
  f=f.replace(needle,'((unsigned short)seq->word[i] >> 1)');out[name+'-word']=text[:a]+f+text[b:]
 return out
d.variants=variants
if __name__=='__main__':d.main()
