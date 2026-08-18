#include "Arduino.h"

#ifndef SCREEN_PERF_MONITOR
#define SCREEN_PERF_MONITOR 0
#endif

#define SCREEN_SRAM_HOT 1
#define LGFX_USE_V1
#define LV_COLOR_16_SWAP 0

#include <Adafruit_TinyUSB.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>

#include "src/ui/ui.h"
#include "LGFX_RP2040_FELA.hpp"

#include "sram_hot.h"
#include "Serial.h"
#include "displayParams.h"


//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////  SSD1309 / U8g2 Library - Section
/////////////////////////////////////////////////////////////////////////////////////////////////
#include <U8g2lib.h>
#include <SPI.h>

#include "display_u8g2.h"

//////////////////////////////////////////////////////////////////////////

static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

// Double buffer: 1/8th screen per buffer (38.4KB each in SRAM)
enum { SCREENBUFFER_SIZE_PIXELS = screenWidth * screenHeight / 16 };
static lv_color_t buf1[SCREENBUFFER_SIZE_PIXELS];
static lv_color_t buf2[SCREENBUFFER_SIZE_PIXELS];

LGFX tft; /* TFT instance */

void SCREEN_HOT(my_disp_flush)(lv_display_t *disp, const lv_area_t *area, uint8_t *pixelmap) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)pixelmap, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

static uint32_t SCREEN_HOT(my_tick_get_cb)(void) {
  return millis();
}

uint32_t paramChangeLastMillis = 0;
static ScreenMode currentMode = ScreenMode::PresetScroll;
static const uint32_t silentModeTimeoutMillis = 150;  // Fast 150ms timeout
static uint32_t silentModeEnteredMillis = 0;

/// LOAD FONT TO SRAM
// 1. Declare an SRAM buffer for your font bitmap (adjust size to fit your font, e.g. 16KB)
static uint8_t ram_font_bitmap[16384];  // 16 KB SRAM buffer
static lv_font_fmt_txt_dsc_t ram_font_dsc;
static lv_font_t ram_big_font;


void setup() {
  Serial.begin(2000000);

#if SCREEN_HAS_MB_PEER
  SCREEN_MB_PORT.setRX(SCREEN_MB_RX_PIN);
  SCREEN_MB_PORT.setTX(SCREEN_MB_TX_PIN);
  SCREEN_MB_PORT.setPollingMode(false);
  SCREEN_MB_PORT.setFIFOSize(512);
  SCREEN_MB_PORT.begin(2500000);
#endif
#if SCREEN_HAS_INPUT_PEER
  SCREEN_INPUT_PORT.setRX(SCREEN_INPUT_RX_PIN);
  SCREEN_INPUT_PORT.setTX(SCREEN_INPUT_TX_PIN);
  SCREEN_INPUT_PORT.setPollingMode(false);
  SCREEN_INPUT_PORT.setFIFOSize(512);
  SCREEN_INPUT_PORT.begin(2500000);
#endif
  init_screen_serial();

  init_u8g2();
}

void setup1() {
  lv_init();
  tft.begin();
  tft.setRotation(3);

  static lv_disp_t *disp;
  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_buffers(disp, buf1, buf2, SCREENBUFFER_SIZE_PIXELS * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_tick_set_cb(my_tick_get_cb);

#if LV_USE_PERF_MONITOR && !SCREEN_PERF_MONITOR
  lv_sysmon_hide_performance(disp);
  lv_sysmon_performance_pause(disp);
#endif

  //load_big_font_to_sram(const lv_font_t *flash_font, size_t bitmap_size)
  ui_init();

  lv_obj_set_style_text_opa(ui_PresetN, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  // lv_obj_set_style_text_opa(ui_PresetNShadow, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  // ADD THIS LINE TO HIDE THE SHADOW:
  lv_obj_add_flag(ui_PresetNShadow, LV_OBJ_FLAG_HIDDEN);
}

void loop(void) {
  serial_read_n();

  update_u8g2_core0();
}

// --- SINGLE ATOMIC STATE CAPTURE (< 1 microsecond) ---
static inline void SCREEN_HOT(captureCore1Snapshot)(Core1Snapshot &snap) {
  screen_state_lock();

  // Signals
  snap.hasSignal = signalFlag;
  if (signalFlag) {
    snap.signalMode = serialSignal;
    signalFlag = false;
  }

  // Presets
  snap.hasPresetScroll = presetScrollFlag;
  presetScrollFlag = false;
  snap.presetNum = presetNumber;
  snapshot_preset_name(snap.presetName);

  snap.hasPresetChar = presetCharFlag;
  if (presetCharFlag) {
    snap.presetChar = presetChar;
    presetCharFlag = false;
  } else {
    snap.presetChar = presetChar;
  }

  // Parameters
  snap.hasParamChange = paramChangeFlag;
  paramChangeFlag = false;
  snap.paramName = paramName;
  snap.paramValue = paramValue;
  snap.paramNumber = paramNumber;

  // Level Bars
  snap.levelBars = levelBarFlag;
  levelBarFlag = 0;
  snap.osc1Level = OSC1Level;
  snap.osc2Level = OSC2Level;
  snap.subLevel = SUBLevel;

  // ADSR 1 & 2
  snap.hasADSR1 = updateADSR1Flag;
  updateADSR1Flag = false;
  if (snap.hasADSR1) {
    snap.a1a = ADSR1Attack;
    snap.a1d = ADSR1Decay;
    snap.a1s = ADSR1Sustain;
    snap.a1r = ADSR1Release;
  }

  snap.hasADSR2 = updateADSR2Flag;
  updateADSR2Flag = false;
  if (snap.hasADSR2) {
    snap.a2a = ADSR2Attack;
    snap.a2d = ADSR2Decay;
    snap.a2s = ADSR2Sustain;
    snap.a2r = ADSR2Release;
  }

  // Unconditionally copy calibration values so snap is always 100% valid
  snap.calOffset = offset;
  snap.calAmp440 = ampComp440Display;
  snap.calPwCenter = calPwCenterDisplay;
  snap.calOscN = manualCalibrationOSCN;
  snap.calStage = manualCalibrationStage;
  snap.calGap = calibrationGap;
  snap.calTopology = screenCalTopology;

  screen_state_unlock();
}

// Lock-Free Mode Handler
static void SCREEN_HOT(handleScreenModeChange)(const Core1Snapshot &snap) {
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
      currentMode = ScreenMode::PresetScroll;
      draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
      break;

    case ScreenMode::SaveSelectPreset:
      {
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
      paramChangeTimerFlag = true;
      paramChangeLastMillis = millis();
      currentMode = ScreenMode::PresetScroll;
      draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
      break;

    case ScreenMode::Silent:
      silentModeEnteredMillis = millis();
      break;

    case ScreenMode::CalibrationMenu:
      lv_obj_add_flag(ui_manualCalibrationPanel, LV_OBJ_FLAG_HIDDEN);
      lv_scr_load(ui_MANUALCALIBRATION);
      break;

    case ScreenMode::ManualCalibration:
      lv_obj_remove_flag(ui_manualCalibrationPanel, LV_OBJ_FLAG_HIDDEN);
      lv_scr_load(ui_MANUALCALIBRATION);
      drawManualCalibration(snap);
      break;

    default:
      break;
  }
}

// Watchdog with Core 0 State Synchronization
static void SCREEN_HOT(expireSilentMode)(const Core1Snapshot &snap) {
  if (currentMode != ScreenMode::Silent) return;
  if (millis() - silentModeEnteredMillis < silentModeTimeoutMillis) return;

  // Sync state back to shared memory so Core 0 knows silence ended
  screen_state_lock();
  if (serialSignal == screen_mode_raw(ScreenMode::Silent)) {
    serialSignal = screen_mode_raw(ScreenMode::PresetScroll);
  }
  screen_state_unlock();

  currentMode = ScreenMode::PresetScroll;
  draw_preset_scroll_1(currentMode, snap.presetNum, snap.presetName, snap.presetChar);
}

// Lock-Free Preset & Toast UI
static void SCREEN_HOT(updateBottomMessageAndPresetUI)(ScreenMode mode, const Core1Snapshot &snap) {
  if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(ScreenMode::SaveCompleted)) {
    return;
  }

  if (paramChangeTimerFlag) {
    if (millis() - paramChangeLastMillis > paramHideTimeMillis) {
      lv_obj_add_flag(ui_BottomMessagePanel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_PresetSavedMesage, LV_OBJ_FLAG_HIDDEN);
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
    lv_refr_now(NULL);  // Instant hardware refresh
  } else if (snap.hasParamChange && snap.paramName && snap.paramName[0] != '\0') {
    draw_param_1(snap.paramName, snap.paramValue);
  }
}

// Lock-Free Level Bars
static void SCREEN_HOT(updateLevelBars)(ScreenMode mode, const Core1Snapshot &snap) {
  if (snap.levelBars == 0 && mode != ScreenMode::Silent) return;

  if (mode == ScreenMode::Silent) {
    lv_bar_set_value(ui_OSC1Level, snap.osc1Level, LV_ANIM_ON);
    lv_bar_set_value(ui_OSC2Level, snap.osc2Level, LV_ANIM_ON);
    lv_bar_set_value(ui_SUBLevel, snap.subLevel, LV_ANIM_ON);
  } else {
    if (snap.levelBars & LEVEL_BAR_OSC1) lv_bar_set_value(ui_OSC1Level, snap.osc1Level, LV_ANIM_ON);
    if (snap.levelBars & LEVEL_BAR_OSC2) lv_bar_set_value(ui_OSC2Level, snap.osc2Level, LV_ANIM_ON);
    if (snap.levelBars & LEVEL_BAR_SUB) lv_bar_set_value(ui_SUBLevel, snap.subLevel, LV_ANIM_ON);
  }
}

// Lock-Free ADSR Bars (uses fast bitshift >> 5 instead of float multiplication)
static void SCREEN_HOT(updateADSRBars)(const Core1Snapshot &snap) {
  if (snap.hasADSR1) {
    lv_bar_set_value(ui_ADSR1AttackBar, (snap.a1a >> 5), LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR1DecayBar, (snap.a1d >> 5), LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR1SustainBar, (snap.a1s >> 5), LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR1ReleaseBar, (snap.a1r >> 5), LV_ANIM_ON);
  }
  if (snap.hasADSR2) {
    lv_bar_set_value(ui_ADSR2AttackBar, (snap.a2a >> 5), LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR2DecayBar, (snap.a2d >> 5), LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR2SustainBar, (snap.a2s >> 5), LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR2ReleaseBar, (snap.a2r >> 5), LV_ANIM_ON);
  }
}

// Lock-Free Calibration UI
static void SCREEN_HOT(updateCalibrationUI)(ScreenMode mode, const Core1Snapshot &snap) {
  if (mode != ScreenMode::CalibrationMenu && mode != ScreenMode::ManualCalibration) return;
  if (!snap.hasParamChange) return;

  if (mode == ScreenMode::CalibrationMenu) {
    if (snap.paramNumber == static_cast<uint8_t>(ParamId::PARAM_UI_MENU_POSITION)) {
      lv_tabview_set_active(ui_calibrationTabs, snap.paramValue, LV_ANIM_ON);
    }
  } else {
    drawManualCalibration(snap);
  }
}

// Core 1 Main Loop
void SCREEN_HOT(loop1)(void) {
  Core1Snapshot snap;

  // 1. Capture ALL shared data in ONE atomic lock (< 1 microsecond)
  captureCore1Snapshot(snap);

  // 2. Perform all UI updates completely lock-free
  handleScreenModeChange(snap);
  expireSilentMode(snap);
  updateBottomMessageAndPresetUI(currentMode, snap);
  updateLevelBars(currentMode, snap);
  updateADSRBars(snap);
  updateCalibrationUI(currentMode, snap);

  // 3. Render and sleep efficiently
  uint32_t time_till_next = lv_timer_handler();
  if (time_till_next > 5) time_till_next = 5;
  if (time_till_next == 0) time_till_next = 1;
  delay(time_till_next);
}

void load_big_font_to_sram(const lv_font_t *flash_font, size_t bitmap_size) {
  const lv_font_fmt_txt_dsc_t *flash_dsc = (const lv_font_fmt_txt_dsc_t *)flash_font->dsc;

  // Copy font bitmap data from Flash to SRAM
  memcpy(ram_font_bitmap, flash_dsc->glyph_bitmap, bitmap_size);

  // Clone the descriptor and point its bitmap pointer to SRAM
  ram_font_dsc = *flash_dsc;
  ram_font_dsc.glyph_bitmap = ram_font_bitmap;

  // Clone the font and point to our new RAM descriptor
  ram_big_font = *flash_font;
  ram_big_font.dsc = &ram_font_dsc;

  // Re-assign the new SRAM font to your big preset number labels
  lv_obj_set_style_text_font(ui_PresetN, &ram_big_font, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_PresetNShadow, &ram_big_font, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_PresetNNew, &ram_big_font, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_PresetNNewShadow, &ram_big_font, LV_PART_MAIN | LV_STATE_DEFAULT);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////

  // Force solid opacity on drop shadow to save CPU rasterization cycles
  lv_obj_set_style_text_opa(ui_PresetNShadow, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_PresetNShadow, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Textarea cursor animations
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_CURSOR | LV_STATE_DEFAULT);
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_CURSOR | LV_STATE_FOCUSED);
}
