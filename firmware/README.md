# Mini Lolly firmware

Adafruit Feather ESP32-S2 firmware for the 63-LED spiral board. **Animations are native C++
effects** compiled into the firmware. The website (`../simulator/index.html`) is the control
surface: it picks an effect, tunes its parameters, keeps an ordered preset list and sets
brightness, all live over the device's Wi-Fi AP. It is served from the firmware image itself,
so joining the AP is all a phone needs.

Adding a new *effect type* means a reflash; everything you actually change day to day — which
effect, its colours and speed, the presets, brightness — is live and reflash-free.

## Effects vs presets

An **effect** is a compiled-in render function plus a declaration of the knobs it reads,
published as `EFFECT_SCHEMA_JSON`. There are two kinds, and they share one id space:

- **Pixel effects** (ids 0–11) return a colour for one LED from geometry and time, with no
  state between frames. The simulator mirrors these exactly.
- **Frame effects** (ids 12–19) are the FastLED animations this board has always had. They own
  the whole strip and carry their own state — heat maps, fade trails, palettes — so the
  simulator can only show a stand-in.

A **preset** is a saved configuration — `{name, effect id, params, brightness, flags}` — so one
effect can back any number of presets. Presets live in NVS, up to `MAX_PRESETS` (32), and are
created, edited and deleted entirely from the website.

`PRESET_IN_CYCLE` in `flags` decides whether the hardware buttons can reach a preset. Presets
without it are website-only — `nextPreset()` skips them. If *nothing* is flagged the buttons
fall back to walking the whole list, so they can never become a dead key. The built-in
`Test red/green/blue` swatches ship off the cycle.

## Layout
```
platformio.ini            Adafruit Feather ESP32-S2 env
include/Config.h          pins (DIN 12, buttons 0 and 5), NUM_LEDS 63, brightness cap, SSID
include/mapping.h         GENERATED per-LED x/y/r/theta — do not edit
include/spa_gz.h          GENERATED gzipped website (embedded in the image)
src/fixmath.*             Q16.16 fixed-point + sine LUT
src/effects.*             12 pixel effects + the schema JSON
src/animations/*          the 8 FastLED frame effects and their dispatch table
src/PresetStore.*         presets in NVS (versioned blob)
src/WebServer.*           REST API + the gzipped page from flash
src/main.cpp              render loop, buttons, preset cycling
scripts/prepare_data.sh   pack ../simulator → include/spa_gz.h
test/run.sh               off-device checks + a localhost twin of the device (test/README.md)
```

## Effects
| id | name | kind | params |
|----|------|------|--------|
| 0  | Solid           | pixel | colorA |
| 1  | Gradient        | pixel | colorA, colorB, axis, speed |
| 2  | Wave            | pixel | colorA, colorB, axis, scale, speed |
| 3  | Sparkle         | pixel | colorA, speed |
| 4  | Plasma          | pixel | colorA, colorB, scale, speed |
| 5  | Ripple          | pixel | colorA, colorB, scale, speed |
| 6  | Breathe         | pixel | colorA, speed |
| 7  | Rainbow         | pixel | axis, scale, speed |
| 8  | Rings           | pixel | scale, speed |
| 9  | Polar Rings     | pixel | scale, speed |
| 10 | Polar Spiral    | pixel | scale, speed |
| 11 | Polar Radial    | pixel | scale, speed |
| 12 | Twinkle Fox     | frame | — (cycles its own palettes) |
| 13 | Fire 2012       | frame | scale (cooling), speed |
| 14 | Cylon           | frame | speed |
| 15 | Rainbow Glitter | frame | speed |
| 16 | Confetti        | frame | speed |
| 17 | Sinelon         | frame | speed |
| 18 | BPM             | frame | speed |

`axis` is vertical, horizontal, radial or **angular** — the last sweeps around the spiral,
which is the one thing this board's geometry offers that a grid does not. Ids 8–11 are the
animations this firmware grew up with, now with parameters behind them.

Either button = next preset **on the cycle**. There is no long press: this board has nothing
to power down for.

## Regenerating the mapping
```
cd ../lolly_pcb && python3 emit_firmware_mapping.py > ../firmware/include/mapping.h
```

## Build / flash
```
./scripts/prepare_data.sh     # regenerate include/spa_gz.h from ../simulator/
pio run -t upload             # flash (the page is embedded — no filesystem upload)
pio device monitor            # 115200 baud
```

## Device API
A fresh board is its own access point: join `MiniLolly Manfred` / `Lumos2024` and open
`http://192.168.4.1`. Once it has been given a network (see **Wi-Fi** below) it is at
`http://lolly.local` instead. Either way:
- `GET /api/effects` — the effect and parameter schema
- `GET /api/state`, `POST /api/active` — `{id}`
- `GET /api/presets` — full list; `GET /api/presets/<n>` — one preset
- `POST /api/presets` — create; `PUT /api/presets/<n>` — update; `DELETE /api/presets/<n>`
  Preset bodies take `{name, effect, params:{colorA,colorB,speed,scale,axis}, brightness,
  cycle}`. `PUT` only applies the keys present, so `{"cycle":false}` alone just moves a preset
  off the buttons. Every mutating route replies with the refreshed full list.
- `PUT /api/brightness` — `{value}`, applied to the active preset and saved with it
- `GET /api/backup` — everything that survives a power cycle in one document;
  `POST /api/restore` — the same document back. See **Backup** below.
- `GET /api/wifi` — radio status; `GET /api/wifi/scan` — networks in range (poll it:
  the first answer is `{"scanning":true}`)
- `PUT /api/wifi` — `{ssid, password}`, saved and joined; `DELETE /api/wifi` — forget it.
  See **Wi-Fi** below.

Routing is substring matching in one `if` chain, so more specific paths must be tested first —
`GET /api/presets/3` has to precede `GET /api/presets`, and `GET /api/wifi/scan` has to precede
`GET /api/wifi`, or the less specific route swallows it.

## Wi-Fi
`src/WifiNet.cpp` owns the radio and makes one decision: join a saved network if there is one,
otherwise be an access point. **The AP is the floor** — it needs no infrastructure, so it is
what the board falls back to whenever joining does not work out, and the board is therefore
never unreachable because a router moved or a password changed.

- Credentials arrive from the website (`PUT /api/wifi`) and live in NVS under their own
  `wifinet` namespace, so a restore cannot overwrite them and a backup cannot leak them.
- They are written when they arrive, not when a join succeeds: a board that cannot reach its
  network right now (router still booting after a shared power cut) still retries on its own.
- A join gets `WIFI_STA_ATTEMPTS` tries of `WIFI_STA_TIMEOUT_MS` each, then the AP comes back
  with the reason kept in `error` — readable exactly when the website can be reached again.
- Once joined, a link gap has `WIFI_STA_LOST_MS` of grace (the driver retries on its own)
  before the board decides the network is gone and puts the AP back up.
- A mode switch drops every socket, including the one that asked for it, so the handler only
  records the request and `WifiNet::loop()` applies it `WIFI_APPLY_DELAY_MS` later, once the
  response is out. `takeNetChanged()` then tells `WebServer` to rebind port 80 on whatever
  interface now exists.
- On a joined board mDNS publishes `WIFI_HOSTNAME` — `http://lolly.local`, which survives a
  new DHCP lease, unlike the address nothing on your phone tells you.

`test/test_wifi.cpp` walks all of it against a scripted stub radio: join, reboot-rejoins,
wrong-password fallback, forget, and an SSID with a backslash in it.

## Backup
`GET /api/backup` returns one document with every preset and the device settings — the
website's **Export all** downloads it as `mini-lolly-backup-<date>.json`:
```json
{ "lollyBackup": 1,
  "settings": {"active": 2},
  "presets": [ {"id":0,"name":"Polar Radial","effect":11,"brightness":15,"cycle":true,
                "params":{"colorA":"#FF9B3D","colorB":"#3C78D2","speed":0.300,
                          "scale":1.000,"axis":2}} ] }
```
`POST /api/restore` takes it back and **replaces** the presets — it is a restore, not a merge.
It answers `{presets, active}` with what landed. Preset objects are read by exactly the same
code as `POST /api/presets`, so `id` is ignored and missing keys fall back to the same
defaults; a body carrying no usable preset is refused with 400 *before* anything is dropped,
so a bad file cannot empty the device. Being plain JSON over the REST shapes rather than a
dump of the stored blob, a backup taken before a `PRESETS_VERSION` bump still restores —
which is the way to carry presets across a change to the `Preset` struct.

The whole document is streamed with `Transfer-Encoding: chunked`, so nothing larger than a
single preset is ever assembled in one piece.

## Notes
- **Preset storage:** presets live in NVS as one blob behind a `magic("LPST") + version +
  sizeof(Preset) + count` header, not on a filesystem — the board's partition table
  (`partitions-4MB-tinyuf2.csv`) has no LittleFS partition, only the TinyUF2 FAT drive. At 32
  presets that is under 2 KB in a 20 KB partition. Any change to `Preset` or `EffectParams` —
  a new field, a reorder, a different `name[]` length — **must** bump `PRESETS_VERSION` in
  `src/PresetStore.cpp`. Without it the old bytes are reinterpreted under the new layout and
  you get garbage names, colours and effect ids with nothing to signal it. A header mismatch
  is not an error: the blob is dropped and the built-in defaults load, so presets saved on the
  device are lost across such a change — take a **Backup** first and import it after the flash.
  Records that pass the header check are still range-clamped on load.
- **Time wraps.** Both clocks in `EffectCtx` wrap at `TIME_WRAP_S` (1024 s). Q16.16 would
  overflow at 32768 s on its own, and `speed * t` long before that. `ts` is a *scaled* clock —
  it advances at `speed` seconds per second rather than being multiplied by it — so moving the
  speed slider changes the rate without jumping the phase. The cost is one discontinuous frame
  per wrap: a blink every 17 minutes at speed 1, less often when slower.
- **No colour pipeline.** Brightness is FastLED's 8-bit scale and the correction is
  `TypicalLEDStrip`, as before. Effects emit sRGB `0xRRGGBB` and blend in that space, so a
  50/50 mix of two saturated colours reads a little muddier than it should. The carrot's
  `ColorOut` (gamma + white balance in linear light) is the fix if that ever matters; the
  effect layer already produces exactly what it consumes.
- **Colour order** is `RGB` in `Config.h`. If red and green come out swapped, that is the
  knob — there is no runtime override. The off-cycle `Test red/green/blue` presets exist to
  make that visible, which a board full of animated colour otherwise makes impossible to judge.
- **Frame stalls.** `WiFiServer` reads block, so the render loop stops for the length of a
  request and a page load shows a visible hitch. Effects derive their phase from the clock
  rather than a frame counter, so they resume in the right place instead of slowing down. A
  client that opens a socket and then says nothing is dropped after 2 s.
- **Sim parity:** the simulator uses float math, the device fixed-point — visually close, not
  bit-exact. The shared sources of truth are the spiral geometry (both derive it from the same
  construction) and the parameter schema, not the arithmetic.
