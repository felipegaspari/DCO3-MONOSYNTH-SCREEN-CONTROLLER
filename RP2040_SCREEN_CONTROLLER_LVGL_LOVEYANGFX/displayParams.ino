#include <lvgl.h>
#include "src/ui/ui.h"
#include "params_def.h"
#include "param_router.h"
#include "sram_hot.h"
#include "displayParams.h"
#include "display_u8g2.h"

// Definitions for display/model shared state
uint16_t paramHideTimeMillis = 3000;
bool     paramChangeTimerFlag = false;

volatile int8_t   offset = 0;
volatile uint16_t ampComp440Display = 0;
volatile uint16_t calPwCenterDisplay = 0;
volatile uint8_t  manualCalibrationOSCN = 0;
volatile uint8_t  manualCalibrationStage = 0;
volatile int32_t  calibrationGap = 0;
volatile uint8_t  calibrationMenuIndex = 0;

volatile CalTopology screenCalTopology = SCREEN_CAL_TOPOLOGY_DEFAULT;

volatile uint8_t OSC1Level = 0;
volatile uint8_t OSC2Level = 0;
volatile uint8_t OSC3Level = 0;
volatile uint8_t SUBLevel = 0;

volatile uint16_t ADSR1Attack = 0;
volatile uint16_t ADSR1Decay = 0;
volatile uint16_t ADSR1Sustain = 0;
volatile uint16_t ADSR1Release = 0;

volatile uint16_t ADSR2Attack = 0;
volatile uint16_t ADSR2Decay = 0;
volatile uint16_t ADSR2Sustain = 0;
volatile uint16_t ADSR2Release = 0;

// --- Global Patch State Cache (Resident in Screen RAM) ---
PatchOscBlock currentOscState;
PatchLfoBlock currentLfoState;
PatchModBlock currentModState;
PatchMixBlock currentMixState;

using ScreenParamValueT = int32_t;
using ScreenParamDescriptor = ParamDescriptorT<ScreenParamValueT>;

// Definitions for inspector shared state
uint16_t inspectorTimeoutMillis = 1800;  // 1.8s hold time
volatile InspectorType activeInspector = InspectorType::None;
volatile uint32_t inspectorLastActivityMillis = 0;
volatile bool inspectorActiveFlag = false;

// ---------------------------------------------------------------------------
// 4-Sample Moving Average Filter
// ---------------------------------------------------------------------------
static int32_t gapSamples[4]  = {0, 0, 0, 0};
static uint8_t gapSampleIdx   = 0;
static uint8_t gapSampleCount = 0;

static void reset_gap_filter() {
  gapSampleCount = 0;
  gapSampleIdx   = 0;
}

// ---------------------------------------------------------------------------
// TFT LVGL Manual Calibration Gap Tracking Bar
// ---------------------------------------------------------------------------
lv_obj_t* ui_calGapTrack  = nullptr;
static lv_obj_t* ui_calGapCenter = nullptr;
static lv_obj_t* ui_calGapCursor = nullptr;

static void init_tft_cal_gap_bar(lv_obj_t* screenParent) {
  if (ui_calGapTrack != nullptr || screenParent == nullptr) return;

  ui_calGapTrack = lv_obj_create(screenParent);
  lv_obj_remove_style_all(ui_calGapTrack);
  lv_obj_set_size(ui_calGapTrack, 480, 54);
  lv_obj_align(ui_calGapTrack, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(ui_calGapTrack, lv_color_hex(0x101010), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_calGapTrack, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_border_color(ui_calGapTrack, lv_color_hex(0xFF7700), LV_PART_MAIN);
  lv_obj_set_style_border_width(ui_calGapTrack, 2, LV_PART_MAIN);
  lv_obj_set_style_border_side(ui_calGapTrack, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_calGapTrack, 0, LV_PART_MAIN);
  lv_obj_clear_flag(ui_calGapTrack, LV_OBJ_FLAG_SCROLLABLE);

  ui_calGapCenter = lv_obj_create(ui_calGapTrack);
  lv_obj_remove_style_all(ui_calGapCenter);
  lv_obj_set_size(ui_calGapCenter, 3, 48);
  lv_obj_align(ui_calGapCenter, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(ui_calGapCenter, lv_color_hex(0xFF7700), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_calGapCenter, LV_OPA_COVER, LV_PART_MAIN);

  ui_calGapCursor = lv_obj_create(ui_calGapTrack);
  lv_obj_remove_style_all(ui_calGapCursor);
  lv_obj_set_size(ui_calGapCursor, 12, 44);
  lv_obj_align(ui_calGapCursor, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(ui_calGapCursor, lv_color_hex(0xFF3344), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_calGapCursor, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_calGapCursor, 3, LV_PART_MAIN);
}

// Lock-Free Calibration UI Draw (Runs on Core 1)
void drawManualCalibration(const Core1Snapshot &snap) {
  char str[8];
  char strLong[12];

  const uint8_t nOsc = screen_cal_nosc(snap.calTopology);
  const bool is440   = cal_stage_is_440_n(snap.calStage, nOsc);

  if (is440) {
    itoa((int)snap.calAmp440, str, 10);
  } else if (cal_stage_is_pw_edit_n(snap.calStage, nOsc)) {
    itoa((int)snap.calPwCenter, str, 10);
  } else {
    itoa(snap.calOffset, str, 10);
  }
  lv_label_set_text(ui_calibrationOffset, str);
  lv_label_set_text(ui_calibrationOffsetShadow, str);

  char oscLabel[4];
  screen_cal_format_osc(snap.calTopology, snap.calOscN, oscLabel, sizeof(oscLabel));
  lv_label_set_text(ui_oscillatorN, oscLabel);
  lv_label_set_text(ui_oscillatorNShadow, oscLabel);

  ltoa(snap.calGap, strLong, 10);
  lv_label_set_text(ui_calibrationGap, strLong);
  lv_label_set_text(ui_calibrationGapShadow, strLong);

  const char* waveformText = screen_cal_stage_label(snap.calTopology, snap.calStage);
  lv_label_set_text(ui_waveform, waveformText);
  lv_label_set_text(ui_waveformShadow, waveformText);

  if (ui_calGapTrack == nullptr) {
    init_tft_cal_gap_bar(ui_MANUALCALIBRATION);
  }

  if (ui_calGapTrack != nullptr) {
    lv_obj_remove_flag(ui_calGapTrack, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui_calGapCursor != nullptr) {
    const int32_t limit = is440 ? 100 : 400;

    int32_t clampedGap = snap.calGap;
    if (clampedGap < -limit) clampedGap = -limit;
    if (clampedGap > limit)  clampedGap = limit;

    const int32_t maxTravel = 228;
    int32_t xOffset = (clampedGap * maxTravel) / limit;

    lv_obj_align(ui_calGapCursor, LV_ALIGN_CENTER, xOffset, 0);

    int32_t absGap = (clampedGap < 0) ? -clampedGap : clampedGap;
    if (absGap <= 2) {
      lv_obj_set_style_bg_color(ui_calGapCursor, lv_color_hex(0x00FF88), LV_PART_MAIN);
    } else if (absGap < (limit / 4)) {
      lv_obj_set_style_bg_color(ui_calGapCursor, lv_color_hex(0xFFCC00), LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(ui_calGapCursor, lv_color_hex(0xFF3344), LV_PART_MAIN);
    }
  }
}

void apply_param_osc1_level(int32_t v) {
  if (OSC1Level != (uint8_t)v) {
    OSC1Level = (uint8_t)v;
    levelBarFlag |= LEVEL_BAR_OSC1;
  }
}

void apply_param_osc2_level(int32_t v) {
  if (OSC2Level != (uint8_t)v) {
    OSC2Level = (uint8_t)v;
    levelBarFlag |= LEVEL_BAR_OSC2;
  }
}

void apply_param_osc3_level(int32_t v) {
  OSC3Level = (uint8_t)v;
}

void apply_param_sub_level(int32_t v) {
  if (SUBLevel != (uint8_t)v) {
    SUBLevel = (uint8_t)v;
    levelBarFlag |= LEVEL_BAR_SUB;
  }
}

static void apply_param_calibration_flag(int32_t v) {
  switch (v) {
    case 0: serialSignal = screen_mode_raw(ScreenMode::LoadSaveExit); break;
    case 1: serialSignal = screen_mode_raw(ScreenMode::CalibrationMenu); break;
    default: return;
  }
  signalFlag = true;
}

static void apply_param_manual_calibration_flag(int32_t v) {
  switch (v) {
    case 1: serialSignal = screen_mode_raw(ScreenMode::ManualCalibration); break;
    case 0: serialSignal = screen_mode_raw(ScreenMode::CalibrationMenu); break;
    default: return;
  }
  signalFlag = true;
}

static void apply_param_manual_calibration_stage(int32_t v) {
  const CalTopology topology = screen_cal_topology();
  const int32_t stageMax     = (int32_t)screen_cal_stage_max(topology);
  int32_t stage = v;
  if (stage < 0) stage = 0;
  if (stage > stageMax) stage = stageMax;
  manualCalibrationStage = (uint8_t)stage;
  manualCalibrationOSCN  = screen_cal_stage_to_osc(topology, manualCalibrationStage);

  reset_gap_filter();
}

static void apply_param_gap_from_dco(int32_t v) {
  gapSamples[gapSampleIdx] = v;
  gapSampleIdx = (gapSampleIdx + 1) & 0x03;
  if (gapSampleCount < 4) gapSampleCount++;

  int64_t sum = 0;
  for (uint8_t i = 0; i < gapSampleCount; ++i) {
    sum += gapSamples[i];
  }

  calibrationGap = (int32_t)(sum / gapSampleCount);
}

static void apply_param_manual_calibration_offset(int32_t v) {
  offset = (int8_t)v;
}

static void apply_param_amp_comp_440(int32_t v) {
  if (v < 0) v = 0;
  ampComp440Display = (uint16_t)v;
}

static void apply_param_cal_pw_center(int32_t v) {
  if (v < 0) v = 0;
  if (v > (int32_t)CAL_PW_CENTER_MAX) v = (int32_t)CAL_PW_CENTER_MAX;
  calPwCenterDisplay = (uint16_t)v;
}

static void apply_param_ui_calibration_dismiss(int32_t) {
  if (serialSignal == screen_mode_raw(ScreenMode::CalibrationMenu)) {
    serialSignal = screen_mode_raw(ScreenMode::LoadSaveExit);
    signalFlag = true;
  }
}

static void apply_param_ui_calibration_menu_mode(int32_t) {
  serialSignal = screen_mode_raw(ScreenMode::CalibrationMenu);
  signalFlag = true;
}

static void apply_param_ui_menu_position(int32_t v) {
  if (v < 0) v = 0;
  calibrationMenuIndex = (uint8_t)v;
}

// =============================================================================
// Live Parameter Ingress Router -> Struct State Cache
// =============================================================================
void screen_cache_param(uint8_t id, int32_t val) {
  const ParamId pId = static_cast<ParamId>(id);

  // 1. Mod Matrix Range (Slots 0..7)
  if (id >= static_cast<uint8_t>(ParamId::PARAM_MOD_SLOT0_SOURCE) &&
      id <= static_cast<uint8_t>(ParamId::PARAM_MOD_SLOT7_DEPTH)) {
    uint8_t offset  = id - static_cast<uint8_t>(ParamId::PARAM_MOD_SLOT0_SOURCE);
    uint8_t slotIdx = offset / 3;
    uint8_t subType = offset % 3;
    if (slotIdx < 8) {
      if (subType == 0)      currentModState.slots[slotIdx].src   = (uint8_t)val;
      else if (subType == 1) currentModState.slots[slotIdx].dest  = (uint8_t)val;
      else                   currentModState.slots[slotIdx].depth = (int16_t)val;
    }
    return;
  }

  // 2. Discrete Parameters, Calibration & UI Signals
  switch (pId) {
    // --- Oscillators & Voice (PatchOscBlock) ---
    case ParamId::PARAM_OSC1_SAW_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 0); else currentOscState.wave_enables &= ~(1u << 0);
      break;
    case ParamId::PARAM_OSC1_PULSE_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 1); else currentOscState.wave_enables &= ~(1u << 1);
      break;
    case ParamId::PARAM_OSC1_TRI_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 2); else currentOscState.wave_enables &= ~(1u << 2);
      break;
    case ParamId::PARAM_OSC2_SAW_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 3); else currentOscState.wave_enables &= ~(1u << 3);
      break;
    case ParamId::PARAM_OSC2_PULSE_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 4); else currentOscState.wave_enables &= ~(1u << 4);
      break;
    case ParamId::PARAM_OSC2_TRI_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 5); else currentOscState.wave_enables &= ~(1u << 5);
      break;
    case ParamId::PARAM_OSC3_SAW_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 6); else currentOscState.wave_enables &= ~(1u << 6);
      break;
    case ParamId::PARAM_OSC3_PULSE_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 7); else currentOscState.wave_enables &= ~(1u << 7);
      break;
    case ParamId::PARAM_OSC3_TRI_ENABLE:
      if (val) currentOscState.wave_enables |= (1u << 8); else currentOscState.wave_enables &= ~(1u << 8);
      break;

    case ParamId::PARAM_OSC1_INTERVAL:       currentOscState.osc1_interval       = (int8_t)val; break;
    case ParamId::PARAM_OSC2_INTERVAL:       currentOscState.osc2_interval       = (int8_t)val; break;
    case ParamId::PARAM_OSC3_INTERVAL:       currentOscState.osc3_interval       = (int8_t)val; break;
    case ParamId::PARAM_OSC2_DETUNE_VAL:     currentOscState.osc2_detune         = (uint16_t)val; break;
    case ParamId::PARAM_UNISON_DETUNE:       currentOscState.unison_detune       = (int16_t)val; break;
    case ParamId::PARAM_VOICE_MODE:          currentOscState.voice_mode          = (uint8_t)val; break;
    case ParamId::PARAM_VOICE_ALLOC_MODE:    currentOscState.voice_alloc_mode    = (uint8_t)val; break;
    case ParamId::PARAM_SYNC_MODE:
    case ParamId::PARAM_OSC_SYNC_MODE:       currentOscState.sync_mode           = (uint8_t)val; break;
    case ParamId::PARAM_SOFT_SYNC:           currentOscState.soft_sync           = (uint8_t)val; break;
    case ParamId::PARAM_SUBOSC_DIVIDE:       currentOscState.subosc_divide       = (uint8_t)val; break;
    case ParamId::PARAM_ANALOG_DRIFT_AMOUNT: currentOscState.analog_drift        = (int8_t)val; break;
    case ParamId::PARAM_ANALOG_DRIFT_SPEED:  currentOscState.analog_drift_speed  = (int16_t)val; break;
    case ParamId::PARAM_ANALOG_DRIFT_SPREAD: currentOscState.analog_drift_spread = (int8_t)val; break;
    case ParamId::PARAM_PORTAMENTO_TIME:     currentOscState.portamento_time     = (uint16_t)val; break;
    case ParamId::PARAM_PORTAMENTO_MODE:     currentOscState.portamento_mode     = (uint8_t)val; break;
    case ParamId::PARAM_CHARACTER:           currentOscState.character           = (uint8_t)val; break;

    // --- LFOs & Envelopes (PatchLfoBlock) ---
    case ParamId::PARAM_LFO1_WAVEFORM:       currentLfoState.lfo1_waveform       = (uint8_t)val; break;
    case ParamId::PARAM_LFO2_WAVEFORM:       currentLfoState.lfo2_waveform       = (uint8_t)val; break;
    case ParamId::PARAM_LFO1_SPEED:          currentLfoState.lfo1_speed          = (uint16_t)val; break;
    case ParamId::PARAM_LFO2_SPEED:          currentLfoState.lfo2_speed          = (uint16_t)val; break;
    case ParamId::PARAM_LFO1_TO_DCO:         currentLfoState.lfo1_to_dco         = (uint16_t)val; break;
    case ParamId::PARAM_LFO1_TO_OSC1:        currentLfoState.lfo1_to_osc1        = (uint8_t)val; break;
    case ParamId::PARAM_LFO1_TO_OSC2:        currentLfoState.lfo1_to_osc2        = (uint8_t)val; break;
    case ParamId::PARAM_LFO1_TO_OSC3:        currentLfoState.lfo1_to_osc3        = (uint8_t)val; break;
    case ParamId::PARAM_LFO2_TO_OSC2:        currentLfoState.lfo2_to_osc2        = (uint16_t)val; break;
    case ParamId::PARAM_LFO2_TO_OSC3:        currentLfoState.lfo2_to_osc3        = (uint16_t)val; break;
    case ParamId::PARAM_LFO2_TO_OSC2_COARSE: currentLfoState.lfo2_to_osc2_coarse = (uint16_t)val; break;
    case ParamId::PARAM_LFO2_TO_OSC3_COARSE: currentLfoState.lfo2_to_osc3_coarse = (uint16_t)val; break;
    case ParamId::PARAM_LFO2_TO_PW:          currentLfoState.lfo2_to_pw          = (uint16_t)val; break;
    case ParamId::PARAM_LFO1_TO_VCA:         currentLfoState.lfo1_to_vca         = (uint16_t)val; break;
    case ParamId::PARAM_PW_VALUE:            currentLfoState.pw_value            = (uint16_t)val; break;
    case ParamId::PARAM_ADSR1_TO_VCA:        
      currentLfoState.adsr1_to_vca = (int16_t)val;
      currentMixState.adsr1_to_vca = (int16_t)val;
      break;
    case ParamId::PARAM_ADSR3_TO_PWM:        currentLfoState.adsr3_to_pwm        = (int16_t)val; break;
    case ParamId::PARAM_ADSR3_TO_DETUNE1:    currentLfoState.adsr3_to_detune1    = (int16_t)val; break;
    case ParamId::PARAM_ADSR3_PITCH_MODE:    currentLfoState.adsr3_pitch_mode    = (uint8_t)val; break;
    case ParamId::PARAM_ADSR3_TO_OSC_SELECT: currentLfoState.adsr3_to_osc_select = (int8_t)val; break;

    // --- Mixer, Filter & Dynamics (PatchMixBlock) ---
    case ParamId::PARAM_OSC1_LEVEL:          
      currentMixState.osc1_level = (uint8_t)val;
      apply_param_osc1_level(val);
      break;
    case ParamId::PARAM_OSC2_LEVEL:          
      currentMixState.osc2_level = (uint8_t)val;
      apply_param_osc2_level(val);
      break;
    case ParamId::PARAM_OSC3_LEVEL:          
      currentMixState.osc3_level = (uint8_t)val;
      apply_param_osc3_level(val);
      break;
    case ParamId::PARAM_SUB_LEVEL:           
      currentMixState.sub_level = (uint8_t)val;
      apply_param_sub_level(val);
      break;
    case ParamId::PARAM_VCA_LEVEL:
    case ParamId::PARAM_VCA_LEVEL_ALT:       currentMixState.vca_level           = (uint8_t)val; break;
    case ParamId::PARAM_FILTER_MODE:         currentMixState.filter_mode         = (uint8_t)val; break;
    case ParamId::PARAM_VELOCITY_TO_VCF:     currentMixState.velocity_to_vcf     = (int8_t)val; break;
    case ParamId::PARAM_VELOCITY_TO_VCA:     currentMixState.velocity_to_vca     = (int8_t)val; break;
    case ParamId::PARAM_VCF_KEYTRACK:        currentMixState.vcf_keytrack        = (int16_t)val; break;
    case ParamId::PARAM_DIST_DRIVE:          currentMixState.dist_drive          = (uint16_t)val; break;
    case ParamId::PARAM_DIST_MIX:            currentMixState.dist_mix            = (uint16_t)val; break;
    case ParamId::PARAM_ADSR1_ATTACK_CURVE:  currentMixState.adsr1_attack_curve  = (uint8_t)val; break;
    case ParamId::PARAM_ADSR1_DECAY_CURVE:   currentMixState.adsr1_decay_curve   = (uint8_t)val; break;
    case ParamId::PARAM_ADSR2_ATTACK_CURVE:  currentMixState.adsr2_attack_curve  = (uint8_t)val; break;
    case ParamId::PARAM_ADSR2_DECAY_CURVE:   currentMixState.adsr2_decay_curve   = (uint8_t)val; break;

    case ParamId::PARAM_RESONANCE_COMPENSATION:
      if (val) currentMixState.misc_flags |= (1 << 0); else currentMixState.misc_flags &= ~(1 << 0);
      break;
    case ParamId::PARAM_VCA_ADSR_RESTART:
      if (val) currentMixState.misc_flags |= (1 << 1); else currentMixState.misc_flags &= ~(1 << 1);
      break;
    case ParamId::PARAM_VCF_ADSR_RESTART:
      if (val) currentMixState.misc_flags |= (1 << 2); else currentMixState.misc_flags &= ~(1 << 2);
      break;
    case ParamId::PARAM_ADSR3_ENABLED:
      if (val) currentMixState.misc_flags |= (1 << 3); else currentMixState.misc_flags &= ~(1 << 3);
      break;

    // --- UI-Specific Parameters for OLED Filter HUD ---
    case ParamId::PARAM_UI_CUTOFF:       guiState.cutoff = val; break;
    case ParamId::PARAM_UI_RESONANCE:    guiState.resonance = val; break;
    case ParamId::PARAM_UI_ADSR2_TO_VCF: guiState.env2vcf = val; break;

    // --- Calibration Models & UI Navigation ---
    case ParamId::PARAM_CALIBRATION_FLAG:        apply_param_calibration_flag(val); break;
    case ParamId::PARAM_MANUAL_CALIBRATION_FLAG: apply_param_manual_calibration_flag(val); break;
    case ParamId::PARAM_MANUAL_CALIBRATION_STAGE:apply_param_manual_calibration_stage(val); break;
    case ParamId::PARAM_MANUAL_CALIBRATION_OFFSET: apply_param_manual_calibration_offset(val); break;
    case ParamId::PARAM_AMP_COMP_440:            apply_param_amp_comp_440(val); break;
    case ParamId::PARAM_CAL_PW_CENTER:           apply_param_cal_pw_center(val); break;
    case ParamId::PARAM_GAP_FROM_DCO:            apply_param_gap_from_dco(val); break;
    case ParamId::PARAM_UI_MENU_POSITION:        apply_param_ui_menu_position(val); break;
    case ParamId::PARAM_UI_CALIBRATION_DISMISS:  apply_param_ui_calibration_dismiss(val); break;
    case ParamId::PARAM_UI_CALIBRATION_MENU_MODE:apply_param_ui_calibration_menu_mode(val); break;

    default:
      break;
  }

  // Only trigger the Inspector if we are NOT in Silent mode (e.g. not loading presets)
  if (serialSignal != screen_mode_raw(ScreenMode::Silent)) {
    InspectorType insp = get_param_inspector_type(pId);
    if (insp != InspectorType::None) {
      trigger_oled_inspector(insp);

      // Also forward to Core 1 TFT snapshot state
      activeInspector             = insp;
      inspectorLastActivityMillis = millis();
      inspectorActiveFlag         = true;
    }
  }
}

// Lock-Free Parameter Toast Draw
void draw_param_1(const char* name, int32_t value) {
  paramChangeLastMillis = millis();
  paramChangeTimerFlag = true;

  char str[12];
  itoa(value, str, 10);
  lv_label_set_text(ui_CommandMessage, name);
  lv_label_set_text(ui_CommandMessageShadow, name);
  lv_label_set_text(ui_CommandValueShadow, str);
  lv_label_set_text(ui_CommandValue, str);

  lv_obj_remove_flag(ui_BottomMessagePanel, LV_OBJ_FLAG_HIDDEN);
}

// Lock-Free Preset Scroll Draw
void SCREEN_HOT(draw_preset_scroll_1)(ScreenMode mode, uint8_t num, const char* name, uint8_t charPos) {
  char str[12];
  itoa(num, str, 10);
  switch (mode) {
    case ScreenMode::PresetScroll:
    case ScreenMode::LoadSaveExit:
      lv_label_set_text(ui_PresetN, str);
      lv_label_set_text(ui_PresetNShadow, str);
      lv_label_set_text(ui_PresetName, name);
      lv_label_set_text(ui_PresetNameShadow, name);
      break;
    case ScreenMode::SaveSelectPreset:
      lv_label_set_text(ui_PresetNNew, str);
      lv_label_set_text(ui_PresetNNewShadow, str);
      lv_label_set_text(ui_PresetNameNew, name);
      lv_label_set_text(ui_PresetNameNewShadow, name);
      break;
    case ScreenMode::SaveSetName:
      lv_textarea_delete_char_forward(ui_PresetNewName);
      lv_textarea_add_char(ui_PresetNewName, name[charPos]);
      lv_textarea_set_cursor_pos(ui_PresetNewName, charPos);
      break;
    default:
      break;
  }
}

void applyNavParam(uint8_t id, int32_t value) {
  screen_cache_param(id, value);
}

void setDisplayParam() {
  screen_cache_param(paramNumber, paramValue);

  paramName = "";

  if (serialSignal == screen_mode_raw(ScreenMode::Silent)) {
    paramChangeTimerFlag = false;
  }

  switch (static_cast<ParamId>(paramNumber)) {
    // --- Oscillator Enables ---
    case ParamId::PARAM_OSC1_SAW_ENABLE: paramName = (paramValue != 0) ? " OSC1 SAW ON" : " OSC1 SAW OFF"; break;
    case ParamId::PARAM_OSC1_PULSE_ENABLE: paramName = (paramValue != 0) ? " OSC1 PULSE ON" : " OSC1 PULSE OFF"; break;
    case ParamId::PARAM_OSC1_TRI_ENABLE: paramName = (paramValue != 0) ? " OSC1 TRI ON" : " OSC1 TRI OFF"; break;
    case ParamId::PARAM_OSC2_SAW_ENABLE: paramName = (paramValue != 0) ? " OSC2 SAW ON" : " OSC2 SAW OFF"; break;
    case ParamId::PARAM_OSC2_PULSE_ENABLE: paramName = (paramValue != 0) ? " OSC2 PULSE ON" : " OSC2 PULSE OFF"; break;
    case ParamId::PARAM_OSC2_TRI_ENABLE: paramName = (paramValue != 0) ? " OSC2 TRI ON" : " OSC2 TRI OFF"; break;
    case ParamId::PARAM_OSC3_SAW_ENABLE: paramName = (paramValue != 0) ? " OSC3 SAW ON" : " OSC3 SAW OFF"; break;
    case ParamId::PARAM_OSC3_PULSE_ENABLE: paramName = (paramValue != 0) ? " OSC3 PULSE ON" : " OSC3 PULSE OFF"; break;
    case ParamId::PARAM_OSC3_TRI_ENABLE: paramName = (paramValue != 0) ? " OSC3 TRI ON" : " OSC3 TRI OFF"; break;
    case ParamId::PARAM_SINE_STATUS: paramName = " SINE (unused)"; break;
    case ParamId::PARAM_RESONANCE_COMPENSATION: paramName = " ResoAmpComp"; break;
    case ParamId::PARAM_VCA_ADSR_RESTART: paramName = " ADSR1 Restart"; break;
    case ParamId::PARAM_VCF_ADSR_RESTART: paramName = " ADSR2 Restart"; break;
    case ParamId::PARAM_ADSR3_TO_OSC_SELECT: paramName = screen_adsr3_osc_select_label(screen_cal_topology(), paramValue); break;
    case ParamId::PARAM_LFO1_WAVEFORM: paramName = " LFO1 Shape"; break;
    case ParamId::PARAM_LFO2_WAVEFORM: paramName = " LFO2 Shape"; break;

    // --- Intervals & Detune ---
    case ParamId::PARAM_OSC1_INTERVAL:
      paramName = " Octave";
      paramValue = (paramValue - 36) / 12;
      break;
    case ParamId::PARAM_OSC2_INTERVAL:
      paramName = " OSC2 Interval";
      paramValue -= 36;
      break;
    case ParamId::PARAM_OSC2_DETUNE_VAL:
      paramName = " OSC2 Detune";
      paramValue -= 256;
      break;
    case ParamId::PARAM_LFO2_TO_OSC2: paramName = " LFO2->OSC2 Pitch"; break;
    case ParamId::PARAM_OSC3_INTERVAL:
      paramName = " OSC3 Interval";
      paramValue -= 36;
      break;
    case ParamId::PARAM_OSC3_DETUNE_VAL:
      paramName = " OSC3 Detune";
      paramValue -= 256;
      break;
    case ParamId::PARAM_LFO2_TO_OSC3: paramName = " LFO2->OSC3 Pitch"; break;
    case ParamId::PARAM_OSC_SYNC_MODE: paramName = " OscPhaseSync"; break;
    case ParamId::PARAM_PORTAMENTO_TIME: paramName = " Portamento"; break;
    case ParamId::PARAM_VCF_KEYTRACK: paramName = " VCF Keytrack"; break;
    case ParamId::PARAM_UI_CUTOFF: paramName = " Cutoff"; break;
    case ParamId::PARAM_UI_RESONANCE: paramName = " Resonance"; break;
    case ParamId::PARAM_UI_ADSR2_TO_VCF: paramName = " ADSR2 -> VCF"; break;
    case ParamId::PARAM_UI_LFO2_TO_VCF: paramName = " LFO2 -> VCF"; break;
    case ParamId::PARAM_VELOCITY_TO_VCF: paramName = " Velocity -> VCF"; break;
    case ParamId::PARAM_VELOCITY_TO_VCA: paramName = " Velocity -> VCA"; break;
    case ParamId::PARAM_OSC1_LEVEL: paramName = " OSC1 Level"; break;
    case ParamId::PARAM_OSC2_LEVEL: paramName = " OSC2 Level"; break;
    case ParamId::PARAM_OSC3_LEVEL: paramName = " OSC3 Level"; break;
    case ParamId::PARAM_SUB_LEVEL: paramName = " SUB Level"; break;
    case ParamId::PARAM_CALIBRATION_VALUE: paramName = " CALIBRATION VAL"; break;

    // --- Voice Mode & Alloc ---
    case ParamId::PARAM_VOICE_MODE:
      switch (paramValue) {
        case 0: paramName = " MONO"; break;
        case 1: paramName = " POLY"; break;
        case 2: paramName = " UNISON"; break;
        default: break;
      }
      break;
      case ParamId::PARAM_VOICE_ALLOC_MODE:
      switch (paramValue) {
        case 0: paramName = " ROUND ROBIN"; break;
        case 1: paramName = " OLDEST"; break;
        case 2: paramName = " QUIETEST"; break;
        case 3: paramName = " QUIETEST LOW"; break;
        case 4: paramName = " QUIETEST HIGH"; break;
        case 5: paramName = " NO STEAL"; break;
        default: paramName = " ROUND ROBIN"; break;
      }
      break;

    case ParamId::PARAM_UNISON_DETUNE: paramName = " Analog Detune"; break;
    case ParamId::PARAM_ANALOG_DRIFT_AMOUNT: paramName = " Analog Drift"; break;
    case ParamId::PARAM_ANALOG_DRIFT_SPEED: paramName = " Analog Drift Speed"; break;
    case ParamId::PARAM_ANALOG_DRIFT_SPREAD: paramName = " Analog Drift Spread"; break;
    case ParamId::PARAM_SYNC_MODE: paramName = " Sync Mode"; break;
    case ParamId::PARAM_LFO1_TO_DCO: paramName = " LFO1 -> Pitch"; break;
    case ParamId::PARAM_LFO1_SPEED: paramName = " LFO1 Speed"; break;
    case ParamId::PARAM_LFO2_SPEED: paramName = " LFO2 Speed"; break;
    case ParamId::PARAM_VCA_LEVEL: paramName = " VCA -> LEVEL"; break;
    case ParamId::PARAM_LFO1_TO_VCA: paramName = " LFO1 -> VCA"; break;
    case ParamId::PARAM_LFO2_TO_PW: paramName = " LFO2 -> PWM"; break;
    case ParamId::PARAM_ADSR3_TO_PWM:
      paramName = " ADSR3 -> PWM";
      paramValue -= 512;
      break;
    case ParamId::PARAM_ADSR3_TO_DETUNE1: paramName = " ADSR3 -> Pitch"; break;

    // --- Curves ---
    case ParamId::PARAM_ADSR1_ATTACK_CURVE:
    case ParamId::PARAM_ADSR2_ATTACK_CURVE:
    case ParamId::PARAM_ADSR1_DECAY_CURVE:
    case ParamId::PARAM_ADSR2_DECAY_CURVE:
      switch (paramValue) {
        case 0: paramName = " EXP"; break;
        case 1: paramName = " SOFT"; break;
        case 2: paramName = " STEEP"; break;
        case 3: paramName = " CONCAVE"; break;
        case 4: paramName = " FAST S"; break;
        case 5: paramName = " SLOW THEN LIN"; break;
        case 6: paramName = " ALMOST LIN"; break;
        case 7: paramName = " LINEAR"; break;
        default: paramName = " EXP"; break;
      }
      break;

    // --- Manual / Hardware Function Keys ---
    case ParamId::PARAM_FUNCTION_KEY: paramName = " FUNCTION KEY"; break;
    case ParamId::PARAM_FADERS_CONTROL_MANUAL: paramName = " MAN FADERS"; break;
    case ParamId::PARAM_FADER_ROW1_CONTROL_MANUAL: paramName = " MAN FADERS 1"; break;
    case ParamId::PARAM_FADER_ROW2_CONTROL_MANUAL: paramName = " MAN FADERS 2"; break;
    case ParamId::PARAM_VCF_POTS_CONTROL_MANUAL: paramName = " MANUAL VCF"; break;
    case ParamId::PARAM_PWM_POTS_CONTROL_MANUAL: paramName = " MANUAL PWM"; break;
    case ParamId::PARAM_VCA_POTS_CONTROL_MANUAL: paramName = " MANUAL VCA"; break;
    case ParamId::PARAM_POTS_CONTROL_MANUAL: paramName = " MANUAL POTS"; break;
    case ParamId::PARAM_ALL_CONTROLS_MANUAL: paramName = " ALL CONTROLS MANUAL"; break;
    case ParamId::PARAM_ADSR3_ENABLED: paramName = " ADSR3 ENABLED"; break;

    // --- Calibration Menu & Stage Names ---
    case ParamId::PARAM_CALIBRATION_FLAG: paramName = " AUTO CALIBRATION"; break;
    case ParamId::PARAM_MANUAL_CALIBRATION_FLAG: paramName = " MANUAL CALIBRATION"; break;
    case ParamId::PARAM_MANUAL_CALIBRATION_STAGE:
      {
        static char calStageToast[20];
        const uint8_t stageNow = (uint8_t)paramValue;
        screen_cal_format_toast(screen_cal_topology(), stageNow, calStageToast, sizeof(calStageToast));
        paramName = calStageToast;
        {
          const uint8_t nOsc = screen_cal_nosc(screen_cal_topology());
          if (cal_stage_is_440_n(stageNow, nOsc)) {
            paramValue = (int32_t)ampComp440Display;
          } else if (cal_stage_is_pw_edit_n(stageNow, nOsc)) {
            paramValue = (int32_t)calPwCenterDisplay;
          } else {
            paramValue = (int32_t)offset;
          }
        }
        break;
      }
    case ParamId::PARAM_MANUAL_CALIBRATION_OFFSET: paramName = " OFFSET"; break;
    case ParamId::PARAM_AMP_COMP_440: paramName = " AMP"; break;
    case ParamId::PARAM_CAL_PW_CENTER: paramName = " PW"; break;
    case ParamId::PARAM_GAP_FROM_DCO: paramName = " GAP"; break;

    // --- Additional Controls ---
    case ParamId::PARAM_PW_VALUE: paramName = " PW"; break;
    case ParamId::PARAM_ADSR1_TO_VCA: paramName = " ADSR1 -> VCA"; break;
    case ParamId::PARAM_LFO3_SPEED: paramName = " LFO3 Speed"; break;
    case ParamId::PARAM_LFO3_WAVEFORM: paramName = " LFO3 Shape"; break;
    case ParamId::PARAM_ADSR3_RESTART: paramName = " ADSR3 Restart"; break;
    case ParamId::PARAM_VCA_LEVEL_ALT: paramName = " VCA -> LEVEL"; break;
    case ParamId::PARAM_LFO1_TO_OSC1: paramName = " LFO1 -> OSC1 extra"; break;
    case ParamId::PARAM_LFO1_TO_OSC2: paramName = " LFO1 -> OSC2 extra"; break;
    case ParamId::PARAM_LFO1_TO_OSC3: paramName = " LFO1 -> OSC3 extra"; break;
    case ParamId::PARAM_LFO2_TO_OSC2_COARSE: paramName = " LFO2 -> OSC2 coarse"; break;
    case ParamId::PARAM_LFO2_TO_OSC3_COARSE: paramName = " LFO2 -> OSC3 coarse"; break;
    case ParamId::PARAM_CHARACTER: paramName = " Character"; break;
    case ParamId::PARAM_ADSR3_PITCH_MODE: paramName = " EnvDCO pitch centered"; break;

    default:
      paramName = "";
      break;
  }
}

// Fast LUT/Switch Classifier (ParamId -> InspectorType)
InspectorType get_param_inspector_type(ParamId id) {
  switch (id) {
    // Envelope 1 (VCA)
    case ParamId::PARAM_VCA_ADSR_RESTART:
    case ParamId::PARAM_ADSR1_ATTACK_CURVE:
    case ParamId::PARAM_ADSR1_DECAY_CURVE:
    case ParamId::PARAM_ADSR1_TO_VCA:
      return InspectorType::ADSR1;

    // Envelope 2 (VCF)
    case ParamId::PARAM_VCF_ADSR_RESTART:
    case ParamId::PARAM_ADSR2_ATTACK_CURVE:
    case ParamId::PARAM_ADSR2_DECAY_CURVE:
    case ParamId::PARAM_UI_ADSR2_TO_VCF:
      return InspectorType::ADSR2;

    // Envelope 3 (Pitch/PWM)
    case ParamId::PARAM_ADSR3_TO_OSC_SELECT:
    case ParamId::PARAM_ADSR3_TO_PWM:
    case ParamId::PARAM_ADSR3_TO_DETUNE1:
    case ParamId::PARAM_ADSR3_ENABLED:
    case ParamId::PARAM_ADSR3_RESTART:
    case ParamId::PARAM_ADSR3_PITCH_MODE:
      return InspectorType::ADSR3;

    // Filter (VCF)
    case ParamId::PARAM_RESONANCE_COMPENSATION:
    case ParamId::PARAM_VCF_KEYTRACK:
    case ParamId::PARAM_UI_CUTOFF:
    case ParamId::PARAM_UI_RESONANCE:
    case ParamId::PARAM_UI_LFO2_TO_VCF:
    case ParamId::PARAM_VELOCITY_TO_VCF:
    case ParamId::PARAM_VCF_POTS_CONTROL_MANUAL:
    case ParamId::PARAM_FILTER_MODE:
    case ParamId::PARAM_DIST_DRIVE:
    case ParamId::PARAM_DIST_MIX:
      return InspectorType::Filter;

    // Oscillators & Mixer
    case ParamId::PARAM_OSC1_SAW_ENABLE:
    case ParamId::PARAM_OSC1_PULSE_ENABLE:
    case ParamId::PARAM_OSC1_TRI_ENABLE:
    case ParamId::PARAM_OSC2_SAW_ENABLE:
    case ParamId::PARAM_OSC2_PULSE_ENABLE:
    case ParamId::PARAM_OSC2_TRI_ENABLE:
    case ParamId::PARAM_OSC3_SAW_ENABLE:
    case ParamId::PARAM_OSC3_PULSE_ENABLE:
    case ParamId::PARAM_OSC3_TRI_ENABLE:
    case ParamId::PARAM_SINE_STATUS:
    case ParamId::PARAM_OSC1_INTERVAL:
    case ParamId::PARAM_OSC2_INTERVAL:
    case ParamId::PARAM_OSC3_INTERVAL:
    case ParamId::PARAM_OSC2_DETUNE_VAL:
    case ParamId::PARAM_OSC3_DETUNE_VAL:
    case ParamId::PARAM_OSC1_LEVEL:
    case ParamId::PARAM_OSC2_LEVEL:
    case ParamId::PARAM_OSC3_LEVEL:
    case ParamId::PARAM_SUB_LEVEL:
    case ParamId::PARAM_OSC_SYNC_MODE:
    case ParamId::PARAM_SYNC_MODE:
    case ParamId::PARAM_SOFT_SYNC:
    case ParamId::PARAM_SUBOSC_DIVIDE:
    case ParamId::PARAM_PORTAMENTO_TIME:
    case ParamId::PARAM_PORTAMENTO_MODE:
    case ParamId::PARAM_PW_VALUE:
    case ParamId::PARAM_PWM_POTS_CONTROL_MANUAL:
      return InspectorType::Oscillators;

    // LFO 1
    case ParamId::PARAM_LFO1_WAVEFORM:
    case ParamId::PARAM_LFO1_SPEED:
    case ParamId::PARAM_LFO1_TO_DCO:
    case ParamId::PARAM_LFO1_TO_VCA:
    case ParamId::PARAM_LFO1_TO_OSC1:
    case ParamId::PARAM_LFO1_TO_OSC2:
    case ParamId::PARAM_LFO1_TO_OSC3:
      return InspectorType::LFO1;

    // LFO 2
    case ParamId::PARAM_LFO2_WAVEFORM:
    case ParamId::PARAM_LFO2_SPEED:
    case ParamId::PARAM_LFO2_TO_OSC2:
    case ParamId::PARAM_LFO2_TO_OSC3:
    case ParamId::PARAM_LFO2_TO_OSC2_COARSE:
    case ParamId::PARAM_LFO2_TO_OSC3_COARSE:
    case ParamId::PARAM_LFO2_TO_PW:
      return InspectorType::LFO2;

    // LFO 3
    case ParamId::PARAM_LFO3_SPEED:
    case ParamId::PARAM_LFO3_WAVEFORM:
      return InspectorType::LFO3;

    // Voice Engine & Drift
    case ParamId::PARAM_VOICE_MODE:
    case ParamId::PARAM_VOICE_ALLOC_MODE:
    case ParamId::PARAM_UNISON_DETUNE:
    case ParamId::PARAM_ANALOG_DRIFT_AMOUNT:
    case ParamId::PARAM_ANALOG_DRIFT_SPEED:
    case ParamId::PARAM_ANALOG_DRIFT_SPREAD:
    case ParamId::PARAM_CHARACTER:
      return InspectorType::VoiceEngine;

    default:
      return InspectorType::None;
  }
}