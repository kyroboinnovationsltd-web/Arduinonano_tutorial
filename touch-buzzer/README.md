[← All projects](../README.md)

# 03 · Touch Buzzer

Tap the touch sensor and the buzzer plays a tune — a different one
depending on how many times you tapped.

| Taps | Tune |
|---|---|
| 1 | Twinkle Twinkle Little Star |
| 2 | Ode to Joy |
| 3 | Mary Had a Little Lamb |
| 4 | Jingle Bells |

All four are traditional or public-domain melodies, so the repo is free to
publish and teach from.

Tap more than four times and it treats it as four.

**Difficulty:** intermediate · **Build time:** about 30 minutes ·
**Cost:** low

---

## Components

| Part | Quantity |
|---|---|
| Arduino Nano | 1 |
| TTP223 touch sensor module | 1 |
| Passive buzzer | 1 |
| Resistor 220 Ω | 1 |
| Breadboard | 1 |
| Jumper wires | 5 |
| USB data cable | 1 |

---

## Wiring

**Touch sensor** — three wires, no resistors:

```
VCC → 5V
GND → GND
SIG → D2
```

The TTP223 drives its output pin itself, so nothing else is needed. Some
modules label the signal pin `I/O`, `OUT` or `SIG` — they all mean the
same thing.

**Buzzer:**

```
D8 → 220 Ω resistor → buzzer + (longer leg)
                      buzzer − → GND
```

Use the **passive** buzzer. The active one has a fixed pitch, so all four
responses would sound identical.

> **The sensor works through plastic.** A TTP223 will read a touch through
> a few millimetres of acrylic, tape or a phone case. Useful if you want to
> mount it behind a panel — and worth demonstrating to students, because it
> makes the point that this is capacitive sensing, not a button.

---

## Setup

Open `arduino/touch_buzzer/touch_buzzer.ino` in the Arduino IDE.

- **Tools → Board →** Arduino Nano
- **Tools → Processor →** ATmega328P
  *(if upload fails, try "ATmega328P (Old Bootloader)" — most clones need this)*
- **Tools → Port →** your COM port

Click Upload. On startup the sketch plays a short rising scale. If you
hear it, the buzzer is wired correctly — check that before blaming the
sensor.

Open the Serial Monitor at **115200 baud**. It prints each tap as it
registers and then the response it chose, which makes the whole thing much
easier to debug:

```
tap 1
tap 2
-> playing tune 2
```

---

## How it works

The interesting problem here is that **the Arduino cannot know what a tap
means at the moment it happens.** Is that first tap a single tap, or the
first of three? There is no way to tell without waiting.

So the sketch waits. Every tap restarts a timer:

```cpp
if (tapCount > 0 && now - lastTapTime > TAP_WINDOW) {
    playResponse(tapCount);
    tapCount = 0;
}
```

When `TAP_WINDOW` milliseconds pass with no new tap, whatever the count
reached is the answer. That's the same logic behind double-clicking a
mouse, and behind the old multi-tap phone keypads where pressing `2` three
times gave you a C.

`TAP_WINDOW` is set to 450 ms. It's the one number worth playing with:

- **Too short** (200 ms) and fast tapping is needed; students will get
  single beeps when they meant three taps.
- **Too long** (1000 ms) and every response feels laggy.

**Edge detection** is the other piece. The loop runs thousands of times a
second, and a finger stays on the pad for maybe 200 ms — so a naive
`if (digitalRead(pin) == HIGH)` would count hundreds of taps per touch.
Instead the sketch only counts the *transition* from untouched to touched:

```cpp
if (state == HIGH && lastState == LOW && ...)
```

That's why `lastState` exists. It is the difference between counting
touches and counting moments.

---

## Troubleshooting

| Problem | Cause and fix |
|---|---|
| Counts far too many taps | The edge-detection check has been altered. It must compare against `lastState`, not just read the pin. |
| Two taps register as one | You are tapping too fast for `DEBOUNCE`. Lower it to 30 ms. |
| Three taps register as one | Tapping too slowly — raise `TAP_WINDOW`, or tap faster. |
| All four tunes sound the same | You are using the active buzzer. Swap it for the passive one. |
| Sensor never responds | Check VCC is on **5V**, not 3V3. Confirm the signal wire is on D2. |
| Sensor stays HIGH constantly | Some TTP223 boards ship in toggle mode, set by solder pads on the back. Look for pads marked A or B. |
| No sound at all | Check buzzer polarity — longer leg toward the resistor. Listen for the startup sequence. |

---

## Ideas to extend this

- **Write your own tunes.** Each tune is two arrays near the top of the
  sketch — the notes and how long to hold each one. Add a pair, point a
  case at it, and you have a new song. This is the single best exercise
  in the project.
- **Change the tempo.** `BEAT` at the top controls every tune at once.
- **Add an LED** that blinks once per tap as you count, so there is visual
  feedback during the waiting window.
- **Make it a secret knock.** Store a target pattern and only play the
  fanfare if the tap count matches — otherwise a low error buzz.
- **Add a fifth response** for long-press: if the pad is held for over a
  second, treat it as a separate gesture.
- **Combine with project 02** so buttons and touch both feed the same
  buzzer.

---

## Files

```
03-touch-buzzer/
├── README.md
├── arduino/
│   └── touch_buzzer/
│       └── touch_buzzer.ino
└── docs/
    ├── circuit_diagram.png
    └── circuit_diagram.svg
```

---

[← Back to all projects](../README.md)
