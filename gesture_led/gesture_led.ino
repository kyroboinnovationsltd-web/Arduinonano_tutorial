/*
 * gesture_led.ino  -  Arduino Nano
 *
 * Receives a 5-character pattern over USB serial (for example "10110\n")
 * from gesture_led.py and switches 5 LEDs to match.
 *
 *   char 0 -> Thumb  -> LED1 on D2
 *   char 1 -> Index  -> LED2 on D3
 *   char 2 -> Middle -> LED3 on D4
 *   char 3 -> Ring   -> LED4 on D5
 *   char 4 -> Pinky  -> LED5 on D6
 *
 * Wiring for each LED:
 *   Dx -> 220 ohm resistor -> LED anode (long leg)
 *   LED cathode (short leg / flat side) -> GND
 *
 * Board:  Tools > Board > Arduino Nano
 * Proc.:  Tools > Processor > ATmega328P   (or "ATmega328P (Old Bootloader)"
 *         if uploading fails on a clone board)
 * Port:   Tools > Port > the COM port your Nano shows up on
 *
 * IMPORTANT: close the Arduino Serial Monitor before running the Python
 * script. Only one program can hold the serial port at a time.
 */

const uint8_t LED_PINS[5] = {2, 3, 4, 5, 6};
const uint8_t NUM_LEDS = 5;

const unsigned long TIMEOUT_MS = 2000;   // no data for 2 s -> all LEDs off

char buffer[16];
uint8_t bufIndex = 0;
unsigned long lastPacket = 0;

void setAll(bool on) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

void applyPattern(const char *pattern) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], pattern[i] == '1' ? HIGH : LOW);
  }
}

void setup() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  Serial.begin(115200);

  // quick startup sweep so you can see all 5 LEDs are wired correctly
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(120);
    digitalWrite(LED_PINS[i], LOW);
  }

  lastPacket = millis();
}

void loop() {
  // ---- read one newline-terminated line from the PC --------------------
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (bufIndex >= NUM_LEDS) {
        buffer[bufIndex] = '\0';
        applyPattern(buffer);
        lastPacket = millis();
      }
      bufIndex = 0;
    }
    else if (c == '0' || c == '1') {
      if (bufIndex < sizeof(buffer) - 1) {
        buffer[bufIndex++] = c;
      }
    }
    else {
      // anything unexpected: throw the partial line away
      bufIndex = 0;
    }
  }

  // ---- safety: if the PC stops talking, switch everything off ----------
  if (millis() - lastPacket > TIMEOUT_MS) {
    setAll(false);
    lastPacket = millis();
  }
}
