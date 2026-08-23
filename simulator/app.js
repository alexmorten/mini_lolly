/* Mini Lolly simulator + device remote.
   Vanilla JS, no dependencies — prepare_data.sh inlines this into the page the
   firmware serves, so anything external would 404 on the device's AP. */

// ---------------------------------------------------------------- geometry
// The same construction as lolly_pcb/emit_firmware_mapping.py, rather than a
// copy of the numbers it emits: 63 LEDs on a Fermat spiral, visited by the data
// line in 21 strips of three with every other strip reversed.
var LED_COUNT = 63;
var LED_SPACING = 5.5;
var GOLDEN_ANGLE_DEG = 137.508;
var STRIP_STARTS = [1, 9, 17, 4, 12, 20, 7, 15, 2, 10, 18, 5, 13, 21, 8, 16, 3, 11, 19, 6, 14];

var LEDS = (function build() {
  var order = [];
  STRIP_STARTS.forEach(function (start, stripIdx) {
    var strip = [];
    for (var n = start; n <= LED_COUNT; n += 21) strip.push(n);
    if (stripIdx % 2 === 1) strip.reverse();
    order = order.concat(strip);
  });

  var raw = order.map(function (n) {
    var r = LED_SPACING * Math.sqrt(n);
    var a = (n * GOLDEN_ANGLE_DEG / 180) * Math.PI;
    return { x: r * Math.cos(a), y: r * Math.sin(a) };
  });

  var divider = 0;
  raw.forEach(function (p) { divider = Math.max(divider, Math.abs(p.x), Math.abs(p.y)); });

  return raw.map(function (p, i) {
    var x = p.x / divider, y = p.y / divider;
    var theta = Math.atan2(y, x) / (2 * Math.PI);
    return { i: i, x: x, y: y, r: Math.hypot(x, y), theta: theta - Math.floor(theta) };
  });
})();

var MAX_RADIUS = LEDS.reduce(function (m, l) { return Math.max(m, l.r); }, 0);

// Chain indices sorted by angle — the firmware's Mapping::ANGULAR_ORDER. The data
// line visits the spiral in an order that zigzags in and out along the arms, so the
// frame effects whose motion should read as going round the board step through this
// instead of through the chain, and wrap at the end rather than turning back.
var ANGULAR_ORDER = LEDS.map(function (l) { return l.i; })
  .sort(function (a, b) { return LEDS[a].theta - LEDS[b].theta; });

function circleIndex(slot, count) { return ANGULAR_ORDER[((slot % count) + count) % count]; }
function circleSlot(pos01, count) {
  var slot = Math.floor(frac(pos01) * count);
  return slot >= count ? count - 1 : slot;
}

// ---------------------------------------------------------------- colour helpers
function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function frac(v) { return v - Math.floor(v); }
function wave01(turns) { return 0.5 + 0.5 * Math.sin(2 * Math.PI * turns); }
// 0..1 and back again over one turn — continuous where frac() would snap.
function pingpong01(turns) { var f = frac(turns); return f < 0.5 ? f * 2 : (1 - f) * 2; }

function hsv(h, s, v) {
  h = frac(h);
  var i = Math.floor(h * 6), f = h * 6 - i;
  var p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s), r, g, b;
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q;
  }
  return [r * 255, g * 255, b * 255];
}

function hexToRgb(hex) {
  var s = String(hex).replace('#', '');
  if (s.length === 3) s = s[0] + s[0] + s[1] + s[1] + s[2] + s[2];
  return [parseInt(s.slice(0, 2), 16) || 0, parseInt(s.slice(2, 4), 16) || 0, parseInt(s.slice(4, 6), 16) || 0];
}

function rgbToHex(c) {
  function h(n) { n = clamp(Math.round(n), 0, 255); return (n < 16 ? '0' : '') + n.toString(16); }
  return '#' + h(c[0]) + h(c[1]) + h(c[2]);
}

// Matches the device: colours are mixed in gamma-encoded sRGB, not linear light.
function mix(a, b, k) {
  k = clamp(k, 0, 1);
  return [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k];
}

function fadeToBlack(colors, amount) {
  for (var i = 0; i < colors.length; i++) {
    colors[i] = [colors[i][0] * amount, colors[i][1] * amount, colors[i][2] * amount];
  }
}

// ---------------------------------------------------------------- effects
// Float mirrors of src/effects.cpp. The device runs fixed-point, so these are
// visually close, not bit-exact — the shared source of truth is the geometry and
// the parameter schema, not the arithmetic.

var AXIS = { VERTICAL: 0, HORIZONTAL: 1, RADIAL: 2, ANGULAR: 3 };

function axisPos01(led, axis) {
  switch (axis) {
    case AXIS.HORIZONTAL: return (led.x + 1) / 2;
    case AXIS.RADIAL: return led.r / MAX_RADIUS;
    case AXIS.ANGULAR: return led.theta;
    default: return (led.y + 1) / 2;
  }
}

function ledHash01(i) {
  // xorshift on the same seed the firmware uses, so the sparkle pattern lines up.
  var st = (Math.imul(i, 747796405) + 2891336453) >>> 0;
  st ^= st << 13; st >>>= 0;
  st ^= st >>> 17;
  st ^= st << 5; st >>>= 0;
  return (st & 0xFFFF) / 65536;
}

// Pixel effects: colour for one LED from geometry and time.
var PIXEL_EFFECTS = {
  0: function solid(led, p) { return p.a; },

  1: function gradient(led, p) {
    // Turns around rather than wrapping — see effects.cpp.
    return mix(p.a, p.b, pingpong01((axisPos01(led, p.axis) + p.ts) / 2));
  },

  2: function wave(led, p) {
    return mix(p.a, p.b, wave01(axisPos01(led, p.axis) * p.scale - p.ts * 0.5));
  },

  3: function sparkle(led, p) {
    var v = wave01(p.ts * 0.5 + ledHash01(led.i));
    v = Math.pow(v, 6);
    return mix(mix([0, 0, 0], p.a, 0.12), [255, 255, 255], v);
  },

  4: function plasma(led, p) {
    var s = p.scale * 0.7;
    var v = Math.sin(2 * Math.PI * (led.x * s + p.ts)) +
            Math.sin(2 * Math.PI * (led.y * s - p.ts * 0.7)) +
            Math.sin(2 * Math.PI * ((led.x + led.y) * s * 0.75 + p.ts * 1.3));
    return mix(p.a, p.b, clamp((v + 3) / 6, 0, 1));
  },

  5: function ripple(led, p) {
    var v = wave01(led.r / MAX_RADIUS * p.scale * 1.5 - p.ts);
    return mix(p.b, p.a, v * v);
  },

  6: function breathe(led, p) {
    return mix([0, 0, 0], p.a, 0.35 + 0.65 * wave01(p.ts * 0.25));
  },

  7: function rainbow(led, p) {
    return hsv(axisPos01(led, p.axis) * p.scale + p.ts, 1, 1);
  },

  8: function rings(led, p) {
    // Hue keyed to radius - time, so the bands travel out with no border to wrap.
    return hsv(frac((led.r / MAX_RADIUS) * p.scale - p.ts), 1, 0.59);
  },

  9: function polarRings(led, p) {
    var v = wave01((led.theta + p.ts) * p.scale * 3);
    return hsv(frac(led.r / MAX_RADIUS + p.ts * 0.5), 1, v);
  },

  10: function polarSpiral(led, p) {
    var sp = led.theta + (led.r / MAX_RADIUS) * p.scale * 0.6 + p.ts;
    return hsv(frac(led.theta + p.ts * 0.3), 1, wave01(sp * 2));
  },

  11: function polarRadial(led, p) {
    // A travelling wave, not a crest folded around |r - frac(ts)|: that snapped
    // the whole board back to the middle once per turn. Mirrors effects.cpp.
    var phase = (led.r / MAX_RADIUS) * p.scale * 1.6 - p.ts;
    return hsv(led.theta, 1, 0.5 + 0.5 * Math.cos(2 * Math.PI * phase));
  }
};

var FRAME_FIRST = 12;

// Frame effects run FastLED code on the device that keeps its own state. These
// are stand-ins with the same character — close enough to choose between, not
// close enough to tune against.
var FRAME_EFFECTS = {
  12: function twinkleFox(colors, p) {
    for (var i = 0; i < colors.length; i++) {
      var phase = frac(p.t * 0.35 * (0.6 + ledHash01(i)) + ledHash01(i) * 7);
      var v = phase < 0.5 ? phase * 2 : (1 - phase) * 2;
      v = Math.pow(clamp(v * 1.6 - 0.6, 0, 1), 2);
      colors[i] = hsv(frac(ledHash01(i) * 3 + p.t * 0.02), 0.6, v);
    }
  },

  13: function fire(colors, p) {
    var st = p.state;
    if (!st.heat) st.heat = new Array(colors.length).fill(0);
    st.pending = (st.pending || 0) + p.dt * p.speed * 60;
    st.pending = Math.min(st.pending, 4);
    while (st.pending >= 1) {
      st.pending -= 1;
      for (var i = 0; i < colors.length; i++) {
        st.heat[i] = Math.max(0, st.heat[i] - Math.random() * ((55 * p.scale * 10) / colors.length + 2));
      }
      var prev = st.heat.slice();
      for (var k = 0; k < colors.length; k++) {
        var a = (k + colors.length - 1) % colors.length;
        var b = (k + colors.length - 2) % colors.length;
        st.heat[k] = (prev[a] + 2 * prev[b]) / 3;
      }
      if (Math.random() * 255 < 120) {
        var y = Math.floor(Math.random() * colors.length);
        st.heat[y] = Math.min(255, st.heat[y] + 160 + Math.random() * 95);
      }
    }
    for (var j = 0; j < colors.length; j++) {
      var h = clamp(st.heat[j] / 255, 0, 1);
      colors[circleIndex(j, colors.length)] =
        [255 * clamp(h * 3, 0, 1), 255 * clamp(h * 3 - 1, 0, 1), 255 * clamp(h * 3 - 2, 0, 1)];
    }
  },

  14: function cylon(colors, p) {
    fadeToBlack(colors, 250 / 256);
    var pos = circleIndex(circleSlot(p.ts * 0.25, colors.length), colors.length);
    colors[pos] = hsv(frac(p.ts * 0.125), 1, 1);
  },

  15: function rainbowGlitter(colors, p) {
    var base = frac(p.ts * 0.2);
    for (var i = 0; i < colors.length; i++) colors[i] = hsv(base + i * (7 / 255), 1, 1);
    if (Math.random() * 255 < 80) colors[Math.floor(Math.random() * colors.length)] = [255, 255, 255];
  },

  16: function confetti(colors, p) {
    fadeToBlack(colors, 1 - 10 / 256);
    var i = Math.floor(Math.random() * colors.length);
    colors[i] = hsv(frac(p.ts * 0.2 + Math.random() * 0.25), 0.78, 1);
  },

  17: function sinelon(colors, p) {
    fadeToBlack(colors, 1 - 20 / 256);
    var pos = circleIndex(circleSlot(p.ts * 0.2, colors.length), colors.length);
    colors[pos] = hsv(frac(p.ts * 0.2), 1, 0.75);
  },

  18: function bpm(colors, p) {
    var hue = frac(p.ts * 0.2);
    var beat = 0.25 + 0.75 * wave01(p.ts * 2.13);
    for (var i = 0; i < colors.length; i++) {
      colors[i] = hsv(hue + i * 2 / 255, 0.85, clamp(beat - hue + i * 10 / 255, 0.05, 1));
    }
  }
};

// ---------------------------------------------------------------- render loop
var canvas = document.getElementById('board');
var ctx = canvas.getContext('2d');

var sim = {
  playing: true,
  showIndices: false,
  ts: 0,           // speed-scaled seconds, mirrors EffectCtx::ts
  t: 0,            // seconds
  last: 0,
  frameState: {},  // per-effect state for the frame stand-ins
  colors: LEDS.map(function () { return [0, 0, 0]; }),
  fps: 0
};

function resizeCanvas() {
  var dpr = window.devicePixelRatio || 1;
  var css = canvas.clientWidth || 600;
  canvas.width = canvas.height = Math.round(css * dpr);
}

function renderFrame(now) {
  requestAnimationFrame(renderFrame);
  var dt = sim.last ? Math.min((now - sim.last) / 1000, 0.1) : 0;
  sim.last = now;
  if (!sim.playing) { draw(); return; }

  sim.fps = sim.fps * 0.9 + (dt > 0 ? 1 / dt : 0) * 0.1;
  var preset = current();
  if (!preset) return;

  sim.t = (sim.t + dt) % 1024;
  sim.ts = (sim.ts + dt * preset.params.speed) % 1024;

  var p = {
    a: hexToRgb(preset.params.colorA),
    b: hexToRgb(preset.params.colorB),
    speed: preset.params.speed,
    scale: preset.params.scale,
    axis: preset.params.axis,
    ts: sim.ts,
    t: sim.t,
    dt: dt,
    state: sim.frameState
  };

  var fn = preset.effect;
  if (fn >= FRAME_FIRST) {
    (FRAME_EFFECTS[fn] || FRAME_EFFECTS[12])(sim.colors, p);
  } else {
    for (var i = 0; i < LEDS.length; i++) {
      sim.colors[i] = (PIXEL_EFFECTS[fn] || PIXEL_EFFECTS[0])(LEDS[i], p);
    }
  }
  draw();
}

function draw() {
  var size = canvas.width;
  ctx.clearRect(0, 0, size, size);

  var pad = size * 0.06;
  var scale = (size / 2 - pad) / MAX_RADIUS;
  var cx = size / 2, cy = size / 2;
  var ledR = size * 0.016;

  // The strip is driven at a brightness the screen has no way to reproduce, so
  // the preview normalizes against the cap instead: full slider = full screen.
  var preset = current();
  var gain = preset ? clamp(preset.brightness / (state.maxBrightness || 64), 0.04, 1) : 1;

  for (var i = 0; i < LEDS.length; i++) {
    var c = sim.colors[i];
    var x = cx + LEDS[i].x * scale;
    // Canvas y grows downward and the board coordinates grow upward.
    var y = cy - LEDS[i].y * scale;
    ctx.fillStyle = 'rgb(' + Math.round(clamp(c[0] * gain, 0, 255)) + ',' +
                             Math.round(clamp(c[1] * gain, 0, 255)) + ',' +
                             Math.round(clamp(c[2] * gain, 0, 255)) + ')';
    // Stacked translucent discs, blurred by CSS into one halo.
    var alphas = [0.1, 0.2, 0.4, 1], sizes = [4.4, 3.3, 2.2, 1];
    for (var j = 0; j < alphas.length; j++) {
      ctx.globalAlpha = alphas[j];
      ctx.beginPath();
      ctx.arc(x, y, ledR * sizes[j], 0, 2 * Math.PI);
      ctx.fill();
    }
    ctx.globalAlpha = 1;

    if (sim.showIndices) {
      ctx.fillStyle = 'rgba(255,255,255,.75)';
      ctx.font = Math.round(size * 0.018) + 'px ui-monospace, monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(String(i), x, y);
    }
  }

  document.getElementById('fps').textContent = 'sim ' + Math.round(sim.fps) + ' fps';
}

// ---------------------------------------------------------------- device client
var state = {
  url: '',
  connected: false,
  schema: null,
  presets: [],
  sel: 0,
  dirty: false,
  maxBrightness: 64
};

function current() { return state.presets[state.sel]; }

function api(path, opts) {
  if (!state.connected && !opts?.force) return Promise.reject(new Error('offline'));
  return fetch(state.url + path, opts).then(function (r) {
    if (!r.ok) return r.text().then(function (t) { throw new Error(r.status + ' ' + t); });
    return r.status === 204 ? null : r.json();
  });
}

function post(path, body, method) {
  return api(path, {
    method: method || 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
}

function setStatus(text, cls) {
  var el = document.getElementById('devState');
  el.textContent = text;
  el.className = cls || '';
}

// fetch rejects with a bare TypeError when nothing answers, and JSON.parse
// throws about a stray "<" when something answers but is not a device. Neither
// says anything useful on its own, so they get named here.
function connectError(e) {
  if (e instanceof TypeError) return 'nothing answered at ' + state.url;
  if (e.message && e.message.indexOf('JSON') >= 0) return 'not a Mini Lolly at ' + state.url;
  return e.message;
}

function connect(auto) {
  state.url = document.getElementById('devUrl').value.replace(/\/+$/, '');
  setStatus('connecting…');
  fetch(state.url + '/api/effects')
    .then(function (r) { return r.json(); })
    .then(function (schema) {
      state.schema = schema;
      state.connected = true;
      return api('/api/state');
    })
    .then(function (st) {
      state.maxBrightness = st.maxBrightness || 64;
      return api('/api/presets');
    })
    .then(function (list) {
      state.presets = list.presets;
      state.sel = 0;
      state.dirty = false;
      setStatus('connected · ' + state.presets.length + ' presets', 'on');
      return api('/api/state');
    })
    .then(function (st) {
      state.sel = clamp(st.active, 0, state.presets.length - 1);
      buildEffectOptions();
      renderPresets();
      renderControls();
    })
    .catch(function (e) {
      state.connected = false;
      // Opening the page anywhere but on the device tries and fails by design,
      // so that first attempt is not an error worth colouring red.
      setStatus(auto ? 'offline — simulating locally' : 'offline · ' + connectError(e),
                auto ? '' : 'err');
      markClean();
    });
}

// ---------------------------------------------------------------- offline defaults
// So the page is a working simulator with no device in reach. Mirrors the
// firmware's built-in list closely enough to explore the effects.
var OFFLINE_SCHEMA = {
  effects: [
    { id: 0, name: 'Solid', params: ['colorA'], defaults: { colorA: '#ff9b3d' } },
    { id: 1, name: 'Gradient', params: ['colorA', 'colorB', 'axis', 'speed'], defaults: { colorA: '#da6f20', colorB: '#3c78d2', speed: 0.2 } },
    { id: 2, name: 'Wave', params: ['colorA', 'colorB', 'axis', 'scale', 'speed'], defaults: { colorA: '#da6f20', colorB: '#ffb454', scale: 3, speed: 0.5 } },
    { id: 3, name: 'Sparkle', params: ['colorA', 'speed'], defaults: { colorA: '#3c78d2', speed: 1 } },
    { id: 4, name: 'Plasma', params: ['colorA', 'colorB', 'scale', 'speed'], defaults: { colorA: '#ff6432', colorB: '#3c78d2', scale: 1, speed: 0.4 } },
    { id: 5, name: 'Ripple', params: ['colorA', 'colorB', 'scale', 'speed'], defaults: { colorA: '#ff9b3d', colorB: '#000000', scale: 1, speed: 0.5 } },
    { id: 6, name: 'Breathe', params: ['colorA', 'speed'], defaults: { colorA: '#ff9b3d', speed: 1 } },
    { id: 7, name: 'Rainbow', params: ['axis', 'scale', 'speed'], defaults: { scale: 1, speed: 0.2 } },
    { id: 8, name: 'Rings', params: ['scale', 'speed'], defaults: { scale: 1, speed: 0.5 } },
    { id: 9, name: 'Polar Rings', params: ['scale', 'speed'], defaults: { scale: 1, speed: 0.2 } },
    { id: 10, name: 'Polar Spiral', params: ['scale', 'speed'], defaults: { scale: 1, speed: 0.2 } },
    { id: 11, name: 'Polar Radial', params: ['scale', 'speed'], defaults: { scale: 1, speed: 0.3 } },
    { id: 12, name: 'Twinkle Fox', params: [], native: true },
    { id: 13, name: 'Fire 2012', params: ['scale', 'speed'], native: true },
    { id: 14, name: 'Cylon', params: ['speed'], native: true },
    { id: 15, name: 'Rainbow Glitter', params: ['speed'], native: true },
    { id: 16, name: 'Confetti', params: ['speed'], native: true },
    { id: 17, name: 'Sinelon', params: ['speed'], native: true },
    { id: 18, name: 'BPM', params: ['speed'], native: true }
  ],
  paramMeta: {
    colorA: { type: 'color', label: 'Color A', default: '#ff9b3d' },
    colorB: { type: 'color', label: 'Color B', default: '#3c78d2' },
    speed: { type: 'float', label: 'Speed', min: 0, max: 5, step: 0.05, default: 1 },
    scale: { type: 'float', label: 'Scale', min: 0.1, max: 10, step: 0.1, default: 1 },
    axis: { type: 'enum', label: 'Axis', options: ['vertical', 'horizontal', 'radial', 'angular'], default: 0 }
  }
};

function offlinePreset(name, effect, over) {
  var p = {
    name: name, effect: effect, brightness: 15, cycle: true,
    params: { colorA: '#ff9b3d', colorB: '#3c78d2', speed: 1, scale: 1, axis: 0 }
  };
  Object.assign(p.params, over || {});
  return p;
}

function loadOffline() {
  state.schema = OFFLINE_SCHEMA;
  state.presets = [
    offlinePreset('Polar Radial', 11, { speed: 0.3, axis: 2 }),
    offlinePreset('Rainbow Glitter', 15, {}),
    offlinePreset('Rings', 8, { speed: 0.5, axis: 2 }),
    offlinePreset('Polar Rings', 9, { speed: 0.2, axis: 2 }),
    offlinePreset('Polar Spiral', 10, { speed: 0.2, axis: 2 }),
    offlinePreset('Twinkle Fox', 12, {}),
    offlinePreset('Fire', 13, {}),
    offlinePreset('Cylon', 14, {}),
    offlinePreset('Rainbow', 7, { speed: 0.2, axis: 3 }),
    offlinePreset('Plasma', 4, { colorA: '#ff6432', colorB: '#3c78d2', speed: 0.4 }),
    offlinePreset('Sparkle', 3, { colorA: '#3c78d2' }),
    offlinePreset('Breathe', 6, { colorA: '#ff9b3d' })
  ];
  state.presets.forEach(function (p) { p.brightness = 40; });
  state.sel = 0;
  buildEffectOptions();
  renderPresets();
  renderControls();
}

// ---------------------------------------------------------------- UI
function effectName(id) {
  var e = (state.schema.effects || []).find(function (x) { return x.id === id; });
  return e ? e.name : 'effect ' + id;
}

function effectDef(id) {
  return (state.schema.effects || []).find(function (x) { return x.id === id; });
}

function buildEffectOptions() {
  var sel = document.getElementById('effectSel');
  sel.innerHTML = '';
  state.schema.effects.forEach(function (e) {
    var o = document.createElement('option');
    o.value = e.id;
    o.textContent = e.name + (e.native ? ' *' : '');
    sel.appendChild(o);
  });
}

function renderPresets() {
  var ul = document.getElementById('presetList');
  ul.innerHTML = '';
  state.presets.forEach(function (p, i) {
    var li = document.createElement('li');
    if (i === state.sel) li.className = 'sel';
    var name = document.createElement('span');
    name.className = 'pname';
    name.textContent = p.name;
    var eff = document.createElement('span');
    eff.className = 'peff';
    eff.textContent = effectName(p.effect);
    li.appendChild(name);
    li.appendChild(eff);
    if (p.cycle) {
      var chip = document.createElement('span');
      chip.className = 'chip';
      chip.textContent = '● button';
      li.appendChild(chip);
    }
    li.addEventListener('click', function () { selectPreset(i); });
    ul.appendChild(li);
  });
  document.getElementById('presetCount').textContent =
    state.presets.length + (state.connected ? '' : ' (offline)');
}

function selectPreset(i) {
  state.sel = i;
  sim.ts = 0;
  sim.frameState = {};
  sim.colors = LEDS.map(function () { return [0, 0, 0]; });
  renderPresets();
  renderControls();
  if (state.connected) post('/api/active', { id: i }).catch(function (e) { setStatus(e.message, 'err'); });
}

function markDirty() {
  state.dirty = true;
  document.getElementById('dirtyMark').textContent = 'unsaved changes';
  document.getElementById('dirtyMark').className = 'grow dirty';
}

function markClean() {
  state.dirty = false;
  document.getElementById('dirtyMark').textContent = state.connected ? 'in sync' : 'not connected';
  document.getElementById('dirtyMark').className = 'grow';
}

// Builds the parameter controls from the schema, so an effect gains a knob on
// the device without this file having to learn about it.
function renderControls() {
  var p = current();
  if (!p) return;

  document.getElementById('presetName').value = p.name;
  document.getElementById('presetCycle').checked = !!p.cycle;
  document.getElementById('effectSel').value = p.effect;
  document.getElementById('bright').max = state.maxBrightness;
  document.getElementById('bright').value = p.brightness;
  document.getElementById('brightVal').textContent = p.brightness + ' / ' + state.maxBrightness;

  var def = effectDef(p.effect) || { params: [] };
  document.getElementById('nativeNote').hidden = !def.native;

  var box = document.getElementById('paramBox');
  box.innerHTML = '';
  (def.params || []).forEach(function (key) {
    var meta = state.schema.paramMeta[key];
    if (!meta) return;
    var row = document.createElement('div');
    row.className = 'row';
    var label = document.createElement('label');
    label.textContent = meta.label || key;
    row.appendChild(label);

    if (meta.type === 'color') {
      var ci = document.createElement('input');
      ci.type = 'color';
      ci.value = p.params[key] || meta.default || '#ffffff';
      ci.addEventListener('input', function () { p.params[key] = ci.value; markDirty(); });
      row.appendChild(ci);
    } else if (meta.type === 'enum') {
      var se = document.createElement('select');
      se.className = 'grow';
      (meta.options || []).forEach(function (name, idx) {
        var o = document.createElement('option');
        o.value = idx;
        o.textContent = name;
        se.appendChild(o);
      });
      se.value = p.params[key];
      se.addEventListener('change', function () { p.params[key] = +se.value; markDirty(); });
      row.appendChild(se);
    } else {
      var rng = document.createElement('input');
      rng.type = 'range';
      rng.className = 'grow';
      rng.min = meta.min; rng.max = meta.max; rng.step = meta.step;
      rng.value = p.params[key];
      var num = document.createElement('input');
      num.type = 'number';
      num.className = 'numin';
      num.min = meta.min; num.max = meta.max; num.step = meta.step;
      num.value = p.params[key];
      function set(v) {
        v = clamp(+v, meta.min, meta.max);
        p.params[key] = v;
        rng.value = v;
        num.value = v;
        markDirty();
      }
      rng.addEventListener('input', function () { set(rng.value); });
      num.addEventListener('change', function () { set(num.value); });
      row.appendChild(rng);
      row.appendChild(num);
    }
    box.appendChild(row);
  });

  markClean();
}

function presetPayload(p) {
  return {
    name: p.name, effect: p.effect, brightness: p.brightness, cycle: !!p.cycle,
    params: {
      colorA: p.params.colorA, colorB: p.params.colorB,
      speed: p.params.speed, scale: p.params.scale, axis: p.params.axis
    }
  };
}

function save() {
  var p = current();
  if (!p) return;
  if (!state.connected) { setStatus('not connected — nothing to save to', 'err'); return; }
  post('/api/presets/' + state.sel, presetPayload(p), 'PUT')
    .then(function (list) {
      state.presets = list.presets;
      renderPresets();
      renderControls();
      setStatus('saved', 'on');
    })
    .catch(function (e) { setStatus(e.message, 'err'); });
}

function addPreset(copyCurrent) {
  var base = copyCurrent && current()
    ? presetPayload(current())
    : { name: 'New preset', effect: 7, brightness: 15, cycle: true,
        params: { colorA: '#ff9b3d', colorB: '#3c78d2', speed: 0.2, scale: 1, axis: 0 } };
  if (copyCurrent) base.name = (base.name + ' copy').slice(0, 23);

  if (!state.connected) {
    state.presets.push(JSON.parse(JSON.stringify(base)));
    selectPreset(state.presets.length - 1);
    return;
  }
  post('/api/presets', base)
    .then(function (list) {
      state.presets = list.presets;
      selectPreset(state.presets.length - 1);
    })
    .catch(function (e) { setStatus(e.message, 'err'); });
}

function deletePreset() {
  if (state.presets.length <= 1) { setStatus('the last preset cannot be deleted', 'err'); return; }
  var idx = state.sel;
  if (!state.connected) {
    state.presets.splice(idx, 1);
    selectPreset(Math.min(idx, state.presets.length - 1));
    return;
  }
  api('/api/presets/' + idx, { method: 'DELETE' })
    .then(function (list) {
      state.presets = list.presets;
      selectPreset(Math.min(idx, state.presets.length - 1));
    })
    .catch(function (e) { setStatus(e.message, 'err'); });
}

function exportAll() {
  if (!state.connected) { setStatus('connect to a device to export its state', 'err'); return; }
  fetch(state.url + '/api/backup')
    .then(function (r) { return r.text(); })
    .then(function (text) {
      var a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
      a.download = 'mini-lolly-backup-' + new Date().toISOString().slice(0, 10) + '.json';
      a.click();
      URL.revokeObjectURL(a.href);
      setStatus('exported ' + text.length + ' bytes', 'on');
    })
    .catch(function (e) { setStatus(e.message, 'err'); });
}

function importAll() {
  var file = document.getElementById('backupFile').files[0];
  if (!file) { setStatus('choose a backup file first', 'err'); return; }
  if (!state.connected) { setStatus('connect to a device to import into it', 'err'); return; }
  if (!confirm('Replace every preset on the device with the ones in this file?')) return;
  file.text()
    .then(function (text) {
      return fetch(state.url + '/api/restore', {
        method: 'POST', headers: { 'Content-Type': 'application/json' }, body: text
      });
    })
    .then(function (r) { return r.json().then(function (j) { if (!r.ok) throw new Error(j.error || r.status); return j; }); })
    .then(function (res) {
      setStatus('imported ' + res.presets + ' presets', 'on');
      return api('/api/presets');
    })
    .then(function (list) {
      state.presets = list.presets;
      selectPreset(0);
    })
    .catch(function (e) { setStatus(e.message, 'err'); });
}

// ---------------------------------------------------------------- wiring
document.getElementById('devConnect').addEventListener('click', function () { connect(false); });
document.getElementById('devUrl').addEventListener('keydown', function (e) {
  if (e.key === 'Enter') connect(false);
});
document.getElementById('presetNew').addEventListener('click', function () { addPreset(false); });
document.getElementById('presetDup').addEventListener('click', function () { addPreset(true); });
document.getElementById('presetDelete').addEventListener('click', deletePreset);
document.getElementById('presetName').addEventListener('input', function (e) {
  current().name = e.target.value.slice(0, 23);
  markDirty();
});
document.getElementById('presetCycle').addEventListener('change', function (e) {
  current().cycle = e.target.checked;
  markDirty();
});
document.getElementById('effectSel').addEventListener('change', function (e) {
  var p = current();
  var wasUsed = (effectDef(p.effect) || {}).params || [];
  p.effect = +e.target.value;

  // Parameters the previous effect did not expose were never editable, so
  // whatever they hold is stale — a preset that ignored colour carries black,
  // and switching it to Plasma would otherwise render nothing at all. Seed those
  // from the new effect's schema defaults; leave the ones already on screen alone.
  var def = effectDef(p.effect) || { params: [] };
  (def.params || []).forEach(function (key) {
    if (wasUsed.indexOf(key) >= 0) return;
    var v = (def.defaults || {})[key];
    if (v === undefined) v = (state.schema.paramMeta[key] || {}).default;
    if (v !== undefined) p.params[key] = v;
  });

  sim.ts = 0;
  sim.frameState = {};
  renderControls();
  markDirty();
});
// Brightness is the one control that pushes live: it is what you adjust while
// looking at the board, and waiting for a save defeats the point. PUT
// /api/brightness writes it into the active preset, so it is saved by the time
// the slider is let go — which is why this one does not mark the preset dirty.
document.getElementById('bright').addEventListener('input', function (e) {
  var v = +e.target.value;
  current().brightness = v;
  document.getElementById('brightVal').textContent = v + ' / ' + state.maxBrightness;
  if (!state.connected) markDirty();
});
document.getElementById('bright').addEventListener('change', function (e) {
  if (state.connected) {
    post('/api/brightness', { value: +e.target.value }, 'PUT')
      .catch(function (err) { setStatus(err.message, 'err'); });
  }
});
document.getElementById('save').addEventListener('click', save);
document.getElementById('backupExport').addEventListener('click', exportAll);
document.getElementById('backupImport').addEventListener('click', importAll);
document.getElementById('playpause').addEventListener('click', function (e) {
  sim.playing = !sim.playing;
  e.target.textContent = sim.playing ? 'Pause' : 'Play';
});
document.getElementById('showidx').addEventListener('change', function (e) {
  sim.showIndices = e.target.checked;
});
window.addEventListener('resize', resizeCanvas);

document.getElementById('ledCount').textContent = LED_COUNT;
resizeCanvas();
loadOffline();
// A page served by the device already knows where the device is. Anywhere else
// this quietly fails and the page stays a local simulator.
if (location.protocol.indexOf('http') === 0) {
  document.getElementById('devUrl').value = location.origin;
  connect(true);
}
requestAnimationFrame(renderFrame);
