# Smart LED Strip

PlatformIO firmware for a Seeed XIAO ESP32-C6 driving a NeoPixel strip, controlled by voice through Matter over Wi-Fi (Alexa, Google Home).

See the [User Manual](docs/USER_MANUAL.md) for setup, grouping, motion, troubleshooting and upcoming features.

## Voice control

Each Unit is a Matter device with five fixed endpoints:

| Endpoint | Type | Voice examples |
|---|---|---|
| Strip | Extended color light | "turn on the Strip", "set the Strip to 40%", "set the Strip to blue", "set the Strip to warm white" |
| Blink | On/off | "turn on Blink", "turn off Blink" |
| Blink speed | Dimmable light (level % × 50 ms is the blink period) | "set Blink speed to 20%" |
| Motion | On/off | "turn on Motion", "turn off Motion" |
| Random color | On/off that turns itself back off | "turn on Random color" |

The app always shows the real state: a dim below 10% reads back as 10%, and changes made by motion or restored on boot show up too.

## Adding a Unit (commissioning)

1. Flash the firmware: `pio run -t upload`.
2. Open the serial monitor: `pio device monitor`. A Unit that isn't commissioned prints a QR code URL and a manual pairing code every 5 seconds until it is added.
3. In Alexa or Google Home, add a Matter device and scan the QR code (open the URL to display it) or type the manual code. Commissioning runs over Bluetooth, and the app hands the Unit your Wi-Fi credentials; no network name is built into the firmware.

## Wiping a Unit (leaving a fabric)

There is no reset button gesture. To remove a Unit from its Matter fabric, or to reset it completely, erase the whole flash and reflash:

```sh
pio run -t erase
pio run -t upload
```

This also clears the saved Look and Open/Closed state. The Unit then boots uncommissioned and prints a new pairing code.

## Tests

Pure logic lives in `lib/` and runs on the host: `pio test -e native`.
