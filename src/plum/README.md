# Plum

A macro oscillator inspired by multi-engine digital voices like Noise Engineering's Alia. Plum has four synthesis modes, picked with the Wave button. In every mode two macros shape the sound, one on the Detune pot and one on the OSC 2 Level pot and CV, so a CV into OSC 2 Level changes the sound quickly.

## Controls

| Input | Pin | Function |
|---|---|---|
| 1 V/Oct CV | A5 | pitch |
| Frequency pot | A1 | coarse pitch, fully left = C1 (32.7 Hz) at 0 V |
| Detune pot | A2 | first macro, see the modes below |
| OSC 2 Level pot + CV | A3 | second macro, see the modes below |
| Wave button | D8 | next mode, saved in EEPROM |
| Out | A0 (DAC) | oscillator output |

There is no detune between two oscillators in Plum: the Detune pot is a macro.

## Modes

The Wave button steps through the modes and wraps around after the last one. The mode is saved, so the module starts with the one it was switched off with.

| # | Mode | Detune pot | OSC 2 Level pot + CV |
|---|---|---|---|
| 1 | 2-op FM | modulator ratio, snapped to 0.5, 1, 2 … 8 | FM depth, from pure sine to metallic |
| 2 | Wavefolder | symmetry, from even to odd harmonics | fold amount, from sine to dense folds |
| 3 | Phase distortion (Casio CZ style) | saw-like shape → resonant shape, resonance rising with it | distortion amount |
| 4 | Supersaw | detune of the 7 saws, up to ±1 semitone | mix: centre saw only → all 7 saws equally loud |

FM ratios are harmonic, so the tone always stays pitched. In supersaw mode the saws are anti-aliased (PolyBLEP); the other modes are smooth waveshapes without hard edges.

## Build & upload

From the repo root:

```bash
pixi run build plum
```

This places the firmware in `src/plum/build/plum.ino.bin`. See the [main README](../../README.md) for building in the Arduino IDE instead.
