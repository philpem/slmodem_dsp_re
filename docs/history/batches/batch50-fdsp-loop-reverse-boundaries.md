# FDSP reverse-block native source ownership

Eightcell raw902 cube on two finalreverse-copyloops: signed descending
source counter159..0, advancing destinationcursor, pre-loop destination
channelcapture. Blob ECX=159 dec/jns and EBX advancingdestination; source
unsignedascending i with two indexedarrays compiles indexed destand0..159.
Blob loads k->chan_a/b before respective reversefillloop; source reads it
afterloop at memcpy. Test independentnormal sourceboundaries, no reordering
majorcalls/zero/temp buffers and no fixedregisters. Originalarrayaliases
publicincoming pointers canaliaschannels, retained reads/writeorder; local
reversebuffers are freshstackowners and cannotcome frompublicincoming pointer.
Complete rawTU/bystander metadata/nontext/data/relocs; no harness/fuzz/mutation.

Measured result: 8cells best advancingoutputSIZE13 vsbaseline36; no gain. CompleteTUaudit151/151 includes everycell and all nonexactbystanders; no adoption.
