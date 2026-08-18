#include "display_u8g2.h"
#include <hardware/spi.h>
#include <hardware/gpio.h>

static uint8_t u8x8_byte_pico_hw_spi1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_BYTE_SEND:
      spi_write_blocking(spi1, (const uint8_t *)arg_ptr, arg_int);
      break;
    case U8X8_MSG_BYTE_INIT:
      break;
    case U8X8_MSG_BYTE_SET_DC:
      gpio_put(OLED_DC_PIN, arg_int);
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      gpio_put(OLED_CS_PIN, 0);
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      gpio_put(OLED_CS_PIN, 1);
      break;
    default:
      return 0;
  }
  return 1;
}

static uint8_t u8x8_gpio_pico_dummy(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
      delay(arg_int);
      break;
    case U8X8_MSG_DELAY_10MICRO:
      delayMicroseconds(arg_int * 10);
      break;
    case U8X8_MSG_DELAY_100NANO:
      delayMicroseconds(1);
      break;
    default:
      break;
  }
  return 1;
}

U8G2_SSD1309_PICO_SPI1::U8G2_SSD1309_PICO_SPI1(const u8g2_cb_t *rotation) : U8G2() {
  u8g2_Setup_ssd1309_128x64_noname0_f(&u8g2, rotation, u8x8_byte_pico_hw_spi1, u8x8_gpio_pico_dummy);
}

U8G2_SSD1309_PICO_SPI1 u8g2(U8G2_R0);

static bool oledDirty = true;
static uint32_t lastOledRefreshMillis = 0;
static constexpr uint32_t MIN_OLED_INTERVAL_MS = 20; // 50 FPS max cap

// Persistent State Tracking
static uint8_t  lastPresetNum = 255;
static ScreenMode lastScreenMode = ScreenMode::PresetScroll;
static uint8_t  currentCalMenuIndex = 0;
static uint8_t  lastCalMenuIndex = 255;
static uint8_t  lastCalStage = 255;
static int32_t  lastCalGap = -999999;
static uint8_t  lastO1 = 255, lastO2 = 255, lastSub = 255;
static uint16_t lastA1 = 65535, lastD1 = 65535, lastS1 = 65535, lastR1 = 65535;
static uint16_t lastA2 = 65535, lastD2 = 65535, lastS2 = 65535, lastR2 = 65535;

void mark_u8g2_dirty() {
  oledDirty = true;
}

void init_u8g2() {
  gpio_set_function(OLED_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(OLED_MOSI_PIN, GPIO_FUNC_SPI);

  gpio_init(OLED_CS_PIN);
  gpio_set_dir(OLED_CS_PIN, GPIO_OUT);
  gpio_put(OLED_CS_PIN, 1);

  gpio_init(OLED_DC_PIN);
  gpio_set_dir(OLED_DC_PIN, GPIO_OUT);
  gpio_put(OLED_DC_PIN, 1);

  spi_init(spi1, 16000000);

  u8g2.initDisplay();
  u8g2.setPowerSave(0);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline void draw_meter_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t val, uint8_t maxVal = 255) {
  u8g2.drawFrame(x, y, w, h);
  if (val > 0 && maxVal > 0) {
    uint8_t fillW = (uint16_t)(val * (w - 2)) / maxVal;
    if (fillW > (w - 2)) fillW = w - 2;
    if (fillW > 0) u8g2.drawBox(x + 1, y + 1, fillW, h - 2);
  }
}

static void draw_dynamic_adsr(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                             uint16_t a, uint16_t d, uint16_t s, uint16_t r, const char* label) {
  u8g2.drawFrame(x, y, w, h);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(x + 2, y + 6, label);

  uint8_t innerW = w - 16;
  uint8_t innerH = h - 4;
  uint8_t bx = x + 14;
  uint8_t by = y + 2;

  uint8_t aW = 2 + ((uint32_t)a * (innerW / 3)) / 1024;
  uint8_t dW = 2 + ((uint32_t)d * (innerW / 3)) / 1024;
  uint8_t rW = 2 + ((uint32_t)r * (innerW / 3)) / 1024;

  uint8_t totalVar = aW + dW + rW;
  if (totalVar > innerW - 4) {
    uint8_t scale = totalVar;
    aW = (aW * (innerW - 4)) / scale;
    dW = (dW * (innerW - 4)) / scale;
    rW = (rW * (innerW - 4)) / scale;
  }
  uint8_t sW = innerW - (aW + dW + rW);

  uint8_t sH = ((uint32_t)s * innerH) / 1024;
  if (sH > innerH) sH = innerH;

  uint8_t p0_x = bx;
  uint8_t p0_y = by + innerH;
  uint8_t p1_x = p0_x + aW;
  uint8_t p1_y = by;
  uint8_t p2_x = p1_x + dW;
  uint8_t p2_y = by + (innerH - sH);
  uint8_t p3_x = p2_x + sW;
  uint8_t p3_y = p2_y;
  uint8_t p4_x = bx + innerW;
  uint8_t p4_y = by + innerH;

  u8g2.drawLine(p0_x, p0_y, p1_x, p1_y);
  u8g2.drawLine(p1_x, p1_y, p2_x, p2_y);
  u8g2.drawLine(p2_x, p2_y, p3_x, p3_y);
  u8g2.drawLine(p3_x, p3_y, p4_x, p4_y);
}

// ---------------------------------------------------------------------------
// VIEW 1: Main Preset Dashboard
// ---------------------------------------------------------------------------
static void draw_view_preset(uint8_t pNum, const char* pName, uint8_t o1, uint8_t o2, uint8_t sub,
                             uint16_t a1, uint16_t d1, uint16_t s1, uint16_t r1,
                             uint16_t a2, uint16_t d2, uint16_t s2, uint16_t r2,
                             bool toastActive, const char* pToastName, int32_t pToastVal, CalTopology topo) {
  u8g2.setFont(u8g2_font_6x10_tf);
  char header[24];
  snprintf(header, sizeof(header), "P%02u: %s", pNum, pName);
  u8g2.drawStr(2, 9, header);
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(2, 21, "O1");
  draw_meter_bar(14, 15, 38, 7, o1);

  u8g2.drawStr(2, 31, "O2");
  draw_meter_bar(14, 25, 38, 7, o2);

  u8g2.drawStr(2, 41, "SB");
  draw_meter_bar(14, 35, 38, 7, sub);

  draw_dynamic_adsr(56, 14, 70, 15, a1, d1, s1, r1, "E1");
  draw_dynamic_adsr(56, 31, 70, 15, a2, d2, s2, r2, "E2");

  u8g2.drawHLine(0, 48, 128);

  if (toastActive && pToastName != nullptr) {
    u8g2.setFont(u8g2_font_6x10_tf);
    char toast[32];
    snprintf(toast, sizeof(toast), "%s: %ld", pToastName, (long)pToastVal);
    u8g2.drawStr(2, 60, toast);
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* topStr = (topo == CalTopology::Voices4x2) ? "DCO4 [4x2 VOICES]" : "DCO3 [MONO 3-OSC]";
    u8g2.drawStr(2, 59, topStr);
  }
}

// ---------------------------------------------------------------------------
// VIEW 2: Calibration Menu (Matches ui_calibrationTabs on main display)
// ---------------------------------------------------------------------------
static void draw_view_cal_menu(uint8_t tabIndex, CalTopology topo) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "CALIBRATION TABS");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  char posHeader[12];
  snprintf(posHeader, sizeof(posHeader), "TAB %u", tabIndex + 1);
  u8g2.drawStr(96, 9, posHeader);
  u8g2.drawHLine(0, 11, 128);

  // Tab Definitions matching the synth tabs
  static const char* calTabsDCO4[] = {
    "1. OSC TUNING (4x2)",
    "2. 440Hz AMP COMP",
    "3. PW CENTER CAL",
    "4. AUTO CALIBRATE",
    "5. RESTORE DEFAULTS"
  };
  static const char* calTabsDCO3[] = {
    "1. OSC 1-3 TUNING",
    "2. 440Hz AMP COMP",
    "3. PW CENTER CAL",
    "4. AUTO CALIBRATE",
    "5. RESTORE DEFAULTS"
  };

  const char** tabs = (topo == CalTopology::Voices4x2) ? calTabsDCO4 : calTabsDCO3;
  const uint8_t totalTabs = 5;
  uint8_t activeTab = tabIndex % totalTabs;

  // Windowed list to fit 3 visible tabs on 128x64 display
  uint8_t startIdx = 0;
  if (activeTab >= 2) {
    startIdx = activeTab - 1;
    if (startIdx + 3 > totalTabs) startIdx = totalTabs - 3;
  }

  u8g2.setFont(u8g2_font_6x10_tf);
  for (uint8_t i = 0; i < 3; ++i) {
    uint8_t idx = startIdx + i;
    uint8_t y = 24 + (i * 11);

    if (idx == activeTab) {
      u8g2.drawBox(2, y - 9, 124, 11);
      u8g2.setDrawColor(0);
      u8g2.drawStr(4, y, tabs[idx]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(4, y, tabs[idx]);
    }
  }

  u8g2.drawHLine(0, 52, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(2, 60, "ROTATE: SCROLL TABS");
  u8g2.drawStr(80, 60, "PUSH: SELECT");
}

// ---------------------------------------------------------------------------
// VIEW 3: Manual Calibration Active Tuner
// ---------------------------------------------------------------------------
static void draw_view_manual_cal(CalTopology topo, uint8_t stage, int32_t gap, int8_t off, uint16_t a440, uint16_t pw) {
  char toastBuf[24];
  screen_cal_format_toast(topo, stage, toastBuf, sizeof(toastBuf));

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, toastBuf);
  u8g2.drawHLine(0, 11, 128);

  u8g2.drawFrame(14, 15, 100, 10);
  u8g2.drawVLine(64, 13, 14);

  int32_t clampedGap = gap;
  if (clampedGap < -50) clampedGap = -50;
  if (clampedGap > 50)  clampedGap = 50;
  uint8_t markerX = 64 + (clampedGap * 46 / 50);
  u8g2.drawBox(markerX - 2, 17, 5, 6);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[32];
  snprintf(buf, sizeof(buf), "GAP: %ld", (long)gap);
  u8g2.drawStr(4, 34, buf);
  snprintf(buf, sizeof(buf), "OFF: %d", off);
  u8g2.drawStr(70, 34, buf);
  snprintf(buf, sizeof(buf), "440: %u", a440);
  u8g2.drawStr(4, 45, buf);
  snprintf(buf, sizeof(buf), "PW: %u", pw);
  u8g2.drawStr(70, 45, buf);

  u8g2.drawHLine(0, 49, 128);
  snprintf(buf, sizeof(buf), "STAGE %u / %u", stage + 1, screen_cal_stage_max(topo) + 1);
  u8g2.drawStr(28, 59, buf);
}

static void draw_view_save_select(uint8_t pNum, const char* pName) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(12, 12, "--- SAVE PRESET ---");
  u8g2.drawHLine(0, 15, 128);

  char buf[24];
  snprintf(buf, sizeof(buf), "Target: P%02u", pNum);
  u8g2.drawStr(16, 32, buf);
  snprintf(buf, sizeof(buf), "\"%s\"", pName);
  u8g2.drawStr(16, 46, buf);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(8, 60, "Turn knob to select slot");
}

static void draw_view_save_name(const char* pName, uint8_t pChar) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(16, 12, "--- EDIT NAME ---");
  u8g2.drawHLine(0, 15, 128);

  uint8_t startX = 16;
  uint8_t startY = 34;
  u8g2.drawFrame(12, 22, 104, 18);

  for (uint8_t i = 0; i < 16; ++i) {
    char c = pName[i] ? pName[i] : ' ';
    char s[2] = {c, '\0'};
    uint8_t charX = startX + (i * 6);

    if (pChar == i) {
      u8g2.drawBox(charX - 1, startY - 8, 7, 10);
      u8g2.setDrawColor(0);
      u8g2.drawStr(charX, startY, s);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(charX, startY, s);
    }
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  char posBuf[16];
  snprintf(posBuf, sizeof(posBuf), "Cursor: %u/16", pChar + 1);
  u8g2.drawStr(36, 56, posBuf);
}

// ---------------------------------------------------------------------------
// Core 0 Non-Blocking Event-Driven Refresh
// ---------------------------------------------------------------------------
void update_u8g2_core0() {
  uint32_t now = millis();

  // Handle toast timeout
  if (paramChangeTimerFlag) {
    if (now - paramChangeLastMillis >= paramHideTimeMillis) {
      paramChangeTimerFlag = false;
      oledDirty = true;
    }
  }

  // Rate limiter
  if (now - lastOledRefreshMillis < MIN_OLED_INTERVAL_MS) {
    return;
  }

  // Quick State Check under Lock
  uint8_t pNum;
  char pName[17];
  uint8_t pChar;
  uint8_t o1, o2, sub;
  uint16_t a1, d1, s1, r1;
  uint16_t a2, d2, s2, r2;
  const char* pToastName;
  int32_t pToastVal;
  bool toastActive;
  CalTopology topo;
  uint8_t stage;
  int32_t gap;
  int8_t off;
  uint16_t a440, pw;
  ScreenMode mode;

  screen_state_lock();
  mode = static_cast<ScreenMode>(serialSignal);

  // Update tab position
  if (mode == ScreenMode::CalibrationMenu) {
    if (paramNumber == static_cast<uint8_t>(ParamId::PARAM_UI_MENU_POSITION)) {
      currentCalMenuIndex = (uint8_t)paramValue;
    }
  }

  pNum = presetNumber;
  snapshot_preset_name(pName);
  pChar = presetChar;
  o1 = OSC1Level;
  o2 = OSC2Level;
  sub = SUBLevel;
  a1 = ADSR1Attack;
  d1 = ADSR1Decay;
  s1 = ADSR1Sustain;
  r1 = ADSR1Release;
  a2 = ADSR2Attack;
  d2 = ADSR2Decay;
  s2 = ADSR2Sustain;
  r2 = ADSR2Release;
  toastActive = paramChangeTimerFlag;
  pToastName = paramName;
  pToastVal = paramValue;
  topo = screenCalTopology;
  stage = manualCalibrationStage;
  gap = calibrationGap;
  off = offset;
  a440 = ampComp440Display;
  pw = calPwCenterDisplay;
  screen_state_unlock();

  // Detect state changes to trigger redraw
  if (mode != lastScreenMode ||
      pNum != lastPresetNum ||
      currentCalMenuIndex != lastCalMenuIndex ||
      stage != lastCalStage ||
      gap != lastCalGap ||
      o1 != lastO1 || o2 != lastO2 || sub != lastSub ||
      a1 != lastA1 || d1 != lastD1 || s1 != lastS1 || r1 != lastR1 ||
      a2 != lastA2 || d2 != lastD2 || s2 != lastS2 || r2 != lastR2) {
    oledDirty = true;
  }

  if (!oledDirty) {
    return;
  }

  // Update tracker variables
  lastScreenMode = mode;
  lastPresetNum = pNum;
  lastCalMenuIndex = currentCalMenuIndex;
  lastCalStage = stage;
  lastCalGap = gap;
  lastO1 = o1; lastO2 = o2; lastSub = sub;
  lastA1 = a1; lastD1 = d1; lastS1 = s1; lastR1 = r1;
  lastA2 = a2; lastD2 = d2; lastS2 = s2; lastR2 = r2;
  lastOledRefreshMillis = now;
  oledDirty = false;

  // Flush UART before rendering
  serial_read_n();

  if (mode == ScreenMode::Silent) {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    serial_read_n();
    return;
  }

  u8g2.clearBuffer();

  switch (mode) {
    case ScreenMode::CalibrationMenu:
      draw_view_cal_menu(currentCalMenuIndex, topo);
      break;

    case ScreenMode::ManualCalibration:
      draw_view_manual_cal(topo, stage, gap, off, a440, pw);
      break;

    case ScreenMode::SaveSelectPreset:
      draw_view_save_select(pNum, pName);
      break;

    case ScreenMode::SaveSetName:
      draw_view_save_name(pName, pChar);
      break;

    case ScreenMode::SaveCompleted:
      u8g2.drawFrame(10, 10, 108, 44);
      u8g2.setFont(u8g2_font_7x14B_tr);
      u8g2.drawStr(18, 28, "PRESET SAVED!");
      break;

    case ScreenMode::PresetScroll:
    case ScreenMode::LoadSaveExit:
    default:
      draw_view_preset(pNum, pName, o1, o2, sub, a1, d1, s1, r1, a2, d2, s2, r2, toastActive, pToastName, pToastVal, topo);
      break;
  }

  u8g2.sendBuffer();

  // Flush UART immediately after rendering
  serial_read_n();
}