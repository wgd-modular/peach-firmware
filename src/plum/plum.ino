// Plum - macro oscillator for the Peach module hardware.
// Four synthesis modes, each shaped by two macros on the Detune pot and the
// OSC 2 Level pot + CV. See README.md for the controls.
#include <EEPROM.h>
#include <math.h>

// 48 kHz leaves headroom for the 7-voice supersaw (80 kHz overran the interrupt).
#define FREQ_SAMPLING 48000.0f
const float pitch_calb = 0.972;  // measured on hardware: 0.985 played ~11 cent/V sharp
const float BASE_HZ = 32.7032f;  // C1, lowest C below the old 39 Hz bottom

enum Mode { MODE_FM, MODE_FOLD, MODE_PD, MODE_SUPERSAW, MODE_COUNT };
volatile int mode = MODE_FM;

// Sine table over one cycle, one guard point for interpolation.
const int SIN_SIZE = 1024;
float sin_table[SIN_SIZE + 1];

// Sine of x given in cycles (1.0 = 2*pi), any real x.
static inline float sinc(float x) {
  x -= (float)(int)x;  // cheaper than floorf
  if (x < 0) x += 1.0f;
  float f = x * SIN_SIZE;
  int i = (int)f;
  float a = sin_table[i];
  return a + (sin_table[i + 1] - a) * (f - i);
}

// Parameters written by loop() at 1 kHz, read by the audio interrupt.
volatile float inc = 0;         // base phase increment, cycles per sample
volatile float fm_ratio = 1;    // FM: modulator / carrier frequency ratio
volatile float fm_index = 0;    // FM: modulation depth in cycles
volatile float fold_gain = 1;   // fold: drive into the folder
volatile float fold_bias = 0;   // fold: offset before folding (symmetry)
volatile float pd_knee = 0.5;   // PD saw: phase knee, 0.5 = no distortion
volatile float pd_res = 1;      // PD resonance: resonant frequency ratio
volatile float pd_mix = 0;      // PD: saw (0) -> resonance (1)
const int VOICES = 7;
volatile float voice_inc[VOICES];  // supersaw: per-voice increments
volatile float center_gain = 1, side_gain = 0;  // supersaw: voice 0 vs. the 6 detuned voices

// Oscillator state, only touched by the audio interrupt.
float phase = 0, mod_phase = 0;
float voice_phase[VOICES];

// FM ratios, harmonic so the tone stays pitched.
const int RATIO_COUNT = 9;
const float ratios[RATIO_COUNT] = { 0.5f, 1, 2, 3, 4, 5, 6, 7, 8 };

bool wave_sw = 1;
bool old_wave_sw = 1;
long timer = 0;
float detune_knob = 0, level_knob = 0;  // smoothed A2 / A3 (pot + CV), 0..1

// 12-Bit D/A Converter.The reference source for the DAC settings is below.
//https://github.com/Grumpy-Mike/Game_of_Life_with_sound
#define DACBASE 0x40050000                                                      // DAC Base - DAC output on A0 (P014 AN09 DAC)
#define DAC12_DADR0 ((volatile unsigned short *)(DACBASE + 0xE000))             // D/A Data Register 0
#define DAC12_DACR ((volatile unsigned char *)(DACBASE + 0xE004))               // D/A Control Register
#define DAC12_DADPR ((volatile unsigned char *)(DACBASE + 0xE005))              // DADR0 Format Select Register
#define DAC12_DAADSCR ((volatile unsigned char *)(DACBASE + 0xE006))            // D/A A/D Synchronous Start Control Register
#define DAC12_DAVREFCR ((volatile unsigned char *)(DACBASE + 0xE007))           // D/A VREF Control Register
#define MSTP_MSTPCRD ((volatile unsigned int *)(MSTP + 0x7008))                 // Module Stop Control Register D
#define MSTPD20 20                                                              // DAC12  - 12-Bit D/A Converter Module
#define MSTP 0x40040000                                                         // Module Registers
#define MSTP_MSTPCRB ((volatile unsigned int *)(MSTP + 0x7000))                 // Module Stop Control Register B
#define PFS_P014PFS ((volatile unsigned int *)(PORTBASE + P000PFS + (14 * 4)))  // A0 / DAC12
#define PORTBASE 0x40040000                                                     /* Port Base */
#define P000PFS 0x0800                                                          // Port 0 Pin Function Select Register

//timer setting
//https://github.com/washiyamagiken/AGTimer_R4_Library
#include "AGTimerR4.h"

void setup() {
  pinMode(D8, INPUT_PULLUP);  //mode sw

  for (int i = 0; i <= SIN_SIZE; ++i) {
    sin_table[i] = sinf(2 * PI * i / SIN_SIZE);
  }
  for (int v = 0; v < VOICES; ++v) {
    voice_phase[v] = v / (float)VOICES;  // spread start phases so voices don't sum into a spike
  }

  mode = EEPROM.read(0);
  if (mode >= MODE_COUNT) {
    mode = MODE_FM;
  }
  timer = millis();

  //DAC setting
  *MSTP_MSTPCRD &= ~(0x01 << MSTPD20);  // Enable DAC12 module
  *DAC12_DADPR = 0x00;                  // DADR0 Format Select Register - Set right-justified format
  *DAC12_DAADSCR = 0x00;                // D/A A/D Synchronous Start Control Register - Default
                                        // 36.3.2 Notes on Using the Internal Reference Voltage as the Reference Voltage
  *DAC12_DAVREFCR = 0x00;               // D/A VREF Control Register - Write 0x00 first - see 36.2.5
  *DAC12_DADR0 = 0x0000;                // D/A Data Register 0
  delayMicroseconds(10);                // Needed delay - see data sheet
  *DAC12_DAVREFCR = 0x01;               // D/A VREF Control Register - Select AVCC0/AVSS0 for Vref
  *DAC12_DACR = 0x5F;                   // D/A Control Register -
  delayMicroseconds(5);                 // Needed delay - see data sheet
  *DAC12_DADR0 = 2048;                  // D/A Data Register 0 - value of mid range bias
  *PFS_P014PFS = 0x00000000;            // Port Mode Control - Make sure all bits cleared
  *PFS_P014PFS |= (0x1 << 15);          // ... use as an analog pin

  // start audio last, after tables and DAC are ready
  AGTimer.init(FREQ_SAMPLING, timerCallback);
  AGTimer.start();
}

// PolyBLEP correction for a saw step; t = phase 0..1, dt = increment.
static inline float polyblep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  }
  if (t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

static inline float wrap(float p) {
  return p >= 1.0f ? p - 1.0f : p;
}

//DAC output
void timerCallback() {
  float dt = inc;
  float out;

  switch (mode) {
    case MODE_FM:
      // 2-op FM: modulator runs on its own phase so fractional ratios stay continuous.
      out = sinc(phase + fm_index * sinc(mod_phase));
      mod_phase = wrap(mod_phase + dt * fm_ratio);
      break;

    case MODE_FOLD:
      // Sine folder: driving a sine through another sine folds the wave back on itself.
      out = sinc(0.25f * fold_gain * (sinc(phase) + fold_bias));
      break;

    case MODE_PD: {
      // Casio CZ style phase distortion.
      float k = pd_knee;
      float warped = phase < k ? 0.5f * phase / k : 0.5f + 0.5f * (phase - k) / (1.0f - k);
      float saw = sinc(warped + 0.25f);  // cosine
      // Resonance: cosine at pd_res times the pitch, windowed so it stays continuous at the wrap.
      // Uses the warped phase too, so the distortion shapes both forms.
      float res = 1.0f - (1.0f - sinc(warped * pd_res + 0.25f)) * (1.0f - warped);
      out = saw + (res - saw) * pd_mix;
      break;
    }

    default: {  // MODE_SUPERSAW
      float sum = 0;
      for (int v = 0; v < VOICES; ++v) {
        float p = voice_phase[v];
        float vdt = voice_inc[v];
        sum += (v == 0 ? center_gain : side_gain) * (2.0f * p - 1.0f - polyblep(p, vdt));
        voice_phase[v] = wrap(p + vdt);
      }
      out = sum;
      break;
    }
  }

  if (out > 1.0f) out = 1.0f;
  if (out < -1.0f) out = -1.0f;
  *DAC12_DADR0 = (unsigned short)(2047.5f + 2047.0f * out);

  phase = wrap(phase + dt);
}

void loop() {
  if (timer + 1 <= millis()) {
    old_wave_sw = wave_sw;
    // Average a burst of reads against ADC noise; no smoothing over time, so no pitch slides.
    int osc_cv = 0, freq_pot = 0;
    for (int n = 0; n < 8; ++n) {
      osc_cv += analogRead(A5);
      freq_pot += analogRead(A1);
    }
    wave_sw = digitalRead(D8);

    // 204.8 ADC steps per octave, continuous (no 5.9 cent steps). Pot fully left at 0 V plays BASE_HZ.
    float octaves = (osc_cv * pitch_calb + freq_pot * 0.5f) / (8 * 204.8f);
    float base = exp2f(octaves) * (BASE_HZ / FREQ_SAMPLING);
    inc = base;

    // light smoothing against ADC jitter
    detune_knob += (analogRead(A2) / 1023.0f - detune_knob) * 0.2f;
    level_knob += (analogRead(A3) / 1023.0f - level_knob) * 0.2f;

    switch (mode) {
      case MODE_FM:
        fm_ratio = ratios[(int)(detune_knob * (RATIO_COUNT - 1) + 0.5f)];
        fm_index = level_knob * level_knob * 2.0f;  // squared: fine control at low depth, up to 2 cycles (~12.6 rad)
        break;
      case MODE_FOLD:
        fold_gain = 1.0f + level_knob * 9.0f;
        fold_bias = (detune_knob - 0.5f) * 1.6f;
        break;
      case MODE_PD:
        // OSC 2 Level = distortion only; Detune fades saw -> resonance and raises the resonance with it.
        pd_knee = 0.5f - 0.48f * level_knob;
        pd_res = 1.0f + detune_knob * 7.0f;
        pd_mix = detune_knob;
        break;
      default: {  // MODE_SUPERSAW
        // Detune spreads the outer pair up to +-1 semitone, OSC 2 Level fades the 6 side saws in (JP-8000 style mix).
        float spread = detune_knob * detune_knob * 100.0f;
        float g = 0.7f / sqrtf(1.0f + 6.0f * level_knob * level_knob);  // keeps loudness roughly constant
        center_gain = g;
        side_gain = g * level_knob;
        for (int v = 0; v < VOICES; ++v) {
          float offset = (v + 1) / 2 * (v % 2 ? 1.0f : -1.0f) / 3.0f;  // 0, +-1/3, +-2/3, +-1
          float cents = offset * spread;
          voice_inc[v] = base * powf(2.0f, cents / 1200.0f);
        }
        break;
      }
    }

    if (wave_sw == 0 && old_wave_sw != 0) {
      mode = (mode + 1) % MODE_COUNT;
      EEPROM.write(0, mode);
    }

    timer = millis();
  }
}
