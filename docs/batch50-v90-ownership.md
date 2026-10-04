# Batch50 V90 ownership

Root allocates src/pump/v90/*.cpp excluding V90Demodulator.cpp/V90DemodulatorProgress.cpp and related parent apparatus work; src/dsp/Float*.cpp, excluding FFT/GenericTone. No shared headers/templates allocated. Also allocated tools/toolchain/jumptable.py and uniquely named replay/fixture tools.

Stable positive production paths: V90MappingParamsInt.cpp (2), V90PreFilter.cpp (1), V90Phase3Modulator.cpp (4), V92Phase3Modulator.cpp (4), V90AutoDigitalImpDetector.cpp (1), V90RDetector.cpp (4), V90Modulator.cpp (1):17 gains. Fixed-frame prover now validates saved-register table functions soundly; zero apparatus-only gains in unchanged300-object census.

All other investigated source remains unchanged. Full audit87 cells; parent owns combined build, period/structural gate, finding numbers and commit/push. No fuzz, mutation or harness execution.
