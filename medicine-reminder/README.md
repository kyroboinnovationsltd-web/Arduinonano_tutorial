Medical Dispenser

Set three medicine times in a day. When a dose is due, its LED turns on and the buzzer beeps until someone presses OK to say the medicine was taken. Ignore it for a minute and the LED keeps blinking with MISSED on the screen, so a forgotten dose stays visible.

Dose	LED	Arduino pin
Dose 1	Red	D10
Dose 2	Yellow	D11
Dose 3	Green	D12

The times are kept in EEPROM and the clock runs on a DS3231 with its own battery, so nothing is lost when the power goes off.

Difficulty: intermediate · Build time: about 1–2 hours · Cost: low

Components
Part	Quantity
Arduino Nano	1
DS3231 RTC module (ZS-042) + coin cell	1
16x2 LCD (HD44780 / JHD162A)	1
Potentiometer 10 kΩ	1
Push button (tactile)	3
Resistor 1 kΩ	3
LED 5 mm (red, yellow, green)	3
Resistor 220 Ω	3
Active buzzer 5 V	1
Breadboard or perfboard	1
Jumper wires	~30
USB data cable	1
Wiring

LCD — 4-bit mode, so only four data lines:

RS → D3        E  → D4
D4 → D5        D5 → D6        D6 → D7        D7 → D8
VSS → GND      VDD → 5V       RW → GND
VO  → 10 kΩ pot middle leg   (pot ends → 5V and GND)
A   → 5V       K   → GND     (backlight)
D0–D3 → not connected

RTC (DS3231):

SDA → A4       SCL → A5       VCC → 5V       GND → GND

Buttons — build this once for each button:

Arduino pin → 1 kΩ resistor → button → GND

SET → A2       INC → A1       OK → A0

LEDs — build this once for each LED:

Arduino pin → 220 Ω resistor → LED long leg (anode)
                               LED short leg (cathode) → GND

Buzzer:

D9 → buzzer + (longer leg)
     buzzer − → GND

The full circuit diagram is in wiring_diagram/circuit_diagram.png.

Two Nano traps worth showing students. A6 and A7 are analog-input only. They have no digital output driver, so a buzzer on A6 stays silent and digitalRead(A6) doesn't work. A0–A5 work as normal digital pins, which is why the buttons can sit on A0–A2.

About the button resistors. The sketch turns on the Nano's internal pull-ups (INPUT_PULLUP), so a released button reads HIGH and a pressed one reads LOW. A 1 kΩ in series is fine. A 10 kΩ in series is not: together with the ~35 kΩ internal pull-up, it leaves the pin around 1.1–1.7 V when pressed, right on the edge of what counts as LOW, so presses get missed.

CR2032 in a ZS-042 module. The board has a charging circuit meant for a rechargeable LIR2032. With a normal CR2032, remove the charging diode/resistor near the header.

Setup
Install the library RTClib by Adafruit — Sketch → Include Library → Manage Libraries…. When it asks about dependencies, click Install All so Adafruit BusIO comes too.
Open Medicine_Dispenser/Medicine_Dispenser.ino in the Arduino IDE.
Tools → Board → Arduino Nano
Tools → Processor → ATmega328P (if upload fails, try "ATmega328P (Old Bootloader)" — most clones need this)
Tools → Port → your COM port

Click Upload. On startup the three LEDs flash in turn and the buzzer gives one short beep — if you see and hear that, the outputs are wired correctly.

On the very first run the clock is set from your computer's time, and all three doses start OFF.

How to use it

Main screen:

Time  09:58:12
Next D1  11:37      ← switches with the date every 3 s

Set medicine times — press SET, choose Dose times, press OK:

Dose 1 enabled? → INC switches ON/OFF → OK
Dose hour 1 → INC to change the hour → OK
Dose minute 1 → INC to change the minute → OK
Repeat for doses 2 and 3

Tap INC once per step — holding it scrolls fast and is easy to overshoot. Times are 24-hour: 08:00 is 8 AM, 14:30 is 2:30 PM, 00:01 is 12:01 AM.

Set the clock — SET → INC to Set clock → OK, then hour, minute, day, month, year.

When a dose is due:

The LED for that dose turns on and the buzzer beeps.
Press OK → "Dose taken", everything turns off.
No OK within 60 seconds → the buzzer stops, the LED keeps blinking and the screen shows MISSED Dose 1 until OK is pressed.

Hold OK while powering on for a test mode where each button lights an LED and sounds the buzzer.

How it works

Firing once per minute. The alarm check compares the RTC's hour and minute with each dose time. The loop runs about ten times a second, so without a guard the alarm would go off again and again for the whole minute. The sketch builds a key from the current day and minute and remembers it:

cpp
long key = (long)now.day() * 1440L + now.hour() * 60L + now.minute();
if (doseOn[i] && now.hour() == doseH[i] && now.minute() == doseM[i]
    && lastFired[i] != key) {
    lastFired[i] = key;
    runAlarm(i);
}

Each dose can only fire once for any given minute — a good example of edge vs. level: react to the moment the time matches, not to the whole time it keeps matching.

Why the RTC and not millis(). millis() resets to zero every power cycle and drifts by minutes per day. The DS3231 is temperature-compensated, keeps time on its coin cell, and talks to the Nano over I2C (A4/A5).

EEPROM with a marker byte. A blank EEPROM reads 255 everywhere, which would look like "hour 255". The sketch writes a marker byte (0x5A) after saving. On boot, if the marker is missing it loads safe defaults instead of garbage. EEPROM.update() is used instead of write() so a cell is only rewritten when the value actually changes — EEPROM cells wear out after about 100,000 writes.

The menu can't block an alarm forever. Every editing screen gives up after 30 seconds with no button press and returns to the clock, so a half-finished menu doesn't sit there through a dose time.

Troubleshooting
Problem	Cause and fix
Row of black boxes, no text	Turn the 10 kΩ contrast pot. Then check RS → D3 and E → D4.
RTClib.h: No such file or directory	Install RTClib by Adafruit and Adafruit BusIO.
RTC not found! on the screen	Check SDA → A4, SCL → A5, and 5V/GND to the module.
Buttons don't respond	Use diagonally opposite legs of the 4-pin button, and make sure the button goes to GND.
A button acts as if always pressed	The button is rotated 90° — the legs on one side are joined inside. Turn it.
Alarm never rings	Check the dose is ON — the screen should show Next D1 hh:mm. Set the time a few minutes ahead of the LCD clock, not your phone.
Shows No dose set-SET	All doses are OFF. SET → Dose times → INC to ON.
Time resets after power-off	The RTC coin cell is flat or missing.
LED works but no sound	Check buzzer polarity — longer leg to D9. A passive buzzer needs tone() instead.
Ideas to extend this
Actually dispense. Add a servo and a 3-compartment pill wheel that rotates to the right slot when each dose is due.
Log doses. Store the time each dose was confirmed and show the last few in a menu.
Snooze. Make INC during an alarm mean "remind me in 10 minutes".
Phone alerts. Swap the Nano for an ESP32 and send a notification when a dose is missed.
Different days. Add a weekday mask so a dose can be skipped on some days.
Files
medical-dispenser/
├── README.md
├── Medicine_Dispenser/
│   └── Medicine_Dispenser.ino
└── wiring_diagram/
    └── circuit_diagram.png
