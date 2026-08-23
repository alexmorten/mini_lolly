# Off-device tests

Runs `src/WebServer.cpp` and `src/PresetStore.cpp` — the real files, not copies — on the
host, so the parts that are awkward to test on hardware can be checked without a flash.
`stub/` stands in for the Arduino runtime: `String`/`Serial`, a `Preferences` backed by a
real temp directory (so the NVS blob is written and read back for real), and a `WiFiClient`
that captures the response instead of writing to a socket.

```
./run.sh                 # API checks — prints ok/FAIL per assertion
./run.sh --serve 8181    # serve the real handler on localhost
```

`test_api.cpp` covers the routes the website depends on, the clamps every value passes
through, `nextPreset()`'s cycle-skipping (including the fallback that stops the buttons
becoming a dead key), and a `GET /api/backup` → `POST /api/restore` round trip: the refusal
paths (an empty preset list and a garbage body must not empty the device), a pretty-printed
document — which is what posting the exported file by hand gives you — and a restart
afterwards, since a restore that only lives in RAM is not a restore. The last check writes a
blob with a bad version straight into the store and asserts the defaults come back.

`--serve` is the other half. It puts the same handler behind a socket so
`../../simulator/index.html` can be pointed at it with **Device URL**
`http://127.0.0.1:8181` and driven in a browser — every route the website uses answers from
the firmware's own code, so the request the SPA really sends is the one being tested, CORS
preflight included. `GET /` is served from `include/spa_gz.h`, so the page the host server
hands back is whatever `scripts/prepare_data.sh` last packed, not the working copy.

Both are host builds: `min`/`constrain` are macros here, `fix16_t` math is the real thing,
but there is no FastLED, no radio and no timing. Anything that depends on the render loop,
the buttons or the frame effects still needs the board — `effects.cpp` builds here because
it never touches FastLED, but the FRAME_EFFECTS half deliberately does and is left out.

The test includes `WebServer.cpp` directly rather than linking it, because the request
handler and the JSON helpers are `static` — deliberately, since nothing on the device should
reach them from outside. The other sources are linked normally.
