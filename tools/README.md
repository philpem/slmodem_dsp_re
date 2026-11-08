# Maintained tools and investigation scratch

Keep utilities in `tools/` when they support an ongoing project workflow.
A tracked utility needs all three:

- A reusable purpose beyond one completed experiment.
- A documented entry point, inputs, outputs and scope of its conclusions.
- Maintained validation appropriate to its result, including a known control
  and reported denominators for tools that make measurement claims.

Build and CI callers, documented manual commands and shared Python imports
can establish ongoing use. Check these dependencies before moving or removing
a tool. A historical finding alone does not justify retaining its one-off
script, and an unfamiliar filename does not establish that a tool is unused.

Put new one-off experiments and temporary files in the current worktree's
`scratch/<investigation>/` directory. `/scratch/` is gitignored. Keep standard
build products under `build/`; an experiment's custom artifacts can stay with
its script under `scratch/`.

Save durable findings in the relevant project documentation. Include the
source revision, commands, compiler identity and artifact hashes needed to
interpret or reproduce a result. When an experiment has become redundant,
retain useful evidence and link to its historical Git version if necessary.
Promote a scratch script only after it meets the ongoing-utility criteria
above; do not copy an investigation directory wholesale into tracked tools.

For reconstruction workflows and the supported analysis commands, start with
[the method guide](../docs/method/README.md) and
[the tool guide](../docs/method/tools.md).
