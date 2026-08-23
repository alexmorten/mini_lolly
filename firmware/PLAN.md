# Mini Lolly firmware — port the carrot_amulet control interface

Bring the `~/private/carrot_amulet` architecture over to the 63-LED Fermat-spiral board:
a native effect table with a published parameter schema, a persisted preset store, a REST
API, and a single-file SPA served from device flash that doubles as the browser simulator.

## Decisions (locked with the user)

- **Board stays ESP32-S2** (Adafruit Feather, `adafruit_feather_esp32s2`). No C6 migration.
- **Port:** effects engine + preset store + REST API + embedded SPA, plus backup/restore.
- **Do not port:** `ColorOut` (gamma/white balance), painted bases / the Paint tab,
  long-press power-off + deep sleep, `Trigger`.
- **Animations:** keep every existing one (twinkle fox, fire2012, cylon, demo reel, rings,
  polar rings ×3) *and* add the carrot's parametric effects that work on `x/y`.
- **Buttons:** both (board GPIO0, external GPIO5) advance to the next in-cycle preset, as today.
- **UI:** one SPA — the browser simulator and the device page are the same file, inlined and
  gzipped into the firmware. The current ES-module simulator's spiral canvas is kept; its
  JS-only patterns (`color_rings`, `color_arms`, `rotating_image`) go away.
- **Tests:** port the off-device harness (`test/run.sh`, Arduino stubs, `--serve` twin).

## What the port has to work around

Three findings that make this more than a copy of the carrot sources.

**1. There is no LittleFS partition on this board.** `adafruit_feather_esp32s2` builds against
`partitions-4MB-tinyuf2.csv`: `nvs 20K · otadata 8K · ota_0 1408K · ota_1 1408K · uf2 256K ·
ffat 960K`. No `spiffs`-subtype partition, and `ffat` belongs to the TinyUF2 drive. Since
painted bases are out of scope, the only thing needing storage is the preset list —
**presets go in NVS as one blob** via `Preferences`, and the filesystem is dropped entirely.
At `MAX_PRESETS = 32` and ~64 bytes per record that is a ~2 KB blob in a 20 KB partition.
Fallback if that ever gets tight: a custom partition CSV, which means re-flashing the TinyUF2
image at its offset — avoid unless forced.

**2. The existing animations drive the strip themselves.** Every `loop*()` in
`src/animations/` ends with `FastLED.show()` + `FastLED.delay(1000/120)`, and they advance on
a tick counter rather than on the clock — which is exactly why Cylon is commented out in
`main.cpp` today ("doesn't have a fast loop"). They must become **render-only** functions that
fill `leds[]` (or return a color) and let one frame-gated loop own `show()`. Phase is then
derived from `ctx.t`, so a blocking HTTP read stalls a frame instead of slowing the animation.

**3. The S2 has no FPU**, same as the C6. `fixmath` (Q16.16 + sine LUT) ports over as-is and is
worth having. The per-LED polar coordinates the spiral effects need (`r`, `theta`) get
precomputed into the generated mapping table, so no `sqrt`/`atan2` runs per pixel per frame.

## Architecture

```
each frame, gated at 16 ms:
  buttons.update()                     // either button -> next in-cycle preset
  web.loop()                           // blocking WiFiServer, same shape as carrot
  p = PresetStore::activePreset()
  ctx = {t, frame, &p.params}
  if EFFECT_KIND[p.effect] == PIXEL:
    for i in 0..62: frame[i] = EFFECTS_PIXEL[p.effect](ctx{i, x, y, r, theta})
    unpack frame -> leds[]
  else:
    EFFECTS_FRAME[p.effect](ctx, leds)  // legacy FastLED animation owns the array
  FastLED.setBrightness(min(p.brightness, MAX_BRIGHTNESS))
  FastLED.show()
```

Two effect kinds behind one id space is the one real departure from the carrot, and it is what
lets the existing FastLED animations sit in the same list as the parametric ones. The schema
JSON marks which is which only insofar as it declares each effect's params; the SPA does not
need to care, and a frame effect simply declares few or no knobs.

`EffectCtx` drops `row`/`col` (grid concepts, meaningless on a spiral) and gains `r`/`theta`.
`EffectParams` drops `blend` (no painted base to composite over) and keeps `aux[2]` spare.
`axis` gains a fourth option, **angular**, alongside vertical/horizontal/radial — the spiral is
the one place where sweeping by angle is natural.

### Effect list

| id | name | kind | from |
|----|------|------|------|
| 0 | Solid | pixel | carrot |
| 1 | Gradient | pixel | carrot (+ angular axis) |
| 2 | Wave | pixel | carrot (+ angular axis) |
| 3 | Sparkle | pixel | carrot — no base to overlay, so it sparkles over `colorA` |
| 4 | Plasma | pixel | carrot |
| 5 | Ripple | pixel | carrot |
| 6 | Breathe | pixel | carrot (breathes `colorA`; the base variant is gone) |
| 7 | Rainbow | pixel | carrot (+ angular axis) |
| 8 | Rings | pixel | existing `rings.cpp`, time-based, speed/scale params |
| 9 | Polar Rings | pixel | existing `polar_rings.cpp` |
| 10 | Polar Spiral | pixel | existing `polar_rings.cpp` |
| 11 | Polar Radial | pixel | existing `polar_rings.cpp` |
| 12 | Twinkle Fox | frame | existing |
| 13 | Fire 2012 | frame | existing |
| 14 | Cylon | frame | existing — re-enabled once it no longer blocks |
| 15 | Rainbow Glitter | frame | existing demo reel |
| 16 | Confetti | frame | existing demo reel |
| 17 | Sinelon | frame | existing demo reel |
| 18 | Juggle | frame | existing demo reel |
| 19 | BPM | frame | existing demo reel |

Demo reel's plain `rainbow()` is dropped — effect 7 covers it with parameters.

### API surface

Carrot's routes minus the ones whose subsystems aren't coming:

- `GET /` — gzipped SPA from PROGMEM
- `GET /api/effects` — `EFFECT_SCHEMA_JSON`
- `GET /api/state` · `POST /api/active`
- `GET /api/presets` · `GET /api/presets/<n>` · `POST /api/presets` ·
  `PUT /api/presets/<n>` · `DELETE /api/presets/<n>`
- `PUT /api/brightness`
- `GET /api/backup` · `POST /api/restore`

Dropped: `/api/paint`, `/api/base/<n>`, `/api/coloradj`, `/api/colorswap`, `/api/power`,
`/api/trigger`, `/api/reset-default-base`. The substring-matching `if` chain and its
ordering rule (specific paths before general) carry over unchanged.

Backup document, with `bases` gone and settings trimmed to what this device keeps:

```json
{ "lollyBackup": 1,
  "settings": {"active": 2},
  "presets": [ {"id":0,"name":"Polar Radial","effect":11,"brightness":15,"cycle":true,
                "params":{"colorA":"#ff9b3d","colorB":"#000000","speed":1.0,"scale":1.0,"axis":0}} ] }
```

Same refusal path as the carrot: a body with no usable preset is rejected with 400 *before*
anything is dropped, so a bad file cannot empty the device.

## File plan

```
firmware/
  platformio.ini                 EDIT   build flags (-Os, CORE_DEBUG_LEVEL=0)
  include/Config.h               NEW    pins, NUM_LEDS 63, brightness caps, SSID/password
  include/mapping.h              NEW    GENERATED x/y/r/theta per LED
  include/spa_gz.h               NEW    GENERATED gzipped SPA
  src/fixmath.{h,cpp}            PORT   verbatim from carrot
  src/effects.{h,cpp}            NEW    dual-kind effect table + EFFECT_SCHEMA_JSON
  src/animations/*.{h,cpp}       EDIT   render-only; time-based; no show()/delay()
  src/PresetStore.{h,cpp}        PORT   NVS blob instead of LittleFS; no bases/colorAdj
  src/WebServer.{h,cpp}          PORT   minus the dropped routes
  src/main.cpp                   REWRITE frame-gated loop, two buttons, preset cycling
  src/led_config.{h,cpp}         DELETE superseded by Config.h + mapping.h
  scripts/prepare_data.sh        PORT   simulator -> spa_gz.h
  test/run.sh, test/stub/*,      PORT   Preferences stub becomes file-backed so the
  test/test_api.cpp,                    restart-after-restore assertion is real
  test/host_server.cpp
lolly_pcb/emit_firmware_mapping.py  NEW  extends mapping.py to emit include/mapping.h
simulator/                       REWRITE single-file SPA (canvas kept, JS patterns dropped)
index.html                       DELETE root loader; the SPA is self-contained
README.md                        EDIT   build/flash/serve instructions
```

## Phases

Each phase is independently checkable; the firmware builds at the end of every one.

**1 — Foundations.** `Config.h`; `emit_firmware_mapping.py` extended from `lolly_pcb/mapping.py`
to emit `x`, `y`, `r`, `theta` (regenerating today's coordinates so nothing shifts); `fixmath`
copied in. *Check:* `pio run` compiles; the regenerated x/y match `led_config.cpp` to float
precision.

**2 — Effect engine.** `effects.{h,cpp}` with both dispatch tables and the schema JSON; the
eight carrot pixel effects; the four spiral animations converted to pixel effects; the legacy
FastLED animations refactored to render-only frame effects. *Check:* flash and step through
every effect from a hardcoded index; Cylon and Fire2012 must now animate without stalling the
loop.

**3 — Preset store.** `PresetStore` on NVS: versioned header, `sanitize()` clamping on every
entry path, `PRESET_IN_CYCLE` flags with the never-a-dead-key fallback, defaults seeded to
match today's pattern list so a fresh device behaves like the current firmware. *Check:*
presets survive a power cycle; a version bump drops the blob and reloads defaults.

**4 — Web server and loop.** `WebServer.{h,cpp}` with the routes above; `main.cpp` rewritten
around the frame-gated loop and the two-button cycling. *Check:* `curl` every route over the AP.

**5 — Host tests.** Port `test/` with a file-backed `Preferences` stub. *Check:* `./run.sh`
passes the backup round trip and both refusal paths; `./run.sh --serve 8181` answers the same
routes on localhost.

**6 — SPA.** Single-file `simulator/index.html`: spiral canvas (the glow rendering from
`lib/draw.js`), preset list with new/duplicate/rename/delete/cycle-flag, effect picker driven
by `/api/effects`, parameter controls, brightness, export/import, Device URL + Connect. JS
mirrors of the effects for offline simulation — float math, visually close, not bit-exact,
same caveat as the carrot. `prepare_data.sh` inlines and gzips it into `spa_gz.h`. *Check:*
drive the SPA against `run.sh --serve` in a browser before ever flashing, then against the
device.

**7 — Docs and cleanup.** Rewrite `README.md` (build, flash, `--serve`, regenerating the
mapping); delete `led_config.*`, the old root `index.html`, `simulator/patterns/`,
`simulator/patterns.js`, `simulator/main.js`.

## Risks and open items

- **Flash budget.** The carrot sits at ~81% of a 1.25 MB slot; this board's app slot is 1408 KB
  and there is no LittleFS image to ship. Expected to fit with room, but worth watching once
  `spa_gz.h` lands — the carrot's gzipped SPA is ~27 KB and this one will be smaller.
- **NVS blob size.** ~2 KB at `MAX_PRESETS = 32` in a 20 KB partition shared with Wi-Fi
  calibration data. Comfortable, but `MAX_PRESETS` is the knob if it ever isn't.
- **Colour order.** `led_config.h` declares `COLOR_ORDER RGB` while the carrot uses `GRB`, and
  the carrot's live `swap R↔G` toggle is not being ported. If effects come out with red and
  green transposed, the fix is `COLOR_ORDER` in `Config.h`, not a runtime switch.
- **Brightness ceiling.** Today: default 15, a UI ladder to 50, and the power limiter commented
  out. The plan keeps default 15 and adds `MAX_BRIGHTNESS`; **what should the cap be, and should
  `setMaxPowerInVoltsAndMilliamps` be re-enabled?** Depends on how this board is powered — I'll
  default to a 64 cap with the limiter left off (matching current behaviour) unless told otherwise.
- **No colour pipeline.** Without `ColorOut`, brightness stays FastLED's 8-bit scale and
  `TypicalLEDStrip` correction, so low brightness will look flatter than the carrot does. Easy
  to add later; the effect layer already emits sRGB `0xRRGGBB`, which is what `ColorOut` consumes.
- **Frame stalls.** The blocking `WiFiServer` read halts rendering during a request, exactly as
  on the carrot. Time-based phase means animations resume in the right place rather than
  slowing down, but a visible hitch during page loads is expected and not a bug.
