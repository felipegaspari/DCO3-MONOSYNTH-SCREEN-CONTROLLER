#ifndef SCREEN_MODE_H
#define SCREEN_MODE_H

#include <stdint.h>

// High-level screen modes carried in serialSignal ('s' frames from Input).
// Keep the numeric values to preserve the existing protocol.
enum class ScreenMode : uint8_t {
  PresetScroll      = 1,  // LOAD (PRESET SCROLL)
  LoadSaveExit      = 2,  // LOAD/SAVE EXIT
  SaveSelectPreset  = 3,  // SAVE MODE - Select destination preset
  SaveSetName       = 4,  // SAVE MODE - set preset name
  SaveCompleted     = 5,  // PRESET SAVED
  Silent            = 6,  // SCREEN SILENCE
  CalibrationMenu   = 7,  // CALIBRATION MENU
  ManualCalibration = 8   // MANUAL CALIBRATION
};

// Protocol byte for a ScreenMode (what goes into serialSignal).
static constexpr uint8_t screen_mode_raw(ScreenMode m) {
  return static_cast<uint8_t>(m);
}

#endif  // SCREEN_MODE_H
