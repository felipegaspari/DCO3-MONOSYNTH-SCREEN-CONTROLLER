#include "display_u8g2.h"
#include <SPI.h>
#include <hardware/dma.h>
#include <hardware/spi.h>

// Full-buffer constructor
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(
  U8G2_R0, 
  /* cs=*/   OLED_CS_PIN, 
  /* dc=*/   OLED_DC_PIN, 
  /* reset=*/ OLED_RST_PIN
);

// DMA Management for SPI1
static int oledDmaChan = -1;
static dma_channel_config oledDmaConfig;
static bool oledDirty = true;
static uint32_t lastOledRefreshMillis = 0;
static constexpr uint32_t OLED_FRAME_PERIOD_MS = 25; // 40 FPS cap

void mark_u8g2_dirty() {
  oledDirty = true;
}

// ---------------------------------------------------------------------------
// Hardware Init & DMA Channel Setup
// ---------------------------------------------------------------------------
void init_u8g2() {
  // 1. Initialize SPI1 on Pico
  SPI1.setSCK(OLED_SCK_PIN);
  SPI1.setTX(OLED_MOSI_PIN);
  SPI1.begin();

  u8g2.begin();
  u8g2.setBusClock(20000000); // Push to 20 MHz for ultra-fast DMA streaming

  // 2. Set SSD1309 to Horizontal Addressing Mode (0x20, 0x00)
  // This allows blasting the entire 1024 bytes linearly without page hopping!
  digitalWrite(OLED_CS_PIN, LOW);
  digitalWrite(OLED_DC_PIN, LOW); // Command mode
  spi_write_blocking(spi1, (const uint8_t[]){0x20, 0x00}, 2);
  digitalWrite(OLED_CS_PIN, HIGH);

  // 3. Claim and configure RP2040 DMA channel for SPI1 TX
  oledDmaChan = dma_claim_unused_channel(true);
  oledDmaConfig = dma_channel_get_default_config(oledDmaChan);
  channel_config_set_transfer_data_size(&oledDmaConfig, DMA_SIZE_8);
  channel_config_set_dreq(&oledDmaConfig, spi_get_dreq(spi1, true)); // Trigger on SPI1 TX FIFO space
  channel_config_set_read_increment(&oledDmaConfig, true);            // Increment buffer address
  channel_config_set_write_increment(&oledDmaConfig, false);          // Fixed SPI TX FIFO address

  u8g2.clearBuffer();
}

// ---------------------------------------------------------------------------
// Non-Blocking DMA Frame Transmission (0% CPU Block)
// ---------------------------------------------------------------------------
static void send_buffer_dma() {
  // If a previous DMA transfer is still running, do not start another
  if (dma_channel_is_busy(oledDmaChan)) {
    return;
  }

  // 1. Set column and page address bounds (Col 0..127, Page 0..7)
  static const uint8_t initCmds[] = {
    0x21, 0x00, 0x7F, // Set Column Address (0 -> 127)
    0x22, 0x00, 0x07  // Set Page Address (0 -> 7)
  };

  digitalWrite(OLED_CS_PIN, LOW);
  digitalWrite(OLED_DC_PIN, LOW); // Command Mode
  spi_write_blocking(spi1, initCmds, sizeof(initCmds));

  // 2. Set DC HIGH for data streaming
  digitalWrite(OLED_DC_PIN, HIGH); // Data Mode

  // 3. Fire DMA directly from U8g2's internal 1024-byte SRAM buffer
  uint8_t* bufPtr = u8g2.getBufferPtr();
  dma_channel_configure(
    oledDmaChan,
    &oledDmaConfig,
    &spi_get_hw(spi1)->dr, // Write target: SPI1 Data Register
    bufPtr,                // Read source: U8g2 1024-byte full buffer
    1024,                  // Total bytes: 128 * 64 / 8
    true                   // Start transfer immediately
  );

  // CPU returns instantly! DMA completes the transfer in ~410 µs in the background.
}

// ---------------------------------------------------------------------------
// Helpers (Meters & Envelope Visualizer)
// ---------------------------------------------------------------------------
static inline void draw_meter_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t val, uint8_t maxVal = 255) {
  u8g2.drawFrame(x, y, w, h);
  if (val > 0 && maxVal > 0) {
    uint8_t fillW = (uint16_t)(val * (w - 2)) / maxVal;
    if (fillW > (w - 2)) fillW = w - 2;
    if (fillW > 0) u8g2.drawBox(x + 1, y + 1, fillW, h - 2);
  }
}

static void draw_mini_adsr(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                           uint16_t a, uint16_t d, uint16_t s, uint16_t r) {
  u8g2.drawFrame(x, y, w, h);

  uint8_t innerW = w - 4;
  uint8_t innerH = h - 4;
  uint8_t bx = x + 2;
  uint8_t by = y + 2;

  uint8_t aW = 6 + ((uint32_t)a * 10) / 1024;
  uint8_t dW = 6 + ((uint32_t)d * 10) / 1024;
  uint8_t rW = 6 + ((uint32_t)r * 10) / 1024;
  
  int8_t sW = innerW - (aW + dW + rW);
  if (sW < 6) sW = 6;

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
// Views
// ---------------------------------------------------------------------------
static void draw_view_preset(const Core1Snapshot &snap) {
  u8g2.setFont(u8g2_font_6x10_tf);
  char header[24];
  snprintf(header, sizeof(header), "P%02u: %s", snap.presetNum, snap.presetName);
  u8g2.drawStr(2, 9, header);
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(2, 21, "O1");
  draw_meter_bar(14, 15, 42, 7, snap.osc1Level);

  u8g2.drawStr(2, 31, "O2");
  draw_meter_bar(14, 25, 42, 7, snap.osc2Level);

  u8g2.drawStr(2, 41, "SB");
  draw_meter_bar(14, 35, 42, 7, snap.subLevel);

  draw_mini_adsr(62, 14, 64, 30, snap.a1a, snap.a1d, snap.a1s, snap.a1r);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(66, 21, "ADSR1");

  u8g2.drawHLine(0, 47, 128);

  if (paramChangeTimerFlag && snap.paramName != nullptr) {
    u8g2.setFont(u8g2_font_6x10_tf);
    char toast[32];
    snprintf(toast, sizeof(toast), "%s: %ld", snap.paramName, (long)snap.paramValue);
    u8g2.drawStr(2, 59, toast);
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* topStr = (snap.calTopology == CalTopology::Voices4x2) ? "DCO4 [4x2 VOICES]" : "DCO3 [MONO 3-OSC]";
    u8g2.drawStr(2, 58, topStr);
  }
}

static void draw_view_save_select(const Core1Snapshot &snap) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(12, 12, "--- SAVE PRESET ---");
  u8g2.drawHLine(0, 15, 128);

  char buf[24];
  snprintf(buf, sizeof(buf), "Target: P%02u", snap.presetNum);
  u8g2.drawStr(16, 32, buf);

  snprintf(buf, sizeof(buf), "\"%s\"", snap.presetName);
  u8g2.drawStr(16, 46, buf);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(8, 60, "Turn knob to select slot");
}

static void draw_view_save_name(const Core1Snapshot &snap) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(16, 12, "--- EDIT NAME ---");
  u8g2.drawHLine(0, 15, 128);

  uint8_t startX = 16;
  uint8_t startY = 34;
  u8g2.drawFrame(12, 22, 104, 18);

  for (uint8_t i = 0; i < 16; ++i) {
    char c = snap.presetName[i] ? snap.presetName[i] : ' ';
    char s[2] = {c, '\0'};
    uint8_t charX = startX + (i * 6);

    if (snap.hasPresetChar && snap.presetChar == i) {
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
  snprintf(posBuf, sizeof(posBuf), "Cursor: %u/16", snap.presetChar + 1);
  u8g2.drawStr(36, 56, posBuf);
}

static void draw_view_save_completed(const Core1Snapshot &snap) {
  u8g2.drawFrame(10, 10, 108, 44);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(18, 28, "PRESET SAVED!");

  u8g2.setFont(u8g2_font_6x10_tf);
  char buf[24];
  snprintf(buf, sizeof(buf), "P%02u: %s", snap.presetNum, snap.presetName);
  u8g2.drawStr(16, 46, buf);
}

static void draw_view_manual_cal(const Core1Snapshot &snap) {
  char toastBuf[24];
  screen_cal_format_toast(snap.calTopology, snap.calStage, toastBuf, sizeof(toastBuf));

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, toastBuf);
  u8g2.drawHLine(0, 11, 128);

  u8g2.drawFrame(14, 15, 100, 10);
  u8g2.drawVLine(64, 13, 14);

  int32_t clampedGap = snap.calGap;
  if (clampedGap < -50) clampedGap = -50;
  if (clampedGap > 50)  clampedGap = 50;
  uint8_t markerX = 64 + (clampedGap * 46 / 50);
  u8g2.drawBox(markerX - 2, 17, 5, 6);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[32];
  snprintf(buf, sizeof(buf), "GAP: %ld", (long)snap.calGap);
  u8g2.drawStr(4, 35, buf);

  snprintf(buf, sizeof(buf), "OFF: %d", snap.calOffset);
  u8g2.drawStr(70, 35, buf);

  snprintf(buf, sizeof(buf), "440: %u", snap.calAmp440);
  u8g2.drawStr(4, 46, buf);

  snprintf(buf, sizeof(buf), "PW: %u", snap.calPwCenter);
  u8g2.drawStr(70, 46, buf);

  u8g2.drawHLine(0, 50, 128);
  snprintf(buf, sizeof(buf), "STAGE %u / %u", snap.calStage + 1, screen_cal_stage_max(snap.calTopology) + 1);
  u8g2.drawStr(32, 60, buf);
}

// ---------------------------------------------------------------------------
// Core 0 Main Update Routine
// ---------------------------------------------------------------------------
void update_u8g2_core0() {
  uint32_t now = millis();

  // Toast timeout check
  if (paramChangeTimerFlag) {
    if (now - paramChangeLastMillis >= paramHideTimeMillis) {
      paramChangeTimerFlag = false;
      oledDirty = true;
    }
  }

  // Frame rate throttle (preserves Core 0 UART time)
  if (!oledDirty && (now - lastOledRefreshMillis < OLED_FRAME_PERIOD_MS)) {
    return;
  }
  lastOledRefreshMillis = now;
  oledDirty = false;

  // Snapshot volatile state
  Core1Snapshot snap;
  screen_state_lock();
  
  snap.hasSignal       = signalFlag;
  snap.signalMode      = serialSignal;
  snap.hasPresetScroll = presetScrollFlag;
  snap.presetNum       = presetNumber;
  snapshot_preset_name(snap.presetName);
  snap.hasPresetChar   = presetCharFlag;
  snap.presetChar      = presetChar;

  snap.hasParamChange  = paramChangeFlag;
  snap.paramName       = paramName;
  snap.paramValue      = paramValue;
  snap.paramNumber     = paramNumber;

  snap.levelBars       = levelBarFlag;
  snap.osc1Level       = OSC1Level;
  snap.osc2Level       = OSC2Level;
  snap.subLevel        = SUBLevel;

  snap.hasADSR1        = updateADSR1Flag;
  snap.a1a             = ADSR1Attack;
  snap.a1d             = ADSR1Decay;
  snap.a1s             = ADSR1Sustain;
  snap.a1r             = ADSR1Release;

  snap.hasADSR2        = updateADSR2Flag;
  snap.a2a             = ADSR2Attack;
  snap.a2d             = ADSR2Decay;
  snap.a2s             = ADSR2Sustain;
  snap.a2r             = ADSR2Release;

  snap.calOffset       = offset;
  snap.calAmp440       = ampComp440Display;
  snap.calPwCenter     = calPwCenterDisplay;
  snap.calOscN         = manualCalibrationOSCN;
  snap.calStage        = manualCalibrationStage;
  snap.calGap          = calibrationGap;
  snap.calTopology     = screenCalTopology;

  screen_state_unlock();

  ScreenMode mode = snap.hasSignal ? static_cast<ScreenMode>(snap.signalMode) : ScreenMode::PresetScroll;

  if (mode == ScreenMode::Silent) {
    u8g2.clearBuffer();
    send_buffer_dma();
    return;
  }

  u8g2.clearBuffer();

  switch (mode) {
    case ScreenMode::SaveSelectPreset:
      draw_view_save_select(snap);
      break;
    case ScreenMode::SaveSetName:
      draw_view_save_name(snap);
      break;
    case ScreenMode::SaveCompleted:
      draw_view_save_completed(snap);
      break;
    case ScreenMode::ManualCalibration:
    case ScreenMode::CalibrationMenu:
      draw_view_manual_cal(snap);
      break;
    case ScreenMode::PresetScroll:
    case ScreenMode::LoadSaveExit:
    default:
      draw_view_preset(snap);
      break;
  }

  // Blasts the 1024-byte buffer over SPI1 using Hardware DMA in the background
  send_buffer_dma();
}