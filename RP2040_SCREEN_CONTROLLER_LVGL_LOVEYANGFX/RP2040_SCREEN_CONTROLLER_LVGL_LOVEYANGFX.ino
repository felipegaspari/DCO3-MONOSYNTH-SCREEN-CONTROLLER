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
#include "display_u8g2.h"
#include "menues.h"
#include "ui_views.h"

static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

enum { SCREENBUFFER_SIZE_PIXELS = screenWidth * screenHeight / 8 };
static lv_color_t buf1[SCREENBUFFER_SIZE_PIXELS];
static lv_color_t buf2[SCREENBUFFER_SIZE_PIXELS];

LGFX tft;

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

// --- SINGLE ATOMIC STATE CAPTURE (< 1 microsecond) ---
static inline void SCREEN_HOT(captureCore1Snapshot)(Core1Snapshot &snap) {
  screen_state_lock();

  if (serialSignal == screen_mode_raw(ScreenMode::Silent)) {
    paramChangeFlag      = false;
    paramChangeTimerFlag = false;
    activeInspector      = InspectorType::None;
    inspectorActiveFlag  = false;
  }

  // Signals & Presets
  snap.hasSignal = signalFlag;
  if (signalFlag) {
    snap.signalMode = serialSignal;
    signalFlag = false;
  }

  snap.hasPresetScroll = presetScrollFlag;
  presetScrollFlag = false;
  snap.presetNum = presetNumber;
  snapshot_preset_name(snap.presetName);

  snap.hasPresetChar = presetCharFlag;
  snap.presetChar = presetChar;
  presetCharFlag = false;

  // Parameters
  snap.hasParamChange = paramChangeFlag;
  paramChangeFlag = false;
  snap.paramName = paramName;
  snap.paramValue = paramValue;
  snap.paramNumber = paramNumber;

  // Inspector Shell Capture
  snap.inspectorType         = activeInspector;
  snap.hasInspectorChange    = inspectorActiveFlag;
  snap.inspectorActivityTime = inspectorLastActivityMillis;
  inspectorActiveFlag        = false;

  // Level Bars & ADSRs
  snap.levelBars = levelBarFlag;
  levelBarFlag = 0;
  snap.osc1Level = OSC1Level;
  snap.osc2Level = OSC2Level;
  snap.subLevel = SUBLevel;

  snap.hasADSR1 = updateADSR1Flag;
  updateADSR1Flag = false;
  snap.a1a = ADSR1Attack; snap.a1d = ADSR1Decay; snap.a1s = ADSR1Sustain; snap.a1r = ADSR1Release;

  snap.hasADSR2 = updateADSR2Flag;
  updateADSR2Flag = false;
  snap.a2a = ADSR2Attack; snap.a2d = ADSR2Decay; snap.a2s = ADSR2Sustain; snap.a2r = ADSR2Release;

  // Calibration
  snap.calMenuIndex = calibrationMenuIndex;
  snap.calOffset = offset;
  snap.calAmp440 = ampComp440Display;
  snap.calPwCenter = calPwCenterDisplay;
  snap.calOscN = manualCalibrationOSCN;
  snap.calStage = manualCalibrationStage;
  snap.calGap = calibrationGap;
  snap.calTopology = screenCalTopology;

  // Menu Navigation
  snap.navMenuIndex = navMenuIndex;
  snap.activeMenuMode = activeMenuMode;

  screen_state_unlock();
}

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

  ui_init();
  setupMenues();

  lv_obj_set_style_text_opa(ui_PresetN, LV_OPA_COVER, (lv_style_selector_t)(LV_PART_MAIN | LV_STATE_DEFAULT));
  lv_obj_add_flag(ui_PresetNShadow, LV_OBJ_FLAG_HIDDEN);
}

void loop(void) {
  serial_read_n();
  update_u8g2_core0();
}

// Core 1 Main Render Loop
void SCREEN_HOT(loop1)(void) {
  Core1Snapshot snap;

  captureCore1Snapshot(snap);

  handleScreenModeChange(snap);
  expireSilentMode(snap);
  updateBottomMessageAndPresetUI(currentMode, snap);
  updateInspectorLifecycle(currentMode, snap);
  updateLevelBars(currentMode, snap);
  updateADSRBars(snap);
  updateCalibrationUI(currentMode, snap);
  updateGenericFocus(snap);

  uint32_t time_till_next = lv_timer_handler();
  if (time_till_next > 5) time_till_next = 5;
  if (time_till_next == 0) time_till_next = 1;
  delay(time_till_next);
}