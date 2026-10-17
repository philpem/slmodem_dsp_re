#!/usr/bin/env python3
"""Replay three fax debug nominations, restoring two witnessed boundaries."""
import playbook_small_patterns as d

d.REV = '95e01c27'
d.OUT_NAME = 'fax-debug-boundaries'
d.SOURCE_PATHS = ('src/fax/cHDLCrx.c', 'src/fax/class1.c', 'src/fax/faxvmi_hdlc.c')
d.DUMP_FLAGS = ('-da', '-fdump-tree-original')


def variants(path, source):
    if path.endswith('cHDLCrx.c'):
        start = source.index('_hdlc_emulate_receive_state(')
        head, body = source[:start], source[start:]
        anchor = '\t\t\tint n;\n\n'
        assert body.count(anchor) == body.count('\t\t\tnext++;') == 1
        inserted = '''			if (dsplibs_debug_level > 1) {
				dsplibs_debug_printf(
				    "%2d.%02d[sec] Receive buffer OK in "
				    "_hdlc_emulate_receive_state\\n",
				    ctx->clock_sec, ctx->clock_frac);
				next = ctx->superframe_read_idx;
			}

'''
        body = body.replace(anchor, anchor + inserted)
        body = body.replace('\t\t\tnext++;', '\t\t\tnext = ctx->superframe_read_idx + 1;')
        return {'baseline': source, 'receive-diagnostic': head + body}
    if path.endswith('/class1.c'):
        start = source.index('fax_class1_create(')
        end = source.index('\n}\n', start) + 3
        body = source[start:end]
        for field in ('vmi_c_cfg', 'vmi_a_cfg'):
            old = '\t\tif (ctx->' + field + ' != NULL)\n\t\t\tctx->'
            assert body.count(old) == 1
            body = body.replace(old, '\t\tif (ctx->' + field + ' == NULL)\n\t\t\tgoto memory_error;\n\t\tctx->')
        ending = '\treturn ctx;\n}\n'
        assert body.endswith(ending)
        body = body[:-len(ending)] + '''	return ctx;

memory_error:
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("Internal memory allocation error!\\n\\n");
	return NULL;
}
'''
        return {'baseline': source, 'published-config-null': source[:start] + body + source[end:]}
    return {'baseline': source}


d.variants = variants
if __name__ == '__main__':
    d.main()
