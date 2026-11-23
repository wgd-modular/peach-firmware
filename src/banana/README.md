# Banana

Hagiwo's chord oscillator, the firmware of WGD Modular's Banana module. Four voices play one of eight chords in one of 16 inversions, and in half of the inversions a fifth voice adds the root as a bass note. All voices play the same wave, one of eight. It plays all the time, there is no envelope.

## Controls

| Input | Pin | Function |
|---|---|---|
| 1 V/Oct CV | A5 | pitch |
| Frequency pot | A1 | coarse pitch, from C1 (32.7 Hz) fully left to about F#3 (185 Hz) fully right, at 0 V |
| Detune pot | A2 | inversion, 16 steps |
| OSC 2 Level pot + CV | A3 | chord, 8 steps |
| Wave button | D8 | next wave, saved in EEPROM |
| Out | A0 (DAC) | all five voices |

Turning the Detune pot right moves the chord voices up an octave one after the other, past the middle the bass comes in and they come back down. A CV into OSC 2 Level steps through the chords.

## Chords

OSC 2 Level from left to right, with the notes of the four voices in semitones above the root:

| Chord | Notes |
|---|---|
| Major | 0, 4, 7, 12 |
| Major 7th | 0, 4, 7, 11 |
| Major add 9 | 0, 4, 7, 14 |
| Sus2 | 0, 2, 7, 12 |
| Minor add 9 | 0, 3, 7, 14 |
| Minor 7th | 0, 3, 7, 10 |
| Minor | 0, 3, 7, 12 |
| Root only | 0, 0, 0, 0 |

With Root only the inversions stack the root in octaves.

## Waves

The Wave button steps through the waves and wraps around after the last one. The wave is saved, so the module starts with the one it was switched off with.

| # | Wave |
|---|---|
| 1 | Saw |
| 2 | Square |
| 3 | Triangle with rounded corners |
| 4 | Sine |
| 5 | Chebyshev, a saw bent by the third Chebyshev polynomial |
| 6 | Half sine that wraps around at the top of its range |
| 7 | Sigmoid, a saw with soft corners |
| 8 | Ramp, a saw in 256 steps, which sounds close to the saw |

## Coming from Banana

The Banana module has a CV input for the chord and one for the inversion. On Peach the OSC 2 Level CV moves the chord, the inversion only has its pot. Banana's frequency knob goes from 0 to 255 Hz, here the Frequency pot goes from C1 to about F#3 and the CV adds to that. To change the wave Banana has you turn the inversion knob fully right and pick it with the chord knob, here the Wave button steps through the waves.

Banana rounds each voice down to whole hertz, which puts chords around C1 up to about half a semitone out of tune and lower ones further still. With a high CV, from about 2.4 V in the middle inversions where the voices sit highest and from about 3.8 V in the outer ones, it reads past the end of its pitch table and plays wrong notes. It also plays each voice with only eight levels at 16 kHz, which sounds gritty. Here every voice is in tune across the whole range and plays at 40 kHz in full resolution. Voices that land on the same note, like the bass and the root in the last inversion, play as one note at the loudness they have on average, instead of adding up or cancelling out depending on how their phases line up. The waves are smoothed where they jump, and between 4.2 and 6.7 kHz a voice turns into a sine, so high notes do not fold back as off-key tones. Only the half sine, the brightest of the waves, still folds back on high chords.

## Build & upload

From the repo root:

```bash
pixi run build banana
```

This places the firmware in `src/banana/build/banana.ino.bin`. See the [main README](../../README.md) for building in the Arduino IDE instead.

## Author

Hagiwo, from his [chord VCO](https://note.com/solder_state/n/n681d2e07e324), which WGD Modular's Banana module runs.
