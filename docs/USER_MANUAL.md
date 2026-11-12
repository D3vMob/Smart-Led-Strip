# Smart LED Strip — User Manual

_Firmware branch `SLS-1-led-strip-control` · Updated 2026-11-12_

The Smart LED Strip is a voice-controlled light. You control it from Alexa or Google Home over Matter. Each light is a **Unit**: a Seeed XIAO ESP32-C6 with its own LED strip and, if you want, an LD2410C presence sensor. You can pair up to 8 Units into one **Group**, and the Group then acts as a single light.

---

## 1. What's in a Unit

| Part | Details |
|---|---|
| Controller | Seeed XIAO ESP32-C6 |
| LED strip | WS2812B, 60 pixels by default, data on **D0** |
| Presence sensor (optional) | LD2410C over UART: sensor TX → **D7**, sensor RX → **D6** |
| Button | The XIAO's **BOOT** button. It is used for pairing and Motion master override. |
| Power budget | 2000 mA by default. Brightness is scaled down automatically so the strip never draws more. |

Every Unit runs the same firmware. At boot, a Unit checks for an LD2410C for about 1 second. If it finds one, it becomes a **Sensor unit**.

The LED count, pins and power budget are set at build time in `include/config.h`. Change them there before you flash if your hardware is different.

---

## 2. First-time setup

### 2.1 Flash the firmware

```sh
pio run -t upload
pio device monitor        # 115200 baud
```

### 2.2 (Optional) Pair several Units into a Group

Skip this step if you only have one Unit. A single Unit works as a Group of one.

1. Power on every Unit you want in the Group and place them within radio range of each other.
2. On each Unit, **hold BOOT for 2 seconds**. Do all of them within 30 seconds of each other. While pairing is open, the strip **breathes cyan**.
3. Pairing stays open for 30 seconds after the last long press. When it closes, each Unit prints `Group formed: id …, N member(s)` and restarts.

Notes:
- A Group holds at most **8 Units**. If more Units try to join, the ones that pressed BOOT last are left out.
- Every pairing starts a fresh Group. To add a Unit to an existing Group, long-press **all** the Units again, including the new one, in the same 30-second window.
- If a Unit prints `No Group secret saved: re-pair the Units`, pair the whole Group again.

### 2.3 Add the light to Alexa or Google Home (commissioning)

1. Open the serial monitor on **any one** Unit in the Group. A Unit that hasn't been commissioned prints a QR code URL and a manual pairing code every 5 seconds.
2. In Alexa or Google Home, add a **Matter** device. Scan the QR code (open the URL to see it) or type the manual code.
3. The phone connects over Bluetooth and hands the Unit your Wi-Fi details. No network name is stored in the firmware.

The Unit you commission becomes the **Coordinator**. It is the only Unit your app sees. It relays every command to the other Members over ESP-NOW, and the other Units go Matter-silent automatically. Commission only one Unit per Group.

> Tip: write the QR code and manual code on a sticker on the Unit. You'll need them again if you ever reset it.

---

## 3. Everyday use

### 3.1 What appears in your app

The Group shows up as one device with five controls:

| Control | Type | What it does | Say… |
|---|---|---|---|
| **Strip** | Color light | Opens and closes the light, dims it, and sets its color or white temperature | "turn on the Strip", "set the Strip to 40%", "set the Strip to blue", "set the Strip to warm white" |
| **Blink** | Switch | Turns blinking on or off | "turn on Blink" |
| **Blink speed** | Dimmable light | Sets the blink period: level % × 50 ms | "set Blink speed to 20%" |
| **Motion** | Switch | Turns presence automation on or off | "turn on Motion" |
| **Random color** | Switch, turns itself back off | Picks a new vivid color | "turn on Random color" |

You can rename these controls in your app, for example "Kitchen light".

### 3.2 Brightness

- You can dim between **10% and 100%**. If you ask for less than 10%, the Strip goes to 10% and the app shows 10%.
- Dimming to 0% closes the Strip. The next time it opens, it uses the brightness it had before.

### 3.3 Color and whites

- Named colors and hues work as in any smart bulb.
- Warm and cool whites (color temperature) work too. The RGB strip mixes an approximation of each white.
- **Random color** picks a fully saturated hue that is at least about 30° away from the current hue, so the change is always visible. Brightness stays the same. If the Strip is closed, Random color opens it.

### 3.4 Blink

- Blinking turns the current color on and off. The period is one full cycle, so 1000 ms means 500 ms on and 500 ms off.
- Blink speed % × 50 ms gives the period:

  | Blink speed | Period |
  |---|---|
  | 1% | 50 ms (fastest) |
  | **20% (default)** | **1000 ms** |
  | 50% | 2500 ms |
  | 100% | 5000 ms (slowest) |

- A higher percentage blinks **slower**.
- Blink is remembered. If you close the Strip while it is blinking, it blinks again when you reopen it.
- All Units in a Group blink in step with each other.

### 3.5 What opens the Strip, and what doesn't

| Command while the Strip is Closed | Result |
|---|---|
| Turn on Strip | Opens with the last Look |
| Dim, color, white, Random color | Opens with the new setting |
| Blink on/off, Blink speed | Saved for next time. The Strip **stays closed**. |
| Turn on Motion | Opens the Strip |

The **Look** is the color, brightness, Blink on/off and Blink speed. Opening the Strip always shows the latest Look.

### 3.6 After a power cut

The Unit saves the Look, Open/Closed and Motion settings about 3 seconds after your last change. At power-up the Strip comes back the way you left it. A brand-new Unit starts Closed, in neutral white at 100%, with Blink speed at 20% and Motion off.

---

## 4. Motion (presence sensing)

Motion needs at least one Sensor unit in the Group (a Unit with an LD2410C).

### 4.1 How it behaves

- **Turn on Motion**: the Strip opens right away. From then on, presence opens it and **60 seconds** without presence closes it.
- **Turn off Motion**: the Strip stays as it is. Only the automatic opening and closing stops.
- **Closing the Strip while someone is in the room**: the Strip stays dark until the room has been empty for 60 seconds. After that, the next person to walk in opens it again.
- **Dimming or changing color during that hold**: the Strip opens and the hold is cancelled.
- **No sensor in the Group**: turning on Motion just opens the Strip, and nothing closes it automatically.

### 4.2 Several Sensor units: the Motion master

Only one Sensor unit drives the Group at a time. This is the **Motion master**.

- **Automatic choice (Calibration).** After commissioning, the Coordinator counts which Sensor unit notices people first. After 10 detections, that Unit becomes the Motion master. Until then, the Sensor unit with the lowest MAC address is used.
- **Choosing it yourself (Override).** Double-press BOOT on the Sensor unit you want. The strip **flashes white twice** to confirm. Your choice is saved and stays in place until the next pairing.
- **Failover.** If the Motion master is silent for more than 10 seconds, the next-best Sensor unit takes over automatically.

---

## 5. When something goes offline

| Situation | What happens |
|---|---|
| A Member loses power | The rest of the Group keeps working. When the Member comes back, it rejoins and catches up within about 2 seconds. |
| The Motion master goes offline | Another Sensor unit takes over after 10 seconds. |
| The Coordinator goes offline | Voice control is unavailable. The other Members keep their last state and **keep following motion on their own**. When the Coordinator comes back, everything syncs again. |
| Your router changes Wi-Fi channel | Members search for the Coordinator on the new channel (a short scan every 30 seconds) and move there on their own. |

---

## 6. Resetting a Unit or leaving a Group

There is no reset button yet (see [Upcoming features](#8-upcoming-features)). To remove a Unit from its Group or from Alexa/Google, or to reset it completely, erase it over USB and reflash:

```sh
pio run -t erase
pio run -t upload
```

This deletes the Group membership, the Matter pairing and the saved Look. The Unit then starts as a fresh Group of one and prints a new pairing code. Also remove the old device from your app.

---

## 7. Troubleshooting

| Problem | Try this |
|---|---|
| The app doesn't find the device | Make sure you are scanning the **Coordinator's** code (the one printing a QR URL) and that Bluetooth is on on your phone. |
| A Unit doesn't follow the Group | Check that its serial log says `Member: Matter-silent, following the Coordinator`. If it doesn't, pair all the Units again. |
| "No Group secret saved: re-pair the Units" | Pair the whole Group again (section 2.2). |
| The strip looks dimmer than expected at full white | The power budget is limiting it. Raise `MAX_MILLIAMPS` only if your power supply can deliver more. |
| Motion never turns the light on | Check that the log says `LD2410C detected: Sensor unit` and that **Motion** is on in the app. |
| A double-press doesn't flash | The double-press only works on a Sensor unit that belongs to a commissioned Group. |
| Serial log messages | `Presence onset …`, `Presence ended`, `Motion master: <mac>` and `Override claimed` show what motion is doing. |

---

## 8. Upcoming features

These features are planned but **not in the current firmware**.

### 8.1 Authenticated pairing with a per-Group key (next up)

- **What changes:** during the 30-second pairing window, Units agree a random Group key with an X25519 key exchange. Anyone recording the radio traffic can no longer work the key out. Every sync message, and the ESP-NOW encryption itself, will use keys derived from it. The shared built-in radio key is removed.
- **What you'll notice:** pairing works the same way. If a Unit fails to get the Group key, or sees two Units claiming the same address, it **flashes red** and stays on its own. Pair again when that happens.
- **Upgrade note:** Groups paired on today's firmware must be **paired again** after the update. Until then, each Unit runs on its own and logs `Group key missing: re-pair`.

### 8.2 Factory reset / leave-Group button gesture

A long BOOT hold (about 10 seconds) will wipe the Unit's saved settings, Group membership and Matter pairing, with no computer needed. For now, use the USB erase in section 6.

### 8.3 Over-the-air (OTA) firmware updates

You will be able to update the firmware over Wi-Fi instead of USB. This needs partition and security work first.

### 8.4 Flash / storage encryption

The Group key will be encrypted in the Unit's flash, so someone with physical access can't read it. This will change the USB reflash and erase workflow, so it is scheduled after the features above.

---

## Glossary

| Term | Meaning |
|---|---|
| **Unit** | One controller with its strip, and an optional sensor |
| **Group** | 1–8 Units acting as one light |
| **Member** | Any Unit in a Group |
| **Coordinator** | The one Member added to Alexa/Google. It relays commands to the others. |
| **Sensor unit** | A Member with an LD2410C detected |
| **Motion master** | The Sensor unit whose readings drive the Group |
| **Look** | Color, brightness, Blink on/off and Blink speed |
| **Open / Closed** | Whether the Strip is lit |
| **Pairing window** | The 30 seconds after a BOOT long press, when Units can join a Group |
| **Calibration** | The automatic ranking of Sensor units after commissioning |
| **Override** | Choosing the Motion master yourself with a BOOT double-press |
