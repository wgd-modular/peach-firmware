# peach-firmware

A repository to collect different firmwares for the Peach module, a small Eurorack oscillator around the Seeed Studio XIAO RA4M1. It has three knobs (Frequency, Detune and OSC 2 Level), two CV inputs (1 V/Oct and OSC 2 Level), a Wave button and one output, driven by the RA4M1's 12-bit DAC. The original firmware is a dual oscillator based on [Hagiwo's RA4M1 oscillator](https://note.com/solder_state/n/na0ae5754fa9c).

## Firmwares

| Folder | Description |
|---|---|
| [`src/peach/`](src/peach/) | Original Peach firmware (Hagiwo-based dual oscillator). See [src/peach/README.md](src/peach/README.md) |
| [`src/apricot/`](src/apricot/) | Wavetable firmware: OSC 2 Level pot/CV scans the wavetables, fixed 50/50 mix, 50 cent detune (fifth at full clockwise). See [src/apricot/README.md](src/apricot/README.md) |

## Flash your module

Flash your module straight from the browser with the [Peach web flasher](https://wgd-modular.github.io/peach-firmware/). Open it in Google Chrome or Edge, connect the XIAO RA4M1 over USB, double-tap RESET to enter the bootloader, then pick a firmware and press flash. It lists every firmware of the latest release together with its controls, and can also flash a local `.bin` you built yourself.

Prefer to grab the raw binaries? Every firmware is pre-compiled and attached to the [latest release](https://github.com/wgd-modular/peach-firmware/releases/latest).

## Manual

The [user manual](https://github.com/wgd-modular/peach-firmware/releases/latest/download/peach-manual.pdf) (PDF) describes the module, how to flash it and every firmware with all of its controls. It is attached to every release, and the CI run of each commit has a build of it as an artifact. To build it yourself run `pixi run manual`; it only needs `typst`, `python` and `git`, managed by pixi, and takes the history of each firmware's folder from `git log`, so clone the repository with its full history.

## Compilation

This step is only required if you want to modify a firmware or create your own. Everything the firmwares need, the arduino-cli toolchain, the Seeed Studio core for the XIAO RA4M1 and the [AGTimer R4 library](https://github.com/washiyamagiken/AGTimer_R4_Library), is managed by [pixi](https://pixi.sh), so nothing has to be installed system-wide apart from pixi itself. It works the same on Linux and macOS (Intel and Apple Silicon); on Windows use WSL. To compile all firmwares run:

```shell
pixi run build
```

The first run downloads the toolchain into `.pixi`, later runs reuse it and work offline. To compile one specific firmware, pass its name:

```shell
pixi run build apricot
```

This places the firmware in `src/apricot/build/apricot.ino.bin`, ready for the web flasher. `pixi run clean` removes all build output again.

### Arduino IDE

The sketches also build in the [Arduino IDE](https://www.arduino.cc/en/software). Add `https://files.seeedstudio.com/arduino/package_renesas_1.2.0_index.json` under *Additional Boards Manager URLs* in **File > Preferences**, install the Seeeduino RA4M1 board package from **Tools > Board > Board Manager**, and add the [AGTimer R4 library as ZIP](https://github.com/washiyamagiken/AGTimer_R4_Library/archive/refs/heads/main.zip) with **Sketch > Include Library > Add .ZIP Library…**. Then select the XIAO RA4M1 and its port under **Tools**, open `src/peach/peach.ino` or `src/apricot/apricot.ino` and upload it.

## Credits

The original Peach firmware builds on [Hagiwo's](https://note.com/solder_state/n/na0ae5754fa9c) code and ideas. Both firmwares use the [AGTimer R4 library](https://github.com/washiyamagiken/AGTimer_R4_Library) by washiyamagiken for their sample timer.

## Contributing

Bugfixes, improvements, and new firmwares are always welcome. Feel free to open an issue or pull request. Each new firmware should be located in a new sub-folder of `src/` with the same name as its `.ino` file, together with a `README.md` file telling users about the controls and other possibly interesting stuff.

The manual is generated from these READMEs, there is no hand-written text per firmware. The first `#` heading is the firmware's name, the text up to the first section heading its introduction, and every heading of the shallowest level below it starts a section. A section called "Controls" is highlighted, and what stands before its first sub-heading goes into a plaque, so put the table of the controls there. Sections for people who build or change the firmware themselves (those called Build & upload, Changing the wavetables, Flashing or Performance, say) are left out, as the manual has a chapter on both. Who wrote a firmware, when it was added and last changed come from the git history of its folder, so credit co-authors with a `Co-authored-by` trailer. Images and tables are fine, keep the text to Markdown.
