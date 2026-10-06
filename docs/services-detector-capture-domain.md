# Detector child publication and initializer result owner

Basee0052eec, four complete TUs. Original ad4b0 returns create_dtmf child in
EAX, copies twelve config words with REP MOVSL, then publishes child atad4bd.
Retained source publishes child before copying config. Test actual child
capture local with publication after cfg copy. Separately original entry loads
incoming detector into EAX then establishes EBP initializer/result owner after
testing incoming pointer. Test separate ordinary result owner initialized from
parameter and used through allocating/initialized paths. No changed object
layout, extra initialization or source attribute; cross these witnessed value
lifetime boundaries, not arbitrary local declarations.

Require raw retained control and audit all emitted bodies/data/export bindings.
Nonexact family closes without nearest-score adoption.
