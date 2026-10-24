# Apricot

Wavetable firmware for the **Peach** module hardware (Seeeduino XIAO RA4M1).
Same hardware, same build steps as `src/peach/` — only the sketch differs.

## Changes vs. Peach

| | Peach | Apricot |
|---|---|---|
| Waves | 16 single-cycle waves, one at a time | 5 banks × 16 frames, scanned smoothly (frame crossfade) |
| OSC 2 Level pot + CV (A3) | OSC 2 volume | **Wavetable position** (frame 0 → 15) |
| OSC 1 / OSC 2 mix | OSC 1 50 %, OSC 2 0–50 % | fixed **50 % / 50 %** |
| Detune pot (A2) | 0 → ~+2 octaves | **0 → +50 cent, fifth at full clockwise** |
| Wave button (D8) | next wave | **next bank** (saved in EEPROM) |
| Table size | 2048 samples | 1024 samples (more frames in the same flash) |

## Controls

| Input | Pin | Function |
|---|---|---|
| V/Oct CV | A5 | pitch, 1 V/oct |
| Pitch pot | A1 | coarse pitch |
| Detune pot | A2 | OSC 2 detune, 0 … +50 cent; full clockwise jumps to a fifth |
| OSC 2 Level pot + CV | A3 | wavetable position |
| Wave button | D8 | cycle bank 0 → 4 |
| Out | A0 (DAC) | OSC 1 + OSC 2, 50/50 |

## Wavetable banks

| Bank | Content (frame 0 → 15) |
|---|---|
| 0 | Saw sweep: soft saw → bright saw (4 → 64 harmonics) |
| 1 | PWM: square → 5 % pulse |
| 2 | The 16 original Peach waves, phase-aligned and sorted bright → soft (pulse125, saw … tri, FM3005, sin) for smooth crossfades |
| 3 | 2-op FM, ratio 2:1, index 1 → 8 |
| 4 | Hard-sync saw, slave/master ratio 1.5 → 8 (brightest bank) |

Every frame is DC-free and normalized to the full 12-bit range.

## Changing the wavetables

`wavetables.h` is generated. Edit the bank functions in `tools/gen_wavetables.py`, then run from the repo root:

```bash
python3 src/apricot/tools/gen_wavetables.py
```

Standard-library Python 3 only. Bank 2 is read from `src/peach/peach.ino`, so keep that file.
Flash budget: each bank is 16 × 1024 × 2 B = 32 KB. Current build uses ~214 KB of 256 KB, so 1 more bank fits.

## Build & upload

From the repo root:

```bash
pixi run build apricot
```

This places the firmware in `src/apricot/build/apricot.ino.bin`. See the [main README](../../README.md) for building in the Arduino IDE instead.
