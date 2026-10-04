#!/usr/bin/env python3
"""Original samples/2 calculation begins before silence state stores."""
import playbook_small_patterns as d
from batch100_fax_idle_bound import variants as predecessor
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/class1.c',);d.OUT_NAME='batch100-fax-silence-half-lifetime'
def variants(path,s):
 x=predecessor(path,s)['split-idle-named-bound'];a,z,f=d.function(x,'_recieve_silence_state_init');assert f.count('int half;')==1 and f.count('\thalf = samples / 2;')==1;new=f.replace('int half;','int half = samples / 2;').replace('\thalf = samples / 2;\n','');return{'baseline':s,'half-before-stores':x[:a]+new+x[z:]}
d.variants=variants
if __name__=='__main__':d.main()
