#include "Arduino.h"

// 1 = show LVGL FPS/CPU overlay (bottom-right). 0 = hide + pause (shipping).
// Requires LV_USE_PERF_MONITOR=1 in libraries/lv_conf.h (always on for the API).
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


static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

enum { SCREENBUFFER_SIZE_PIXELS = screenWidth * screenHeight / 10 };
static lv_color_t buf[SCREENBUFFER_SIZE_PIXELS];

LGFX tft; /* TFT instance */

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing: push LVGL dirty area to LovyanGFX, then mark flush ready. */
void SCREEN_HOT(my_disp_flush)(lv_display_t *disp, const lv_area_t *area, uint8_t *pixelmap) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  if (LV_COLOR_16_SWAP) {
    size_t len = lv_area_get_size(area);
    lv_draw_sw_rgb565_swap(pixelmap, len);
  }

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)pixelmap, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/* LVGL tick source: Arduino millis(). */
static uint32_t SCREEN_HOT(my_tick_get_cb)(void) {
  return millis();
}

uint32_t paramChangeLastMillis = 0;

// Core1-owned screen mode. Each 's' signal (or apply_param_* mode change) raises
// signalFlag on Core0 and Core1 latches the new mode here in
// handleScreenModeChange(). The silence watchdog below is the one place Core1
// writes serialSignal back, and only when no fresher signal is pending.
static ScreenMode currentMode = ScreenMode::PresetScroll;

// Silence is opened and closed by a pair of 's' frames around a preset recall
// (DCO preset_store_load). Losing the closing one would leave the UI ignoring
// every param frame from then on, so time it out: longer than a recall's mirror
// burst, short enough that a dropped marker reads as a hiccup and not a hang.
static const uint32_t silentModeTimeoutMillis = 100;
static uint32_t silentModeEnteredMillis = 0;

#if defined(NO_USB) || defined(DISABLE_USB_SERIAL)
#error This sketch needs USB Serial (Serial.begin). Arduino IDE: Tools -> USB Stack -> Pico SDK (not No USB).
#endif

/////////////////////////////////////////////////////////// setup ///////////////////////////////////////////////////
// Core0 boot: USB debug Serial + peer UART(s). See Serial.h pin map.
// DCO3: Serial1 GP13 ← Input.
// DCO4: Serial2 GP21 ← Mainboard PA9, and Serial1 GP13 ← Input.
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

  // USBDevice.setManufacturerDescriptor("FELA         ");   /// Why doesnt it work?
  // USBDevice.setProductDescriptor("DCO4 Screen Controller       ");
}


// Core1 boot: LVGL + LovyanGFX display + SquareLine ui_init.
void setup1() {
  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  tft.begin();        /* TFT init */
  tft.setRotation(3); /* Landscape orientation, flipped */

  static lv_disp_t *disp;
  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_buffers(disp, buf, NULL, SCREENBUFFER_SIZE_PIXELS * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, my_disp_flush);

  lv_tick_set_cb(my_tick_get_cb);

#if LV_USE_PERF_MONITOR && !SCREEN_PERF_MONITOR
  // lv_display_create auto-shows the sysmon label; hide + pause for shipping.
  lv_sysmon_hide_performance(disp);
  lv_sysmon_performance_pause(disp);
#endif

  ui_init();
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_CURSOR | LV_STATE_DEFAULT);
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_anim_time(ui_PresetNewName, 140, LV_PART_CURSOR | LV_STATE_FOCUSED);
}

/////////////////////////////////////////////////////////// loop ///////////////////////////////////////////////////
// Core0 hot path: drain peer UART parser(s).
void loop(void) {
  serial_read_n();
}

// --- Helpers for loop1() ---

// Consume signalFlag: latch the new mode and load LVGL screens / show-hide
// panels for it. The flag is consumed atomically together with serialSignal
// so a signal arriving mid-update is never lost.
static void SCREEN_HOT(handleScreenModeChange)() {
  bool pending = false;
  uint8_t rawMode = 0;
  screen_state_lock();
  if (signalFlag) {
    rawMode    = serialSignal;
    signalFlag = false;
    pending    = true;
  }
  screen_state_unlock();
  if (!pending) {
    return;
  }

  currentMode = static_cast<ScreenMode>(rawMode);

  switch (currentMode) {
    case ScreenMode::PresetScroll:
      draw_preset_scroll_1(currentMode);
      break;

    case ScreenMode::LoadSaveExit:
      lv_scr_load(ui_Main);
      lv_obj_add_flag(ui_PresetSavePanel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_PresetNewName, LV_OBJ_FLAG_HIDDEN);
      currentMode = ScreenMode::PresetScroll;
      draw_preset_scroll_1(currentMode);
      break;

    case ScreenMode::SaveSelectPreset: {
      // SAVE MODE - Select destination preset
      char name[17];
      byte num;
      screen_state_lock();
      num = presetNumber;
      snapshot_preset_name(name);
      screen_state_unlock();

      char str[12];
      itoa(num, str, 10);
      lv_textarea_set_text(ui_PresetNewName, name);
      lv_label_set_text(ui_PresetNOLD, str);
      lv_label_set_text(ui_PresetNOLDShadow, str);
      lv_label_set_text(ui_PresetNameOLD, name);
      lv_label_set_text(ui_PresetNameOLDShadow, name);
      draw_preset_scroll_1(currentMode);
      lv_obj_remove_flag(ui_PresetSavePanel, LV_OBJ_FLAG_HIDDEN);
      break;
    }

    case ScreenMode::SaveSetName:
      // SAVE MODE - set preset name
      screen_state_lock();
      presetChar = 0;
      screen_state_unlock();
      lv_obj_remove_flag(ui_PresetNewName, LV_OBJ_FLAG_HIDDEN);
      lv_textarea_set_cursor_pos(ui_PresetNewName, 0);
      break;

    case ScreenMode::SaveCompleted:
      // PRESET SAVED
      lv_obj_add_flag(ui_PresetNewName, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_PresetSavePanel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_PresetSavedMesage, LV_OBJ_FLAG_HIDDEN);
      paramChangeTimerFlag  = true;
      paramChangeLastMillis = millis();
      currentMode = ScreenMode::PresetScroll;
      draw_preset_scroll_1(currentMode);
      break;

    case ScreenMode::Silent:
      // SCREEN SILENCE - no immediate action, only the watchdog below
      silentModeEnteredMillis = millis();
      break;

    case ScreenMode::CalibrationMenu:
      lv_obj_add_flag(ui_manualCalibrationPanel, LV_OBJ_FLAG_HIDDEN);
      lv_scr_load(ui_MANUALCALIBRATION);
      break;

    case ScreenMode::ManualCalibration:
      lv_obj_remove_flag(ui_manualCalibrationPanel, LV_OBJ_FLAG_HIDDEN);
      lv_scr_load(ui_MANUALCALIBRATION);
      drawManualCalibration();  // initial render; updates are flag-driven
      break;

    default:
      break;
  }
}

// Watchdog for a lost end-of-silence marker (see silentModeTimeoutMillis). Runs
// right after handleScreenModeChange, so signalFlag has just been drained: if it
// is set again a newer signal beat us to it and wins.
static void SCREEN_HOT(expireSilentMode)() {
  if (currentMode != ScreenMode::Silent) return;
  if (millis() - silentModeEnteredMillis < silentModeTimeoutMillis) return;

  bool expired = false;
  screen_state_lock();
  if (!signalFlag && serialSignal == screen_mode_raw(ScreenMode::Silent)) {
    serialSignal = screen_mode_raw(ScreenMode::PresetScroll);
    expired      = true;
  }
  screen_state_unlock();

  if (expired) {
    currentMode = ScreenMode::PresetScroll;
    draw_preset_scroll_1(currentMode);
  }
}

// Modes ≤5: hide timed toasts; update preset scroll / param label / name cursor.
static void SCREEN_HOT(updateBottomMessageAndPresetUI)(ScreenMode mode) {
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

  bool charSelected = false;
  bool scrolled     = false;
  bool paramChanged = false;
  uint8_t charPos   = 0;
  screen_state_lock();
  if (presetCharFlag) {
    presetCharFlag = false;
    charSelected   = true;
    charPos        = presetChar;
  }
  if (presetScrollFlag) {
    presetScrollFlag = false;
    scrolled         = true;
  }
  if (paramChangeFlag) {
    paramChangeFlag = false;
    paramChanged    = true;
  }
  screen_state_unlock();

  if (charSelected) {
    lv_textarea_set_cursor_pos(ui_PresetNewName, charPos);
  }
  if (scrolled) {
    lv_obj_add_flag(ui_BottomMessagePanel, LV_OBJ_FLAG_HIDDEN);
    draw_preset_scroll_1(mode);
  }
  if (paramChanged) {
    draw_param_1();
  }
}

// Push OSC1/OSC2/SUB level bars for pending levelBarFlag bits (all in Silent).
static void SCREEN_HOT(updateLevelBars)(ScreenMode mode) {
  uint8_t bars;
  uint8_t osc1, osc2, sub;
  screen_state_lock();
  bars         = levelBarFlag;
  levelBarFlag = 0;
  osc1         = OSC1Level;
  osc2         = OSC2Level;
  sub          = SUBLevel;
  screen_state_unlock();

  if (bars == 0) {
    return;
  }

  if (mode == ScreenMode::Silent) {
    // In silent mode, always update all three bars.
    lv_bar_set_value(ui_OSC1Level, osc1, LV_ANIM_ON);
    lv_bar_set_value(ui_OSC2Level, osc2, LV_ANIM_ON);
    lv_bar_set_value(ui_SUBLevel, sub, LV_ANIM_ON);
  } else {
    if (bars & LEVEL_BAR_OSC1) {
      lv_bar_set_value(ui_OSC1Level, osc1, LV_ANIM_ON);
    }
    if (bars & LEVEL_BAR_OSC2) {
      lv_bar_set_value(ui_OSC2Level, osc2, LV_ANIM_ON);
    }
    if (bars & LEVEL_BAR_SUB) {
      lv_bar_set_value(ui_SUBLevel, sub, LV_ANIM_ON);
    }
  }
}

// Refresh ADSR1/ADSR2 bar widgets when update flags are set from serial.
static void SCREEN_HOT(updateADSRBars)() {
  bool adsr1 = false, adsr2 = false;
  uint16_t a1a, a1d, a1s, a1r;
  uint16_t a2a, a2d, a2s, a2r;
  screen_state_lock();
  if (updateADSR1Flag) {
    updateADSR1Flag = false;
    adsr1 = true;
    a1a = ADSR1Attack;
    a1d = ADSR1Decay;
    a1s = ADSR1Sustain;
    a1r = ADSR1Release;
  }
  if (updateADSR2Flag) {
    updateADSR2Flag = false;
    adsr2 = true;
    a2a = ADSR2Attack;
    a2d = ADSR2Decay;
    a2s = ADSR2Sustain;
    a2r = ADSR2Release;
  }
  screen_state_unlock();

  if (adsr1) {
    lv_bar_set_value(ui_ADSR1AttackBar, 0.03125f * a1a, LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR1DecayBar, 0.03125f * a1d, LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR1SustainBar, 0.03125f * a1s, LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR1ReleaseBar, 0.03125f * a1r, LV_ANIM_ON);
  }
  if (adsr2) {
    lv_bar_set_value(ui_ADSR2AttackBar, 0.03125f * a2a, LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR2DecayBar, 0.03125f * a2d, LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR2SustainBar, 0.03125f * a2s, LV_ANIM_ON);
    lv_bar_set_value(ui_ADSR2ReleaseBar, 0.03125f * a2r, LV_ANIM_ON);
  }
}

// Calibration menu tabs / manual calibration panel redraw for modes 7–8.
// Flag-driven: only touches LVGL when a param update arrived since last loop.
static void SCREEN_HOT(updateCalibrationUI)(ScreenMode mode) {
  if (mode != ScreenMode::CalibrationMenu && mode != ScreenMode::ManualCalibration) {
    return;
  }

  bool changed = false;
  uint8_t id   = 0;
  int32_t value = 0;
  screen_state_lock();
  if (paramChangeFlag) {
    paramChangeFlag = false;
    changed = true;
    id      = paramNumber;
    value   = paramValue;
  }
  screen_state_unlock();
  if (!changed) {
    return;
  }

  if (mode == ScreenMode::CalibrationMenu) {
    if (id == static_cast<uint8_t>(ParamId::PARAM_UI_MENU_POSITION)) {
      lv_tabview_set_active(ui_calibrationTabs, value, LV_ANIM_ON);
    }
  } else {
    drawManualCalibration();
  }
}

// Core1 hot path: apply serial-driven UI updates, then LVGL timer handler.
void SCREEN_HOT(loop1)(void) {
  handleScreenModeChange();
  expireSilentMode();
  updateBottomMessageAndPresetUI(currentMode);
  updateLevelBars(currentMode);
  updateADSRBars();
  updateCalibrationUI(currentMode);

  lv_timer_handler();
  delay(2);  // try if it works
}
