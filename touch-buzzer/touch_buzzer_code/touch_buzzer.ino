/*
 * touch_buzzer.ino  -  Arduino Nano
 *
 * Tap the touch sensor and the buzzer plays a different TUNE depending on
 * how many times you tapped:
 *
 *   1 tap   -> Twinkle Twinkle Little Star
 *   2 taps  -> Ode to Joy
 *   3 taps  -> Mary Had a Little Lamb
 *   4 taps  -> Jingle Bells
 *
 * More than 4 taps is treated as 4. All four are traditional or
 * public-domain melodies, so this is free to publish and teach from.
 *
 * WIRING
 *   Touch sensor VCC -> 5V
 *   Touch sensor GND -> GND
 *   Touch sensor SIG -> D2
 *
 *   D8 -> 220 ohm resistor -> buzzer + (longer leg)
 *                             buzzer - -> GND
 *
 * HOW THE COUNTING WORKS
 *   The sketch cannot know whether a tap is "the first of three" or "a
 *   single tap" at the moment it happens -- it has to wait and see if
 *   another one follows. Each tap restarts a timer; when the timer runs
 *   out with no new tap, whatever the count reached is the answer.
 *
 * Board: Tools > Board > Arduino Nano
 * Proc.: Tools > Processor > ATmega328P
 *        (or "ATmega328P (Old Bootloader)" if uploading fails)
 */

// ---------------------------------------------------------------------
// Note frequencies in hertz. REST means silence.
// ---------------------------------------------------------------------
#define REST 0
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6 1047

// Note lengths in milliseconds. Raise BEAT to slow every tune down.
const uint16_t BEAT    = 300;          // one quarter note
const uint16_t EIGHTH  = BEAT / 2;
const uint16_t QUARTER = BEAT;
const uint16_t HALF    = BEAT * 2;

const uint8_t TOUCH_PIN  = 2;
const uint8_t BUZZER_PIN = 8;

const unsigned long TAP_WINDOW = 450;   // ms to wait for another tap
const unsigned long DEBOUNCE   = 60;    // ms to ignore after an edge
const uint8_t MAX_TAPS = 4;

uint8_t  tapCount   = 0;
bool     lastState  = LOW;
unsigned long lastEdgeTime = 0;
unsigned long lastTapTime  = 0;

// ---------------------------------------------------------------------
// THE FOUR TUNES
//
// Each tune is two matching arrays: the notes, and how long to hold each
// one. To add your own, write both arrays and make sure they are the
// same length.
// ---------------------------------------------------------------------

// 1 tap -- Twinkle Twinkle Little Star
const uint16_t twinkleNotes[] = {
  NOTE_C5, NOTE_C5, NOTE_G5, NOTE_G5, NOTE_A5, NOTE_A5, NOTE_G5
};
const uint16_t twinkleTimes[] = {
  QUARTER, QUARTER, QUARTER, QUARTER, QUARTER, QUARTER, HALF
};

// 2 taps -- Ode to Joy
const uint16_t odeNotes[] = {
  NOTE_E5, NOTE_E5, NOTE_F5, NOTE_G5,
  NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5,
  NOTE_C5, NOTE_C5, NOTE_D5, NOTE_E5, NOTE_E5
};
const uint16_t odeTimes[] = {
  QUARTER, QUARTER, QUARTER, QUARTER,
  QUARTER, QUARTER, QUARTER, QUARTER,
  QUARTER, QUARTER, QUARTER, QUARTER, HALF
};

// 3 taps -- Mary Had a Little Lamb
const uint16_t maryNotes[] = {
  NOTE_E5, NOTE_D5, NOTE_C5, NOTE_D5,
  NOTE_E5, NOTE_E5, NOTE_E5, REST,
  NOTE_D5, NOTE_D5, NOTE_D5, REST,
  NOTE_E5, NOTE_G5, NOTE_G5
};
const uint16_t maryTimes[] = {
  QUARTER, QUARTER, QUARTER, QUARTER,
  QUARTER, QUARTER, QUARTER, EIGHTH,
  QUARTER, QUARTER, HALF,    EIGHTH,
  QUARTER, QUARTER, HALF
};

// 4 taps -- Jingle Bells
const uint16_t jingleNotes[] = {
  NOTE_E5, NOTE_E5, NOTE_E5, REST,
  NOTE_E5, NOTE_E5, NOTE_E5, REST,
  NOTE_E5, NOTE_G5, NOTE_C5, NOTE_D5, NOTE_E5
};
const uint16_t jingleTimes[] = {
  QUARTER, QUARTER, HALF,    EIGHTH,
  QUARTER, QUARTER, HALF,    EIGHTH,
  QUARTER, QUARTER, QUARTER, QUARTER, HALF
};

// ---------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------
void playTune(const uint16_t *notes, const uint16_t *times, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    if (notes[i] == REST) {
      noTone(BUZZER_PIN);
    } else {
      tone(BUZZER_PIN, notes[i], times[i] * 0.9);   // 90% note, 10% gap
    }
    delay(times[i]);
  }
  noTone(BUZZER_PIN);
}

void playResponse(uint8_t taps) {
  switch (taps) {
    case 1:
      playTune(twinkleNotes, twinkleTimes,
               sizeof(twinkleNotes) / sizeof(twinkleNotes[0]));
      break;
    case 2:
      playTune(odeNotes, odeTimes,
               sizeof(odeNotes) / sizeof(odeNotes[0]));
      break;
    case 3:
      playTune(maryNotes, maryTimes,
               sizeof(maryNotes) / sizeof(maryNotes[0]));
      break;
    case 4:
      playTune(jingleNotes, jingleTimes,
               sizeof(jingleNotes) / sizeof(jingleNotes[0]));
      break;
  }
}

// ---------------------------------------------------------------------
void setup() {
  pinMode(TOUCH_PIN, INPUT);       // TTP223 modules drive the pin
                                   // themselves, so no pull-up needed
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("Touch buzzer ready. Tap 1-4 times.");

  // Startup test: a short rising scale.
  // If you hear it, the buzzer is wired correctly.
  const uint16_t testNotes[] = {NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6};
  for (uint8_t i = 0; i < 4; i++) {
    tone(BUZZER_PIN, testNotes[i], 100);
    delay(130);
  }
  noTone(BUZZER_PIN);
}

void loop() {
  bool state = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // --- count a tap on the rising edge (untouched -> touched) ---------
  if (state == HIGH && lastState == LOW && now - lastEdgeTime > DEBOUNCE) {
    if (tapCount < MAX_TAPS) {
      tapCount++;
    }
    lastTapTime  = now;
    lastEdgeTime = now;

    Serial.print("tap ");
    Serial.println(tapCount);
  }
  lastState = state;

  // --- the window has closed, so play the tune ----------------------
  if (tapCount > 0 && now - lastTapTime > TAP_WINDOW) {
    Serial.print("-> playing tune ");
    Serial.println(tapCount);

    playResponse(tapCount);
    tapCount = 0;
  }
}
