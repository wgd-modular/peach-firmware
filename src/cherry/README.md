# Cherry

Hagiwo's additive oscillator, the firmware of WGD Modular's Cherry module. It adds up a fundamental and seven harmonics, all sine waves. Instead of a level for each one, two controls step through Hagiwo's tables of 256 settings: one sets how loud the partials are, the other which harmonics they play. It plays all the time, there is no envelope.

## Controls

| Input | Pin | Function |
|---|---|---|
| 1 V/Oct CV | A5 | pitch |
| Frequency pot | A1 | coarse pitch, from C1 (32.7 Hz) fully left to about F#3 (185 Hz) fully right, at 0 V |
| Detune pot | A2 | harmonic gain, how loud the fundamental and each harmonic are |
| OSC 2 Level pot + CV | A3 | harmonic spectrum, which harmonics play, up to the 16th |
| Wave button | D8 | swaps gain and spectrum between the two, saved in EEPROM |
| Out | A0 (DAC) | all eight partials |

A CV into OSC 2 Level moves the spectrum. Press the Wave button to have it move the gain instead: the Detune pot then sets the spectrum and OSC 2 Level the gain. Press it again to go back. The sound changes as soon as you press it, and nothing shows which way round the controls are, so listen or press again. The module starts the way it was switched off.

## Coming from Cherry

The Cherry module has a CV input for the gain and one for the spectrum. Peach has one, and the Wave button picks which of the two it moves. Cherry's pitch knob goes from 0 to 127 Hz, here the Frequency pot goes from C1 to about F#3 and the CV adds to that.

WGD's Cherry plays six of the eight partials, its code left out the last two because of pops. Here all eight play as in Hagiwo's original, so it sounds brighter than Cherry. The level never clips. The tables almost always put two or more partials on the same harmonic, and on Cherry how loud that harmonic gets depends on how they happen to line up after the spectrum knob has moved. Here they play as one at the level they have on average, so a setting always sounds the same however you got there, and turning the spectrum control does not change the volume. Cherry plays at 16 kHz, where partials above 8 kHz fold back as off-key tones. Here they play at 48 kHz and fade out between 16 and 20 kHz instead.

## Build & upload

From the repo root:

```bash
pixi run build cherry
```

This places the firmware in `src/cherry/build/cherry.ino.bin`. See the [main README](../../README.md) for building in the Arduino IDE instead.

## Author

Hagiwo, from his [additive VCO](https://note.com/solder_state/n/n30b3a8737b1e), which WGD Modular's Cherry module runs.
