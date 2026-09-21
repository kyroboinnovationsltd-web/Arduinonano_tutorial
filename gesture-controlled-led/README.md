[← All projects](../README.md)

# 01 · LED Control Using Arduino Nano

Control five LEDs by opening and closing your fingers. A webcam watches
your hand, Python works out which fingers are extended, and the Arduino
Nano switches the matching LED on.

| Finger | LED | Arduino pin |
|---|---|---|
| Thumb | LED 1 | D2 |
| Index | LED 2 | D3 |
| Middle | LED 3 | D4 |
| Ring | LED 4 | D5 |
| Pinky | LED 5 | D6 |

---

## Components

| Part | Quantity |
|---|---|
| Arduino Nano | 1 |
| LED 5 mm | 5 |
| Resistor 220 Ω | 5 |
| Breadboard | 1 |
| Jumper wire | 1 |
| USB data cable | 1 |
| Laptop with a webcam | 1 |

---

## Wiring

Build this once for each LED:

```
Arduino pin → 220 Ω resistor → LED long leg (anode)
                               LED short leg (cathode) → GND rail
```

Then run **one** wire from the breadboard's ground rail back to the Nano's
GND pin.

Use pins D2, D3, D4, D5 and D6 — one per LED, in that order.

> The long leg of an LED is positive. A backwards LED stays dark but is not
> damaged — just turn it round. Never skip the resistor.

---

## Setup

### 1. Upload the Arduino sketch

Open `gesture_led/gesture_led.ino` in the Arduino IDE.

- **Tools → Board →** Arduino Nano
- **Tools → Processor →** ATmega328P
  *(if upload fails, try "ATmega328P (Old Bootloader)" — most clones need this)*
- **Tools → Port →** your COM port

Click Upload. The LEDs flash once in sequence — that startup sweep checks
your wiring before any other software is involved. If one stays dark, fix
that LED now.

**Then close the Serial Monitor.** Only one program can use a serial port
at a time.

### 2. Install Python

Use **Python 3.11**. MediaPipe has no builds for 3.12 or 3.13 — on those
versions the install simply fails. Tick **"Add python.exe to PATH"** during
installation.

### 3. Set up and run

```bash
cd python_file
py -3.11 -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python gesture_led.py
```

A window opens showing your webcam. Hold your hand up about 50 cm away.
Press **q** to quit.

> If activation fails with *"running scripts is disabled on this system"*,
> run this once and try again:
> `Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned`

---

## How it works

```
webcam → OpenCV → MediaPipe → finger states → "10110" → USB serial → Arduino → LEDs
```

MediaPipe finds 21 landmark points on your hand. A finger counts as open
if its fingertip sits higher on screen than the joint below it. The thumb
bends sideways rather than curling, so it is compared left-to-right
instead.

The five results become a string like `10110`, sent over USB at 115200
baud. The Arduino reads one line at a time and switches each pin to match.

Two details keep it steady: Python waits for three identical frames before
accepting a change, so the LEDs don't flicker; and the Arduino blanks all
LEDs if it hears nothing for two seconds, so closing the script never
leaves one stuck on.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `No matching distribution found for mediapipe` | You're on Python 3.12+. Use 3.11. |
| `Access is denied` on the COM port | Close the Arduino Serial Monitor. |
| `Could not auto-detect the Arduino` | Set `PORT = "COM3"` at the top of `gesture_led.py`. |
| Camera window is black | Another app is using the webcam, or try `CAM_INDEX = 1`. |
| One LED never lights | Wiring. Re-upload and watch the startup sweep. |
| Detection is jumpy | Better lighting. Don't sit with a window behind you. |

---

## Files

```
01-led-control-using-arduino-nano/
├── gesture_led/
│   └── gesture_led.ino          Arduino sketch
├── python_file/
│   ├── gesture_led.py           the OpenCV + MediaPipe script
│   └── requirements.txt         Python dependencies
└── wiring_diagram/
    └── circuit_diagram.png      the circuit
```

---

[← Back to all projects](../README.md)
