/*
  ============================================================
   MEDICINE DISPENSER / REMINDER  -  3 doses per day
  ============================================================
  Hardware : Arduino Nano, DS3231 RTC, 16x2 LCD, 3 buttons,
             3 LEDs, active buzzer
  Library  : "RTClib" by Adafruit (install from Library Manager)

  WIRING
    LCD   RS=D3  E=D4  D4=D5  D5=D6  D6=D7  D7=D8
          VSS=GND VDD=5V RW=GND VO=10K pot middle  A=5V  K=GND
    RTC   SDA=A4  SCL=A5  VCC=5V  GND=GND
    SET   A2 -> 1K -> button -> GND
    INC   A1 -> 1K -> button -> GND
    OK    A0 -> 1K -> button -> GND
    LED1 (Dose 1, red)    D10 -> 220R -> LED+ , LED- -> GND
    LED2 (Dose 2, yellow) D11 -> 220R -> LED+ , LED- -> GND
    LED3 (Dose 3, green)  D12 -> 220R -> LED+ , LED- -> GND
    Buzzer + -> D9 , Buzzer - -> GND

  HOW TO USE
    Normal screen : time on line 1, date / next dose on line 2.
    Press SET     : menu  ->  "Dose times" / "Set clock" / "Exit"
                    INC = next option / increase value (hold to scroll)
                    OK  = select / confirm
    Dose times    : for each dose choose ON/OFF, then hour, then minute.
                    Times are saved in EEPROM (kept after power off).
    Set clock     : hour, minute, day, month, year -> saved in the RTC.
    At dose time  : LED of that dose ON + buzzer beeping.
                    Press OK = medicine taken (alarm stops).
                    No OK within 60 s -> buzzer stops, LED keeps
                    blinking and screen shows "MISSED" until OK.
    Hold OK while powering on -> button / LED / buzzer test mode.
    Menu with no button press for 30 s -> returns to clock screen.
  ============================================================
*/

#include <Wire.h>
#include <EEPROM.h>
#include <RTClib.h>
#include <LiquidCrystal.h>

// ---------------- pins ----------------
LiquidCrystal lcd(3, 4, 5, 6, 7, 8);   // RS, E, D4, D5, D6, D7
RTC_DS3231 rtc;

const uint8_t BTN_OK  = A0;
const uint8_t BTN_INC = A1;
const uint8_t BTN_SET = A2;
const uint8_t LED_PINS[3] = {10, 11, 12};
const uint8_t BUZZER = 9;

// ---------------- settings ----------------
const unsigned long RING_TIME_MS   = 60000UL;  // buzzer rings max 60 s
const unsigned long MENU_TIMEOUT_MS = 30000UL; // leave menu after 30 s idle
const char* GROUP_NAME[3] = {"One", "Two", "Three"};

// EEPROM layout
const int EE_TIME   = 11;   // 11..16 : hour/minute for dose 1..3
const int EE_ENABLE = 17;   // 17..19 : 1 = dose enabled
const int EE_MAGIC  = 20;   // marker so we know EEPROM was initialised
const uint8_t MAGIC = 0x5A;

// ---------------- state ----------------
uint8_t doseH[3], doseM[3];
bool    doseOn[3];
bool    missed[3] = {false, false, false};
long    lastFired[3] = {-1, -1, -1};           // stops repeat in same minute
bool    menuTimedOut = false;

// ============================================================
//  Small helpers
// ============================================================
void print2(int v) {                           // 7 -> "07"
  if (v < 10) lcd.print('0');
  lcd.print(v);
}

void clearLine(uint8_t row) {
  lcd.setCursor(0, row);
  lcd.print(F("                "));
  lcd.setCursor(0, row);
}

// true once per press (debounced, waits for release)
bool pressedOnce(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(25);
    if (digitalRead(pin) == LOW) {
      while (digitalRead(pin) == LOW) {}
      delay(25);
      return true;
    }
  }
  return false;
}

// true on press and repeats every ~300 ms while held (for fast scrolling)
bool pressedRepeat(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(25);
    if (digitalRead(pin) == LOW) {
      unsigned long t = millis();
      while (digitalRead(pin) == LOW && millis() - t < 300) {}
      return true;
    }
  }
  return false;
}

void beep(uint16_t ms) {
  digitalWrite(BUZZER, HIGH);
  delay(ms);
  digitalWrite(BUZZER, LOW);
}

// ============================================================
//  EEPROM
// ============================================================
void saveDoses() {
  for (uint8_t i = 0; i < 3; i++) {
    EEPROM.update(EE_TIME + i * 2,     doseH[i]);
    EEPROM.update(EE_TIME + i * 2 + 1, doseM[i]);
    EEPROM.update(EE_ENABLE + i,       doseOn[i] ? 1 : 0);
  }
  EEPROM.update(EE_MAGIC, MAGIC);
}

void loadDoses() {
  if (EEPROM.read(EE_MAGIC) != MAGIC) {        // first run -> defaults
    const uint8_t defH[3] = {8, 14, 20};
    for (uint8_t i = 0; i < 3; i++) {
      doseH[i] = defH[i];
      doseM[i] = 0;
      doseOn[i] = false;                        // off until the user sets them
    }
    saveDoses();
    return;
  }
  for (uint8_t i = 0; i < 3; i++) {
    doseH[i]  = EEPROM.read(EE_TIME + i * 2);
    doseM[i]  = EEPROM.read(EE_TIME + i * 2 + 1);
    doseOn[i] = EEPROM.read(EE_ENABLE + i) == 1;
    if (doseH[i] > 23) doseH[i] = 8;
    if (doseM[i] > 59) doseM[i] = 0;
  }
}

// ============================================================
//  Value editors (used by the menu)
// ============================================================
// Edit a number. INC = +1 (wraps), OK = confirm.
// Returns the new value, or -1 if nobody pressed anything for 30 s.
int editNumber(const __FlashStringHelper* title, uint8_t doseNo,
               int val, int minV, int maxV, uint8_t digits) {
  unsigned long lastAct = millis();
  unsigned long blinkT = millis();
  bool show = true;
  lcd.clear();
  lcd.print(title);
  if (doseNo) { lcd.print(' '); lcd.print(doseNo); }

  while (true) {
    if (pressedRepeat(BTN_INC)) {
      val = (val >= maxV) ? minV : val + 1;
      show = true; blinkT = millis(); lastAct = millis();
    }
    if (pressedOnce(BTN_OK)) { beep(40); return val; }
    if (millis() - lastAct > MENU_TIMEOUT_MS) return -1;
    if (millis() - blinkT > 400) { blinkT = millis(); show = !show; }

    lcd.setCursor(0, 1);
    lcd.print(F("  > "));
    if (show) {
      if (digits == 2) print2(val); else lcd.print(val);
    } else {
      lcd.print(digits == 2 ? F("  ") : F("    "));
    }
    lcd.print(F("  INC/OK"));
  }
}

// ON/OFF choice. Returns 1 = ON, 0 = OFF, -1 = timeout
int editOnOff(uint8_t doseNo, bool current) {
  unsigned long lastAct = millis();
  bool v = current;
  lcd.clear();
  lcd.print(F("Dose "));
  lcd.print(doseNo);
  lcd.print(F(" enabled?"));
  while (true) {
    lcd.setCursor(0, 1);
    lcd.print(v ? F("  > ON    INC/OK") : F("  > OFF   INC/OK"));
    if (pressedOnce(BTN_INC)) { v = !v; lastAct = millis(); }
    if (pressedOnce(BTN_OK))  { beep(40); return v ? 1 : 0; }
    if (millis() - lastAct > MENU_TIMEOUT_MS) return -1;
  }
}

// ============================================================
//  Menu actions
// ============================================================
void setDoseTimes() {
  for (uint8_t i = 0; i < 3; i++) {
    int on = editOnOff(i + 1, doseOn[i]);
    if (on < 0) { menuTimedOut = true; return; }
    doseOn[i] = on;
    if (on) {
      int h = editNumber(F("Dose hour  "), i + 1, doseH[i], 0, 23, 2);
      if (h < 0) { menuTimedOut = true; return; }
      int m = editNumber(F("Dose minute"), i + 1, doseM[i], 0, 59, 2);
      if (m < 0) { menuTimedOut = true; return; }
      doseH[i] = h;
      doseM[i] = m;
    }
    lastFired[i] = -1;
    missed[i] = false;
    digitalWrite(LED_PINS[i], LOW);
  }
  saveDoses();

  lcd.clear();
  lcd.print(F("Medicine times"));
  lcd.setCursor(0, 1);
  lcd.print(F("saved!"));
  Serial.println(F("Dose times saved:"));
  for (uint8_t i = 0; i < 3; i++) {
    Serial.print(F("  Dose ")); Serial.print(i + 1); Serial.print(F(": "));
    if (doseOn[i]) {
      Serial.print(doseH[i]); Serial.print(':');
      if (doseM[i] < 10) Serial.print('0');
      Serial.println(doseM[i]);
    } else Serial.println(F("OFF"));
  }
  delay(1500);
}

uint8_t daysInMonth(uint8_t m, uint16_t y) {
  const uint8_t d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return d[m - 1];
}

void setClock() {
  DateTime now = rtc.now();
  int h  = editNumber(F("Clock hour"),   0, now.hour(),   0, 23, 2);   if (h  < 0) { menuTimedOut = true; return; }
  int mi = editNumber(F("Clock minute"), 0, now.minute(), 0, 59, 2);   if (mi < 0) { menuTimedOut = true; return; }
  int d  = editNumber(F("Date day"),     0, now.day(),    1, 31, 2);   if (d  < 0) { menuTimedOut = true; return; }
  int mo = editNumber(F("Date month"),   0, now.month(),  1, 12, 2);   if (mo < 0) { menuTimedOut = true; return; }
  int y  = editNumber(F("Date year"),    0, constrain(now.year(), 2024, 2099), 2024, 2099, 4);
  if (y < 0) { menuTimedOut = true; return; }

  if (d > daysInMonth(mo, y)) d = daysInMonth(mo, y);
  rtc.adjust(DateTime(y, mo, d, h, mi, 0));
  for (uint8_t i = 0; i < 3; i++) lastFired[i] = -1;

  lcd.clear();
  lcd.print(F("Clock saved!"));
  Serial.println(F("Clock updated"));
  delay(1500);
}

void menu() {
  const char* items[3] = {"Dose times", "Set clock", "Exit"};
  uint8_t sel = 0;
  unsigned long lastAct = millis();
  menuTimedOut = false;
  beep(40);

  while (true) {
    lcd.setCursor(0, 0);
    lcd.print(F("MENU  INC=next  "));
    lcd.setCursor(0, 1);
    lcd.print(F("> "));
    lcd.print(items[sel]);
    lcd.print(F("            "));

    if (pressedOnce(BTN_INC)) { sel = (sel + 1) % 3; lastAct = millis(); }
    if (pressedOnce(BTN_OK)) {
      beep(40);
      if (sel == 0) setDoseTimes();
      else if (sel == 1) setClock();
      break;
    }
    if (millis() - lastAct > MENU_TIMEOUT_MS) break;
  }
  if (menuTimedOut) {
    lcd.clear();
    lcd.print(F("Not saved"));
    lcd.setCursor(0, 1);
    lcd.print(F("(timeout)"));
    delay(1200);
  }
  lcd.clear();
}

// ============================================================
//  Alarm
// ============================================================
void runAlarm(uint8_t i) {
  Serial.print(F("ALARM: Dose ")); Serial.println(i + 1);
  lcd.clear();
  lcd.print(F("Take Group "));
  lcd.print(GROUP_NAME[i]);
  lcd.setCursor(0, 1);
  lcd.print(F("Medicine  OK=Yes"));
  digitalWrite(LED_PINS[i], HIGH);

  unsigned long start = millis();
  bool taken = false;
  while (millis() - start < RING_TIME_MS) {
    digitalWrite(BUZZER, (millis() / 400) % 2);      // beep - beep - beep
    if (pressedOnce(BTN_OK)) { taken = true; break; }
  }
  digitalWrite(BUZZER, LOW);

  lcd.clear();
  if (taken) {
    digitalWrite(LED_PINS[i], LOW);
    missed[i] = false;
    lcd.print(F("Dose "));
    lcd.print(i + 1);
    lcd.print(F(" taken :)"));
    Serial.println(F("  -> taken"));
  } else {
    missed[i] = true;                               // LED keeps blinking in loop
    lcd.print(F("Dose "));
    lcd.print(i + 1);
    lcd.print(F(" MISSED!"));
    Serial.println(F("  -> missed"));
  }
  delay(1500);
  lcd.clear();
}

// ============================================================
//  Main screen
// ============================================================
// index of next enabled dose (0..2) or -1 if none
int nextDose(const DateTime& now) {
  int nowMin = now.hour() * 60 + now.minute();
  int best = -1, bestDiff = 24 * 60 + 1;
  for (uint8_t i = 0; i < 3; i++) {
    if (!doseOn[i]) continue;
    int diff = (doseH[i] * 60 + doseM[i]) - nowMin;
    if (diff <= 0) diff += 24 * 60;
    if (diff < bestDiff) { bestDiff = diff; best = i; }
  }
  return best;
}

void showMainScreen(const DateTime& now) {
  lcd.setCursor(0, 0);
  lcd.print(F("Time  "));
  print2(now.hour());   lcd.print(':');
  print2(now.minute()); lcd.print(':');
  print2(now.second());
  lcd.print(F("  "));

  lcd.setCursor(0, 1);
  int anyMissed = -1;
  for (uint8_t i = 0; i < 3; i++) if (missed[i]) { anyMissed = i; break; }

  if (anyMissed >= 0) {
    lcd.print(F("MISSED Dose "));
    lcd.print(anyMissed + 1);
    lcd.print(F(" OK"));
    return;
  }

  // alternate every 3 s: date  <->  next dose
  if ((now.second() / 3) % 2 == 0) {
    lcd.print(F("Date "));
    print2(now.day());   lcd.print('/');
    print2(now.month()); lcd.print('/');
    lcd.print(now.year());
  } else {
    int n = nextDose(now);
    if (n < 0) {
      lcd.print(F("No dose set-SET "));
    } else {
      lcd.print(F("Next D"));
      lcd.print(n + 1);
      lcd.print(F("  "));
      print2(doseH[n]); lcd.print(':'); print2(doseM[n]);
      lcd.print(F("   "));
    }
  }
}

// ============================================================
//  Hardware test (hold OK while powering on)
// ============================================================
void hardwareTest() {
  lcd.clear();
  lcd.print(F("HW TEST (reset)"));
  while (true) {
    bool s = digitalRead(BTN_SET) == LOW;
    bool n = digitalRead(BTN_INC) == LOW;
    bool o = digitalRead(BTN_OK)  == LOW;
    lcd.setCursor(0, 1);
    lcd.print(F("SET:")); lcd.print(s);
    lcd.print(F(" INC:")); lcd.print(n);
    lcd.print(F(" OK:")); lcd.print(o);
    digitalWrite(LED_PINS[0], s);
    digitalWrite(LED_PINS[1], n);
    digitalWrite(LED_PINS[2], o);
    digitalWrite(BUZZER, s || n || o);
    delay(50);
  }
}

// ============================================================
//  setup / loop
// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(BTN_OK,  INPUT_PULLUP);
  pinMode(BTN_INC, INPUT_PULLUP);
  pinMode(BTN_SET, INPUT_PULLUP);
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  lcd.begin(16, 2);
  Wire.begin();

  if (digitalRead(BTN_OK) == LOW) hardwareTest();

  if (!rtc.begin()) {
    lcd.print(F("RTC not found!"));
    lcd.setCursor(0, 1);
    lcd.print(F("Check SDA/SCL"));
    Serial.println(F("ERROR: DS3231 not found"));
    while (true) {}
  }
  if (rtc.lostPower()) {                             // new battery / first use
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  loadDoses();

  lcd.print(F("Medicin Reminder"));
  lcd.setCursor(0, 1);
  lcd.print(F(" Using Arduino  "));
  // short LED + buzzer self-check
  for (uint8_t i = 0; i < 3; i++) { digitalWrite(LED_PINS[i], HIGH); delay(250); digitalWrite(LED_PINS[i], LOW); }
  beep(100);
  delay(1200);
  lcd.clear();
  Serial.println(F("Medicine reminder ready. Press SET for menu."));
}

void loop() {
  if (pressedOnce(BTN_SET)) menu();

  // OK on main screen clears "missed" reminders
  if (pressedOnce(BTN_OK)) {
    for (uint8_t i = 0; i < 3; i++) {
      missed[i] = false;
      digitalWrite(LED_PINS[i], LOW);
    }
    lcd.clear();
  }

  DateTime now = rtc.now();

  // ---- check the 3 dose times ----
  long key = (long)now.day() * 1440L + now.hour() * 60L + now.minute();
  for (uint8_t i = 0; i < 3; i++) {
    if (doseOn[i] && now.hour() == doseH[i] && now.minute() == doseM[i] && lastFired[i] != key) {
      lastFired[i] = key;
      runAlarm(i);
      now = rtc.now();
    }
  }

  // ---- blink LEDs of missed doses ----
  bool blinkOn = (millis() / 500) % 2;
  for (uint8_t i = 0; i < 3; i++) {
    if (missed[i]) digitalWrite(LED_PINS[i], blinkOn);
  }

  showMainScreen(now);
  delay(100);
}
