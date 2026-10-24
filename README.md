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
| [`peach/`](peach/) | Original Peach firmware (Hagiwo-based dual oscillator) |
| [`apricot/`](apricot/) | Wavetable firmware: OSC 2 Level pot/CV scans the wavetables, fixed 50/50 mix, 12 cent detune (fifth at full clockwise). See [apricot/README.md](apricot/README.md) |

## Installation

1. **Install Arduino IDE**  
   Download and install the [Arduino IDE](https://www.arduino.cc/en/software) for your OS.

2. **Add Seeduino RA4M1 support**  
   Go to **File > Preferences** and paste this URL under *Additional Boards Manager URLs*:  

```https://files.seeedstudio.com/arduino/package_renesas_1.2.0_index.json```

Then open **Tools > Board > Board Manager**, search for “RA4M1”, and install the Seeeduino RA4M1 board package.

3. **Install AGTimer_R4 Library**  
Download the [AGTimer_R4 library as ZIP](https://github.com/washiyamagiken/AGTimer_R4_Library/archive/refs/heads/main.zip), then in Arduino IDE go to **Sketch > Include Library > Add .ZIP Library…** and select the downloaded file.

4. **Connect Seeduino XIAO (RA4M1)**  
Select the board and port under **Tools > Board** and **Tools > Port**, then upload `peach.ino`.

## Credits

Huge thanks to **Hagiwo** for the original concept, code, and inspiration. This firmware builds upon his public example and adapts it to a more production-ready, PCB-based format.

## Contributing

Bugfixes, improvements, and additional waveform support are always welcome — feel free to open an issue or pull request.

---

🧡 Stay patched!
