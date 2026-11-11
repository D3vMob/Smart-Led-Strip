# Smart LED Strip

PlatformIO firmware (Arduino framework) for a Seeed XIAO ESP32-C6 driving a NeoPixel strip via Adafruit NeoPixel. Pure logic lives in `lib/` and is tested on the host under Unity (`native` env). Hardware adapters live in `src/`.

## Agent commands

| Key | Command |
|---|---|
| `test-one` | `~/.platformio/penv/bin/pio test -e native -f {name}` |
| `test-all` | `~/.platformio/penv/bin/pio test -e native` |
| `lint` | `~/.platformio/penv/bin/pio check -e seeed_xiao_esp32c6` |
| `run` | `~/.platformio/penv/bin/pio run -e seeed_xiao_esp32c6 -t upload` |
| `deps` | `.pio/` |

`{name}` is a folder under `test/`. `pio` is the standard PlatformIO Core install path.
