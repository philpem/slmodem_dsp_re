#!/usr/bin/env python3
"""Remove the unsupported nearest-entry initialization in a proved first-win scan."""
import playbook_small_patterns as driver

def variants(path,source):
 start,end,fn=driver.function(source,'V90AutoDigitalImpDetector::unSuspectedPhaseNearestLinMapp')
 assert fn.count('unsigned short at = 5;')==1
 candidate=fn.replace('unsigned short at = 5;','unsigned short at;')
 return {'baseline':source,'first-win':source[:start]+candidate+source[end:]}

if __name__=='__main__':
 driver.REV='0a9b564f'
 # Preserve the failed -da control separately; dumps are measurement apparatus.
 driver.DUMP_FLAGS=()
 driver.OUT_NAME='playbook-adid-nearest-init'
 driver.SOURCE_PATHS=('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
 driver.variants=variants
 driver.main()
