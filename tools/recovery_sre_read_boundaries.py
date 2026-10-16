#!/usr/bin/env python3
"""Test three independently witnessed reads on the joint SRE count control."""
import playbook_small_patterns as driver
import recovery_sre_boundary as boundary


def variants(path, source):
    assert source.count('(short)n);') == 1
    joint = source.replace('(short)n);', 'n);')
    if path.endswith('V17r_int.c'):
        declaration = '\t\tshort *buf = (short *)RXCTL(modem)->scratch;\n'
        old = '\t\tfor (i = 0; i < count; i++)\n\t\t\tbuf[i] = in[i];'
        new = ('\t\tfor (i = 0; i < count; i++) {\n' + declaration.replace('\t\t', '\t\t\t', 1) +
               '\n\t\t\tbuf[i] = in[i];\n\t\t}')
        assert joint.count(declaration) == joint.count(old) == 1
        candidate = joint.replace(declaration, '').replace(old, new)
    elif path.endswith('V29r_int.c'):
        declaration = '\t\tshort *buf = (short *)((struct v29_rx *)modem)->det->buf;\n'
        old = '\t\tfor (i = 0; i < count; i++)\n\t\t\tbuf[i] = (short)(in[i] >> 1);'
        new = ('\t\tfor (i = 0; i < count; i++) {\n' + declaration.replace('\t\t', '\t\t\t', 1) +
               '\n\t\t\tbuf[i] = (short)(in[i] >> 1);\n\t\t}')
        assert joint.count(declaration) == joint.count(old) == 1
        candidate = joint.replace(declaration, '').replace(old, new)
    else:
        assert path.endswith('V27r_int.c')
        old = ('\trx = ((struct v27_rx *)modem)->rx;\n'
               '\tn = (unsigned short)FPM_MRF_filter((&rx->mrf), in,\n'
               '\t\t\t\t\t   (short *)rx->buf_a,\n'
               '\t\t\t\t\t   count);')
        new = ('\tn = (unsigned short)FPM_MRF_filter(\n'
               '\t\t\t&((struct v27_rx *)modem)->rx->mrf, in,\n'
               '\t\t\t(short *)((struct v27_rx *)modem)->rx->buf_a, count);')
        assert joint.count(old) == 1
        candidate = joint.replace(old, new)
    return {'baseline': source, 'int-joint': joint, 'int-read-boundary': candidate}


if __name__ == '__main__':
    driver.REV = boundary.REV
    driver.OUT_NAME = 'recovery-sre-read-boundaries'
    driver.SOURCE_PATHS = boundary.CALLERS
    driver.variants = variants
    driver.HEADER_OVERLAYS = boundary.overlays
    driver.DUMP_FLAGS = ()
    driver.main()
