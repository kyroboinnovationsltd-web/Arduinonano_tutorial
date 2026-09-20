"""
gesture_led.py  —  Control 5 LEDs on an Arduino Nano with your fingers.

Thumb -> D2   Index -> D3   Middle -> D4   Ring -> D5   Pinky -> D6

Setup:
    py -3.11 -m venv .venv
    .venv\\Scripts\\activate
    pip install opencv-python mediapipe pyserial "numpy<2"

Run:
    python gesture_led.py        (press q to quit)
"""

import sys
import time

import cv2
import mediapipe as mp
import serial
import serial.tools.list_ports

# ---------------------------------------------------------------- CONFIG ---
PORT = None            # e.g. "COM3". None = auto-detect.
BAUD = 115200
CAM_INDEX = 0
DEBOUNCE_FRAMES = 3
MIRROR = True

FINGER_NAMES = ["Thumb", "Index", "Middle", "Ring", "Pinky"]
PIN_NAMES = ["D2", "D3", "D4", "D5", "D6"]

# One colour per finger, BGR. Warm to cool across the hand.
LED_COLORS = [
    (72, 104, 240),    # red-orange
    (60, 168, 245),    # amber
    (90, 210, 160),    # green
    (200, 170, 70),    # teal
    (210, 120, 150),   # violet
]

INK = (236, 233, 228)
INK_DIM = (150, 148, 143)
PANEL = (26, 24, 22)
OK = (120, 210, 130)
BAD = (85, 85, 235)

TIP_IDS = [4, 8, 12, 16, 20]
PIP_IDS = [3, 6, 10, 14, 18]


# ---------------------------------------------------------------- SERIAL ---
def find_arduino_port():
    keywords = ("arduino", "ch340", "ch910", "usb-serial", "wch", "ftdi", "cp210")
    for p in serial.tools.list_ports.comports():
        blob = f"{p.description} {p.manufacturer}".lower()
        if any(k in blob for k in keywords):
            return p.device
    return None


def open_serial():
    port = PORT or find_arduino_port()
    if port is None:
        print("Could not auto-detect the Arduino. Available ports:")
        for p in serial.tools.list_ports.comports():
            print("   ", p.device, "-", p.description)
        print('Set PORT = "COMx" at the top of this file.')
        sys.exit(1)
    ser = serial.Serial(port, BAUD, timeout=1)
    time.sleep(2.0)
    ser.reset_input_buffer()
    print(f"Connected to {port} @ {BAUD}")
    return ser


# ------------------------------------------------------------- DETECTION ---
def fingers_up(landmarks, handedness_label):
    """Return 5 ints, 1 = open. Thumb bends sideways so it uses x."""
    lm = landmarks.landmark
    if handedness_label == "Right":
        thumb = lm[TIP_IDS[0]].x < lm[PIP_IDS[0]].x
    else:
        thumb = lm[TIP_IDS[0]].x > lm[PIP_IDS[0]].x
    state = [1 if thumb else 0]
    for tip, pip in zip(TIP_IDS[1:], PIP_IDS[1:]):
        state.append(1 if lm[tip].y < lm[pip].y else 0)
    return state


# ------------------------------------------------------------------- HUD ---
def filled_rounded(img, x1, y1, x2, y2, r, color):
    cv2.rectangle(img, (x1 + r, y1), (x2 - r, y2), color, -1)
    cv2.rectangle(img, (x1, y1 + r), (x2, y2 - r), color, -1)
    for cx, cy in ((x1 + r, y1 + r), (x2 - r, y1 + r),
                   (x1 + r, y2 - r), (x2 - r, y2 - r)):
        cv2.circle(img, (cx, cy), r, color, -1)


def glass(frame, x1, y1, x2, y2, r=18, color=PANEL, alpha=0.78):
    """Translucent rounded panel drawn straight onto the frame."""
    overlay = frame.copy()
    filled_rounded(overlay, x1, y1, x2, y2, r, color)
    cv2.addWeighted(overlay, alpha, frame, 1 - alpha, 0, frame)


def text(frame, s, org, scale=0.6, color=INK, weight=1, font=cv2.FONT_HERSHEY_SIMPLEX):
    cv2.putText(frame, s, org, font, scale, color, weight, cv2.LINE_AA)


def text_w(s, scale=0.6, weight=1, font=cv2.FONT_HERSHEY_SIMPLEX):
    return cv2.getTextSize(s, font, scale, weight)[0][0]


def draw_led(frame, cx, cy, on, color, pin, name):
    """A single LED indicator with a soft glow when lit."""
    if on:
        glow = frame.copy()
        for rad, a in ((34, 0.16), (27, 0.22)):
            cv2.circle(glow, (cx, cy), rad, color, -1)
            cv2.addWeighted(glow, a, frame, 1 - a, 0, frame)
            glow = frame.copy()
        cv2.circle(frame, (cx, cy), 20, color, -1, cv2.LINE_AA)
        cv2.circle(frame, (cx, cy), 20, INK, 1, cv2.LINE_AA)
        cv2.circle(frame, (cx - 6, cy - 7), 5, (255, 255, 255), -1, cv2.LINE_AA)
        label_col = INK
    else:
        cv2.circle(frame, (cx, cy), 20, (58, 55, 52), -1, cv2.LINE_AA)
        cv2.circle(frame, (cx, cy), 20, (84, 80, 76), 1, cv2.LINE_AA)
        label_col = INK_DIM

    text(frame, pin, (cx - text_w(pin, 0.42) // 2, cy + 45), 0.42, label_col)
    text(frame, name, (cx - text_w(name, 0.46) // 2, cy + 66), 0.46, label_col)


def draw_hud(frame, state, connected, hand_seen):
    h, w = frame.shape[:2]

    # ---- bottom LED strip --------------------------------------------
    strip_top = h - 148
    glass(frame, 16, strip_top, w - 16, h - 16, 18)

    n = len(state)
    span = w - 32 - 80
    step = span / (n - 1)
    cy = strip_top + 52
    for i, on in enumerate(state):
        cx = int(56 + i * step)
        draw_led(frame, cx, cy, on, LED_COLORS[i], PIN_NAMES[i], FINGER_NAMES[i])

    # serial status
    dot = OK if connected else BAD
    cv2.circle(frame, (40, h - 34), 5, dot, -1, cv2.LINE_AA)
    text(frame, "serial" if connected else "no serial", (54, h - 30), 0.42, INK_DIM)

    # ---- hint --------------------------------------------------------
    if not hand_seen:
        msg = "Show your hand to the camera"
        glass(frame, w // 2 - 190, h // 2 - 26, w // 2 + 190, h // 2 + 22, 14, PANEL, 0.6)
        text(frame, msg, (w // 2 - text_w(msg, 0.6, 1) // 2, h // 2 + 8), 0.6, INK)


# ------------------------------------------------------------------ MAIN ---
def main():
    ser = open_serial()

    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_DSHOW)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 960)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    if not cap.isOpened():
        print("Cannot open the webcam. Try a different CAM_INDEX.")
        sys.exit(1)

    mp_hands = mp.solutions.hands
    mp_draw = mp.solutions.drawing_utils
    hands = mp_hands.Hands(model_complexity=0, max_num_hands=1,
                           min_detection_confidence=0.7,
                           min_tracking_confidence=0.6)

    bone = mp_draw.DrawingSpec(color=(190, 190, 190), thickness=1, circle_radius=1)
    joint = mp_draw.DrawingSpec(color=(110, 220, 255), thickness=1, circle_radius=3)

    last_sent, candidate, stable = None, None, 0
    state = [0] * 5
    last_beat = time.time()

    win = "Gesture LED Control"
    cv2.namedWindow(win, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(win, 960, 720)
    print("Running. Press q to quit.")

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue
            if MIRROR:
                frame = cv2.flip(frame, 1)

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            rgb.flags.writeable = False
            result = hands.process(rgb)

            hand_seen = bool(result.multi_hand_landmarks)
            if hand_seen:
                hand = result.multi_hand_landmarks[0]
                label = result.multi_handedness[0].classification[0].label
                state = fingers_up(hand, label)
                mp_draw.draw_landmarks(frame, hand, mp_hands.HAND_CONNECTIONS,
                                       joint, bone)
            else:
                state = [0] * 5

            pattern = "".join(str(b) for b in state)

            if pattern == candidate:
                stable += 1
            else:
                candidate, stable = pattern, 1

            now = time.time()
            if (stable == DEBOUNCE_FRAMES and pattern != last_sent) or now - last_beat > 1.0:
                try:
                    ser.write((candidate + "\n").encode())
                    last_sent, last_beat = candidate, now
                except serial.SerialException as e:
                    print("Serial write failed:", e)
                    break

            draw_hud(frame, state, ser.is_open, hand_seen)
            cv2.imshow(win, frame)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    finally:
        try:
            ser.write(b"00000\n")
            time.sleep(0.1)
            ser.close()
        except Exception:
            pass
        hands.close()
        cap.release()
        cv2.destroyAllWindows()
        print("Closed cleanly.")


if __name__ == "__main__":
    main()