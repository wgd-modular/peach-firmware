// Apricot - wavetable firmware for the Peach module hardware.
// Based on Peach / Hagiwo. See apricot/README.md for controls.
#include <EEPROM.h>
#include "wavetables.h"

const float pitch_calb = 0.985;
const int octave_select = 2;
// Detune: 0..50 cent over the pot travel, jumps to a fifth (700 cent) at full clockwise.
const float DETUNE_MAX_CENT = 50.0f;
const int DETUNE_FIFTH_THRESHOLD = 1010;  // ADC value counted as full clockwise (headroom for noise)

volatile float pitch = 0;   // phase increment osc1 (samples of WT_SIZE)
volatile float pitch2 = 0;  // phase increment osc2
volatile float morph = 0;   // frame position 0..WT_FRAMES-1
volatile int bank = 0;      // active wavetable bank
float table_progress = 0;
float table_progress2 = 0;

bool wave_sw = 1;
bool old_wave_sw = 1;
long timer = 0;
float voct_table[2048];

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
#define FREQ_SAMPLING 80000.0f

void setup() {
  pinMode(D8, INPUT_PULLUP);  //bank sw

  // make V/oct table
  for (int i = 0; i < 2048; ++i) {
    voct_table[i] = pow(2, i / 204.8);
  }

  bank = EEPROM.read(0);
  if (bank >= WT_BANKS) {
    bank = 0;
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

// Crossfade between two neighbouring frames at sample index i.
static inline float readMorph(const uint16_t (*frames)[WT_SIZE], int fi, float ff, int i) {
  float a = frames[fi][i];
  float b = frames[fi + 1][i];
  return a + (b - a) * ff;
}

//DAC output
void timerCallback() {
  if (table_progress >= WT_SIZE) table_progress -= WT_SIZE;
  if (table_progress2 >= WT_SIZE) table_progress2 -= WT_SIZE;

  float m = morph;
  int fi = (int)m;
  if (fi > WT_FRAMES - 2) fi = WT_FRAMES - 2;
  float ff = m - fi;
  const uint16_t(*frames)[WT_SIZE] = wavetable[bank];

  // OSC1 + OSC2 fixed 50/50 mix
  float out = (readMorph(frames, fi, ff, (int)table_progress) + readMorph(frames, fi, ff, (int)table_progress2)) * 0.5f;
  *DAC12_DADR0 = (unsigned short)out;

  table_progress += pitch;
  table_progress2 += pitch2;
}

void loop() {
  if (timer + 1 <= millis()) {
    old_wave_sw = wave_sw;
    int osc_cv = analogRead(A5);
    wave_sw = digitalRead(D8);

    // voct_table increments are for a 2048 table; WT_SIZE scales them down
    float p = voct_table[(int)(osc_cv * pitch_calb) + analogRead(A1) / 2] * octave_select * 0.5f * (WT_SIZE / 2048.0f);
    pitch = p;
    int detune_pot = analogRead(A2);
    float cent = detune_pot >= DETUNE_FIFTH_THRESHOLD ? 700.0f : detune_pot * (DETUNE_MAX_CENT / DETUNE_FIFTH_THRESHOLD);
    pitch2 = p * powf(2.0f, cent / 1200.0f);

    // former OSC 2 Level pot + CV: scans through the wavetable frames
    morph = analogRead(A3) * ((WT_FRAMES - 1) / 1023.0f);

    if (wave_sw == 0 && old_wave_sw != 0) {
      bank = (bank + 1) % WT_BANKS;
      EEPROM.write(0, bank);
    }

    timer = millis();
  }
}
