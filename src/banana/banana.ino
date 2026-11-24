/*
  Banana for Peach
  ================
  Hagiwo's chord VCO, the firmware of WGD Modular's Banana module, on Peach.
  Four voices play one of eight chords in 16 inversions, and in half of the
  inversions a fifth voice adds the root as a bass note. All voices play the
  same wave, one of eight.

  Pins:
  1 V/Oct CV            A5  pitch
  Frequency pot         A1  coarse pitch, fully left = C1 at 0 V
  Detune pot            A2  inversion, 16 steps
  OSC 2 Level pot + CV  A3  chord, 8 steps
  Wave button           D8  next wave, saved in EEPROM
  Out                   A0  DAC

  Hagiwo's chord VCO: https://note.com/solder_state/n/n681d2e07e324
*/
#include <EEPROM.h>
#include <math.h>

// Lower than on Melon, so five voices of the busiest waves still leave the
// audio interrupt a good margin at 48 MHz. The fades and the sine band below
// sit at the same fractions of the sample rate as on Melon.
const float SAMPLE_RATE = 40000.0f;
const int VOICES = 5;  // four for the chord, the last one is the bass
const float VOICE_LEVEL = 0.19f;  // five voices at full level stay below full scale

// Pitch as in Plum, with 8 reads of the 12-bit ADC added up and 819.2 steps
// per octave.
const float PITCH_CAL = 0.972f;  // measured on hardware for Plum
const float BASE_HZ = 32.7032f;  // C1, Frequency pot fully left at 0 V

// Voices fade out between these frequencies, before they would fold back
// from half the sample rate.
const float FADE_START_HZ = 13500.0f;
const float FADE_END_HZ = 16500.0f;

// Between these frequencies a voice turns into a sine, as the overtones of
// the other waves would start to fold back. The triangle drops its overtones
// one by one instead.
const float SINE_START_HZ = 4200.0f;
const float SINE_END_HZ = 6700.0f;

// A voice fades in or out with a time constant of 7 ms instead of clicking,
// and changing the wave dips the output for 3 ms each way. A voice whose
// note the pitch pushes past the top cannot follow it, so it fades out on its
// old frequency with a time constant of 1.5 ms instead. A note of the new
// chord that lands there takes it over, see updateVoices(). If it is too
// quiet for that and plays a sine, it moves up to PARKED_HZ, where no note
// plays, so the two do not cancel out.
const float LEVEL_SMOOTHING = 1.0f / (0.007f * SAMPLE_RATE);
const float RELEASE_SMOOTHING = 1.0f / (0.0015f * SAMPLE_RATE);
const float WAVE_FADE_STEP = 1.0f / (0.003f * SAMPLE_RATE);
const float PARKED_HZ = (FADE_END_HZ + SAMPLE_RATE / 2.0f) / 2.0f;
const uint32_t PARKED_INCREMENT = (uint32_t)(PARKED_HZ * (4294967296.0f / SAMPLE_RATE));

enum Wave { SAW, SQUARE, TRIANGLE, SINE, CHEBYSHEV, HALF_SINE, SIGMOID, RAMP, WAVES };

// The wave is saved at address 0 as SETTING_BASE plus its number, anything
// else (such as another firmware's setting) starts on the saw.
const int SETTING_ADDRESS = 0;
const uint8_t SETTING_BASE = 0xB0;

// Semitones above the root of the four chord voices.
const uint8_t CHORDS[8][4] = {
  { 0, 4, 7, 12 },  // major
  { 0, 4, 7, 11 },  // major 7th
  { 0, 4, 7, 14 },  // major add 9
  { 0, 2, 7, 12 },  // sus2
  { 0, 3, 7, 14 },  // minor add 9
  { 0, 3, 7, 10 },  // minor 7th
  { 0, 3, 7, 12 },  // minor
  { 0, 0, 0, 0 },   // root only
};

// How many octaves each chord voice moves up, and whether the bass plays.
// Going up, the voices move up one after the other, then the bass comes in
// and they come back down.
struct Inversion {
  uint8_t up[4];
  bool bass;
};
const Inversion INVERSIONS[16] = {
  { { 0, 0, 0, 0 }, false }, { { 1, 0, 0, 0 }, false }, { { 1, 1, 0, 0 }, false }, { { 1, 1, 1, 0 }, false },
  { { 1, 1, 1, 1 }, false }, { { 2, 1, 1, 1 }, false }, { { 2, 2, 1, 1 }, false }, { { 2, 2, 2, 1 }, false },
  { { 2, 2, 2, 1 }, true },  { { 2, 2, 1, 1 }, true },  { { 2, 1, 1, 1 }, true },  { { 1, 1, 1, 1 }, true },
  { { 1, 1, 1, 0 }, true },  { { 1, 1, 0, 0 }, true },  { { 1, 0, 0, 0 }, true },  { { 0, 0, 0, 0 }, true },
};

// One cycle each, with the value at the end of the cycle as the last entry.
const int TABLE_SIZE = 1024;
float sineTable[TABLE_SIZE + 1];
float sigmoidTable[TABLE_SIZE + 1];
const float SIGMOID_PEAK = 0.9950548f;  // tanh(3), where the sigmoid jumps back

// The triangle Banana plays is made of these four odd harmonics, a triangle
// with rounded corners. Each voice leaves out the ones that would fold back
// at its pitch.
const float TRIANGLE_HARMONICS[4] = { 0.6927f, -0.1696f, 0.0220f, -0.0044f };

// The fundamental of each wave, the sine a high voice turns into. The half
// sine's is a cosine.
const float FUNDAMENTAL[WAVES] = { -0.63662f, 1.14592f, 0.0f, 1.0f, 0.91145f, 0.25383f, -1.05586f, -0.63662f };
const uint32_t PHASE_QUARTER = 0x40000000u;

struct Voice {
  // Written by loop(), read by the audio interrupt. loop() writes them with
  // the interrupts off, so the interrupt never sees half of a change.
  uint32_t increment;
  float inverseIncrement;  // 1 / increment, so the audio code needs no division
  float targetLevel;
  float smoothing;  // LEVEL_SMOOTHING or RELEASE_SMOOTHING
  float sineMix;  // 0 plays the wave, 1 only its fundamental
  uint8_t sound;   // PLAY_WAVE, PLAY_BLEND or PLAY_SINE, from sineMix
  float triangleShape[4];
  // Only touched by the audio interrupt.
  uint32_t phase;
  float level;
};
enum Sound { PLAY_WAVE, PLAY_BLEND, PLAY_SINE };
volatile Voice voices[VOICES];
volatile bool anySineMix = false;  // whether any voice has a sineMix above 0
volatile uint8_t selectedWave = SAW;

// Only touched by the audio interrupt.
uint8_t wave = SAW;
float waveFade = 1.0f;

int inversionIndex = 0;
int chordIndex = 0;

// The note each voice plays, in semitones above the root, or played last
// while it fades out, and for how many of the five voices it plays.
int8_t voiceNote[VOICES] = { -1, -1, -1, -1, -1 };
int voiceCount[VOICES];
bool voiceOn[VOICES];

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

// The audio code below is inlined into one loop per wave, which keeps the
// audio interrupt short. It runs from RAM, as the flash needs a wait state
// at 48 MHz and five voices of the busier waves would take up most of the
// time between two samples from there.
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define IN_RAM __attribute__((section(".code_in_ram")))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// A table at a phase, the top 10 bits pick the entry and the rest
// interpolate to the next one.
static ALWAYS_INLINE float lookup(const float *table, uint32_t phase) {
  uint32_t i = phase >> 22;
  float a = table[i];
  return a + (table[i + 1] - a) * ((phase & 0x3FFFFF) * (1.0f / 4194304.0f));
}

// Where the waves jump or have a corner.
const uint32_t PHASE_SIXTH = 0x2AAAAAABu;
const uint32_t PHASE_HALF = 0x80000000u;
const uint32_t PHASE_FIVE_SIXTHS = 0xD5555555u;

// Jumps and corners are smoothed over one sample on either side, so they do
// not alias. x is the phase since the jump plus one increment, which is
// below two increments only for the two samples next to it.
#define NEAR(x) UNLIKELY((x) < 2 * inc)

// What a jump of -2 is smoothed by.
static ALWAYS_INLINE float polyblep(uint32_t x, uint32_t inc, float inverse) {
  float t = (int32_t)(x - inc) * inverse;  // -1 to 1 samples from the jump
  return t < 0.0f ? (t + 1.0f) * (t + 1.0f) : -(1.0f - t) * (1.0f - t);
}

// The same for a corner where the slope rises by 1 per cycle.
static ALWAYS_INLINE float polyblamp(uint32_t x, uint32_t inc, float inverse) {
  float t = (int32_t)(x - inc) * inverse;
  t = 1.0f - (t < 0.0f ? -t : t);
  return t * t * t * (inc * (2.3283064e-10f / 6.0f));
}

// One sample of voice v's wave at the given phase and phase increment.
static ALWAYS_INLINE float waveAt(uint8_t w, uint32_t phase, uint32_t inc, float inverse, volatile Voice &v) {
  // The top 24 bits convert to a float below 1 exactly.
  float p = (phase >> 8) * (1.0f / 16777216.0f);
  uint32_t next = phase + inc;
  float out;
  switch (w) {
    case SAW:
      out = 2.0f * p - 1.0f;
      if (NEAR(next)) out -= polyblep(next, inc, inverse);
      return out;
    case SQUARE:
      out = phase < PHASE_HALF ? 0.9f : -0.9f;
      if (NEAR(next)) out += 0.9f * polyblep(next, inc, inverse);
      if (NEAR(next - PHASE_HALF)) out -= 0.9f * polyblep(next - PHASE_HALF, inc, inverse);
      return out;
    case TRIANGLE: {
      // The harmonics summed as a polynomial in the sine, see setVoice().
      float s = lookup(sineTable, phase);
      float s2 = s * s;
      const volatile float *c = v.triangleShape;
      return s * (c[0] + s2 * (c[1] + s2 * (c[2] + s2 * c[3])));
    }
    case SINE:
      return lookup(sineTable, phase);
    case CHEBYSHEV: {
      // The third Chebyshev polynomial swept from -1 to 1, then back.
      float x = 2.0f * p - 1.0f;
      out = x * (4.0f * x * x - 3.0f);
      if (NEAR(next)) out -= polyblep(next, inc, inverse);
      return out;
    }
    case HALF_SINE:
      // Banana's half sine wraps around where it passes the top of its 8-bit
      // range, at 1/6 and 5/6 of the cycle. This keeps those two jumps. At
      // the start of the cycle its slope turns from -2 pi to 2 pi.
      out = 2.0f * lookup(sineTable, phase >> 1);
      if (phase - PHASE_SIXTH < PHASE_FIVE_SIXTHS - PHASE_SIXTH) out -= 2.0f;
      if (NEAR(next - PHASE_SIXTH)) out -= polyblep(next - PHASE_SIXTH, inc, inverse);
      if (NEAR(next - PHASE_FIVE_SIXTHS)) out += polyblep(next - PHASE_FIVE_SIXTHS, inc, inverse);
      if (NEAR(next)) out += 12.566371f * polyblamp(next, inc, inverse);  // 4 pi
      return out;
    case SIGMOID:
      out = lookup(sigmoidTable, phase);
      if (NEAR(next)) out -= SIGMOID_PEAK * polyblep(next, inc, inverse);
      return out;
    default:
      // The ramp table Banana plays rises in 256 steps. The steps stay as
      // they are, the jump back down is smoothed like the saw's.
      out = (phase >> 24) * (1.0f / 128.0f) - 1.0f;
      if (NEAR(next)) out -= (255.0f / 256.0f) * polyblep(next, inc, inverse);
      return out;
  }
}

// All voices playing wave w. Called with constants, so each call becomes a
// loop of its own for that wave. Only the loop for when some voices are high
// blends them with their fundamental.
static ALWAYS_INLINE float sumVoices(uint8_t w, bool blend) {
  float sum = 0.0f;
  for (int i = 0; i < VOICES; i++) {
    volatile Voice &v = voices[i];
    uint32_t inc = v.increment;
    uint32_t p = v.phase;
    v.phase = p + inc;
    float l = v.level;
    l += (v.targetLevel - l) * v.smoothing;
    v.level = l;
    if (!blend) {
      sum += l * waveAt(w, p, inc, v.inverseIncrement, v);
      continue;
    }
    uint8_t sound = v.sound;
    float out;
    if (LIKELY(sound == PLAY_WAVE)) {
      out = waveAt(w, p, inc, v.inverseIncrement, v);
    } else {
      out = FUNDAMENTAL[w] * lookup(sineTable, w == HALF_SINE ? p + PHASE_QUARTER : p);
      if (sound == PLAY_BLEND) out += (1.0f - v.sineMix) * (waveAt(w, p, inc, v.inverseIncrement, v) - out);
    }
    sum += l * out;
  }
  return sum;
}

static ALWAYS_INLINE float playWave(uint8_t w) {
  if (w == SINE || w == TRIANGLE || !anySineMix) return sumVoices(w, false);
  return sumVoices(w, true);
}

IN_RAM float computeSample() {
  // A new wave fades in after the old one has faded out.
  uint8_t next = selectedWave;
  if (next != wave) {
    waveFade -= WAVE_FADE_STEP;
    if (waveFade <= 0.0f) {
      waveFade = 0.0f;
      wave = next;
    }
  } else if (waveFade < 1.0f) {
    waveFade = min(1.0f, waveFade + WAVE_FADE_STEP);
  }

  float sum;
  switch (wave) {
    case SAW: sum = playWave(SAW); break;
    case SQUARE: sum = playWave(SQUARE); break;
    case TRIANGLE: sum = playWave(TRIANGLE); break;
    case SINE: sum = playWave(SINE); break;
    case CHEBYSHEV: sum = playWave(CHEBYSHEV); break;
    case HALF_SINE: sum = playWave(HALF_SINE); break;
    case SIGMOID: sum = playWave(SIGMOID); break;
    default: sum = playWave(RAMP); break;
  }
  return sum * waveFade;
}

IN_RAM void timerCallback() {
  // The sample computed last time goes out first, so the samples leave at
  // even intervals however long each one takes to compute.
  static unsigned short next = 2048;
  *DAC12_DADR0 = next;
  float out = computeSample();
  if (out > 1.0f) out = 1.0f;
  if (out < -1.0f) out = -1.0f;
  next = (unsigned short)(2047.5f + 2047.0f * out);
}

// 0 to 1, averaged against ADC noise.
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return sum / (4 * 4095.0f);
}

// Steps a pot through `count` positions. The position only changes once the
// pot is a bit past the edge, so it does not flicker on a boundary. With
// current at -1 it takes the position the pot is at.
int stepOf(float position, int current, int count) {
  float x = position * count;
  if (current >= 0 && x > current - 0.25f && x < current + 1.25f) return current;
  return constrain((int)x, 0, count - 1);
}

bool buttonPressed() {
  static bool last = HIGH, stable = HIGH;
  static uint32_t changedAt = 0;
  bool now = digitalRead(D8);
  if (now != last) {
    last = now;
    changedAt = millis();
  }
  if (now != stable && millis() - changedAt >= 10) {
    stable = now;
    return stable == LOW;
  }
  return false;
}

float fadeAt(float frequency) {
  return constrain((FADE_END_HZ - frequency) / (FADE_END_HZ - FADE_START_HZ), 0.0f, 1.0f);
}

// Sets voice v to play the note at `frequency` for `count` of the five
// voices.
// A voice that is off gets the frequency too, so it fades out in tune.
void setVoice(int v, float frequency, int count, bool on) {
  float inc = frequency * (4294967296.0f / SAMPLE_RATE);
  float inverse = 1.0f / inc;
  // Voices on the same note would add up or cancel out depending on how
  // their phases happen to line up. Played as one note, they are as loud as
  // they are on average.
  float target = on ? VOICE_LEVEL * sqrtf((float)count) * fadeAt(frequency) : 0.0f;
  float mix = constrain((frequency - SINE_START_HZ) / (SINE_END_HZ - SINE_START_HZ), 0.0f, 1.0f);

  // sin(3x) = 3s - 4s^3, sin(5x) = 5s - 20s^3 + 16s^5 and
  // sin(7x) = 7s - 56s^3 + 112s^5 - 64s^7 with s = sin(x), so the triangle
  // is an odd polynomial in s.
  float a[4];
  a[0] = TRIANGLE_HARMONICS[0];
  for (int k = 1; k < 4; k++) a[k] = TRIANGLE_HARMONICS[k] * fadeAt(frequency * (2 * k + 1));
  float shape[4] = {
    a[0] + 3.0f * a[1] + 5.0f * a[2] + 7.0f * a[3],
    -4.0f * a[1] - 20.0f * a[2] - 56.0f * a[3],
    16.0f * a[2] + 112.0f * a[3],
    -64.0f * a[3],
  };

  volatile Voice &voice = voices[v];
  noInterrupts();
  voice.increment = (uint32_t)inc;
  voice.inverseIncrement = inverse;
  voice.targetLevel = target;
  voice.smoothing = LEVEL_SMOOTHING;
  voice.sineMix = mix;
  voice.sound = mix <= 0.0f ? PLAY_WAVE : mix < 1.0f ? PLAY_BLEND : PLAY_SINE;
  for (int k = 0; k < 4; k++) voice.triangleShape[k] = shape[k];
  interrupts();
}

void updateVoices() {
  // Average a burst of reads against ADC noise, without smoothing over time
  // so the pitch does not slide.
  uint32_t cv = 0, pot = 0;
  for (int i = 0; i < 8; i++) {
    cv += analogRead(A5);
    pot += analogRead(A1);
  }
  float root = BASE_HZ * exp2f((cv * PITCH_CAL + pot * 0.5f) / (8 * 819.2f));
  const Inversion &inversion = INVERSIONS[inversionIndex];

  // The notes of the chord and how many voices play each. Notes too high to
  // play are left out.
  int8_t note[VOICES];
  int count[VOICES];
  int notes = 0;
  for (int v = 0; v < VOICES; v++) {
    if (v == 4 && !inversion.bass) break;
    int n = v < 4 ? CHORDS[chordIndex][v] + 12 * inversion.up[v] : 0;
    if (root * exp2f(n / 12.0f) >= FADE_END_HZ) continue;
    int k = 0;
    while (k < notes && note[k] != n) k++;
    if (k == notes) {
      note[notes] = n;
      count[notes++] = 0;
    }
    count[k]++;
  }

  // A note that stays keeps its voice, so only the notes that change move.
  int8_t voiceOf[VOICES];
  bool freed[VOICES];
  for (int k = 0; k < notes; k++) voiceOf[k] = -1;
  for (int v = 0; v < VOICES; v++) {
    freed[v] = false;
    if (!voiceOn[v]) continue;
    int k = 0;
    while (k < notes && note[k] != voiceNote[v]) k++;
    if (k < notes) {
      voiceOf[k] = v;
    } else {
      voiceOn[v] = false;
      freed[v] = true;
    }
  }
  // A note that is still fading out comes back on its voice.
  for (int k = 0; k < notes; k++) {
    for (int v = 0; v < VOICES && voiceOf[k] < 0; v++) {
      if (!voiceOn[v] && !freed[v] && voiceNote[v] == note[k]) {
        voiceOf[k] = v;
        voiceOn[v] = true;
      }
    }
  }
  // The other new notes, from low to high, take over the voices whose notes
  // went away, from low to high, as they do on Banana, if the voice played
  // as many voices' worth and so does not get louder or quieter on the new
  // note. Otherwise the new note takes the quietest voice that is off. That
  // one is silent unless the knobs move quickly, then the new note starts
  // from the level it still has.
  while (true) {
    int k = -1;
    for (int i = 0; i < notes; i++) {
      if (voiceOf[i] < 0 && (k < 0 || note[i] < note[k])) k = i;
    }
    if (k < 0) break;
    int v = -1;
    for (int u = 0; u < VOICES; u++) {
      if (freed[u] && !voiceOn[u] && voiceCount[u] == count[k] && (v < 0 || voiceNote[u] < voiceNote[v])) v = u;
    }
    if (v < 0) {
      for (int u = 0; u < VOICES; u++) {
        if (!voiceOn[u] && (v < 0 || voices[u].level < voices[v].level)) v = u;
      }
    }
    voiceOf[k] = v;
    voiceOn[v] = true;
  }

  for (int k = 0; k < notes; k++) {
    int v = voiceOf[k];
    voiceNote[v] = note[k];
    voiceCount[v] = count[k];
  }
  // The other voices fade out on the note they had and follow the pitch on
  // it. No voice that is on plays that note, so they never meet one. Those
  // the pitch pushes past the top stay where they are.
  float frequencyOf[VOICES];
  bool follows[VOICES];
  bool high = false;
  for (int v = 0; v < VOICES; v++) {
    frequencyOf[v] = voiceNote[v] < 0 ? 0.0f : root * exp2f(voiceNote[v] / 12.0f);
    follows[v] = voiceNote[v] >= 0 && frequencyOf[v] < FADE_END_HZ;
    if (follows[v] && frequencyOf[v] > SINE_START_HZ) high = true;
  }
  // If a note lands within about a semitone of a voice that stayed, while
  // that voice is still at least half as loud as the note will be, the two
  // would add up or cancel out. The note takes the closest such voice over
  // instead, if it is closer than the note's own voice still is, and the
  // voice it had stays where it was.
  bool tookOver[VOICES] = { false };
  for (int pass = 0; pass < VOICES; pass++) {
    bool moved = false;
    for (int k = 0; k < notes; k++) {
      if (tookOver[k]) continue;
      int a = voiceOf[k];
      float frequency = frequencyOf[a];
      float level = VOICE_LEVEL * sqrtf((float)count[k]) * fadeAt(frequency);
      int b = -1;
      float here = voices[a].increment * (SAMPLE_RATE / 4294967296.0f);
      float gap = fminf(frequency / 16.0f, fabsf(here - frequency));
      for (int u = 0; u < VOICES; u++) {
        if (follows[u] || voices[u].level < 0.5f * level) continue;
        float there = voices[u].increment * (SAMPLE_RATE / 4294967296.0f);
        if (fabsf(there - frequency) < gap) {
          b = u;
          gap = fabsf(there - frequency);
        }
      }
      if (b < 0) continue;
      voiceOf[k] = b;
      voiceOn[b] = true;
      voiceNote[b] = note[k];
      voiceCount[b] = count[k];
      frequencyOf[b] = frequency;
      follows[b] = true;
      voiceOn[a] = false;
      voiceNote[a] = -1;
      follows[a] = false;
      tookOver[k] = moved = true;
    }
    if (!moved) break;
  }
  // The audio code only looks at the sine mix of each voice while
  // anySineMix is set, so it is set before a voice gets one.
  if (high) anySineMix = true;
  for (int v = 0; v < VOICES; v++) {
    if (follows[v]) {
      setVoice(v, frequencyOf[v], voiceCount[v], voiceOn[v]);
      continue;
    }
    // A quieter one that plays a sine moves out of the way instead.
    bool park = false;
    if (voices[v].sound == PLAY_SINE) {
      float frequency = voices[v].increment * (SAMPLE_RATE / 4294967296.0f);
      for (int u = 0; u < VOICES; u++) {
        if (voiceOn[u] && fabsf(frequencyOf[u] - frequency) < frequency / 16.0f) park = true;
      }
    }
    noInterrupts();
    if (park) {
      voices[v].increment = PARKED_INCREMENT;
      voices[v].inverseIncrement = 1.0f / PARKED_INCREMENT;
    }
    voices[v].targetLevel = 0.0f;
    voices[v].smoothing = RELEASE_SMOOTHING;
    interrupts();
  }
  bool any = false;
  for (int v = 0; v < VOICES; v++) {
    if (voices[v].sineMix > 0.0f) any = true;
  }
  anySineMix = any;
}

void setup() {
  pinMode(D8, INPUT_PULLUP);
  analogReadResolution(12);

  for (int i = 0; i <= TABLE_SIZE; i++) {
    float p = (float)i / TABLE_SIZE;
    sineTable[i] = sinf(2.0f * (float)PI * p);
    sigmoidTable[i] = tanhf(3.0f * (2.0f * p - 1.0f));
  }

  uint8_t saved = EEPROM.read(SETTING_ADDRESS);
  if (saved >= SETTING_BASE && saved < SETTING_BASE + WAVES) {
    selectedWave = saved - SETTING_BASE;
    wave = selectedWave;
  }

  inversionIndex = stepOf(readPot(A2), -1, 16);
  chordIndex = stepOf(readPot(A3), -1, 8);
  updateVoices();

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
  AGTimer.init(SAMPLE_RATE, timerCallback);
  AGTimer.start();
}

void loop() {
  static uint32_t lastRead = 0;
  uint32_t now = micros();
  if (now - lastRead < 1000) return;
  lastRead = now;

  inversionIndex = stepOf(readPot(A2), inversionIndex, 16);
  chordIndex = stepOf(readPot(A3), chordIndex, 8);
  if (buttonPressed()) {
    selectedWave = (selectedWave + 1) % WAVES;
    EEPROM.update(SETTING_ADDRESS, SETTING_BASE + selectedWave);
  }
  updateVoices();
}
