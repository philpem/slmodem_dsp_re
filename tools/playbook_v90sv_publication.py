#!/usr/bin/env python3
"""Three complete-TU owned-Psd publication controls."""
import playbook_small_patterns as driver


def variants(path, source):
    old = '\tpsd = (Psd *)sysdep_malloc(sizeof(Psd));\n\tnew (psd) Psd(fftLength,\n\t\t      (WindowType)params->SPECTRAL_VERIFIER_FFT_WINDOW,\n\t\t      params->SPECTRAL_VERIFIER_PSD_OVERLAP_LEN);'
    assert source.count(old) == 1
    local = old.replace('psd = (Psd *)', 'Psd *constructed = (Psd *)').replace('new (psd)', 'new (constructed)') + '\n\tpsd = constructed;'
    expression = old.replace('\tpsd = (Psd *)sysdep_malloc(sizeof(Psd));\n\tnew (psd)', '\tpsd = new (sysdep_malloc(sizeof(Psd)))')
    cells = {'baseline': source, 'local-publish': source.replace(old, local), 'expression-publish': source.replace(old, expression)}
    assert len(cells) == len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    driver.REV = '014667c0'
    driver.OUT_NAME = 'playbook-v90sv-publication'
    driver.SOURCE_PATHS = ('src/pump/v90/V90SpectralVerifier.cpp',)
    driver.variants = variants
    driver.main()
