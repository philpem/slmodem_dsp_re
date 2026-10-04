#!/usr/bin/env python3
import gcc3_batch50_v90_cp_owner as parent
d=parent.d
d.OUT_NAME='gcc3-batch50-v90-cp-index'
def variants(path,source):
 cached=parent.variants(path,source)['cached-weight'];cells={'baseline':source,'cached-weight':cached}
 for label,local in [('arm-entry-shared-index',False),('arm-entry-local-index',True)]:
  start,end,body=d.function(cached,'float2Bits');text=body
  if local:
   text=text.replace('\tint i;\n','',1).replace('\tcase 0:','\tcase 0: {\n\t\tint i = 0;',1).replace('\tcase 1:','\t}\n\tcase 1: {\n\t\tint i = 0;',1)
   fn=text;assert fn.endswith('\n\t}\n}');fn=fn[:-5]+'\n\t}\n\t}\n}';text=fn
  else:
   text=text.replace('\tcase 0:','\tcase 0:\n\t\ti = 0;',1).replace('\tcase 1:','\tcase 1:\n\t\ti = 0;',1)
  fn=text;assert fn.count('for (i = 0;')==2;fn=fn.replace('for (i = 0;','for (;');text=cached[:start]+fn+cached[end:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
