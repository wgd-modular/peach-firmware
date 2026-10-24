# Peach

The original Peach firmware, a dual oscillator built on [Hagiwo's RA4M1 oscillator](https://note.com/solder_state/n/na0ae5754fa9c). Both oscillators play the same single-cycle wave, picked from 16 with the Wave button. OSC 2 can be tuned up to two octaves above OSC 1 and mixed in with its own level.

## Controls

| Input | Pin | Function |
|---|---|---|
| 1 V/Oct CV | A5 | pitch of both oscillators |
| Frequency pot | A1 | coarse pitch |
| Detune pot | A2 | OSC 2 pitch, from unison up to about two octaves above OSC 1 |
| OSC 2 Level pot + CV | A3 | OSC 2 level, from silent to as loud as OSC 1 |
| Wave button | D8 | next wave, saved in EEPROM |
| Out | A0 (DAC) | OSC 1 + OSC 2 |

OSC 1 always plays at half level, so with OSC 2 turned down the output is a single oscillator. The Detune pot is not quantized: just above unison it beats slowly against OSC 1, further up it gives intervals and chords.

## Waves

The Wave button steps through the 16 waves and wraps around after the last one. The wave is saved, so the module starts with the one it was switched off with.

| # | Wave |
|---|---|
| 1 | Sine |
| 2 | Saw |
| 3 | Triangle |
| 4 | Square |
| 5–8 | Four additive waves |
| 9 | Pulse, 25 % |
| 10 | Pulse, 12.5 % |
| 11–16 | Six FM waves |

All waves are 2048 samples long and played at 80 kHz through the RA4M1's 12-bit DAC.
