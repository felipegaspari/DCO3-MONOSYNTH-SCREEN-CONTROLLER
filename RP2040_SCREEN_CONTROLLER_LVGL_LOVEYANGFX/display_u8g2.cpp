#include "display_u8g2.h"
#include <hardware/spi.h>
#include <hardware/gpio.h>
#include <math.h>

static uint8_t u8x8_byte_pico_hw_spi1(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  switch (msg) {
    case U8X8_MSG_BYTE_SEND:
      spi_write_blocking(spi1, (const uint8_t*)arg_ptr, arg_int);
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

static uint8_t u8x8_gpio_pico_dummy(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
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

U8G2_SSD1309_PICO_SPI1::U8G2_SSD1309_PICO_SPI1(const u8g2_cb_t* rotation)
  : U8G2() {
  u8g2_Setup_ssd1309_128x64_noname0_f(&u8g2, rotation, u8x8_byte_pico_hw_spi1, u8x8_gpio_pico_dummy);
}

U8G2_SSD1309_PICO_SPI1 u8g2(U8G2_R0);

static bool oledDirty = true;
static uint32_t lastOledRefreshMillis = 0;
static constexpr uint32_t MIN_OLED_INTERVAL_MS = 20;  // 50 FPS cap

// Persistent State Tracking
static uint8_t lastPresetNum = 255;
static uint8_t lastPChar = 255;
static ScreenMode lastScreenMode = ScreenMode::PresetScroll;
static InspectorType lastOledInspector = InspectorType::None;
static uint32_t lastInspTime = 0;
static const char* lastToastName = nullptr;
static int32_t lastToastVal = -999999;
static bool lastToastActive = false;

static uint8_t currentCalMenuIndex = 0;
static uint8_t lastCalMenuIndex = 255;
static uint8_t lastCalStage = 255;
static int32_t lastCalGap = -999999;
static int8_t lastOff = 0;
static uint16_t lastA440 = 0;
static uint16_t lastPw = 0;

static uint8_t lastO1 = 255, lastO2 = 255, lastSub = 255;
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

  spi_init(spi1, 8000000);

  u8g2.initDisplay();
  u8g2.setPowerSave(0);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline void draw_meter_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t val, uint8_t maxVal = 127) {
  u8g2.drawFrame(x, y, w, h);
  if (val > 0 && maxVal > 0) {
    uint8_t fillW = (uint16_t)(val * (w - 2)) / maxVal;
    if (fillW > (w - 2)) fillW = w - 2;
    if (fillW > 0) u8g2.drawBox(x + 1, y + 1, fillW, h - 2);
  }
}

static inline void draw_vert_meter(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t val, uint8_t maxVal = 127) {
  u8g2.drawFrame(x, y, w, h);
  if (val > 0 && maxVal > 0) {
    uint8_t fillH = (uint16_t)(val * (h - 2)) / maxVal;
    if (fillH > (h - 2)) fillH = h - 2;
    if (fillH > 0) u8g2.drawBox(x + 1, (y + h - 1) - fillH, w - 2, fillH);
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
  u8g2.setFont(u8g2_font_7x14B_tf);
  char header[24];
  snprintf(header, sizeof(header), "P%02u:%s", pNum, pName);
  u8g2.drawStr(2, 10, header);
  u8g2.drawHLine(0, 12, 128);

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
    u8g2.drawStr(2, 59, toast);
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* topStr = (topo == CalTopology::Voices4x2) ? "DCO4 [4x2 VOICES]" : "DCO3 [MONO 3-OSC]";
    u8g2.drawStr(2, 58, topStr);
  }
}

// ---------------------------------------------------------------------------
// VIEW: Full-Screen ADSR Contextual Inspector
// ---------------------------------------------------------------------------
static void draw_view_inspector_adsr(InspectorType type, uint16_t a, uint16_t d, uint16_t s, uint16_t r,
                                     const char* pToastName, int32_t pToastVal) {
  u8g2.setFont(u8g2_font_6x10_tf);
  if (type == InspectorType::ADSR1) u8g2.drawStr(2, 9, "ENV 1 [VCA]");
  else if (type == InspectorType::ADSR2) u8g2.drawStr(2, 9, "ENV 2 [VCF]");
  else u8g2.drawStr(2, 9, "ENV 3 [MOD]");

  if (pToastName && pToastName[0] != '\0') {
    u8g2.setFont(u8g2_font_5x8_tf);
    char toastStr[20];
    snprintf(toastStr, sizeof(toastStr), "%ld", (long)pToastVal);
    uint8_t strW = u8g2.getStrWidth(toastStr);
    u8g2.drawStr(126 - strW, 9, toastStr);
  }
  u8g2.drawHLine(0, 11, 128);

  const uint8_t x0 = 4, yBase = 47, yTop = 15, maxGraphW = 120;

  uint8_t aW = 4 + ((uint32_t)a * 34) / 1024;
  uint8_t dW = 4 + ((uint32_t)d * 34) / 1024;
  uint8_t rW = 4 + ((uint32_t)r * 34) / 1024;

  uint8_t totalVar = aW + dW + rW;
  if (totalVar > 92) {
    uint8_t scale = totalVar;
    aW = (aW * 92) / scale;
    dW = (dW * 92) / scale;
    rW = (rW * 92) / scale;
  }
  uint8_t sW = maxGraphW - (aW + dW + rW);

  uint8_t sH = ((uint32_t)s * 30) / 1024;
  if (sH > 30) sH = 30;
  uint8_t yS = yBase - sH;

  uint8_t p0_x = x0;
  uint8_t p1_x = p0_x + aW;
  uint8_t p2_x = p1_x + dW;
  uint8_t p3_x = p2_x + sW;
  uint8_t p4_x = x0 + maxGraphW;

  uint8_t aMid_x = p0_x + (aW / 2);
  uint8_t aMid_y = yBase - ((yBase - yTop) * 65 / 100);
  uint8_t dMid_x = p1_x + (dW / 2);
  uint8_t dMid_y = yTop + ((yS - yTop) * 70 / 100);
  uint8_t rMid_x = p3_x + (rW / 2);
  uint8_t rMid_y = yS + ((yBase - yS) * 70 / 100);

  for (uint8_t gx = x0; gx <= p4_x; gx += 4) u8g2.drawPixel(gx, yBase);
  for (uint8_t gy = yTop; gy <= yBase; gy += 3) u8g2.drawPixel(p3_x, gy);

  // 2-pixel bold curve
  u8g2.drawLine(p0_x, yBase, aMid_x, aMid_y);
  u8g2.drawLine(p0_x, yBase - 1, aMid_x, aMid_y - 1);
  u8g2.drawLine(aMid_x, aMid_y, p1_x, yTop);
  u8g2.drawLine(aMid_x, aMid_y - 1, p1_x, yTop - 1);

  u8g2.drawLine(p1_x, yTop, dMid_x, dMid_y);
  u8g2.drawLine(p1_x, yTop - 1, dMid_x, dMid_y - 1);
  u8g2.drawLine(dMid_x, dMid_y, p2_x, yS);
  u8g2.drawLine(dMid_x, dMid_y - 1, p2_x, yS - 1);

  u8g2.drawLine(p2_x, yS, p3_x, yS);
  u8g2.drawLine(p2_x, yS - 1, p3_x, yS - 1);

  u8g2.drawLine(p3_x, yS, rMid_x, rMid_y);
  u8g2.drawLine(p3_x, yS - 1, rMid_x, rMid_y - 1);
  u8g2.drawLine(rMid_x, rMid_y, p4_x, yBase);
  u8g2.drawLine(rMid_x, rMid_y - 1, p4_x, yBase - 1);

  u8g2.drawDisc(p1_x, yTop, 2);
  u8g2.drawDisc(p2_x, yS, 2);
  u8g2.drawDisc(p3_x, yS, 2);

  u8g2.drawHLine(0, 49, 128);

  bool hlA = (pToastName && (strstr(pToastName, "Attack") || strstr(pToastName, "ATTACK")));
  bool hlD = (pToastName && (strstr(pToastName, "Decay") || strstr(pToastName, "DECAY")));
  bool hlS = (pToastName && (strstr(pToastName, "Sustain") || strstr(pToastName, "SUSTAIN")));
  bool hlR = (pToastName && (strstr(pToastName, "Release") || strstr(pToastName, "RELEASE")));

  char strA[10], strD[10], strS[10], strR[10];
  snprintf(strA, sizeof(strA), "A:%u", a >> 3);
  snprintf(strD, sizeof(strD), "D:%u", d >> 3);
  snprintf(strS, sizeof(strS), "S:%u%%", (uint16_t)(((uint32_t)s * 100) / 1024));
  snprintf(strR, sizeof(strR), "R:%u", r >> 3);

  u8g2.setFont(u8g2_font_5x7_tf);

  if (hlA) {
    u8g2.drawBox(2, 51, 29, 11);
    u8g2.setDrawColor(0);
  }
  u8g2.drawStr(4, 59, strA);
  u8g2.setDrawColor(1);

  if (hlD) {
    u8g2.drawBox(33, 51, 29, 11);
    u8g2.setDrawColor(0);
  }
  u8g2.drawStr(35, 59, strD);
  u8g2.setDrawColor(1);

  if (hlS) {
    u8g2.drawBox(64, 51, 29, 11);
    u8g2.setDrawColor(0);
  }
  u8g2.drawStr(66, 59, strS);
  u8g2.setDrawColor(1);

  if (hlR) {
    u8g2.drawBox(95, 51, 31, 11);
    u8g2.setDrawColor(0);
  }
  u8g2.drawStr(97, 59, strR);
  u8g2.setDrawColor(1);
}

// ---------------------------------------------------------------------------
// VIEW: Filter (VCF) Contextual Inspector
// ---------------------------------------------------------------------------
static void draw_view_inspector_filter(const char* pToastName, int32_t pToastVal) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "VCF / FILTER [24dB]");

  if (pToastName && pToastName[0] != '\0') {
    u8g2.setFont(u8g2_font_5x7_tf);
    char toastStr[24];
    snprintf(toastStr, sizeof(toastStr), "%s: %ld", pToastName, (long)pToastVal);
    uint8_t strW = u8g2.getStrWidth(toastStr);
    u8g2.drawStr(126 - strW, 9, toastStr);
  }
  u8g2.drawHLine(0, 11, 128);

  uint8_t cutoffX = 64;
  if (pToastName && (strstr(pToastName, "Cutoff") || strstr(pToastName, "CUTOFF"))) {
    cutoffX = 14 + ((uint32_t)(pToastVal & 0x7F) * 96) / 127;
  }

  const uint8_t yFloor = 46;
  const uint8_t yFlat = 26;
  const uint8_t yPeak = 16;

  for (uint8_t x = 4; x <= 124; x += 6) u8g2.drawPixel(x, yFloor);
  for (uint8_t y = yPeak; y <= yFloor; y += 4) u8g2.drawPixel(cutoffX, y);

  u8g2.drawLine(4, yFlat, cutoffX - 8, yFlat);
  u8g2.drawLine(4, yFlat - 1, cutoffX - 8, yFlat - 1);

  u8g2.drawLine(cutoffX - 8, yFlat, cutoffX, yPeak);
  u8g2.drawLine(cutoffX - 8, yFlat - 1, cutoffX, yPeak - 1);

  u8g2.drawLine(cutoffX, yPeak, cutoffX + 16, yFloor - 4);
  u8g2.drawLine(cutoffX, yPeak - 1, cutoffX + 16, yFloor - 5);

  u8g2.drawLine(cutoffX + 16, yFloor - 4, 124, yFloor);
  u8g2.drawLine(cutoffX + 16, yFloor - 5, 124, yFloor - 1);

  u8g2.drawDisc(cutoffX, yPeak, 2);

  u8g2.drawHLine(0, 49, 128);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(4, 59, "SLOPE: 24dB");
  u8g2.drawStr(66, 59, "MOD: ENV2 / LFO2");
}

// ---------------------------------------------------------------------------
// VIEW: Oscillators & Mixer Contextual Inspector
// ---------------------------------------------------------------------------
static void draw_view_inspector_oscillators(uint8_t o1, uint8_t o2, uint8_t sub,
                                            const char* pToastName, int32_t pToastVal) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "OSC MIXER & WAVE");
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(4, 20, "O1");
  draw_vert_meter(4, 22, 10, 24, o1);

  u8g2.drawStr(18, 20, "O2");
  draw_vert_meter(18, 22, 10, 24, o2);

  u8g2.drawStr(32, 20, "O3");
  draw_vert_meter(32, 22, 10, 24, 0);

  u8g2.drawStr(46, 20, "SB");
  draw_vert_meter(46, 22, 10, 24, sub);

  u8g2.drawFrame(64, 14, 60, 32);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(68, 23, "PULSE WIDTH");

  u8g2.drawLine(68, 37, 78, 37);
  u8g2.drawLine(78, 37, 78, 27);
  u8g2.drawLine(78, 27, 98, 27);
  u8g2.drawLine(98, 27, 98, 37);
  u8g2.drawLine(98, 37, 118, 37);

  u8g2.drawHLine(0, 49, 128);
  if (pToastName && pToastName[0] != '\0') {
    u8g2.setFont(u8g2_font_5x7_tf);
    char toast[32];
    snprintf(toast, sizeof(toast), "%s: %ld", pToastName, (long)pToastVal);
    u8g2.drawStr(4, 59, toast);
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 59, "O1:SAW+PULSE  O2:TRI  SYNC:OFF");
  }
}

// ---------------------------------------------------------------------------
// VIEW: LFO (1 / 2 / 3) Contextual Inspector
// ---------------------------------------------------------------------------
static void draw_view_inspector_lfo(InspectorType type, const char* pToastName, int32_t pToastVal) {
  u8g2.setFont(u8g2_font_6x10_tf);
  if (type == InspectorType::LFO1) u8g2.drawStr(2, 9, "LFO 1 [PITCH/VCA]");
  else if (type == InspectorType::LFO2) u8g2.drawStr(2, 9, "LFO 2 [PWM/VCF]");
  else u8g2.drawStr(2, 9, "LFO 3 [MOD]");
  u8g2.drawHLine(0, 11, 128);

  u8g2.drawFrame(4, 14, 78, 32);
  for (uint8_t x = 8; x <= 78; x += 4) u8g2.drawPixel(x, 30);

  for (uint8_t x = 0; x < 70; ++x) {
    int8_t yOffset = (int8_t)(sinf(x * 0.18f) * 11.0f);
    u8g2.drawPixel(8 + x, 30 - yOffset);
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(86, 23, "DEST:");
  u8g2.drawStr(86, 33, (type == InspectorType::LFO1) ? "> PITCH" : "> CUTOFF");
  u8g2.drawStr(86, 43, "FREE-RUN");

  u8g2.drawHLine(0, 49, 128);
  if (pToastName && pToastName[0] != '\0') {
    u8g2.setFont(u8g2_font_5x7_tf);
    char toast[32];
    snprintf(toast, sizeof(toast), "%s: %ld", pToastName, (long)pToastVal);
    u8g2.drawStr(4, 59, toast);
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 59, "SHAPE: SINE   RATE: SYNC 1/4");
  }
}

// ---------------------------------------------------------------------------
// VIEW: Voice Engine & Analog Drift Contextual Inspector
// ---------------------------------------------------------------------------
static void draw_view_inspector_voice(const char* pToastName, int32_t pToastVal) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "VOICE & DRIFT ENGINE");
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(4, 23, "VOICES:");
  for (uint8_t v = 0; v < 4; ++v) {
    uint8_t vx = 50 + (v * 16);
    u8g2.drawCircle(vx, 20, 5);
    u8g2.drawDisc(vx, 20, 2);
  }

  u8g2.drawStr(4, 39, "DRIFT SPREAD:");
  u8g2.drawFrame(74, 32, 48, 8);
  u8g2.drawBox(76, 34, 24, 4);

  u8g2.drawHLine(0, 49, 128);
  if (pToastName && pToastName[0] != '\0') {
    u8g2.setFont(u8g2_font_5x7_tf);
    char toast[32];
    snprintf(toast, sizeof(toast), "%s: %ld", pToastName, (long)pToastVal);
    u8g2.drawStr(4, 59, toast);
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 59, "MODE: POLY 4x2   ALLOC: R-ROBIN");
  }
}

// ---------------------------------------------------------------------------
// VIEW: Calibration & Save Preset Views
// ---------------------------------------------------------------------------
static void draw_view_cal_menu(uint8_t tabIndex, CalTopology topo) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "CALIBRATION TABS");

  u8g2.setFont(u8g2_font_5x7_tf);
  char posHeader[12];
  snprintf(posHeader, sizeof(posHeader), "TAB %u", tabIndex + 1);
  u8g2.drawStr(96, 9, posHeader);
  u8g2.drawHLine(0, 11, 128);

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

  uint8_t startIdx = 0;
  if (activeTab >= 2) {
    startIdx = activeTab - 1;
    if (startIdx + 3 > totalTabs) startIdx = totalTabs - 3;
  }

  u8g2.setFont(u8g2_font_6x10_tf);
  for (uint8_t i = 0; i < 3; ++i) {
    uint8_t idx = startIdx + i;
    uint8_t y = 23 + (i * 11);

    if (idx == activeTab) {
      u8g2.drawBox(2, y - 9, 124, 11);
      u8g2.setDrawColor(0);
      u8g2.drawStr(4, y, tabs[idx]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(4, y, tabs[idx]);
    }
  }

  u8g2.drawHLine(0, 50, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(2, 60, "ROT: SCROLL");
  u8g2.drawStr(74, 60, "PUSH: SELECT");
}

static void draw_view_manual_cal(CalTopology topo, uint8_t stage, int32_t gap, int8_t off, uint16_t a440, uint16_t pw) {
  char toastBuf[24];
  screen_cal_format_toast(topo, stage, toastBuf, sizeof(toastBuf));

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, toastBuf);
  u8g2.drawHLine(0, 11, 128);

  // Outer frame & center zero line
  u8g2.drawFrame(14, 15, 100, 10);
  u8g2.drawVLine(64, 13, 14);

  // Dynamic limits: ±100 for 440Hz stages, ±400 for all other stages
  const uint8_t nOsc = screen_cal_nosc(topo);
  const bool is440 = cal_stage_is_440_n(stage, nOsc);
  const int32_t limit = is440 ? 100 : 400;

  int32_t clampedGap = gap;
  if (clampedGap < -limit) clampedGap = -limit;
  if (clampedGap > limit) clampedGap = limit;

  // Map clamped gap to cursor position within the 100px bar
  uint8_t markerX = 64 + (clampedGap * 46 / limit);
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
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(8, 12, "SAVE PRESET");
  u8g2.drawHLine(0, 15, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  char buf[24];
  snprintf(buf, sizeof(buf), "Target: P%02u", pNum);
  u8g2.drawStr(16, 30, buf);
  snprintf(buf, sizeof(buf), "\"%s\"", pName);
  u8g2.drawStr(16, 44, buf);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(8, 59, "Turn knob to select slot");
}

static void draw_view_save_name(const char* pName, uint8_t pChar) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(16, 10, "EDIT NAME");
  u8g2.drawHLine(0, 13, 128);

  uint8_t startX = 16;
  uint8_t startY = 34;
  u8g2.drawFrame(12, 22, 104, 18);

  u8g2.setFont(u8g2_font_6x12_tf);
  for (uint8_t i = 0; i < 16; ++i) {
    char c = pName[i] ? pName[i] : ' ';
    char s[2] = { c, '\0' };
    uint8_t charX = startX + (i * 6);

    if (pChar == i) {
      u8g2.drawBox(charX - 1, startY - 9, 7, 12);
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
  u8g2.drawStr(36, 57, posBuf);
}

// ---------------------------------------------------------------------------
// Core 0 Non-Blocking Event-Driven Refresh
// ---------------------------------------------------------------------------
void update_u8g2_core0() {
  uint32_t now = millis();

  if (now - lastOledRefreshMillis < MIN_OLED_INTERVAL_MS) {
    return;
  }

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
  InspectorType insp;
  uint32_t inspTime;

  screen_state_lock();
  mode = static_cast<ScreenMode>(serialSignal);
  currentCalMenuIndex = calibrationMenuIndex;

  insp = activeInspector;
  inspTime = inspectorLastActivityMillis;

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

  bool isInspectorActive = (insp != InspectorType::None) && (now - inspTime < inspectorTimeoutMillis) && (mode != ScreenMode::Silent) && (mode == ScreenMode::PresetScroll || mode == ScreenMode::LoadSaveExit);

  if (!isInspectorActive) {
    insp = InspectorType::None;
  }

  // Detect state change on ANY parameter or navigation update
  if (mode != lastScreenMode || insp != lastOledInspector || inspTime != lastInspTime || pNum != lastPresetNum || pChar != lastPChar || pToastVal != lastToastVal || pToastName != lastToastName || toastActive != lastToastActive || currentCalMenuIndex != lastCalMenuIndex || stage != lastCalStage || gap != lastCalGap || off != lastOff || a440 != lastA440 || pw != lastPw || o1 != lastO1 || o2 != lastO2 || sub != lastSub || a1 != lastA1 || d1 != lastD1 || s1 != lastS1 || r1 != lastR1 || a2 != lastA2 || d2 != lastD2 || s2 != lastS2 || r2 != lastR2) {
    oledDirty = true;
  }

  if (!oledDirty) {
    return;
  }

  if (mode == ScreenMode::Silent) {
    return;
  }

  lastScreenMode = mode;
  lastOledInspector = insp;
  lastInspTime = inspTime;
  lastPresetNum = pNum;
  lastPChar = pChar;
  lastToastVal = pToastVal;
  lastToastName = pToastName;
  lastToastActive = toastActive;
  lastCalMenuIndex = currentCalMenuIndex;
  lastCalStage = stage;
  lastCalGap = gap;
  lastOff = off;
  lastA440 = a440;
  lastPw = pw;
  lastO1 = o1;
  lastO2 = o2;
  lastSub = sub;
  lastA1 = a1;
  lastD1 = d1;
  lastS1 = s1;
  lastR1 = r1;
  lastA2 = a2;
  lastD2 = d2;
  lastS2 = s2;
  lastR2 = r2;
  lastOledRefreshMillis = now;
  oledDirty = false;

  u8g2.clearBuffer();

  if (isInspectorActive) {
    switch (insp) {
      case InspectorType::ADSR1:
        draw_view_inspector_adsr(insp, a1, d1, s1, r1, pToastName, pToastVal);
        break;

      case InspectorType::ADSR2:
        draw_view_inspector_adsr(insp, a2, d2, s2, r2, pToastName, pToastVal);
        break;

      case InspectorType::ADSR3:
        draw_view_inspector_adsr(insp, a1, d1, s1, r1, pToastName, pToastVal);
        break;

      case InspectorType::Filter:
        draw_view_inspector_filter(pToastName, pToastVal);
        break;

      case InspectorType::Oscillators:
        draw_view_inspector_oscillators(o1, o2, sub, pToastName, pToastVal);
        break;

      case InspectorType::LFO1:
      case InspectorType::LFO2:
      case InspectorType::LFO3:
        draw_view_inspector_lfo(insp, pToastName, pToastVal);
        break;

      case InspectorType::VoiceEngine:
        draw_view_inspector_voice(pToastName, pToastVal);
        break;

      default:
        draw_view_preset(pNum, pName, o1, o2, sub, a1, d1, s1, r1, a2, d2, s2, r2, toastActive, pToastName, pToastVal, topo);
        break;
    }
  } else {
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
        u8g2.setFont(u8g2_font_9x18B_tf);
        u8g2.drawStr(28, 37, "SAVED!");
        break;

      case ScreenMode::PresetScroll:
      case ScreenMode::LoadSaveExit:
      default:
        draw_view_preset(pNum, pName, o1, o2, sub, a1, d1, s1, r1, a2, d2, s2, r2, toastActive, pToastName, pToastVal, topo);
        break;
    }
  }

  u8g2.sendBuffer();
}
