#ifndef __SERIAL_H__
#define __SERIAL_H__

// #define SERIAL_FRAMING_COBS  // must match DCO/Input; host: dco_control --cobs

#include <pico/mutex.h>

#include "project_config.h"
#include "sram_hot.h"
#include "screen_mode.h"

#define DCO_PROTOCOL_IMPLEMENT_DMA // Only define this in ONE .cpp/.ino file!

#include "_build_libs/DCO-PROTOCOL/serial_param_protocol.h"
#include "_build_libs/DCO-PROTOCOL/serial_input_protocol.h"
#include "_build_libs/DCO-PROTOCOL/serial_frame.h"
#include "_build_libs/DCO-PROTOCOL/serial_parser.h"

// NEW: The shared DMA library replaces serial_dma.h!
#include "_build_libs/DCO-PROTOCOL/serial_dma_tx.h"

// Peer UARTs — do not infer the peer from the port number.
//
// DCO3: Input only on Serial1 GP13.
// DCO4 board traces: Mainboard PA9 TX → GP21, PA10 RX → GP20 (Serial2 / UART1).
//           Input still drives GP13 on Serial1; drain both.
#if PROJECT_INSTRUMENT == 4
#define SCREEN_HAS_MB_PEER    1
#define SCREEN_HAS_INPUT_PEER 1
#define SCREEN_MB_PORT        Serial2
#define SCREEN_MB_RX_PIN      21
#define SCREEN_MB_TX_PIN      20
#define SCREEN_INPUT_PORT     Serial1
#define SCREEN_INPUT_RX_PIN   13
#define SCREEN_INPUT_TX_PIN   12
#else
#define SCREEN_HAS_MB_PEER    0
#define SCREEN_HAS_INPUT_PEER 1
#define SCREEN_INPUT_PORT     Serial1
#define SCREEN_INPUT_RX_PIN   13
#define SCREEN_INPUT_TX_PIN   12
#endif

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
