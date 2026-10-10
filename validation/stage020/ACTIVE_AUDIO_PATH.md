# ACTIVE_AUDIO_PATH

One applied Core snapshot, MUSIC, FP64/192 kHz. Original profile preserved.

Original SHA256: `0C7FB25B0A8A5910BBAFC004EBA984AB08DE28C4D9B1753BEAA4AF0383DE3292`.
Headroom MANUAL_SAFE -3 dB; current=target=0.7079457843841379; attack 217 ms / release 4000 ms.
Guardian bypass; PCM24 V2 TPDF, clip-and-report. DAC pin numbers below are one based.

| XO | Role | Matrix destination / physical pin | Gain dB | Total delay samples | Manual AP Hz | Fractional mode | HP / LP Hz | FIR |
|---|---|---|---:|---:|---:|---|---|---|
| CH1 | CH1 L WAY 1 | D1 DAC A:1 | 0.000 | 2179.906250 | 1000 | Linear | 80.0999984741211 / 2040.5999755859375 | no |
| CH2 | CH2 L WAY 2 | D3 DAC A:3 | 0.000 | 2348.867188 | 1000 | Thiran3 | 775.6000366210938 / 8604.7001953125 | no |
| CH3 | CH3 L WAY 3 | D5 DAC A:5 | 0.000 | 2348.867188 | 1000 | Thiran5 | 3417.199951171875 / off | no |
| CH4 | CH4 CENTER WAY 1 | D7 DAC A:7 | 0.000 | 2576.386719 | off | Linear | 203.8000030517578 / 12193.2001953125 | no |
| CH5 | CH5 R WAY 1 | D2 DAC A:2 | 0.000 | 2048.011719 | 1000 | Linear | 80.0999984741211 / 2040.5999755859375 | no |
| CH6 | CH6 R WAY 2 | D4 DAC A:4 | 0.000 | 2048.011719 | 1000 | Thiran3 | 775.6000366210938 / 8604.7001953125 | no |
| CH7 | CH7 R WAY 3 | D6 DAC A:6 | 0.000 | 2048.011719 | 1000 | Thiran5 | 3417.199951171875 / off | no |
| CH8 | CH8 SUB 1 | D8 DAC A:8 | 0.000 | 629.765625 | 110.4000015258789 | Linear | off / 119.5 | yes |
| CH9 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |
| CH10 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |
| CH11 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |
| CH12 | CH12 SCOOP 1 | D9 DAC B:1 | -12.100 | 0.011719 | 111.5999984741211 | Linear | 81 / 600.4000244140625 | yes |
| CH13 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |
| CH14 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |
| CH15 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |
| CH16 |  | unrouted | 0.000 | 2048.011719 | off | Linear | off / off | no |

Input L/R/C trims: -1.1999988556 dB each; applied control floats are retained in the numeric model.
LR ways, Center and SUB all route to DAC A; SCOOP routes to DAC B.
Applied Stage2 EQ/static/dynamic/Center/alignment: BLOCKED (runtime acknowledgement unavailable).
Source code confirms Center's raw-LR pre-EQ tap. The historical fragment includes the real Center, with measured RMS/correlation in numeric.json.

Live A/B estimate: -82.842456 frames; software alignment not verified, analog skew not measured.
Offline zero-origin ASRC mapping preserves epoch/source identity in identity.csv and stepBegin/stepEnd in positions CSV.
No endpoint identifiers, source recording or profile contents are published.
