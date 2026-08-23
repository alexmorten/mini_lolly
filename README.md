# Mini Lolly

63 WS2812B LEDs on a [Fermat spiral with the golden
angle](https://en.wikipedia.org/wiki/Fermat%27s_spiral#The_golden_ratio_and_the_golden_angle),
driven by an Adafruit Feather ESP32-S2.

```
firmware/     the board firmware (see firmware/README.md)
simulator/    the website — both a browser simulator and the page the device serves
lolly_pcb/    KiCad board generation and the spiral geometry everything else derives from
printing/     diffuser and housing (3MF)
```

## The website

`simulator/index.html` is one self-contained page that does two jobs: it simulates the board
in a browser, and — when it is the copy the firmware serves over its own Wi-Fi AP — it is the
remote control. Pick an effect, tune its parameters, arrange presets, set brightness, export
and import the whole setup.

Open it against a device by joining the AP `MiniLolly Manfred` (password `Lumos2024`) and
visiting <http://192.168.4.1>. It connects to whatever origin served it, so nothing to set up.

The Wi-Fi card in that page hands the board a network of its own to join, after which it lives
at <http://lolly.local> and the AP is gone. Nothing is lost if the join fails or the network
later disappears: the board brings its own AP back up, so the address above always gets you in.

To run it as a plain simulator with no device in reach, serve the repo and open it — it falls
back to a local preset list and says so:

```bash
python3 -m http.server 8000
```

Then <http://localhost:8000/simulator/>. To point a locally-opened page at a real board,
type its address into **Device URL** and press **Connect** (the firmware answers CORS
preflights, so this works cross-origin).

The device's own copy is baked into the firmware image, so changes here only reach the board
through `firmware/scripts/prepare_data.sh` and a flash.

## Firmware

See [firmware/README.md](firmware/README.md) for effects, the preset model, the REST API and
the build. In short:

```bash
cd firmware
./scripts/prepare_data.sh    # pack simulator/ into include/spa_gz.h
pio run -t upload
./test/run.sh                # off-device checks, no board needed
```
