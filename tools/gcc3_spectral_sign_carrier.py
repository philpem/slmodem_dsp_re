#!/usr/bin/env python3
"""Cross sign-helper result and display cache widths on the complete TU."""
import playbook_small_patterns as d


def variants(path, source):
    assert source.count('static char\nsign_of(')==1
    assert source.count('\tchar s;')==1
    cells={}
    for result in ('char','int'):
        for cache in ('char','int'):
            label='baseline' if result==cache=='char' else result+'-'+cache
            base=source.replace('static char\nsign_of(', 'static '+result+'\nsign_of(').replace('\tchar s;', '\t'+cache+' s;')
            for factoring in ('retained','sign-last','arguments'):
                text=base
                for field in ('A1','A2','B1','B2'):
                    member='params->shaper'+field
                    sign='\ts = sign_of('+member+');\n'
                    whole='\tw = whole_of('+member+');\n'
                    frac='\tf = frac_of('+member+');\n'
                    assert text.count(sign+whole+frac)==1
                    if factoring=='sign-last':
                        text=text.replace(sign+whole+frac,whole+frac+sign)
                    elif factoring=='arguments':
                        text=text.replace(sign+whole+frac,'')
                        # One independently matched field-specific call per block.
                        target='\"V90MappingParams: '+field+'  = %c%d.%06d\\r\\n\", s, w, f'
                        assert text.count(target)==1,target
                        text=text.replace(target,'\"V90MappingParams: '+field+'  = %c%d.%06d\\r\\n\", sign_of('+member+'), whole_of('+member+'), frac_of('+member+')')
                if factoring=='arguments':
                    text=text.replace('\t'+cache+' s;\n\tint w, f;\n','')
                key=label if factoring=='retained' else label+'-'+factoring
                cells[key]=text
                old='((long double)v - (long double)(int)v)'
                assert text.count(old)==1
                cells[key+'-df']=text.replace(old,'((double)v - (double)(int)v)')
    assert len(cells)==24 and len(set(cells.values()))==20
    return cells


if __name__=='__main__':
    d.REV='66087521'
    d.SOURCE_PATHS=('src/pump/v90/V90MappingParamsInt.cpp',)
    d.OUT_NAME='gcc3-spectral-sign-carrier'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
