# Smart LED Strip

PlatformIO firmware for a Seeed XIAO ESP32-C6 driving a NeoPixel strip. Pure logic lives in `lib/` and is tested on the host under Unity (`native` env). Hardware adapters live in `src/`.

## Agent commands

| Key | Command |
|---|---|
| `test-one` | `~/.platformio/penv/bin/pio test -e native -f {name}` |
| `test-all` | `~/.platformio/penv/bin/pio test -e native` |
| `deps` | `.pio/` |
