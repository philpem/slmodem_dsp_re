#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/cid.c',);d.OUT_NAME='gcc3-batch100-cid-process-result'
def variants(path,source):
 start=source.index('CID_process(void *cidp');end=source.index('\n}',start)+2;fn=source[start:end];cells={'baseline':source}
 for width,dispatch,label in [(1,0,'int-result'),(0,1,'nonzero-dispatch'),(1,1,'int-nonzero')]:
  text=fn
  if width:assert text.count('short res;')==1;text=text.replace('short res;','int res;')
  if dispatch:
   assert text.count('\t\tif (res == 1) {')==1;text=text.replace('\t\tif (res == 1) {','\t\tif (res != 0) {\n\t\tif (res == 1) {').replace('} else if (res != 0) {','} else {')
   mark='\t\tif (ret)';assert text.count(mark)==1;text=text.replace(mark,'\t\t}\n'+mark)
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
