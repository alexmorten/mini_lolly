#include "PresetStore.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

static Preferences prefs;
static const char* NVS_NAMESPACE = "lolly";
static const char* KEY_PRESETS = "presets";
static const char* KEY_ACTIVE = "active";

// The blob is a raw dump of the Preset struct, so any change to that struct
// silently reinterprets the old bytes — garbage names, colors and effect ids with
// nothing to notice it. The header makes a layout change detectable: a blob that
// does not match exactly is dropped and the built-in defaults take over.
static const uint32_t PRESETS_MAGIC   = 0x4C505354;  // "LPST"
static const uint8_t  PRESETS_VERSION = 2;

struct PresetsHeader {
  uint32_t magic;
  uint8_t  version;
  uint8_t  recordSize;
  uint8_t  count;
};

Preset PresetStore::s_presets[MAX_PRESETS];
uint8_t PresetStore::s_presetCount = 0;
uint8_t PresetStore::s_activeIndex = 0;

// Normalizes a record from anywhere it can enter: NVS is trusted for nothing but
// its layout, and the API is a client that can ask for a nonsensical combination.
static void sanitize(Preset& p) {
  p.name[sizeof(p.name) - 1] = 0;
  if (!p.name[0]) strlcpy(p.name, "Preset", sizeof(p.name));
  if (p.effect >= EFFECT_COUNT) p.effect = EFFECT_SOLID;
  if (p.params.axis >= AXIS_COUNT) p.params.axis = AXIS_VERTICAL;
  if (p.brightness > MAX_BRIGHTNESS) p.brightness = MAX_BRIGHTNESS;
  if (p.brightness == 0) p.brightness = 1;
  // A speed of zero freezes an animation, which is a legitimate thing to ask
  // for; a negative one runs it backwards, which is not what any control offers.
  if (p.params.speed < 0) p.params.speed = 0;
  if (p.params.scale <= 0) p.params.scale = ffromf(0.1f);
}

uint8_t PresetStore::defaultPresetIndex() {
  for (uint8_t i = 0; i < s_presetCount; i++) {
    if (strcmp(s_presets[i].name, DEFAULT_PRESET_NAME) == 0) return i;
  }
  return 0;
}

void PresetStore::loadDefaults() {
  s_presetCount = 0;

  auto add = [&](const char* name, uint8_t effect, uint32_t ca, uint32_t cb,
                 float speed, float scale, uint8_t axis, bool cycle = true) {
    if (s_presetCount >= MAX_PRESETS) return;
    Preset& p = s_presets[s_presetCount++];
    memset(&p, 0, sizeof(p));
    strncpy(p.name, name, sizeof(p.name) - 1);
    p.effect = effect;
    p.flags = cycle ? PRESET_IN_CYCLE : 0;
    effectParamsDefaults(p.params);
    // A preset whose effect ignores colour still carries one, so switching it to
    // an effect that uses colour from the website does not land on black.
    if (ca) p.params.colorA = ca;
    if (cb) p.params.colorB = cb;
    p.params.speed = ffromf(speed);
    p.params.scale = ffromf(scale);
    p.params.axis = axis;
    p.brightness = BRIGHTNESS;
  };

  // The list the buttons used to walk, in the order they walked it.
  add("Polar Radial", EFFECT_POLAR_RADIAL, 0, 0, 0.3f, 1.0f, AXIS_RADIAL);
  add("Rainbow Glitter", EFFECT_RAINBOW_GLITTER, 0, 0, 1.0f, 1.0f, AXIS_VERTICAL);
  add("Rings", EFFECT_RINGS, 0, 0, 0.5f, 1.0f, AXIS_RADIAL);
  add("Polar Rings", EFFECT_POLAR_RINGS, 0, 0, 0.2f, 1.0f, AXIS_RADIAL);
  add("Polar Spiral", EFFECT_POLAR_SPIRAL, 0, 0, 0.2f, 1.0f, AXIS_RADIAL);

  // Then the ones the old firmware had but kept commented out, plus a few of the
  // parametric effects to show what the website can now edit.
  add("Twinkle Fox", EFFECT_TWINKLE_FOX, 0, 0, 1.0f, 1.0f, AXIS_VERTICAL);
  add("Fire", EFFECT_FIRE2012, 0, 0, 1.0f, 1.0f, AXIS_VERTICAL);
  add("Cylon", EFFECT_CYLON, 0, 0, 1.0f, 1.0f, AXIS_VERTICAL);
  add("Rainbow", EFFECT_RAINBOW, 0, 0, 0.2f, 1.0f, AXIS_ANGULAR);
  add("Plasma", EFFECT_PLASMA, rgb8(255, 100, 50), rgb8(60, 120, 210), 0.4f, 1.0f, AXIS_VERTICAL);
  add("Sparkle", EFFECT_SPARKLE, rgb8(60, 120, 210), 0, 1.0f, 1.0f, AXIS_VERTICAL);
  add("Breathe", EFFECT_BREATHE, rgb8(255, 155, 61), 0, 1.0f, 1.0f, AXIS_VERTICAL);

  // Off the cycle: swatches for checking that COLOR_ORDER is right, which is the
  // one thing a spiral of animated colour makes impossible to eyeball.
  add("Test red", EFFECT_SOLID, rgb8(255, 0, 0), 0, 1.0f, 1.0f, AXIS_VERTICAL, false);
  add("Test green", EFFECT_SOLID, rgb8(0, 255, 0), 0, 1.0f, 1.0f, AXIS_VERTICAL, false);
  add("Test blue", EFFECT_SOLID, rgb8(0, 0, 255), 0, 1.0f, 1.0f, AXIS_VERTICAL, false);

  for (uint8_t i = 0; i < s_presetCount; i++) sanitize(s_presets[i]);
  s_activeIndex = defaultPresetIndex();
}

bool PresetStore::begin() {
  // Dropped up front so a blob that fails its header check falls through to the
  // defaults instead of leaving whatever happened to be in memory in place.
  s_presetCount = 0;

  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("NVS open failed, using defaults");
    loadDefaults();
    return false;
  }

  size_t len = prefs.getBytesLength(KEY_PRESETS);
  if (len >= sizeof(PresetsHeader) && len <= sizeof(PresetsHeader) + sizeof(s_presets)) {
    uint8_t buf[sizeof(PresetsHeader) + sizeof(s_presets)];
    size_t got = prefs.getBytes(KEY_PRESETS, buf, len);
    PresetsHeader h;
    memcpy(&h, buf, sizeof(h));
    if (got == len && h.magic == PRESETS_MAGIC && h.version == PRESETS_VERSION &&
        h.recordSize == sizeof(Preset) && h.count <= MAX_PRESETS &&
        len == sizeof(PresetsHeader) + (size_t)h.count * sizeof(Preset)) {
      memcpy(s_presets, buf + sizeof(PresetsHeader), (size_t)h.count * sizeof(Preset));
      s_presetCount = h.count;
      for (uint8_t i = 0; i < s_presetCount; i++) sanitize(s_presets[i]);
    } else {
      Serial.println("presets blob: unrecognised format, reloading defaults");
    }
  }

  if (s_presetCount == 0) {
    // The stored active index belongs to the list that was just dropped, so it
    // points at whatever now happens to sit at that slot. Start from the default
    // preset instead, and write it back so the stale value is gone for good.
    loadDefaults();
    savePresets();
    prefs.putUChar(KEY_ACTIVE, s_activeIndex);
    return true;
  }

  s_activeIndex = prefs.getUChar(KEY_ACTIVE, defaultPresetIndex());
  if (s_activeIndex >= s_presetCount) s_activeIndex = 0;
  return true;
}

uint8_t PresetStore::presetCount() { return s_presetCount; }
const Preset* PresetStore::presets() { return s_presets; }
uint8_t PresetStore::activeIndex() { return s_activeIndex; }

void PresetStore::setActiveIndex(uint8_t idx) {
  if (idx >= s_presetCount) return;
  s_activeIndex = idx;
  prefs.putUChar(KEY_ACTIVE, idx);
}

void PresetStore::nextPreset() {
  if (s_presetCount == 0) return;
  for (uint8_t k = 1; k <= s_presetCount; k++) {
    uint8_t idx = (s_activeIndex + k) % s_presetCount;
    if (s_presets[idx].flags & PRESET_IN_CYCLE) {
      // k == s_presetCount means the only preset on the buttons is the one already
      // showing, so the press is deliberately a no-op rather than a reselect.
      if (idx != s_activeIndex) setActiveIndex(idx);
      return;
    }
  }
  // Nothing is on the buttons at all. Walking the full list is better than letting
  // the only physical control on the lolly turn into a dead key.
  setActiveIndex((s_activeIndex + 1) % s_presetCount);
}

const Preset& PresetStore::activePreset() {
  return s_presets[s_activeIndex];
}

bool PresetStore::savePresets() {
  uint8_t buf[sizeof(PresetsHeader) + sizeof(s_presets)];
  PresetsHeader h = {PRESETS_MAGIC, PRESETS_VERSION, (uint8_t)sizeof(Preset), s_presetCount};
  memcpy(buf, &h, sizeof(h));
  size_t len = sizeof(PresetsHeader) + (size_t)s_presetCount * sizeof(Preset);
  memcpy(buf + sizeof(PresetsHeader), s_presets, (size_t)s_presetCount * sizeof(Preset));
  return prefs.putBytes(KEY_PRESETS, buf, len) == len;
}

bool PresetStore::addPreset(const Preset& p) {
  if (s_presetCount >= MAX_PRESETS) return false;
  s_presets[s_presetCount] = p;
  sanitize(s_presets[s_presetCount]);
  s_presetCount++;
  return savePresets();
}

bool PresetStore::updatePreset(uint8_t idx, const Preset& p) {
  if (idx >= s_presetCount) return false;
  s_presets[idx] = p;
  sanitize(s_presets[idx]);
  return savePresets();
}

bool PresetStore::deletePreset(uint8_t idx) {
  if (idx >= s_presetCount || s_presetCount <= 1) return false;
  for (uint8_t i = idx; i + 1 < s_presetCount; i++) {
    s_presets[i] = s_presets[i + 1];
  }
  s_presetCount--;
  if (s_activeIndex >= s_presetCount) s_activeIndex = s_presetCount - 1;
  prefs.putUChar(KEY_ACTIVE, s_activeIndex);
  return savePresets();
}

bool PresetStore::reorderPresets(const uint8_t* order, uint8_t count) {
  if (count != s_presetCount) return false;
  Preset tmp[MAX_PRESETS];
  for (uint8_t i = 0; i < count; i++) {
    if (order[i] >= s_presetCount) return false;
    tmp[i] = s_presets[order[i]];
  }
  memcpy(s_presets, tmp, sizeof(Preset) * count);
  return savePresets();
}

void PresetStore::beginImport() {
  s_presetCount = 0;
}

bool PresetStore::importPreset(const Preset& p) {
  if (s_presetCount >= MAX_PRESETS) return false;
  s_presets[s_presetCount] = p;
  sanitize(s_presets[s_presetCount]);
  s_presetCount++;
  return true;
}

bool PresetStore::endImport(uint8_t activeIdx) {
  // A device with no presets has nothing to render and no way back from the
  // buttons, so an import that ended up empty falls back to the built-ins rather
  // than leaving that state in NVS.
  if (s_presetCount == 0) loadDefaults();
  s_activeIndex = activeIdx < s_presetCount ? activeIdx : 0;
  prefs.putUChar(KEY_ACTIVE, s_activeIndex);
  return savePresets();
}
