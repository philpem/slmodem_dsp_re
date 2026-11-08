"""Load exactly captured helper bytes, independent of mutable repository files.

Default helpers come from this package's parent tools directory. Set
NATIVE_EFFECT_DEPENDENCY_TOOLS to a historical tools directory to replay it.
The helper source bytes are copied to a private temporary tree before any
helper import; their hashes describe the files consumed by Python and ELF
analysis helpers. A caller's already imported, differently sourced helper is
an error. No external manifest or scratch layout is required.
"""
import hashlib
import importlib
import os
from pathlib import Path
import sys
import tempfile

FILES = ('gcc3_owner_trace.py', 'gcc3_search_width.py',
         'toolchain/byteident.py', 'toolchain/jumptable.py',
         'toolchain/induction_jumptable.py', 'toolchain/readonly_copy.py',
         'toolchain/framefree.py', 'toolchain/fidelity.py',
         'toolchain/guarded_switch.py', 'toolchain/callee_effects.py',
         'toolchain/linkedcopies.py', 'ci/period_metrics.py')
MODULES = {'gcc3_owner_trace': 'gcc3_owner_trace.py',
           'gcc3_search_width': 'gcc3_search_width.py',
           'byteident': 'toolchain/byteident.py',
           'jumptable': 'toolchain/jumptable.py',
           'induction_jumptable': 'toolchain/induction_jumptable.py',
           'readonly_copy': 'toolchain/readonly_copy.py',
           'framefree': 'toolchain/framefree.py',
           'fidelity': 'toolchain/fidelity.py',
           'guarded_switch': 'toolchain/guarded_switch.py',
           'callee_effects': 'toolchain/callee_effects.py',
           'linkedcopies': 'toolchain/linkedcopies.py',
           'period_metrics': 'ci/period_metrics.py'}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


class HelperContext:
    def __init__(self, directory=None):
        self.directory = Path(directory or os.environ.get(
            'NATIVE_EFFECT_DEPENDENCY_TOOLS', str(Path(__file__).resolve().parent.parent))).resolve()
        # Capture bytes once. Neither import nor analysis returns to original paths.
        self.contents = {name: (self.directory / name).read_bytes() for name in FILES}
        self.temporary = tempfile.TemporaryDirectory(prefix='native-graph-helpers-')
        self.tools = Path(self.temporary.name) / 'tools'
        self.records = {}
        for name, data in self.contents.items():
            target = self.tools / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            target.chmod(0o400)
            self.records[name] = {
                'source': str(self.directory / name),
                'sha256': hashlib.sha256(data).hexdigest(),
                'captured_bytes': len(data)}
        for name in MODULES:
            require(name not in sys.modules,
                    'conflicting preloaded helper module: ' + name)
        # owner_trace handles stdlib dis shadowing before loading toolchain modules.
        previous_path = sys.path[:]
        try:
            # byteident.body_digest imports this helper lazily. Preload its
            # captured copy too, so a later call cannot consult live CI files.
            sys.path.insert(0, str(self.tools / 'ci'))
            sys.path.insert(0, str(self.tools))
            self.owner = importlib.import_module('gcc3_owner_trace')
            for name in MODULES:
                importlib.import_module(name)
        finally:
            sys.path[:] = previous_path
        self.verify()

    def verify(self):
        for name, wanted in self.records.items():
            actual = hashlib.sha256((self.tools / name).read_bytes()).hexdigest()
            require(actual == wanted['sha256'], 'captured helper changed: ' + name)
        for name, relative in MODULES.items():
            module = sys.modules.get(name)
            if module is not None:
                require(Path(module.__file__).resolve() == self.tools / relative,
                        'helper imported from unexpected path: ' + name)

    def manifest(self):
        self.verify()
        return {'source_tools_directory': str(self.directory),
                'capture': 'private helper snapshot before import',
                'files': self.records}


CONTEXT = HelperContext()
