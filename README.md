# Peach Firmware 🍑

**Firmware for the Peach Eurorack oscillator module**  
Powered by the Seeduino XIAO (RA4M1) and inspired by the amazing work of [Hagiwo](https://note.com/solder_state/n/na0ae5754fa9c).

Peach is a super compact and beginner-friendly dual oscillator Eurorack module with selectable waveforms, CV control, and stereo output — great for tiny cases or as a Plaits-style budget voice.

This firmware drives the DAC directly using register-level access and uses the [AGTimerR4 library](https://github.com/washiyamagiken/AGTimer_R4_Library) for high-speed waveform updates.

## Features

- Dual oscillator with crossfading and pitch spread
- Selectable wavetables (up to 16)
- V/Oct input
- Waveform switch with EEPROM memory
- Compact and efficient implementation
- Built on top of Hagiwo's code and ideas

## Firmwares

| Folder | Description |
|---|---|
| [`src/peach/`](src/peach/) | Original Peach firmware (Hagiwo-based dual oscillator) |
| [`src/apricot/`](src/apricot/) | Wavetable firmware: OSC 2 Level pot/CV scans the wavetables, fixed 50/50 mix, 50 cent detune (fifth at full clockwise). See [src/apricot/README.md](src/apricot/README.md) |

## Flash your module

Flash your module straight from the browser with the [Peach web flasher](https://wgd-modular.github.io/peach-firmware/). Open it in Google Chrome or Edge, connect the XIAO RA4M1 over USB, double-tap RESET to enter the bootloader, then pick a firmware and press flash. It lists every firmware of the latest release together with its controls, and can also flash a local `.bin` you built yourself.

Prefer to grab the raw binaries? Every firmware is pre-compiled and attached to the [latest release](https://github.com/wgd-modular/peach-firmware/releases/latest).

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

Huge thanks to **Hagiwo** for the original concept, code, and inspiration. This firmware builds upon his public example and adapts it to a more production-ready, PCB-based format.

## Contributing

Bugfixes, improvements, and additional waveform support are always welcome — feel free to open an issue or pull request.

---

🧡 Stay patched!
