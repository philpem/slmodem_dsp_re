# V34 diagnostic found-edge bypasses the scan counter

New independent CFG witness after the eight-axis cross: blob nonzero test
at0x72205 jumps directly to report block0x7222e. No post-scan k==n comparison
on that edge. Source break keeps k alive and tests k==n after the loop.
Declare baseline902 raw, three-axis seed signedscan+coeffcapture+perprint
positive guards, and one direct goto from nonzero to coefficients label.
This is original CFG recovery, not branch-layout/register fitting. No further
synonyms; audit whole TU including data/string multiplicities and all bystanders.
Parent finalperiod batch gate, no fuzz/mutation/harness run.
