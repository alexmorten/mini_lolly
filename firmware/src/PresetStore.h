#pragma once
#include "effects.h"
#include "Config.h"
#include <stdint.h>

enum PresetFlags : uint8_t {
  // Reachable by the hardware buttons. Presets without it are website-only.
  PRESET_IN_CYCLE = 1 << 0,
};

struct Preset {
  char     name[24];
  uint8_t  effect;
  uint8_t  flags;      // PresetFlags
  EffectParams params;
  uint8_t  brightness;
};

// Presets live in NVS rather than on a filesystem: this board's partition table
// (partitions-4MB-tinyuf2.csv) has no LittleFS partition, only the TinyUF2 FAT
// drive, and with no painted bases to store the whole state is under 2 KB.
class PresetStore {
public:
  static constexpr uint8_t MAX_PRESETS = 32;

  static bool begin();
  static uint8_t presetCount();
  static const Preset* presets();
  static uint8_t activeIndex();
  static void setActiveIndex(uint8_t idx);

  static bool savePresets();
  static bool addPreset(const Preset& p);
  static bool updatePreset(uint8_t idx, const Preset& p);
  static bool deletePreset(uint8_t idx);
  static bool reorderPresets(const uint8_t* order, uint8_t count);

  // Wholesale replacement, for POST /api/restore. The list is rebuilt in place
  // and written once at the end rather than once per preset — and the caller is
  // expected to have checked the payload first, because beginImport() drops what
  // is already there.
  static void beginImport();
  static bool importPreset(const Preset& p);
  static bool endImport(uint8_t activeIdx);

  static void nextPreset();
  static const Preset& activePreset();

  static void loadDefaults();

private:
  static Preset s_presets[MAX_PRESETS];
  static uint8_t s_presetCount;
  static uint8_t s_activeIndex;

  static uint8_t defaultPresetIndex();
};
