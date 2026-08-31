#include "display_u8g2.h"
#include <hardware/spi.h>
#include <hardware/gpio.h>
#include <string.h>
#include "_build_libs/DCO-PROTOCOL/param_meta.h" 
#include "menues.h"


// ---------------------------------------------------------------------------
// Hardware SPI1 Callback (Zero SPI0 Collision)
// ---------------------------------------------------------------------------
static uint8_t u8x8_byte_pico_hw_spi1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_BYTE_SEND:          spi_write_blocking(spi1, (const uint8_t *)arg_ptr, arg_int); break;
    case U8X8_MSG_BYTE_SET_DC:        gpio_put(OLED_DC_PIN, arg_int); break;
    case U8X8_MSG_BYTE_START_TRANSFER:gpio_put(OLED_CS_PIN, 0); break;
    case U8X8_MSG_BYTE_END_TRANSFER:  gpio_put(OLED_CS_PIN, 1); break;
    default: return 0;
  }
  return 1;
}

static uint8_t u8x8_gpio_pico_dummy(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  if (msg == U8X8_MSG_DELAY_MILLI) delay(arg_int);
  else if (msg == U8X8_MSG_DELAY_10MICRO) delayMicroseconds(arg_int * 10);
  return 1;
}

U8G2_SSD1309_PICO_SPI1::U8G2_SSD1309_PICO_SPI1(const u8g2_cb_t *rotation) : U8G2() {
  u8g2_Setup_ssd1309_128x64_noname0_f(&u8g2, rotation, u8x8_byte_pico_hw_spi1, u8x8_gpio_pico_dummy);
}

U8G2_SSD1309_PICO_SPI1 u8g2(U8G2_R0);
LocalGuiState guiState;

static bool oledDirty = true;
static uint32_t lastOledRefreshMillis = 0;
static constexpr uint32_t MIN_OLED_INTERVAL_MS = 25; // 40 FPS Limit

// Core 0 Dedicated Inspector State (Completely decoupled from Core 1)
static InspectorType oledActiveInspector = InspectorType::None;
static uint32_t      oledInspectorActivityMillis = 0;

void trigger_oled_inspector(InspectorType type) {
  if (type != InspectorType::None) {
    oledActiveInspector = type;
    oledInspectorActivityMillis = millis();
    oledDirty = true;
  }
}

// State Trackers (Automatic dirty detection)
static uint8_t       lastPresetNum = 255;
static char          lastPresetName[17] = "";
static ScreenMode    lastScreenMode = static_cast<ScreenMode>(255);
static InspectorType lastActiveInsp = InspectorType::None;
static uint8_t       lastMenuMode = 255;
static uint8_t       lastNavIdx = 255;
static uint8_t       lastCalStage = 255;
static int32_t       lastCalGap = -999999;
static int32_t       lastCutoff = -1;
static int32_t       lastReso = -1;
static int32_t       lastEnv2vcf = -1;
static uint8_t       lastO1Level = 255, lastO2Level = 255, lastSubLevel = 255;

void mark_u8g2_dirty() { oledDirty = true; }

void init_u8g2() {
  gpio_set_function(OLED_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(OLED_MOSI_PIN, GPIO_FUNC_SPI);
  gpio_init(OLED_CS_PIN); gpio_set_dir(OLED_CS_PIN, GPIO_OUT); gpio_put(OLED_CS_PIN, 1);
  gpio_init(OLED_DC_PIN); gpio_set_dir(OLED_DC_PIN, GPIO_OUT); gpio_put(OLED_DC_PIN, 1);
  spi_init(spi1, 16000000);
  u8g2.initDisplay();
  u8g2.setPowerSave(0);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Drawing Primitives
// ---------------------------------------------------------------------------
static void draw_meter_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t val, uint16_t maxVal = 128) {
  u8g2.drawFrame(x, y, w, h);
  if (val > 0 && maxVal > 0) {
    uint8_t fillW = ((uint16_t)val * (w - 2)) / maxVal;
    if (fillW > w - 2) fillW = w - 2;
    if (fillW > 0) u8g2.drawBox(x + 1, y + 1, fillW, h - 2);
  }
}

static void SCREEN_HOT(draw_bipolar_bar)(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int16_t val, int16_t maxVal) {
  u8g2.drawFrame(x, y, w, h);
  uint8_t cx = x + (w / 2);
  u8g2.drawVLine(cx, y + 1, h - 2);
  if (val == 0) return;
  
  int16_t clamped = val;
  if (clamped < -maxVal) clamped = -maxVal;
  if (clamped > maxVal) clamped = maxVal;

  uint8_t barW = ((uint32_t)(clamped < 0 ? -clamped : clamped) * ((w - 2) / 2)) / maxVal;
  if (clamped < 0) u8g2.drawBox(cx - barW, y + 1, barW, h - 2);
  else u8g2.drawBox(cx, y + 1, barW, h - 2);
}

// ---------------------------------------------------------------------------
// INSPECTOR: FILTER
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_inspector_filter)(const PatchMixBlock& mix) {
  u8g2.setFont(u8g2_font_6x10_tf);
  const char* fModeStr = "4P LOWPASS";
  if (mix.filter_mode == 1) fModeStr = "2P LOWPASS";
  else if (mix.filter_mode == 2) fModeStr = "BANDPASS";
  else if (mix.filter_mode == 3) fModeStr = "HIGHPASS";

  u8g2.drawStr(2, 9, "FILTER INSP.");
  u8g2.drawStr(128 - u8g2.getStrWidth(fModeStr) - 2, 9, fModeStr);
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[32];
  snprintf(buf, sizeof(buf), "CUTOFF: %ld", (long)guiState.cutoff);
  u8g2.drawStr(2, 21, buf);
  snprintf(buf, sizeof(buf), "RESONC: %ld%%", (long)(guiState.resonance * 100 / 4095));
  u8g2.drawStr(2, 31, buf);
  snprintf(buf, sizeof(buf), "ENV->F: %+ld", (long)guiState.env2vcf);
  u8g2.drawStr(2, 41, buf);
  snprintf(buf, sizeof(buf), "KTRACK: %+d", mix.vcf_keytrack);
  u8g2.drawStr(2, 51, buf);

  // Procedural Filter Response Curve
  uint8_t fx = 68, fy = 15, fw = 58, fh = 32;
  u8g2.drawFrame(fx, fy, fw, fh);
  
  uint8_t peakX = fx + 4 + ((guiState.cutoff * (fw - 8)) / 4095);
  uint8_t peakY = fy + fh - 2 - ((guiState.resonance * (fh - 6)) / 4095);
  
  if (mix.filter_mode <= 1) { // Lowpass
    u8g2.drawLine(fx + 2, fy + fh - 2, peakX, peakY);
    u8g2.drawLine(peakX, peakY, fx + fw - 2, fy + fh - 2);
  } else if (mix.filter_mode == 3) { // Highpass
    u8g2.drawLine(fx + 2, fy + fh - 2, peakX, peakY);
    u8g2.drawLine(peakX, peakY, fx + fw - 2, peakY);
  } else { // Bandpass
    u8g2.drawLine(fx + 2, fy + fh - 2, peakX, peakY);
    u8g2.drawLine(peakX, peakY, fx + fw - 2, fy + fh - 2);
  }

  u8g2.drawHLine(0, 54, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  bool rComp = (mix.misc_flags & (1 << 0));
  bool vRst  = (mix.misc_flags & (1 << 2));
  snprintf(buf, sizeof(buf), "RES CMP:[%s]  VEL->F:%+d  RST:[%s]", 
           rComp ? "ON" : "OFF", mix.velocity_to_vcf, vRst ? "ON" : "OFF");
  u8g2.drawStr(2, 62, buf);
}

// ---------------------------------------------------------------------------
// INSPECTOR: OSCILLATORS
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_inspector_osc)(const PatchOscBlock& osc, const PatchMixBlock& mix, const PatchLfoBlock& lfo) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "OSC ENGINE");

  const char* vMode = "MONO";
  if (osc.voice_mode == 1) vMode = "POLY";
  else if (osc.voice_mode == 2) vMode = "UNISON";
  u8g2.drawStr(128 - u8g2.getStrWidth(vMode) - 2, 9, vMode);
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[40];

  snprintf(buf, sizeof(buf), "O1: OCT:%+d   VOL:%u%%  PW:%u%%", osc.osc1_interval, (mix.osc1_level * 100)/128, (lfo.pw_value * 100)/4095);
  u8g2.drawStr(2, 21, buf);

  snprintf(buf, sizeof(buf), "O2: INT:%+d   VOL:%u%%  DET:%+d", osc.osc2_interval, (mix.osc2_level * 100)/128, osc.osc2_detune - 256);
  u8g2.drawStr(2, 31, buf);

  int subOct = (osc.subosc_divide == 2) ? -1 : (osc.subosc_divide == 4) ? -2 : 0;
  snprintf(buf, sizeof(buf), "SB: %d OCT   VOL:%u%%", subOct, (mix.sub_level * 100)/128);
  u8g2.drawStr(2, 41, buf);

  u8g2.drawHLine(0, 48, 128);
  
  u8g2.setFont(u8g2_font_4x6_tf);
  snprintf(buf, sizeof(buf), "SYNC: MODE %u     SOFT: %u", osc.sync_mode, osc.soft_sync);
  u8g2.drawStr(2, 56, buf);
}

// ---------------------------------------------------------------------------
// INSPECTOR: ENVELOPES (ADSR)
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_inspector_adsr)(uint8_t envNum, const PatchMixBlock& mix, uint16_t a, uint16_t d, uint16_t s, uint16_t r, const PatchLfoBlock& lfo) {
  u8g2.setFont(u8g2_font_6x10_tf);
  char title[32];
  snprintf(title, sizeof(title), "ENV %u", envNum);
  if (envNum == 1) strcat(title, " (VCA)");
  else if (envNum == 2) strcat(title, " (VCF)");
  else if (envNum == 3) strcat(title, " (PITCH/PWM)");
  u8g2.drawStr(2, 9, title);

  // Exact Curve Name Mapping
  uint8_t curveIdx = (envNum == 1) ? mix.adsr1_attack_curve : mix.adsr2_attack_curve;
  const char* curveStr = "[EXP]";
  switch (curveIdx) {
    case 0: curveStr = "[EXP]"; break;
    case 1: curveStr = "[SOFT]"; break;
    case 2: curveStr = "[STEEP]"; break;
    case 3: curveStr = "[CONCAVE]"; break;
    case 4: curveStr = "[FAST S]"; break;
    case 5: curveStr = "[SLOW>LIN]"; break;
    case 6: curveStr = "[ALM LIN]"; break;
    case 7: curveStr = "[LINEAR]"; break;
    default: curveStr = "[EXP]"; break;
  }
  u8g2.drawStr(128 - u8g2.getStrWidth(curveStr) - 2, 9, curveStr);
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[32];
  snprintf(buf, sizeof(buf), "A: %u", a); u8g2.drawStr(2, 21, buf);
  snprintf(buf, sizeof(buf), "D: %u", d); u8g2.drawStr(2, 31, buf);
  snprintf(buf, sizeof(buf), "S: %u%%", (s*100)/4095); u8g2.drawStr(2, 41, buf);
  snprintf(buf, sizeof(buf), "R: %u", r); u8g2.drawStr(50, 41, buf);

  // Big Envelope Visualizer
  uint8_t bx = 70, by = 15, bw = 56, bh = 28;
  u8g2.drawFrame(bx, by, bw, bh);
  uint8_t aw = 2 + (a * (bw/3)) / 4095;
  uint8_t dw = 2 + (d * (bw/3)) / 4095;
  uint8_t rw = 2 + (r * (bw/3)) / 4095;
  uint8_t sw = bw - (aw + dw + rw);
  if (sw > bw) sw = 2; 
  uint8_t sh = (s * (bh-4)) / 4095;

  uint8_t px1 = bx + 2 + aw;
  uint8_t px2 = px1 + dw;
  uint8_t px3 = px2 + sw;
  uint8_t pSustY = by + (bh - 2) - sh;

  u8g2.drawLine(bx+2, by+bh-2, px1, by+2);
  u8g2.drawLine(px1, by+2, px2, pSustY);
  u8g2.drawLine(px2, pSustY, px3, pSustY);
  u8g2.drawLine(px3, pSustY, bx+bw-2, by+bh-2);

  u8g2.drawHLine(0, 52, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  
  if (envNum == 1) {
    bool vcaRst = mix.misc_flags & (1 << 1);
    snprintf(buf, sizeof(buf), "VEL->VCA: %+d      RESTART: [%s]", mix.velocity_to_vca, vcaRst ? "ON" : "OFF");
  } else if (envNum == 2) {
    bool vcfRst = mix.misc_flags & (1 << 2);
    snprintf(buf, sizeof(buf), "VEL->VCF: %+d      RESTART: [%s]", mix.velocity_to_vcf, vcfRst ? "ON" : "OFF");
  } else {
    snprintf(buf, sizeof(buf), "DEST: PWM:%+d  PITCH:%+d  MOD:%d", lfo.adsr3_to_pwm - 512, lfo.adsr3_to_detune1, lfo.adsr3_mode);
  }
  u8g2.drawStr(2, 60, buf);
}
// ---------------------------------------------------------------------------
// INSPECTOR: LFO
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_inspector_lfo)(uint8_t lfoNum, const PatchLfoBlock& lfo) {
  u8g2.setFont(u8g2_font_6x10_tf);
  char title[16]; snprintf(title, sizeof(title), "LFO %u MOD", lfoNum);
  u8g2.drawStr(2, 9, title);

  uint8_t shape = (lfoNum == 1) ? lfo.lfo1_waveform : (lfoNum == 2) ? lfo.lfo2_waveform : 0;
  const char* sStr = (shape==0)?"[TRI]":(shape==1)?"[SAW]":(shape==2)?"[RAMP]":(shape==3)?"[SQR]":(shape==4)?"[SINE]":"[S&H]";
  u8g2.drawStr(128 - u8g2.getStrWidth(sStr) - 2, 9, sStr);
  u8g2.drawHLine(0, 11, 128);

  uint16_t speed = (lfoNum == 1) ? lfo.lfo1_speed : lfo.lfo2_speed;

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[32];
  snprintf(buf, sizeof(buf), "SPEED: %u", speed);
  u8g2.drawStr(2, 21, buf);
  u8g2.drawStr(2, 31, "ROUTINGS:");

  if (lfoNum == 1) {
    snprintf(buf, sizeof(buf), "-> DCO: %u", lfo.lfo1_to_dco); u8g2.drawStr(10, 41, buf);
    snprintf(buf, sizeof(buf), "-> VCA: %u", lfo.lfo1_to_vca); u8g2.drawStr(70, 41, buf);
  } else {
    snprintf(buf, sizeof(buf), "-> O2: %u", lfo.lfo2_to_osc2); u8g2.drawStr(10, 41, buf);
    snprintf(buf, sizeof(buf), "-> PWM: %u", lfo.lfo2_to_pw); u8g2.drawStr(70, 41, buf);
  }

  // Procedural Wave Render
  uint8_t wx = 84, wy = 13, ww = 42, wh = 16;
  u8g2.drawFrame(wx, wy, ww, wh);
  
  uint8_t cycles = 1 + (speed / 1000); 
  if (cycles > 4) cycles = 4;
  uint8_t step = ww / (cycles * 2);
  uint8_t cy = wy + (wh/2);
  uint8_t cx = wx + 2;

  for (uint8_t i = 0; i < cycles; ++i) {
    if (shape == 0 || shape == 4) {
      u8g2.drawLine(cx, cy, cx + step/2, wy + 2);
      u8g2.drawLine(cx + step/2, wy + 2, cx + step + step/2, wy + wh - 2);
      u8g2.drawLine(cx + step + step/2, wy + wh - 2, cx + 2*step, cy);
    } else if (shape == 1) {
      u8g2.drawLine(cx, wy + wh - 2, cx + 2*step, wy + 2);
      u8g2.drawLine(cx + 2*step, wy + 2, cx + 2*step, wy + wh - 2);
    } else if (shape == 3) {
      u8g2.drawLine(cx, wy + 2, cx + step, wy + 2);
      u8g2.drawLine(cx + step, wy + 2, cx + step, wy + wh - 2);
      u8g2.drawLine(cx + step, wy + wh - 2, cx + 2*step, wy + wh - 2);
      u8g2.drawLine(cx + 2*step, wy + wh - 2, cx + 2*step, wy + 2);
    }
    cx += 2*step;
  }

  u8g2.drawHLine(0, 52, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  if (lfoNum == 1) snprintf(buf, sizeof(buf), "TARGETS: O1[%c] O2[%c] O3[%c]", lfo.lfo1_to_osc1?'X':' ', lfo.lfo1_to_osc2?'X':' ', lfo.lfo1_to_osc3?'X':' ');
  else snprintf(buf, sizeof(buf), "DESTINATIONS: OSC2, OSC3, PWM");
  u8g2.drawStr(2, 60, buf);
}

// ---------------------------------------------------------------------------
// INSPECTOR: VOICE ENGINE
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_inspector_voice)(const PatchOscBlock& osc) {
  u8g2.setFont(u8g2_font_6x10_tf);
  const char* vMode = (osc.voice_mode == 0) ? "[MONO]" : (osc.voice_mode == 1) ? "[POLY]" : "[UNISON]";
  u8g2.drawStr(2, 9, "VOICE ENGINE");
  u8g2.drawStr(128 - u8g2.getStrWidth(vMode) - 2, 9, vMode);
  u8g2.drawHLine(0, 11, 128);

  // Exact Voice Stealing / Allocation Modes
  const char* alloc = "ALLOC: ROUND ROBIN";
  switch (osc.voice_alloc_mode) {
    case 0: alloc = "ALLOC: ROUND ROBIN"; break;
    case 1: alloc = "ALLOC: OLDEST"; break;
    case 2: alloc = "ALLOC: QUIETEST"; break;
    case 3: alloc = "ALLOC: QUIET (LOW)"; break;
    case 4: alloc = "ALLOC: QUIET (HIGH)"; break;
    case 5: alloc = "ALLOC: NO STEAL"; break;
    default: alloc = "ALLOC: ROUND ROBIN"; break;
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(2, 21, alloc);

  u8g2.drawStr(2, 33, "UNISON DET:");
  draw_bipolar_bar(64, 27, 60, 7, osc.unison_detune, 4095);

  char buf[40];
  snprintf(buf, sizeof(buf), "DRIFT: DEP:%u  SPD:%u  SPR:%u", osc.analog_drift, osc.analog_drift_speed, osc.analog_drift_spread);
  u8g2.drawStr(2, 45, buf);

  u8g2.drawHLine(0, 52, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  snprintf(buf, sizeof(buf), "PORTAMENTO: %ums    MODE: %u", osc.portamento_time, osc.portamento_mode);
  u8g2.drawStr(2, 60, buf);
}

// ---------------------------------------------------------------------------
// IDLE DASHBOARD: ENHANCED PRESET VIEW
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_view_preset_enhanced)(const Core1Snapshot& snap, const PatchOscBlock& osc, const PatchMixBlock& mix, const PatchModBlock& mod) {
  u8g2.setFont(u8g2_font_6x10_tf);
  char header[24];
  snprintf(header, sizeof(header), "P%02u: %s", snap.presetNum, snap.presetName);
  u8g2.drawStr(2, 9, header);
  
  u8g2.setFont(u8g2_font_4x6_tf);
  const char* vMode = (osc.voice_mode == 0) ? "[M]" : (osc.voice_mode == 1) ? "[P]" : "[U]";
  u8g2.drawStr(128 - u8g2.getStrWidth(vMode) - 2, 8, vMode);
  u8g2.drawHLine(0, 11, 128);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[16];
  snprintf(buf, sizeof(buf), "O1: %u%%", (mix.osc1_level * 100) / 128); u8g2.drawStr(2, 21, buf);
  snprintf(buf, sizeof(buf), "O2: %u%%", (mix.osc2_level * 100) / 128); u8g2.drawStr(2, 31, buf);
  snprintf(buf, sizeof(buf), "SB: %u%%", (mix.sub_level * 100) / 128);  u8g2.drawStr(2, 41, buf);

  u8g2.drawStr(56, 21, "MOD:");
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t x = 80 + (i % 4) * 11;
    uint8_t y = 15 + (i / 4) * 10;
    u8g2.drawFrame(x, y, 9, 9);
    if (mod.slots[i].src != 0 && mod.slots[i].src != 0xFF && mod.slots[i].depth != 0) {
      u8g2.drawBox(x + 2, y + 2, 5, 5);
    }
  }

  u8g2.drawStr(56, 41, "FLT:");
  draw_meter_bar(80, 35, 42, 7, guiState.cutoff, 4095);

  u8g2.drawHLine(0, 48, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  const char* fMode = (mix.filter_mode == 0) ? "4P-LP" : (mix.filter_mode == 1) ? "2P-LP" : "BP/HP";
  snprintf(buf, sizeof(buf), "FLT: %s   DRIFT: %s", fMode, (osc.analog_drift > 0) ? "ON" : "OFF");
  u8g2.drawStr(2, 58, buf);
}


// ---------------------------------------------------------------------------
// MENUS & CALIBRATION
// ---------------------------------------------------------------------------
static void SCREEN_HOT(draw_view_mod_matrix)(uint8_t slotIdx, const PatchModBlock& mod) {
  if (slotIdx > 7) slotIdx = 7;
  
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, "MOD MATRIX");
  
  char buf[32];
  snprintf(buf, sizeof(buf), "SLOT %u/8", slotIdx + 1);
  u8g2.drawStr(128 - u8g2.getStrWidth(buf) - 2, 9, buf);
  u8g2.drawHLine(0, 11, 128);

  const auto& slot = mod.slots[slotIdx];
  
  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "SRC: %s", param_mod_source_name(slot.src));
  u8g2.drawStr(4, 24, buf);

  snprintf(buf, sizeof(buf), "DST: %s", param_mod_dest_name(slot.dest));
  u8g2.drawStr(4, 36, buf);

  snprintf(buf, sizeof(buf), "AMT: %+d", (int)slot.depth);
  u8g2.drawStr(4, 48, buf);

  draw_bipolar_bar(64, 42, 60, 7, slot.depth, 4096);
  
  u8g2.drawHLine(0, 52, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(2, 60, "SELECT TO EDIT");
}
static void SCREEN_HOT(draw_view_generic_menu)(uint8_t modeIdx, uint8_t navIndex, const PatchOscBlock& oscSnap, const PatchMixBlock& mixSnap, const PatchLfoBlock& lfoSnap) {
  if (modeIdx > 10) return;
  const MenuScreenDef& def = screenMenus[modeIdx];

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 9, def.title);
  u8g2.drawHLine(0, 11, 128);

  if (def.count == 0) {
    u8g2.drawStr(2, 30, "No Items");
    return;
  }

  uint8_t activeTab = (navIndex < def.count) ? navIndex : def.count - 1;

  uint8_t startIdx = 0;
  if (activeTab >= 2) {
    startIdx = activeTab - 1;
    if (startIdx + 3 > def.count) {
      startIdx = (def.count > 3) ? def.count - 3 : 0;
    }
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  for (uint8_t i = 0; i < 3; ++i) {
    uint8_t idx = startIdx + i;
    if (idx >= def.count) break;

    uint8_t y = 24 + (i * 12);

    int32_t val = get_cached_param_value(def.items[idx].id, oscSnap, mixSnap, lfoSnap);
    char valBuf[16];
    format_menu_value(def.items[idx].id, val, valBuf, sizeof(valBuf));

    if (idx == activeTab) {
      u8g2.drawBox(1, y - 7, 126, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(3, y, def.items[idx].label);
      uint8_t vw = u8g2.getStrWidth(valBuf);
      u8g2.drawStr(125 - vw, y, valBuf);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(3, y, def.items[idx].label);
      uint8_t vw = u8g2.getStrWidth(valBuf);
      u8g2.drawStr(125 - vw, y, valBuf);
    }
  }

  u8g2.drawHLine(0, 52, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(2, 60, "ROTATE: SCROLL");
  u8g2.drawStr(80, 60, "PUSH: EDIT");
}
static void SCREEN_HOT(draw_view_manual_cal)(const Core1Snapshot& snap) {
  char toastBuf[24]; screen_cal_format_toast(snap.calTopology, snap.calStage, toastBuf, sizeof(toastBuf));
  u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(2, 9, toastBuf); u8g2.drawHLine(0, 11, 128);

  u8g2.drawFrame(14, 15, 100, 10); u8g2.drawVLine(64, 13, 14);
  int32_t cGap = snap.calGap; if(cGap < -50) cGap = -50; if(cGap > 50) cGap = 50;
  u8g2.drawBox(64 + (cGap * 46 / 50) - 2, 17, 5, 6);

  u8g2.setFont(u8g2_font_5x7_tf);
  char buf[32]; snprintf(buf, sizeof(buf), "GAP: %ld", (long)snap.calGap); u8g2.drawStr(4, 34, buf);
  snprintf(buf, sizeof(buf), "OFF: %d", snap.calOffset); u8g2.drawStr(70, 34, buf);
  snprintf(buf, sizeof(buf), "440: %u", snap.calAmp440); u8g2.drawStr(4, 45, buf);
  snprintf(buf, sizeof(buf), "PW: %u", snap.calPwCenter); u8g2.drawStr(70, 45, buf);
}

// ---------------------------------------------------------------------------
// Core 0 Entry Point
// ---------------------------------------------------------------------------
void SCREEN_HOT(update_u8g2_core0)() {
  uint32_t now = millis();

  // 1. Core 0 Local Inspector Timeout Check
  if (oledActiveInspector != InspectorType::None) {
    if (now - oledInspectorActivityMillis >= inspectorTimeoutMillis) {
      oledActiveInspector = InspectorType::None;
      oledDirty           = true;
    }
  }

  // 2. Framerate limiter (40 FPS)
  if (now - lastOledRefreshMillis < MIN_OLED_INTERVAL_MS) return;

  // 3. Snapshot Data under Mutex
  Core1Snapshot snap;
  PatchOscBlock oscSnap;
  PatchLfoBlock lfoSnap;
  PatchModBlock modSnap;
  PatchMixBlock mixSnap;

  screen_state_lock();
  snap.signalMode = serialSignal;
  snap.presetNum  = presetNumber;
  snapshot_preset_name(snap.presetName);
  
  snap.a1a = ADSR1Attack; snap.a1d = ADSR1Decay; snap.a1s = ADSR1Sustain; snap.a1r = ADSR1Release;
  snap.a2a = ADSR2Attack; snap.a2d = ADSR2Decay; snap.a2s = ADSR2Sustain; snap.a2r = ADSR2Release;
  
  snap.calTopology = screenCalTopology;
  snap.calStage    = manualCalibrationStage;
  snap.calGap      = calibrationGap;
  snap.calOffset   = offset;
  snap.calAmp440   = ampComp440Display;
  snap.calPwCenter = calPwCenterDisplay;
  
  snap.activeMenuMode = activeMenuMode;
  snap.navMenuIndex = navMenuIndex;

  oscSnap = currentOscState;
  lfoSnap = currentLfoState;
  modSnap = currentModState;
  mixSnap = currentMixState;
  screen_state_unlock();

  ScreenMode mode = static_cast<ScreenMode>(snap.signalMode);

  // =========================================================================
  // FIX: Suppress OLED rendering during Silent mode / Preset loading
  // =========================================================================
  if (mode == ScreenMode::Silent) {
    oledActiveInspector = InspectorType::None; // Kill any active inspector
    oledDirty = true;                           // Ensure clean redraw when silence ends
    lastScreenMode = ScreenMode::Silent;
    return; // Completely bypass drawing and SPI transmission
  }

  // Detect transition OUT of Silent mode -> force immediate full refresh
  // Detect transition OUT of Silent mode -> force immediate full refresh
  if (lastScreenMode == ScreenMode::Silent) {
    oledDirty = true;
  }

  static PatchOscBlock lastOscSnap;
  static PatchMixBlock lastMixSnap;
  static PatchLfoBlock lastLfoSnap;
  static PatchModBlock lastModSnap;
  static LocalGuiState lastGuiState;

  bool structChanged = false;
  if (memcmp(&oscSnap, &lastOscSnap, sizeof(PatchOscBlock)) != 0) { structChanged = true; lastOscSnap = oscSnap; }
  if (memcmp(&mixSnap, &lastMixSnap, sizeof(PatchMixBlock)) != 0) { structChanged = true; lastMixSnap = mixSnap; }
  if (memcmp(&lfoSnap, &lastLfoSnap, sizeof(PatchLfoBlock)) != 0) { structChanged = true; lastLfoSnap = lfoSnap; }
  if (memcmp(&modSnap, &lastModSnap, sizeof(PatchModBlock)) != 0) { structChanged = true; lastModSnap = modSnap; }
  if (memcmp(&guiState, &lastGuiState, sizeof(LocalGuiState)) != 0) { structChanged = true; lastGuiState = guiState; }

  if (snap.presetNum != lastPresetNum ||
      strncmp(snap.presetName, lastPresetName, 16) != 0 ||
      mode != lastScreenMode ||
      oledActiveInspector != lastActiveInsp ||
      snap.activeMenuMode != lastMenuMode ||
      snap.navMenuIndex != lastNavIdx ||
      snap.calStage != lastCalStage ||
      snap.calGap != lastCalGap ||
      structChanged) {
    oledDirty = true;
  }

  if (!oledDirty) return;

  // Update State Tracking Cache
  lastPresetNum    = snap.presetNum;
  strncpy(lastPresetName, snap.presetName, 16);
  lastScreenMode   = mode;
  lastActiveInsp   = oledActiveInspector;
  lastMenuMode     = snap.activeMenuMode;
  lastNavIdx       = snap.navMenuIndex;
  lastCalStage     = snap.calStage;
  lastCalGap       = snap.calGap;

  lastOledRefreshMillis = now;
  oledDirty = false;

  u8g2.clearBuffer();

  if (mode == ScreenMode::ManualCalibration) {
    // The visual gap tracker
    draw_view_manual_cal(snap);
  } 
  else if (snap.activeMenuMode == 8) {
    // Custom Mod Matrix Rendering Table
    draw_view_mod_matrix(snap.navMenuIndex, modSnap);
  }
  else if (snap.activeMenuMode != 0) {
    // Any of the other generic menu lists (Envelopes, Calibration, LFOs, etc)
    draw_view_generic_menu(snap.activeMenuMode, snap.navMenuIndex, oscSnap, mixSnap, lfoSnap);
  } 
  else {
    // Standard Dashboard or Inspectors
    if (oledActiveInspector != InspectorType::None) {
        switch (oledActiveInspector) {
          case InspectorType::Filter:      draw_inspector_filter(mixSnap); break;
          case InspectorType::Oscillators: draw_inspector_osc(oscSnap, mixSnap, lfoSnap); break;
          case InspectorType::ADSR1:       draw_inspector_adsr(1, mixSnap, snap.a1a, snap.a1d, snap.a1s, snap.a1r, lfoSnap); break;
          case InspectorType::ADSR2:       draw_inspector_adsr(2, mixSnap, snap.a2a, snap.a2d, snap.a2s, snap.a2r, lfoSnap); break;
          case InspectorType::ADSR3:       draw_inspector_adsr(3, mixSnap, snap.a1a, snap.a1d, snap.a1s, snap.a1r, lfoSnap); break;
          case InspectorType::LFO1:        draw_inspector_lfo(1, lfoSnap); break;
          case InspectorType::LFO2:        draw_inspector_lfo(2, lfoSnap); break;
          case InspectorType::LFO3:        draw_inspector_lfo(3, lfoSnap); break;
          case InspectorType::VoiceEngine: draw_inspector_voice(oscSnap); break;
          default:                         draw_view_preset_enhanced(snap, oscSnap, mixSnap, modSnap); break;
        }
      } else {
        draw_view_preset_enhanced(snap, oscSnap, mixSnap, modSnap);
      }
  }

  u8g2.sendBuffer();
}