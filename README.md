# peach-firmware

A repository to collect different firmwares for the Peach module, a small Eurorack oscillator around the Seeed Studio XIAO RA4M1. It has three knobs (Frequency, Detune and OSC 2 Level), a 1 V/Oct and an OSC 2 Level CV input, a Wave button and one output, driven by the RA4M1's 12-bit DAC. The original firmware is a dual oscillator based on [Hagiwo's RA4M1 oscillator](https://note.com/solder_state/n/na0ae5754fa9c).

## Firmwares

| Folder | Description |
|---|---|
| [`src/peach/`](src/peach/) | Original Peach firmware (Hagiwo-based dual oscillator) |
| [`src/apricot/`](src/apricot/) | Wavetable firmware: OSC 2 Level pot/CV scans the wavetables, fixed 50/50 mix, 50 cent detune (fifth at full clockwise). See [src/apricot/README.md](src/apricot/README.md) |

## Compilation

This step is only required if you want to modify a firmware or create your own. Everything the firmwares need, the arduino-cli toolchain, the Seeed Studio core for the XIAO RA4M1 and the [AGTimer R4 library](https://github.com/washiyamagiken/AGTimer_R4_Library), is managed by [pixi](https://pixi.sh), so nothing has to be installed system-wide apart from pixi itself. It works the same on Linux and macOS (Intel and Apple Silicon); on Windows use WSL. To compile all firmwares run:

```shell
pixi run build
```

The first run downloads the toolchain into `.pixi`, later runs reuse it and work offline. To compile one specific firmware, pass its name:

```shell
pixi run build apricot
```

This places the firmware in `src/apricot/build/apricot.ino.bin`. `pixi run clean` removes all build output again.

### Arduino IDE

The sketches also build in the [Arduino IDE](https://www.arduino.cc/en/software). Add `https://files.seeedstudio.com/arduino/package_renesas_1.2.0_index.json` under *Additional Boards Manager URLs* in **File > Preferences**, install the Seeeduino RA4M1 board package from **Tools > Board > Board Manager**, and add the [AGTimer R4 library as ZIP](https://github.com/washiyamagiken/AGTimer_R4_Library/archive/refs/heads/main.zip) with **Sketch > Include Library > Add .ZIP Library…**. Then select the XIAO RA4M1 and its port under **Tools**, open `src/peach/peach.ino` or `src/apricot/apricot.ino` and upload it.

## Credits

The original Peach firmware builds on [Hagiwo's](https://note.com/solder_state/n/na0ae5754fa9c) code and ideas and uses the [AGTimer R4 library](https://github.com/washiyamagiken/AGTimer_R4_Library) by washiyamagiken for its sample timer.

## Contributing

Bugfixes, improvements, and new firmwares are always welcome, feel free to open an issue or pull request.
