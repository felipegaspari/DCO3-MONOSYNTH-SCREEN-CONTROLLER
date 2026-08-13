#ifndef __SCREEN_TARGET_H__
#define __SCREEN_TARGET_H__

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "params_def.h"

// Which synth this screen is attached to, as far as the calibration UI cares.
//
// DCO3: 3 osc × saw/pulse/440 (stage 0..8). DCO4: packed A4+B3 per voice
// (saw/tri/pulse-PW/440 and saw/pulse/440, stage 0..27). ADSR3 labels also
// differ. WAVEFORM comes from cal_stage_kind_n.
enum class CalTopology : uint8_t {
  Monosynth3Osc,  // 3 osc x 3 substages, stage 0..8
  Voices4x2       // packed A4+B3, stage 0..27
};

// Pre-announcement fallback only, overridable with -DSCREEN_CAL_TOPOLOGY_DEFAULT=...
// The Input announces the real topology over 'y' (PARAM_UI_VOICE_TOPOLOGY,
// see Serial.ino) at boot and at manual-calibration entry, so this value is
// only live for the brief window before that first message arrives — or for as
// long as a screen is powered up with no Input attached, on the bench.
//
// project_config.h is a symlink to the superproject root: the same committed
// file in both trees, resolving to whichever instrument this checkout belongs
// to. It is what keeps that window showing the right layout without a build
// flag. Absent (a standalone clone), the monosynth layout is assumed, which is
// harmless here because the Input overrides it a moment later either way.
#if __has_include("project_config.h")
#include "project_config.h"
#endif

#ifndef SCREEN_CAL_TOPOLOGY_DEFAULT
#  if defined(PROJECT_INSTRUMENT) && PROJECT_INSTRUMENT == 4
#    define SCREEN_CAL_TOPOLOGY_DEFAULT CalTopology::Voices4x2
#  else
#    define SCREEN_CAL_TOPOLOGY_DEFAULT CalTopology::Monosynth3Osc
#  endif
#endif

// MCU module GP23/24 (same DCO_MCU_BOARD as the DCO). Live TFT/UART pins do
// not use 23/24. Pico/Pico 2: SMPS PS high. WeAct KEY returns to preset-scroll.
static constexpr uint8_t MCU_PIN_UNASSIGNED = 0xFF;
#if defined(DCO_MCU_BOARD) && DCO_MCU_BOARD == DCO_MCU_WEACT_RP2040
static constexpr uint8_t SMPS_PS_PIN = MCU_PIN_UNASSIGNED;
static constexpr uint8_t USER_KEY_PIN = 23;
#elif defined(DCO_MCU_BOARD) && ((DCO_MCU_BOARD == DCO_MCU_PICO) || (DCO_MCU_BOARD == DCO_MCU_PICO2))
static constexpr uint8_t SMPS_PS_PIN = 23;
static constexpr uint8_t USER_KEY_PIN = MCU_PIN_UNASSIGNED;
#elif defined(DCO_MCU_BOARD)
#error "DCO_MCU_BOARD must be DCO_MCU_WEACT_RP2040, DCO_MCU_PICO, or DCO_MCU_PICO2"
#else
static constexpr uint8_t SMPS_PS_PIN = MCU_PIN_UNASSIGNED;
static constexpr uint8_t USER_KEY_PIN = MCU_PIN_UNASSIGNED;
#endif

// Definition lives in displayParams.ino; guarded by screen_state_lock() like
// every other cross-core field (Serial.h). Snapshot it under the lock before
// calling screen_cal_topology() from Core1 code that isn't already holding
// the lock (see drawManualCalibration).
extern volatile CalTopology screenCalTopology;

// The one seam. Every caller goes through this.
inline CalTopology screen_cal_topology() {
  return screenCalTopology;
}

// Oscillator count is what the synth can report about itself; 3 is the only
// count that implies the three-oscillator monosynth layout.
constexpr CalTopology screen_topology_from_osc_count(uint8_t oscCount) {
  return (oscCount <= 3) ? CalTopology::Monosynth3Osc : CalTopology::Voices4x2;
}

constexpr uint8_t screen_cal_nosc(CalTopology topology) {
  return (topology == CalTopology::Monosynth3Osc) ? 3 : 8;
}

// Highest stage index the Input can send.
inline uint8_t screen_cal_stage_max(CalTopology topology) {
  return cal_stage_max_n(screen_cal_nosc(topology));
}

// Stage -> 0-based oscillator index (DCO4 0..7, DCO3 0..2).
inline uint8_t screen_cal_stage_to_osc(CalTopology topology, uint8_t stage) {
  return cal_stage_to_osc_n(stage, screen_cal_nosc(topology));
}

// Text for ui_waveform / ui_waveformShadow.
inline const char* screen_cal_stage_label(CalTopology topology, uint8_t stage) {
  switch (cal_stage_kind_n(stage, screen_cal_nosc(topology))) {
    case CAL_KIND_TRI:      return "TRI";
    case CAL_KIND_PULSE:    return "PULSE";
    case CAL_KIND_PULSE_PW: return "PULSE";
    case CAL_KIND_440:      return "440";
    default:                return "SAW";
  }
}

// DCO4 voice/chip ("0A" / "0B" / "1A" …); DCO3 1-based OSC1–3 ("1" / "2" / "3").
inline void screen_cal_format_osc(CalTopology topology, uint8_t osc, char* buf, size_t n) {
  if (n == 0) return;
  if (topology == CalTopology::Voices4x2) {
    snprintf(buf, n, "%u%c", (unsigned)(osc / 2u), (osc % 2u) ? 'B' : 'A');
  } else {
    snprintf(buf, n, "%u", (unsigned)(osc + 1u));
  }
}

// Stage toast: " OSC 0A PULSE" / DCO3 " OSC 1 SAW". Not the raw stage index.
inline void screen_cal_format_toast(CalTopology topology, uint8_t stage, char* buf, size_t n) {
  char oscBuf[4];
  screen_cal_format_osc(topology, screen_cal_stage_to_osc(topology, stage), oscBuf, sizeof(oscBuf));
  snprintf(buf, n, " OSC %s %s", oscBuf, screen_cal_stage_label(topology, stage));
}

// Toast text for PARAM_ADSR3_TO_OSC_SELECT. The same wire value means
// different things per topology: the monosynth selects among 3 oscillators,
// the 4x2 board selects among its two DCO chips.
inline const char* screen_adsr3_osc_select_label(CalTopology topology, int32_t value) {
  if (topology == CalTopology::Voices4x2) {
    switch (value) {
      case 0:  return " ADSR3 TO A";
      case 1:  return " ADSR3 TO B";
      case 2:  return " ADSR3 TO A+B";
      default: return " ADSR3 TO OSC";
    }
  }
  switch (value) {
    case 0:  return " ADSR3 TO OSC1";
    case 1:  return " ADSR3 TO OSC2";
    case 2:  return " ADSR3 TO BOTH";
    case 3:  return " ADSR3 TO OSC3";
    case 4:  return " ADSR3 TO ALL";
    default: return " ADSR3 TO OSC";
  }
}

#endif
