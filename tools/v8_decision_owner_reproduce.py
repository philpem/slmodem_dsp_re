#!/usr/bin/env python3
"""Bound original V8 captured decision arithmetic and three-case dispatch."""
import playbook_small_patterns as d
import v8_remainder_owner_reproduce as prior


def decision(source, wrapped, switch):
    a,z,fn=d.function(source,'v8_fskdemodulate')
    start=fn.index('\t\tif (e_space <= V8_FSK_SILENCE')
    end=fn.index('\n\t}\n\n\t/* Flush',start)
    block=fn[start:end]
    quiet=block[block.index('\t\t\t/* Nothing'):block.index('\t\t\tcontinue;')]
    direction=block.index('\t\tif (e_mark > e_space) {')
    arms=block[direction:]
    left,right=arms.split('\n\t\t} else {',1)
    left=left[left.index('\n')+1:]
    assert right.endswith('\n\t\t}')
    right=right[:-len('\n\t\t}')].lstrip('\n')
    expr='(int)((unsigned int)e_mark - (unsigned int)e_space) > 0' if wrapped else 'e_mark > e_space'
    capture='\t\t{\n\t\t\tint decision = '+expr+';\n\n\t\t\tif (e_space <= V8_FSK_SILENCE && e_mark <= V8_FSK_SILENCE)\n\t\t\t\tdecision = 2;\n\n'
    if switch:
        body='\t\t\tswitch (decision) {\n\t\t\tcase 0:\n'+right+'\n\t\t\t\tbreak;\n\t\t\tcase 1:\n'+left+'\n\t\t\t\tbreak;\n\t\t\tcase 2:\n'+quiet+'\t\t\t\tbreak;\n\t\t\tdefault:\n\t\t\t\tbreak;\n\t\t\t}\n'
    else:
        body='\t\t\tif (decision == 0) {\n'+right+'\n\t\t\t} else if (decision == 1) {\n'+left+'\n\t\t\t} else if (decision == 2) {\n'+quiet+'\t\t\t}\n'
    fn=fn[:start]+capture+body+'\t\t}'+fn[end:]
    return source[:a]+fn+source[z:]


def variants(path, source):
    owner=prior.variants(path,source)['published-countdown']
    cells={'baseline':source,'published-countdown-control':owner,
           'direct-captured-if':decision(owner,False,False),
           'wrapped-captured-if':decision(owner,True,False),
           'wrapped-captured-switch':decision(owner,True,True)}
    assert len(cells)==len(set(cells.values()))==5
    return cells


if __name__=='__main__':
    d.REV='38626248';d.SOURCE_PATHS=('src/v8/V8Dpsk.c',);d.OUT_NAME='v8-decision-owner'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
