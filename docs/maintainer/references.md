# Standards and references

Use standards to understand the waveform and protocol; use the reference
object and measured fixtures to establish what this implementation does.
The editions below are reading references, not a claim that Smart Link
implemented every clause or amendment. No clause-level conformance audit was
performed for this guide. ITU catalogue links provide edition histories and
access to the publications; check the exact edition before citing a clause.

## Protocol standards

| Reference and reading edition | Use in this guide | Implementation reading path |
|---|---|---|
| [V.8, November 2000](https://www.itu.int/rec/T-REC-V.8/en) | Session-start negotiation | `src/v8/v8.c`, `V8Interface.c`, `V8.c` |
| [V.21, November 1988](https://www.itu.int/rec/T-REC-V.21/en) | 300 bit/s duplex modem and low-rate signalling context | Bell 103/V.21 selection and fax V.21 |
| [V.23, November 1988](https://www.itu.int/rec/T-REC-V.23/en) | 600/1200-baud modem context | `src/pump/v23/` |
| [V.22bis, November 1988](https://www.itu.int/rec/T-REC-V.22bis/en) | 2400 bit/s frequency-division duplex modem | `src/pump/v22/` |
| [V.32, March 1993](https://www.itu.int/rec/T-REC-V.32/en) | Duplex modem family up to 9600 bit/s | `src/pump/v32/` |
| [V.32bis, February 1991](https://www.itu.int/rec/T-REC-V.32bis/en) | Extension to 14,400 bit/s | V.32 configuration, training and coding |
| [V.34, February 1998](https://www.itu.int/rec/T-REC-V.34/en) | Modem up to 33,600 bit/s; probing, training and data context | `src/pump/v34/` |
| [V.90, September 1998](https://www.itu.int/rec/T-REC-V.90/en) | Digital/analogue pair; maxima of 56,000 bit/s downstream and 33,600 bit/s upstream | `V90Modem`, phase components and PCM mapping |
| [V.92, November 2000](https://www.itu.int/rec/T-REC-V.92/en) | Enhancements to V.90 | `V92Modem`, transmitter and phase components |
| [G.711, November 1988](https://www.itu.int/rec/T-REC-G.711/en) | Voice-frequency PCM and companding reference | `src/service/pcm.c` and PCM-level tests |
| [V.250, July 2003](https://www.itu.int/rec/T-REC-V.250/en) | Host dialling/control vocabulary | Host boundary; not a description of the DSP's complete API |
| [T.31, August 1995](https://www.itu.int/rec/T-REC-T.31/en) | Fax DCE Service Class 1 control | `class1*.c`, `Vmi_v*.c`, `faxadapt.h` |
| [T.30, July 2003](https://www.itu.int/rec/T-REC-T.30/en) | Fax procedure context above modem signalling | Host/fax boundary; no claim that this library owns the complete procedure |
| [V.17, February 1991](https://www.itu.int/rec/T-REC-V.17/en) | Fax modem up to 14,400 bit/s | `src/fax/V17*`, `src/fax/v17.c` |
| [V.27ter, November 1988](https://www.itu.int/rec/T-REC-V.27ter/en) | 4800/2400 bit/s modem | `src/fax/V27*` |
| [V.29, November 1988](https://www.itu.int/rec/T-REC-V.29/en) | 9600 bit/s modem specification, also relevant to the fax path | `src/fax/V29*` |

T.30's later catalogue edition does not establish which version the blob
targeted. V.92 amendments likewise require their own evidence. For Bell 103
tone and configuration claims this guide uses the recovered implementation
and the repository's measured configuration record; it does not present an
ITU recommendation as the original Bell specification. Caller ID country
variants also need a specific national specification before a conformance
claim can be made.

## DSP background

Steven W. Smith's
[*The Scientist and Engineer's Guide to Digital Signal Processing*](https://www.dspguide.com/)
provides background on sampling, convolution, Fourier analysis and digital
filters. It is explanatory reading, not evidence of the Smart Link algorithms
or coefficient provenance. For the recovered filter designs, use the
repository's
[coefficient derivations](coefficients.md).

## Repository evidence

| Source | What to use it for |
|---|---|
| [Reference inputs](https://github.com/philpem/slmodem_dsp_re/tree/master/ref/slmodemd) | Original object and surviving host headers |
| [Findings](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/findings.md) | Dated observations and reproduction commands |
| [Translation-unit map](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/modules.md) and [attribution](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/attribution.md) | Original layout evidence and confidence boundaries |
| [Configuration](configuration.md) and [parameters](parameters.md) | Constructor effects, host parameters and units; check historical passages against current code |
| [Deviation register](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/deviations.md) | Suspected and measured departures in the original, including reachability limits |
| [Compiler method](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/method/compilers.md) | Period authority, modern portability and floating-point differences |
| [Third-party boundary](https://github.com/philpem/slmodem_dsp_re/blob/master/third_party/README.md) | Independent interoperability peers and licensing separation |

SpanDSP is a test peer. Its code, tables and algorithms must not be copied
into reconstructed source. A peer disagreement calls for a bounded standards
and implementation investigation, not automatic replacement of the blob's
behaviour.

## Documentation tooling

The build follows the official
[Material for MkDocs installation guide](https://squidfunk.github.io/mkdocs-material/getting-started/)
and [MkDocs getting-started guide](https://www.mkdocs.org/getting-started/).
`tools/ci/docs-requirements.txt` pins the two direct documentation tools.
The strict site build checks configured pages and internal links; it does
not validate external URLs or verify protocol claims.
