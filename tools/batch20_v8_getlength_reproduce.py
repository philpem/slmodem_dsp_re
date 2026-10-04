#!/usr/bin/env python3
"""V8 GetMessage signed-short guard value with separately promoted length."""
import playbook_small_patterns as d
import batch20_v8_interface_reproduce as old
d.REV='93d7eee1';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.OUT_NAME='batch20-v8-getlength'
def variants(path,source):
 text=old.variants(path,source)['shared-direct'];a,b,fn=d.function(text,'V8GetMessage')
 fn=fn.replace('\tint n;','\tshort words = seq->wordidx;\n\tint n;').replace('if (seq->wordidx > 0)','if (words > 0)').replace('n = seq->wordidx;','n = words;')
 needle='(seq->word[i] >> 1)';assert fn.count(needle)==1
 return {'baseline':source,'short-length':text[:a]+fn+text[b:],'short-length-word':text[:a]+fn.replace(needle,'((unsigned short)seq->word[i] >> 1)')+text[b:]}
d.variants=variants
if __name__=='__main__':d.main()
