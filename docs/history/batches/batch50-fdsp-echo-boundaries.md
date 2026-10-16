# FDSP echo quiet verdict, history owner and count

Eightcell raw902 cube: capture the nearquiet predicate usedtwice; native
peakscan historyowner; unsignedquiet counter. Reference evaluates fabs(x)
once,setbCL,counts truth then reuses ECX for adaptation. Current evaluates
fabs twice. Reference forms hist+pos before peak and traverses k from that
owner; current forms pos+k eachpass. Reference final quiet>80 seta; current
setg. Quiet count bounded0..160 on every path, unsignedcount does not alter
verdict. All other arithmetic/aliases/order and shortprediction counter remain.
Do not touch bValidateEnergyValue/RMS F11561 orflags. FullTU raw/headerparity,
metadata/data/nontext/relocs/allbystanders; original register names never
forced. No harness/mutation/fuzz.

Measured result: 8cells closestSIZE2, no gain; unsignedquiet alone changes signedverdict but notsize. CompleteTUaudit151/151 includes everycell and all nonexactbystanders; no adoption.
