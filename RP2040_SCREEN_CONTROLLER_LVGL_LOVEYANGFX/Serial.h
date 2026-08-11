#ifndef __SERIAL_H__
#define __SERIAL_H__

// #define SERIAL_FRAMING_COBS  // must match DCO/Input; host: dco_control --cobs

// Preset-scroll 'q' is [preset#][16 chars] = 17. DCO/Input default is 8.
#define SERIAL_INNER_MAX_PAYLOAD 17

#include <pico/mutex.h>

#include "sram_hot.h"
#include "screen_mode.h"
#include "serial_input_protocol.h"
#include "serial_frame.h"
#include "serial_parser.h"
#include "serial_param_protocol.h"

void serial_read_n();
void init_screen_serial();

// ---------------------------------------------------------------------------
// Cross-core lock guarding the Core0 (serial parser) -> Core1 (LVGL) shared
// state below. Placed in the pico-sdk .mutex_array section so the runtime
// initializes it before either core starts executing sketch code.
//
// Rules:
//   - Core0 handlers publish related fields + their flag inside one lock.
//   - Core1 snapshots fields / consumes flags inside one lock, then calls
//     LVGL with the lock RELEASED (never hold the lock across lv_* calls).
// ---------------------------------------------------------------------------
extern mutex_t screenStateMutex;

static inline void screen_state_lock()   { mutex_enter_blocking(&screenStateMutex); }
static inline void screen_state_unlock() { mutex_exit(&screenStateMutex); }

extern volatile byte    presetNumber;

extern volatile bool    presetScrollFlag;

extern volatile byte    paramNumber;
extern volatile int32_t paramValue;
// Always points at a string literal (set in setDisplayParam), so Core1 can
// safely dereference a snapshotted pointer without heap/String races.
extern const char* volatile paramName;

extern volatile bool    paramChangeFlag;

extern volatile bool    updateADSR1Flag;
extern volatile bool    updateADSR2Flag;

extern volatile bool    signalFlag;
extern volatile byte    serialSignal;

extern volatile bool    presetCharFlag;
extern volatile byte    presetChar;

// Bitmask of pending level-bar updates (LEVEL_BAR_* in displayParams.h).
extern volatile byte    levelBarFlag;

// +1 for null terminator so LVGL/string APIs see a clean C-string.
extern volatile char    presetNameBytes[17];     // 16-char names + '\0'

// Copy the preset name into a local buffer. Caller must hold screen_state_lock().
static inline void snapshot_preset_name(char out[17]) {
  for (int i = 0; i < 16; ++i) {
    out[i] = presetNameBytes[i];
  }
  out[16] = '\0';
}
#endif

/*
SIGNAL LIST: see ScreenMode in screen_mode.h (values 1..8 on the wire).
*/
