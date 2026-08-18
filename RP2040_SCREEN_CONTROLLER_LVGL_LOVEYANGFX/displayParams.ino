#include "params_def.h"
#include "param_router.h"
#include "sram_hot.h"

// Definitions for display/model shared state
uint16_t paramHideTimeMillis = 3000;
bool     paramChangeTimerFlag = false;

volatile int8_t   offset = 0;
volatile uint16_t ampComp440Display = 0;
volatile uint16_t calPwCenterDisplay = 0;
volatile uint8_t  manualCalibrationOSCN = 0;
volatile uint8_t  manualCalibrationStage = 0;
volatile int32_t  calibrationGap = 0;

volatile CalTopology screenCalTopology = SCREEN_CAL_TOPOLOGY_DEFAULT;

volatile uint8_t  OSC1Level = 0;
volatile uint8_t  OSC2Level = 0;
volatile uint8_t  OSC3Level = 0;
volatile uint8_t  SUBLevel = 0;

volatile uint16_t ADSR1Attack = 0;
volatile uint16_t ADSR1Decay = 0;
volatile uint16_t ADSR1Sustain = 0;
volatile uint16_t ADSR1Release = 0;

volatile uint16_t ADSR2Attack = 0;
volatile uint16_t ADSR2Decay = 0;
volatile uint16_t ADSR2Sustain = 0;
volatile uint16_t ADSR2Release = 0;

using ScreenParamValueT = int32_t;
using ScreenParamDescriptor = ParamDescriptorT<ScreenParamValueT>;

// Mixer levels -> bar values (with change check to avoid redundant work)
static void apply_param_osc1_level(int32_t v) {
  if (OSC1Level != (uint8_t)v) {
    OSC1Level     = (uint8_t)v;
    levelBarFlag |= LEVEL_BAR_OSC1;
  }
}

static void apply_param_osc2_level(int32_t v) {
  if (OSC2Level != (uint8_t)v) {
    OSC2Level     = (uint8_t)v;
    levelBarFlag |= LEVEL_BAR_OSC2;
  }
}

static void apply_param_osc3_level(int32_t v) {
  OSC3Level = (uint8_t)v;
}

static void apply_param_sub_level(int32_t v) {
  if (SUBLevel != (uint8_t)v) {
    SUBLevel      = (uint8_t)v;
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

static void apply_param_gap_from_dco(int32_t v) {
  calibrationGap = (int32_t)v;
}

static void apply_param_ui_calibration_dismiss(int32_t) {
  if (serialSignal == screen_mode_raw(ScreenMode::CalibrationMenu)) {
    serialSignal = screen_mode_raw(ScreenMode::LoadSaveExit);
    signalFlag   = true;
  }
}

static void apply_param_ui_calibration_menu_mode(int32_t) {
  serialSignal = screen_mode_raw(ScreenMode::CalibrationMenu);
  signalFlag   = true;
}

static const ScreenParamDescriptor screenParamTable[] = {
  { ParamId::PARAM_OSC1_LEVEL,                     apply_param_osc1_level                       },
  { ParamId::PARAM_OSC2_LEVEL,                     apply_param_osc2_level                       },
  { ParamId::PARAM_OSC3_LEVEL,                     apply_param_osc3_level                       },
  { ParamId::PARAM_SUB_LEVEL,                      apply_param_sub_level                        },
  { ParamId::PARAM_CALIBRATION_FLAG,               apply_param_calibration_flag                 },
  { ParamId::PARAM_MANUAL_CALIBRATION_FLAG,        apply_param_manual_calibration_flag          },
  { ParamId::PARAM_MANUAL_CALIBRATION_STAGE,       apply_param_manual_calibration_stage         },
  { ParamId::PARAM_MANUAL_CALIBRATION_OFFSET,      apply_param_manual_calibration_offset        },
  { ParamId::PARAM_AMP_COMP_440,                   apply_param_amp_comp_440                     },
  { ParamId::PARAM_CAL_PW_CENTER,                  apply_param_cal_pw_center                    },
  { ParamId::PARAM_GAP_FROM_DCO,                   apply_param_gap_from_dco                     },
  { ParamId::PARAM_UI_CALIBRATION_DISMISS,         apply_param_ui_calibration_dismiss           },
  { ParamId::PARAM_UI_CALIBRATION_MENU_MODE,       apply_param_ui_calibration_menu_mode         },
};

static const size_t screenParamTableSize = sizeof(screenParamTable) / sizeof(screenParamTable[0]);

// Lock-Free Parameter Toast Draw
void draw_param_1(const char* name, int32_t value) {
  paramChangeLastMillis = millis();
  paramChangeTimerFlag  = true;

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

// Lock-Free Calibration UI Draw
void drawManualCalibration(const Core1Snapshot &snap) {
  char str[8];
  char strLong[12];

  const uint8_t nOsc = screen_cal_nosc(snap.calTopology);
  if (cal_stage_is_440_n(snap.calStage, nOsc)) {
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
}

static void applyParamToModelAndSignals() {
  param_router_apply<ScreenParamValueT>(
    screenParamTable,
    screenParamTableSize,
    paramNumber,
    (ScreenParamValueT)paramValue
  );
}

void applyNavParam(uint8_t id, int32_t value) {
  param_router_apply<ScreenParamValueT>(
    screenParamTable,
    screenParamTableSize,
    id,
    (ScreenParamValueT)value
  );
}

void setDisplayParam() {
  applyParamToModelAndSignals();

  paramName = ""; // Clear stale string

  switch (static_cast<ParamId>(paramNumber)) {
    // --- Oscillator Enables ---
    case ParamId::PARAM_OSC1_SAW_ENABLE:      paramName = (paramValue != 0) ? " OSC1 SAW ON" : " OSC1 SAW OFF"; break;
    case ParamId::PARAM_OSC1_PULSE_ENABLE:    paramName = (paramValue != 0) ? " OSC1 PULSE ON" : " OSC1 PULSE OFF"; break;
    case ParamId::PARAM_OSC1_TRI_ENABLE:      paramName = (paramValue != 0) ? " OSC1 TRI ON" : " OSC1 TRI OFF"; break;
    case ParamId::PARAM_OSC2_SAW_ENABLE:      paramName = (paramValue != 0) ? " OSC2 SAW ON" : " OSC2 SAW OFF"; break;
    case ParamId::PARAM_OSC2_PULSE_ENABLE:    paramName = (paramValue != 0) ? " OSC2 PULSE ON" : " OSC2 PULSE OFF"; break;
    case ParamId::PARAM_OSC2_TRI_ENABLE:      paramName = (paramValue != 0) ? " OSC2 TRI ON" : " OSC2 TRI OFF"; break;
    case ParamId::PARAM_OSC3_SAW_ENABLE:      paramName = (paramValue != 0) ? " OSC3 SAW ON" : " OSC3 SAW OFF"; break;
    case ParamId::PARAM_OSC3_PULSE_ENABLE:    paramName = (paramValue != 0) ? " OSC3 PULSE ON" : " OSC3 PULSE OFF"; break;
    case ParamId::PARAM_OSC3_TRI_ENABLE:      paramName = (paramValue != 0) ? " OSC3 TRI ON" : " OSC3 TRI OFF"; break;
    case ParamId::PARAM_SINE_STATUS:          paramName = " SINE (unused)"; break;
    case ParamId::PARAM_RESONANCE_COMPENSATION: paramName = " ResoAmpComp"; break;
    case ParamId::PARAM_VCA_ADSR_RESTART:     paramName = " ADSR1 Restart"; break;
    case ParamId::PARAM_VCF_ADSR_RESTART:     paramName = " ADSR2 Restart"; break;
    case ParamId::PARAM_ADSR3_TO_OSC_SELECT:  paramName = screen_adsr3_osc_select_label(screen_cal_topology(), paramValue); break;
    case ParamId::PARAM_LFO1_WAVEFORM:        paramName = " LFO1 Shape"; break;
    case ParamId::PARAM_LFO2_WAVEFORM:        paramName = " LFO2 Shape"; break;
    
    // --- Intervals & Detune ---
    case ParamId::PARAM_OSC1_INTERVAL:        paramName = " Octave"; paramValue = (paramValue - 36) / 12; break;
    case ParamId::PARAM_OSC2_INTERVAL:        paramName = " OSC2 Interval"; paramValue -= 36; break;
    case ParamId::PARAM_OSC2_DETUNE_VAL:      paramName = " OSC2 Detune"; paramValue -= 256; break;
    case ParamId::PARAM_LFO2_TO_OSC2:         paramName = " LFO2->OSC2 Pitch"; break;
    case ParamId::PARAM_OSC3_INTERVAL:        paramName = " OSC3 Interval"; paramValue -= 36; break;
    case ParamId::PARAM_OSC3_DETUNE_VAL:      paramName = " OSC3 Detune"; paramValue -= 256; break;
    case ParamId::PARAM_LFO2_TO_OSC3:         paramName = " LFO2->OSC3 Pitch"; break;
    case ParamId::PARAM_OSC_SYNC_MODE:        paramName = " OscPhaseSync"; break;
    case ParamId::PARAM_PORTAMENTO_TIME:      paramName = " Portamento"; break;
    case ParamId::PARAM_VCF_KEYTRACK:         paramName = " VCF Keytrack"; break;
    case ParamId::PARAM_UI_CUTOFF:            paramName = " Cutoff"; break;
    case ParamId::PARAM_UI_RESONANCE:         paramName = " Resonance"; break;
    case ParamId::PARAM_UI_ADSR2_TO_VCF:      paramName = " ADSR2 -> VCF"; break;
    case ParamId::PARAM_UI_LFO2_TO_VCF:       paramName = " LFO2 -> VCF"; break;
    case ParamId::PARAM_VELOCITY_TO_VCF:      paramName = " Velocity -> VCF"; break;
    case ParamId::PARAM_VELOCITY_TO_VCA:      paramName = " Velocity -> VCA"; break;
    case ParamId::PARAM_OSC1_LEVEL:           paramName = " OSC1 Level"; break;
    case ParamId::PARAM_OSC2_LEVEL:           paramName = " OSC2 Level"; break;
    case ParamId::PARAM_OSC3_LEVEL:           paramName = " OSC3 Level"; break;
    case ParamId::PARAM_SUB_LEVEL:            paramName = " SUB Level"; break;
    case ParamId::PARAM_CALIBRATION_VALUE:    paramName = " CALIBRATION VAL"; break;

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
        case 0: paramName = " ROUND ROBIN / LAST"; break;
        case 1: paramName = " OLDEST / FIRST"; break;
        case 2: paramName = " QUIETEST / LAST"; break;
        case 3: paramName = " QUIETEST KEEP LOW / LOW"; break;
        case 4: paramName = " QUIETEST KEEP HIGH / HIGH"; break;
        case 5: paramName = " NO STEAL / FIRST"; break;
        default: break;
      }
      break;

    case ParamId::PARAM_UNISON_DETUNE:        paramName = " Analog Detune"; break;
    case ParamId::PARAM_ANALOG_DRIFT_AMOUNT:  paramName = " Analog Drift"; break;
    case ParamId::PARAM_ANALOG_DRIFT_SPEED:   paramName = " Analog Drift Speed"; break;
    case ParamId::PARAM_ANALOG_DRIFT_SPREAD:  paramName = " Analog Drift Spread"; break;
    case ParamId::PARAM_SYNC_MODE:            paramName = " Sync Mode"; break;
    case ParamId::PARAM_LFO1_TO_DCO:          paramName = " LFO1 -> Pitch"; break;
    case ParamId::PARAM_LFO1_SPEED:           paramName = " LFO1 Speed"; break;
    case ParamId::PARAM_LFO2_SPEED:           paramName = " LFO2 Speed"; break;
    case ParamId::PARAM_VCA_LEVEL:            paramName = " VCA -> LEVEL"; break;
    case ParamId::PARAM_LFO1_TO_VCA:          paramName = " LFO1 -> VCA"; break;
    case ParamId::PARAM_LFO2_TO_PW:           paramName = " LFO2 -> PWM"; break;
    case ParamId::PARAM_ADSR3_TO_PWM:         paramName = " ADSR3 -> PWM"; paramValue -= 512; break;
    case ParamId::PARAM_ADSR3_TO_DETUNE1:     paramName = " ADSR3 -> Pitch"; break;

    // --- Curves ---
    case ParamId::PARAM_ADSR1_ATTACK_CURVE:
    case ParamId::PARAM_ADSR2_ATTACK_CURVE:
      switch (paramValue) {
        case 0: paramName = " EXP"; break;
        case 1: paramName = " SOFT"; break;
        case 2: paramName = " STEEP"; break;
        case 3: paramName = " CONCAVE"; break;
        case 4: paramName = " FAST S"; break;
        case 5: paramName = " SLOW THEN LIN"; break;
        case 6: paramName = " ALMOST LIN"; break;
        case 7: paramName = " LINEAR"; break;
        case 100: paramName = (paramNumber == static_cast<uint8_t>(ParamId::PARAM_ADSR1_ATTACK_CURVE)) ? " ADSR1 Curves" : " ADSR2 Curves"; break;
        default: break;
      }
      break;

    case ParamId::PARAM_ADSR1_DECAY_CURVE:
    case ParamId::PARAM_ADSR2_DECAY_CURVE:
      switch (paramValue) {
        case 0: paramName = " EXP"; break;
        case 1: paramName = " SOFT"; break;
        case 2: paramName = " STEEP"; break;
        case 3: paramName = " CONVEX"; break;
        case 4: paramName = " FAST START S"; break;
        case 5: paramName = " SLOW THEN LIN"; break;
        case 6: paramName = " FAST THEN LIN"; break;
        case 7: paramName = " ALMOST LIN"; break;
        case 8: paramName = " LINEAR"; break;
        case 100: paramName = (paramNumber == static_cast<uint8_t>(ParamId::PARAM_ADSR1_DECAY_CURVE)) ? " ADSR1 Decay" : " ADSR2 Decay"; break;
        default: break;
      }
      break;

    // --- Manual / Hardware Function Keys (RESTORED) ---
    case ParamId::PARAM_FUNCTION_KEY:             paramName = " FUNCTION KEY"; break;
    case ParamId::PARAM_FADERS_CONTROL_MANUAL:     paramName = " MAN FADERS"; break;
    case ParamId::PARAM_FADER_ROW1_CONTROL_MANUAL: paramName = " MAN FADERS 1"; break;
    case ParamId::PARAM_FADER_ROW2_CONTROL_MANUAL: paramName = " MAN FADERS 2"; break;
    case ParamId::PARAM_VCF_POTS_CONTROL_MANUAL:   paramName = " MANUAL VCF"; break;
    case ParamId::PARAM_PWM_POTS_CONTROL_MANUAL:   paramName = " MANUAL PWM"; break;
    case ParamId::PARAM_VCA_POTS_CONTROL_MANUAL:   paramName = " MANUAL VCA"; break;
    case ParamId::PARAM_POTS_CONTROL_MANUAL:       paramName = " MANUAL POTS"; break;
    case ParamId::PARAM_ALL_CONTROLS_MANUAL:       paramName = " ALL CONTROLS MANUAL"; break;
    case ParamId::PARAM_ADSR3_ENABLED:             paramName = " ADSR3 ENABLED"; break;

    // --- Calibration Menu & Stage Names (RESTORED) ---
    case ParamId::PARAM_CALIBRATION_FLAG:          paramName = " AUTO CALIBRATION"; break;
    case ParamId::PARAM_MANUAL_CALIBRATION_FLAG:   paramName = " MANUAL CALIBRATION"; break;
    case ParamId::PARAM_MANUAL_CALIBRATION_STAGE: {
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
    case ParamId::PARAM_AMP_COMP_440:              paramName = " AMP"; break;
    case ParamId::PARAM_CAL_PW_CENTER:             paramName = " PW"; break;
    case ParamId::PARAM_GAP_FROM_DCO:              paramName = " GAP"; break;

    // --- Additional Controls ---
    case ParamId::PARAM_PW_VALUE:                  paramName = " PW"; break;
    case ParamId::PARAM_ADSR1_TO_VCA:              paramName = " ADSR1 -> VCA"; break;
    case ParamId::PARAM_LFO3_SPEED:                paramName = " LFO3 Speed"; break;
    case ParamId::PARAM_LFO3_WAVEFORM:             paramName = " LFO3 Shape"; break;
    case ParamId::PARAM_ADSR3_RESTART:             paramName = " ADSR3 Restart"; break;
    case ParamId::PARAM_VCA_LEVEL_ALT:             paramName = " VCA -> LEVEL"; break;
    case ParamId::PARAM_LFO1_TO_OSC1:              paramName = " LFO1 -> OSC1 extra"; break;
    case ParamId::PARAM_LFO1_TO_OSC2:              paramName = " LFO1 -> OSC2 extra"; break;
    case ParamId::PARAM_LFO1_TO_OSC3:              paramName = " LFO1 -> OSC3 extra"; break;
    case ParamId::PARAM_LFO2_TO_OSC2_COARSE:       paramName = " LFO2 -> OSC2 coarse"; break;
    case ParamId::PARAM_LFO2_TO_OSC3_COARSE:       paramName = " LFO2 -> OSC3 coarse"; break;
    case ParamId::PARAM_CHARACTER:                 paramName = " Character"; break;
    case ParamId::PARAM_ADSR3_PITCH_MODE:          paramName = " EnvDCO pitch centered"; break;

    default:
      paramName = "";
      break;
  }
}