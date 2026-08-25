#include "ui_views.h"
#include "src/ui/ui.h"
#include "display_u8g2.h"
#include "Serial.h"

ScreenMode currentMode = ScreenMode::PresetScroll;

static const uint32_t silentModeTimeoutMillis = 1000;
static uint32_t silentModeEnteredMillis = 0;

static bool     saveMessageTimerFlag = false;
static uint32_t saveMessageLastMillis = 0;
static const uint32_t saveMessageHideTimeMillis = 1800;

static uint8_t  lastRenderedStage = 255;
static int32_t  lastRenderedGap = -999999;
static int8_t   lastRenderedOff = 0;
static uint16_t lastRendered440 = 0;
static uint16_t lastRenderedPw = 0;
static uint8_t  lastRenderedMenuIndex = 255;

static InspectorType currentInspector = InspectorType::None;
static bool inspectorVisible = false;

static void show_inspector_panel(InspectorType type, const Core1Snapshot &snap) {}
static void update_inspector_panel(InspectorType type, const Core1Snapshot &snap) {}
static void hide_inspector_panel() {}

void SCREEN_HOT(updateInspectorLifecycle)(ScreenMode mode, const Core1Snapshot &snap) {
  if (mode != ScreenMode::PresetScroll && mode != ScreenMode::LoadSaveExit) {
    if (inspectorVisible) {
      hide_inspector_panel();
      inspectorVisible = false;
      currentInspector = InspectorType::None;
    }
    return;
  }

  uint32_t now = millis();

  if (snap.inspectorType != InspectorType::None) {
    if (!inspectorVisible || currentInspector != snap.inspectorType) {
      currentInspector = snap.inspectorType;
      inspectorVisible = true;
      show_inspector_panel(currentInspector, snap);
    } else if (snap.hasParamChange || snap.hasADSR1 || snap.hasADSR2 || snap.levelBars) {
      update_inspector_panel(currentInspector, snap);
    }
  }

  if (inspectorVisible) {
    if (now - snap.inspectorActivityTime >= inspectorTimeoutMillis) {
      hide_inspector_panel();
      inspectorVisible = false;
      currentInspector = InspectorType::None;

      screen_state_lock();
      activeInspector = InspectorType::None;
      screen_state_unlock();
    }
  }
}

void SCREEN_HOT(handleScreenModeChange)(const Core1Snapshot &snap) {
  if (!snap.hasSignal) return;

  currentMode = static_cast<ScreenMode>(snap.signalMode);

  switch (currentMode) {
    case ScreenMode::PresetScroll:
      draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
      break;

    case ScreenMode::LoadSaveExit:
      lv_scr_load(ui_Main);
      lv_obj_add_flag(ui_PresetSavePanel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_PresetNewName, LV_OBJ_FLAG_HIDDEN);
      if (ui_calGapTrack) lv_obj_add_flag(ui_calGapTrack, LV_OBJ_FLAG_HIDDEN);
      currentMode = ScreenMode::PresetScroll;
      draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
      break;

    case ScreenMode::SaveSelectPreset: {
      char str[12];
      itoa(snap.presetNum, str, 10);
      lv_textarea_set_text(ui_PresetNewName, snap.presetName);
      lv_label_set_text(ui_PresetNOLD, str);
      lv_label_set_text(ui_PresetNOLDShadow, str);
      lv_label_set_text(ui_PresetNameOLD, snap.presetName);
      lv_label_set_text(ui_PresetNameOLDShadow, snap.presetName);
      draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
      lv_obj_remove_flag(ui_PresetSavePanel, LV_OBJ_FLAG_HIDDEN);
      break;
    }

    case ScreenMode::SaveSetName:
      lv_obj_remove_flag(ui_PresetNewName, LV_OBJ_FLAG_HIDDEN);
      lv_textarea_set_cursor_pos(ui_PresetNewName, 0);
      break;

    case ScreenMode::SaveCompleted:
      lv_obj_add_flag(ui_PresetNewName, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_PresetSavePanel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_width(ui_PresetSavedMesage, 200);
      lv_obj_set_height(ui_PresetSavedMesage, 100);
      lv_obj_align(ui_PresetSavedMesage, LV_ALIGN_CENTER, -20, 0);
      lv_obj_remove_flag(ui_PresetSavedMesage, LV_OBJ_FLAG_HIDDEN);

      saveMessageTimerFlag  = true;
      saveMessageLastMillis = millis();

      currentMode = ScreenMode::PresetScroll;
      draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
      break;

    case ScreenMode::Silent:
      silentModeEnteredMillis = millis();
      break;

    case ScreenMode::CalibrationMenu:
      if (ui_calGapTrack) lv_obj_add_flag(ui_calGapTrack, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_manualCalibrationPanel, LV_OBJ_FLAG_HIDDEN);
      lv_scr_load(ui_MANUALCALIBRATION);
      lv_tabview_set_active(ui_calibrationTabs, snap.calMenuIndex, LV_ANIM_OFF);
      break;

    case ScreenMode::ManualCalibration:
      lv_obj_remove_flag(ui_manualCalibrationPanel, LV_OBJ_FLAG_HIDDEN);
      if (ui_calGapTrack) lv_obj_remove_flag(ui_calGapTrack, LV_OBJ_FLAG_HIDDEN);
      lv_scr_load(ui_MANUALCALIBRATION);
      drawManualCalibration(snap);
      break;

    default:
      break;
  }
}

void SCREEN_HOT(expireSilentMode)(const Core1Snapshot &snap) {
  if (currentMode != ScreenMode::Silent) return;
  if (millis() - silentModeEnteredMillis < silentModeTimeoutMillis) return;

  screen_state_lock();
  if (serialSignal == screen_mode_raw(ScreenMode::Silent)) {
    serialSignal = screen_mode_raw(ScreenMode::PresetScroll);
  }
  screen_state_unlock();

  currentMode = ScreenMode::PresetScroll;
  draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
}

void SCREEN_HOT(updateBottomMessageAndPresetUI)(ScreenMode mode, const Core1Snapshot &snap) {
  if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(ScreenMode::SaveCompleted)) return;

  uint32_t now = millis();

  if (saveMessageTimerFlag) {
    if (now - saveMessageLastMillis > saveMessageHideTimeMillis) {
      lv_obj_add_flag(ui_PresetSavedMesage, LV_OBJ_FLAG_HIDDEN);
      saveMessageTimerFlag = false;

      screen_state_lock();
      if (serialSignal == screen_mode_raw(ScreenMode::SaveCompleted)) {
        serialSignal = screen_mode_raw(ScreenMode::PresetScroll);
      }
      screen_state_unlock();
      mark_u8g2_dirty();
    }
  }

  if (paramChangeTimerFlag) {
    if (now - paramChangeLastMillis > paramHideTimeMillis) {
      lv_obj_add_flag(ui_BottomMessagePanel, LV_OBJ_FLAG_HIDDEN);
      paramChangeTimerFlag = false;
    }
  }

  if (snap.hasPresetChar) {
    lv_textarea_set_cursor_pos(ui_PresetNewName, snap.presetChar);
  }

  if (snap.hasPresetScroll) {
    lv_obj_add_flag(ui_BottomMessagePanel, LV_OBJ_FLAG_HIDDEN);
    draw_preset_scroll_1(mode, snap.presetNum, snap.presetName, snap.presetChar);
    paramChangeTimerFlag = false;
    lv_refr_now(NULL);
  } else if (snap.hasParamChange && snap.paramName && snap.paramName[0] != '\0') {
    draw_param_1(snap.paramName, snap.paramValue);
  }
}

void SCREEN_HOT(updateLevelBars)(ScreenMode mode, const Core1Snapshot &snap) {
  if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(ScreenMode::SaveCompleted) && mode != ScreenMode::Silent) return;
  if (snap.levelBars == 0 && mode != ScreenMode::Silent) return;

  if (mode == ScreenMode::Silent) {
    lv_bar_set_value(ui_OSC1Level, snap.osc1Level, LV_ANIM_OFF);
    lv_bar_set_value(ui_OSC2Level, snap.osc2Level, LV_ANIM_OFF);
    lv_bar_set_value(ui_SUBLevel, snap.subLevel, LV_ANIM_OFF);
  } else {
    if (snap.levelBars & LEVEL_BAR_OSC1) lv_bar_set_value(ui_OSC1Level, snap.osc1Level, LV_ANIM_OFF);
    if (snap.levelBars & LEVEL_BAR_OSC2) lv_bar_set_value(ui_OSC2Level, snap.osc2Level, LV_ANIM_OFF);
    if (snap.levelBars & LEVEL_BAR_SUB) lv_bar_set_value(ui_SUBLevel, snap.subLevel, LV_ANIM_OFF);
  }
}

void SCREEN_HOT(updateADSRBars)(const Core1Snapshot &snap) {
  if (snap.hasADSR1) {
    lv_bar_set_value(ui_ADSR1AttackBar, (snap.a1a >> 5), LV_ANIM_OFF);
    lv_bar_set_value(ui_ADSR1DecayBar, (snap.a1d >> 5), LV_ANIM_OFF);
    lv_bar_set_value(ui_ADSR1SustainBar, (snap.a1s >> 5), LV_ANIM_OFF);
    lv_bar_set_value(ui_ADSR1ReleaseBar, (snap.a1r >> 5), LV_ANIM_OFF);
  }
  if (snap.hasADSR2) {
    lv_bar_set_value(ui_ADSR2AttackBar, (snap.a2a >> 5), LV_ANIM_OFF);
    lv_bar_set_value(ui_ADSR2DecayBar, (snap.a2d >> 5), LV_ANIM_OFF);
    lv_bar_set_value(ui_ADSR2SustainBar, (snap.a2s >> 5), LV_ANIM_OFF);
    lv_bar_set_value(ui_ADSR2ReleaseBar, (snap.a2r >> 5), LV_ANIM_OFF);
  }
}

void SCREEN_HOT(updateCalibrationUI)(ScreenMode mode, const Core1Snapshot &snap) {
  if (mode == ScreenMode::CalibrationMenu) {
    if (snap.calMenuIndex != lastRenderedMenuIndex) {
      lastRenderedMenuIndex = snap.calMenuIndex;
      lv_tabview_set_active(ui_calibrationTabs, snap.calMenuIndex, LV_ANIM_OFF);
    }
    return;
  }

  if (mode == ScreenMode::ManualCalibration) {
    if (snap.calStage != lastRenderedStage || snap.calGap != lastRenderedGap || 
        snap.calOffset != lastRenderedOff || snap.calAmp440 != lastRendered440 || 
        snap.calPwCenter != lastRenderedPw || snap.hasParamChange) {

      lastRenderedStage = snap.calStage;
      lastRenderedGap   = snap.calGap;
      lastRenderedOff   = snap.calOffset;
      lastRendered440   = snap.calAmp440;
      lastRenderedPw    = snap.calPwCenter;

      drawManualCalibration(snap);
    }
  }
}