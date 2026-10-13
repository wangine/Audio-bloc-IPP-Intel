# Third-party notices

## Faust 2.88.0

- Project: [Faust](https://github.com/grame-cncm/faust), GRAME, Centre National de Création Musicale.
- Version: 2.88.0, official [release](https://github.com/grame-cncm/faust/releases/tag/2.88.0).
- Windows compiler asset SHA-256: `cc898c55526f8f52859c0c5e47995a5fa2cd6f82bf57cfdeaabfa7d149885656` (verified against the release API's digest).
- Used only as a build-time source generator. The compiler binary and its runtime libraries are not packaged with `EntropyDynamicRecovery.dll`.
- `Generated/EntropyDynamicRecoveryFaust.cpp` was produced from the Entropy-authored `.dsp` source using `-lang cpp -double -scal`. No Faust library is imported and no Faust architecture template is embedded. The wrapper's minimal C++ architecture is original to this module.
- Compiler distribution terms are published in the [Faust repository](https://github.com/grame-cncm/faust). The generated program's license is not inferred from the compiler's license.

## 7-Zip

7-Zip was used locally to extract the official Faust compiler package. It is not linked or distributed with the DLL. [7-Zip project and license](https://www.7-zip.org/).

No Whetstone source or binary is included.
