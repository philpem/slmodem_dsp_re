#!/usr/bin/env python3
"""Unknown explicit period selections fail before Docker or any test executes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]


class SelectionTests(unittest.TestCase):
    def check_selection(self,names):
        with tempfile.TemporaryDirectory(prefix='period-selection-')as directory:
            docker=Path(directory)/'docker'
            docker.write_text('#!/bin/sh\necho DOCKER_MUST_NOT_RUN >&2\nexit 99\n')
            docker.chmod(0o755)
            env=os.environ.copy();env['PATH']=directory+os.pathsep+env.get('PATH','')
            env['T']=names;env['PERIOD_IMG']='period-selection-never-docker'
            r=subprocess.run(['sh','tools/toolchain/period.sh'],cwd=ROOT,env=env,
                             text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            self.assertEqual(r.returncode,1,r.stdout)
            self.assertIn('invalid selection; no tests run',r.stdout)
            self.assertIn(str(len(names.split()))+' requested, 1 unknown',r.stdout)
            self.assertNotIn('DOCKER_MUST_NOT_RUN',r.stdout)
            self.assertNotIn('period differential:',r.stdout)

    def test_unknown_only(self):
        self.check_selection('t_no_such_period_fixture')

    def test_mixed_selection_is_not_partially_run(self):
        self.assertTrue((ROOT/'test/unit/t_genericiir.cpp').is_file())
        self.check_selection('t_genericiir t_no_such_period_fixture')


if __name__=='__main__':unittest.main(verbosity=2)
