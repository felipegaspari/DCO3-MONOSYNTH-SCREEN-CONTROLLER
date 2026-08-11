#ifndef __SCREEN_TARGET_H__
#define __SCREEN_TARGET_H__

#include <stdint.h>

// Which synth this screen is attached to, as far as the calibration UI cares.
//
// The two boards lay the manual-calibration screen out differently: the
// monosynth walks 3 oscillators through 2 waveform stages each, while the 4x2
// voice board walks 8 oscillators through 1 stage each and repurposes the
// WAVEFORM label to show which DCO chip is being trimmed. Everything the UI
// needs to know about that difference is derived from this enum, so no source
// file outside this header carries a per-project #if.
enum class CalTopology : uint8_t {
  Monosynth3Osc,  // 3 osc x 2 stages, stage 0..5, WAVEFORM = SAW/TRI/SQR
  Voices4x2       // 8 osc x 1 stage,  stage 0..7, WAVEFORM = DCO chip A/B
};

// Compile-time selection, overridable with -DSCREEN_CAL_TOPOLOGY_DEFAULT=...
// Phase 2 replaces the accessor body with the value the synth announces over
// the serial link, at which point this becomes a pre-announcement fallback and
// the same binary serves both projects.
#ifndef SCREEN_CAL_TOPOLOGY_DEFAULT
#define SCREEN_CAL_TOPOLOGY_DEFAULT CalTopology::Monosynth3Osc
#endif

// The one seam. Every caller goes through this.
inline CalTopology screen_cal_topology() {
  return SCREEN_CAL_TOPOLOGY_DEFAULT;
}

// Oscillator count is what the synth can report about itself; 3 is the only
// count that implies the two-stages-per-oscillator monosynth layout.
constexpr CalTopology screen_topology_from_osc_count(uint8_t oscCount) {
  return (oscCount <= 3) ? CalTopology::Monosynth3Osc : CalTopology::Voices4x2;
}

// Highest stage index the Input can send.
inline uint8_t screen_cal_stage_max(CalTopology topology) {
  return (topology == CalTopology::Monosynth3Osc) ? 5 : 7;
}

// Stage -> oscillator number shown in ui_oscillatorN.
inline uint8_t screen_cal_stage_to_osc(CalTopology topology, uint8_t stage) {
  return (topology == CalTopology::Monosynth3Osc) ? (uint8_t)(stage / 2) : stage;
}

// Text for ui_waveform / ui_waveformShadow.
inline const char* screen_cal_stage_label(CalTopology topology, uint8_t stage) {
  if (topology == CalTopology::Voices4x2) {
    return ((stage & 1) == 0) ? "A" : "B";
  }
  // Monosynth: even stages are SAW; odd stages 1 and 5 are TRI (OSC1/OSC3),
  // odd stage 3 is SQR (OSC2), matching the Input/DCO manual-cal conventions.
  if ((stage % 2) == 0) {
    return "SAW";
  }
  return (stage == 1 || stage == 5) ? "TRI" : "SQR";
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
