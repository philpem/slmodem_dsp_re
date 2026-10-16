# FDSP initializer default step before kernel geometry

Fourcells raw902 crossed stepdefaultgroup before kernelstatus/ntaps stores
and original floatclearhelper calls. Reference materializes A stepwordthen
stores at+168c before kernelstatus=2, Bstep=0 before tapcountstores; current
source stores allkernelgeometry before stepdefaults. Test the two natural
initializationgroups exchanged (steps then status/ntaps) only, preserving
all flag/array stores. Object does not uniquely establish sourceorder versus
scheduling, so no claim of unique spelling. Trace scheduler/RTL and wholeTU
identity, no arbitrarypermutation domain. Liveobjects sourcecreate owns distinct
channel/kernel allocations; originalbehavior on overlapping synthetic objects
is not inferred by math. Allmetadata/data/nontext/relocs+bystanders audited.

Measured result: 4cells SIZE17unchanged; no gain. CompleteTUaudit151/151 includes everycell and all nonexactbystanders; no adoption.
