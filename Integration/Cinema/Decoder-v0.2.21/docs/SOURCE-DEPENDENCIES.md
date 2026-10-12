# Source and build reference

All decoder, transport, WDL SRC and unchanged GUI source files are in this
source bundle. The original installer wrapper is retained in
tools/InstallerTemplate-v0.2.13.exe and its machine sections are verified by
tools/Package.py. The runtime bundle includes the original FFmpeg binary and
notices. No new codec library or driver is introduced.

The GUI build uses the existing external JUCE checkout at D:/Projects/JUCE,
commit 2cdfca8feb300fb424002ba2c2751569e5bacb64. Reproduce that checkout from
https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64
and retain its license terms. MSVC 19.51.36257.0, CMake 3.30 and Ninja are the
recorded Windows build tools. This bundle does not vendor the Windows SDK or
compiler. The local absolute JUCE path in CMakeLists.txt must exist or be
adapted on another machine.

The live source is UR23's existing Windows USB audio driver, not Music Render
V2. Stage 1 r9 and the Music Render V2 kernel/Provider are unchanged references.
The source archive, runtime installer and retained test evidence have separate
SHA256 manifests in the delivery directory.
