#include "menues.h"
#include <stdio.h>
#include <stdlib.h>
#include "_build_libs/DCO-PROTOCOL/param_meta.h"
#include "screen_target.h"


LV_FONT_DECLARE(orbtiron_10);
LV_FONT_DECLARE(orbtiron_12);
LV_FONT_DECLARE(orbtiron_14);
LV_FONT_DECLARE(orbtiron_16);
LV_FONT_DECLARE(orbtiron_18);
LV_FONT_DECLARE(orbtiron_20);
LV_FONT_DECLARE(orbtiron_22);

LV_FONT_DECLARE(roboto_10);
LV_FONT_DECLARE(roboto_12);
LV_FONT_DECLARE(roboto_14);
LV_FONT_DECLARE(roboto_16);
LV_FONT_DECLARE(roboto_18);
LV_FONT_DECLARE(roboto_20);
LV_FONT_DECLARE(roboto_22);

LV_FONT_DECLARE(raj_10);
LV_FONT_DECLARE(raj_12);
LV_FONT_DECLARE(raj_14);
LV_FONT_DECLARE(raj_16);
LV_FONT_DECLARE(raj_18);
LV_FONT_DECLARE(raj_20);
LV_FONT_DECLARE(raj_22);
LV_FONT_DECLARE(raj_24);

LV_FONT_DECLARE(raj_med_18);
LV_FONT_DECLARE(raj_med_20);
LV_FONT_DECLARE(raj_med_22);
LV_FONT_DECLARE(raj_med_24);
LV_FONT_DECLARE(raj_med_26);
LV_FONT_DECLARE(raj_med_28);

// -----------------------------------------------------------------------------
// FONT CONFIGURATION
// -----------------------------------------------------------------------------
#if defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
  #define MENU_TITLE_FONT &lv_font_montserrat_18
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  #define MENU_TITLE_FONT &lv_font_montserrat_20
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
  #define MENU_TITLE_FONT &lv_font_montserrat_16
#else
  #define MENU_TITLE_FONT LV_FONT_DEFAULT
#endif

#if defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
  #define MENU_ITEM_FONT &lv_font_montserrat_16
#elif defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
  #define MENU_ITEM_FONT &lv_font_montserrat_14
#else
  #define MENU_ITEM_FONT LV_FONT_DEFAULT
#endif

#define MENU_TITLE_FONT &raj_med_28
#define MENU_ITEM_FONT  &raj_med_26
// -----------------------------------------------------------------------------
// 1. MENU DEFINITIONS
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// 1. MENU DEFINITIONS
// -----------------------------------------------------------------------------
const MenuScreenDef screenMenus[] = {
  {"", 0, {}}, // Mode 0: NORMAL (Closed)
  
  // Mode 1: ENVELOPE MENU
  {"ENVELOPE MENU", 10, {
      {"ADSR1 Attack Curve", ParamId::PARAM_ADSR1_ATTACK_CURVE},
      {"ADSR1 Decay Curve",  ParamId::PARAM_ADSR1_DECAY_CURVE},
      {"ADSR2 Attack Curve", ParamId::PARAM_ADSR2_ATTACK_CURVE},
      {"ADSR2 Decay Curve",  ParamId::PARAM_ADSR2_DECAY_CURVE},
      {"VCA Env Restart",    ParamId::PARAM_ADSR1_RESTART},
      {"VCF Env Restart",    ParamId::PARAM_ADSR2_RESTART},
      {"ADSR3 Enable",       ParamId::PARAM_ADSR3_ENABLED},
      {"ADSR3 Osc Select",   ParamId::PARAM_ADSR3_TO_OSC_SELECT},
      {"ADSR3 -> PWM",       ParamId::PARAM_ADSR3_TO_PWM},
      {"ADSR3 -> Pitch",     ParamId::PARAM_ADSR3_TO_DETUNE1}
  }},
  
  // Mode 2: ADSR 1
  {"ADSR 1 (VCA) SETTINGS", 5, {
      {"Attack Curve",    ParamId::PARAM_ADSR1_ATTACK_CURVE},
      {"Decay Curve",     ParamId::PARAM_ADSR1_DECAY_CURVE},
      {"Release Curve",   ParamId::PARAM_ADSR1_RELEASE_CURVE},
      {"VCA Env Restart", ParamId::PARAM_ADSR1_RESTART},
      {"Envelope Mode",   ParamId::PARAM_ADSR1_MODE}
  }},
  
  // Mode 3: ADSR 2
  {"ADSR 2 (VCF) SETTINGS", 5, {
      {"Attack Curve",    ParamId::PARAM_ADSR2_ATTACK_CURVE},
      {"Decay Curve",     ParamId::PARAM_ADSR2_DECAY_CURVE},
      {"Release Curve",   ParamId::PARAM_ADSR2_RELEASE_CURVE},
      {"VCF Env Restart", ParamId::PARAM_ADSR2_RESTART},
      {"Envelope Mode",   ParamId::PARAM_ADSR2_MODE}
  }},
  
  // Mode 4: ADSR 3 
  {"ADSR 3 (PITCH/PWM) SETTINGS", 9, {
      {"Attack Curve",      ParamId::PARAM_ADSR3_ATTACK_CURVE},
      {"Decay Curve",       ParamId::PARAM_ADSR3_DECAY_CURVE},
      {"Release Curve",     ParamId::PARAM_ADSR3_RELEASE_CURVE},
      {"Env Restart",       ParamId::PARAM_ADSR3_RESTART},
      {"Envelope Mode",     ParamId::PARAM_ADSR3_MODE},
      {"Oscillator Select", ParamId::PARAM_ADSR3_TO_OSC_SELECT},
      {"Mod Depth -> PWM",  ParamId::PARAM_ADSR3_TO_PWM},
      {"Mod Depth -> Pitch",ParamId::PARAM_ADSR3_TO_DETUNE1},
      {"Env Enable",        ParamId::PARAM_ADSR3_ENABLED}
  }},
  
  {"SYSTEM SETTINGS", 1, {{"Placeholder", ParamId::PARAM_SINE_STATUS}}}, // Mode 5

  // Mode 6: DCO ARCHITECTURE
#if PROJECT_INSTRUMENT == 3
  {"DCO ARCHITECTURE", 8, {
      {"Voice Mode",        ParamId::PARAM_VOICE_MODE},
      {"Voice Alloc",       ParamId::PARAM_VOICE_ALLOC_MODE},
      {"OSC 1 Octave",      ParamId::PARAM_OSC1_INTERVAL},
      {"OSC 2 Interval",    ParamId::PARAM_OSC2_INTERVAL},
      {"OSC 3 Interval",    ParamId::PARAM_OSC3_INTERVAL},
      {"Phase Sync",        ParamId::PARAM_OSC_PHASE_SYNC},
      {"Sync Mode",         ParamId::PARAM_SYNC_MODE},
      {"Soft Sync Limit",   ParamId::PARAM_SOFT_SYNC}
  }},
#else
  {"DCO ARCHITECTURE", 7, {
      {"Voice Mode",        ParamId::PARAM_VOICE_MODE},
      {"Voice Alloc",       ParamId::PARAM_VOICE_ALLOC_MODE},
      {"OSC 1 Octave",      ParamId::PARAM_OSC1_INTERVAL},
      {"OSC 2 Interval",    ParamId::PARAM_OSC2_INTERVAL},
      {"Phase Sync",        ParamId::PARAM_OSC_PHASE_SYNC},
      {"Sync Mode",         ParamId::PARAM_SYNC_MODE},
      {"Soft Sync Limit",   ParamId::PARAM_SOFT_SYNC}
  }},
#endif

  // Mode 7: DCO MODULATION (Combined 14 / 9 items)
#if PROJECT_INSTRUMENT == 3
  {"DCO MODULATION", 14, {
      {"OSC 2 Detune",      ParamId::PARAM_OSC2_DETUNE_VAL},
      {"OSC 3 Detune",      ParamId::PARAM_OSC3_DETUNE_VAL},
      {"Unison Detune",     ParamId::PARAM_UNISON_DETUNE},
      {"Portamento Time",   ParamId::PARAM_PORTAMENTO_TIME},
      {"Portamento Mode",   ParamId::PARAM_PORTAMENTO_MODE},
      {"Subosc Divide",     ParamId::PARAM_SUBOSC_DIVIDE},
      {"LFO1 -> All DCOs",  ParamId::PARAM_LFO1_TO_DCO},
      {"LFO1 -> OSC1",      ParamId::PARAM_LFO1_TO_OSC1},
      {"LFO1 -> OSC2",      ParamId::PARAM_LFO1_TO_OSC2},
      {"LFO1 -> OSC3",      ParamId::PARAM_LFO1_TO_OSC3},
      {"LFO2 -> OSC2 Fine", ParamId::PARAM_LFO2_TO_OSC2},
      {"LFO2 -> OSC3 Fine", ParamId::PARAM_LFO2_TO_OSC3},
      {"LFO2 -> OSC2 Crse", ParamId::PARAM_LFO2_TO_OSC2_COARSE},
      {"LFO2 -> OSC3 Crse", ParamId::PARAM_LFO2_TO_OSC3_COARSE}
  }},
#else
  {"DCO MODULATION", 9, {
      {"OSC 2 Detune",      ParamId::PARAM_OSC2_DETUNE_VAL},
      {"Unison Detune",     ParamId::PARAM_UNISON_DETUNE},
      {"Portamento Time",   ParamId::PARAM_PORTAMENTO_TIME},
      {"Portamento Mode",   ParamId::PARAM_PORTAMENTO_MODE},
      {"LFO1 -> All DCOs",  ParamId::PARAM_LFO1_TO_DCO},
      {"LFO1 -> OSC1",      ParamId::PARAM_LFO1_TO_OSC1},
      {"LFO1 -> OSC2",      ParamId::PARAM_LFO1_TO_OSC2},
      {"LFO2 -> OSC2 Fine", ParamId::PARAM_LFO2_TO_OSC2},
      {"LFO2 -> OSC2 Crse", ParamId::PARAM_LFO2_TO_OSC2_COARSE}
  }},
#endif

  // Mode 8: MOD MATRIX
  {"MODULATION MATRIX", 1, {
      {"Slot 0 Source",     ParamId::PARAM_MOD_SLOT0_SOURCE}
  }},
  
  // Mode 9: LFO MENU
  {"LFO MENU", 8, {
      {"LFO1 Waveform",      ParamId::PARAM_LFO1_WAVEFORM},
      {"LFO1 Speed",         ParamId::PARAM_LFO1_SPEED},
      {"LFO1 -> OSC1",       ParamId::PARAM_LFO1_TO_OSC1},
      {"LFO1 -> OSC2",       ParamId::PARAM_LFO1_TO_OSC2},
      {"LFO2 Waveform",      ParamId::PARAM_LFO2_WAVEFORM},
      {"LFO2 Speed",         ParamId::PARAM_LFO2_SPEED},
      {"LFO2 -> OSC2 Fine",  ParamId::PARAM_LFO2_TO_OSC2},
      {"LFO2 -> OSC2 Crse",  ParamId::PARAM_LFO2_TO_OSC2_COARSE}
  }},

  // Mode 10: CALIBRATION MENU
  {"CALIBRATION MENU", 6, {
      {"Fast Amp-Comp",      ParamId::PARAM_CALIBRATION_FLAG},
      {"Normal Amp-Comp",    ParamId::PARAM_CALIBRATION_FLAG},
      {"Refine Amp-Comp",    ParamId::PARAM_CALIBRATION_FLAG},
      {"PW Calibration",     ParamId::PARAM_CALIBRATION_FLAG},
      {"Full Routine",       ParamId::PARAM_CALIBRATION_FLAG},
      {"Manual Calibration", ParamId::PARAM_MANUAL_CALIBRATION_FLAG}

  }}
};

lv_obj_t* ui_FullMenuPanel = nullptr;
lv_obj_t* ui_FullMenuTitle = nullptr;
lv_obj_t* ui_MenuRows[16] = { nullptr };
lv_obj_t* ui_MenuRowLabels[16] = { nullptr };
lv_obj_t* ui_MenuRowValues[16] = { nullptr };

static uint8_t lastFocusedIndex = 255;
static uint8_t lastMenuMode = 255;

// -----------------------------------------------------------------------------
// 2. LIVE CACHE LOOKUP
// -----------------------------------------------------------------------------
int32_t SCREEN_HOT(get_cached_param_value)(ParamId id, const PatchOscBlock& osc, const PatchMixBlock& mix, const PatchLfoBlock& lfo) {
  switch(id) {
      // ADSR 1
      case ParamId::PARAM_ADSR1_ATTACK_CURVE:  return mix.adsr1_attack_curve;
      case ParamId::PARAM_ADSR1_DECAY_CURVE:   return mix.adsr1_decay_curve;
      case ParamId::PARAM_ADSR1_RELEASE_CURVE: return mix.adsr1_release_curve;
      case ParamId::PARAM_ADSR1_RESTART:       return (mix.misc_flags & (1<<1)) ? 1 : 0;
      case ParamId::PARAM_ADSR1_MODE:          return mix.adsr1_mode;
      
      // ADSR 2
      case ParamId::PARAM_ADSR2_ATTACK_CURVE:  return mix.adsr2_attack_curve;
      case ParamId::PARAM_ADSR2_DECAY_CURVE:   return mix.adsr2_decay_curve;
      case ParamId::PARAM_ADSR2_RELEASE_CURVE: return mix.adsr2_release_curve;
      case ParamId::PARAM_ADSR2_RESTART:       return (mix.misc_flags & (1<<2)) ? 1 : 0;
      case ParamId::PARAM_ADSR2_MODE:          return mix.adsr2_mode;
      
      // ADSR 3
      case ParamId::PARAM_ADSR3_ATTACK_CURVE:  return mix.adsr3_attack_curve;
      case ParamId::PARAM_ADSR3_DECAY_CURVE:   return mix.adsr3_decay_curve;
      case ParamId::PARAM_ADSR3_RELEASE_CURVE: return mix.adsr3_release_curve;
      case ParamId::PARAM_ADSR3_RESTART:       return (mix.misc_flags & (1<<3)) ? 1 : 0;
      case ParamId::PARAM_ADSR3_ENABLED:       return (mix.misc_flags & (1<<4)) ? 1 : 0;
      case ParamId::PARAM_ADSR3_MODE:          return lfo.adsr3_mode;
      case ParamId::PARAM_ADSR3_TO_OSC_SELECT: return lfo.adsr3_to_osc_select;
      case ParamId::PARAM_ADSR3_TO_PWM:        return lfo.adsr3_to_pwm;
      case ParamId::PARAM_ADSR3_TO_DETUNE1:    return lfo.adsr3_to_detune1;

      // LFO Menu & Routings
      case ParamId::PARAM_LFO1_WAVEFORM:       return currentLfoState.lfo1_waveform;
      case ParamId::PARAM_LFO1_SPEED:          return currentLfoState.lfo1_speed;
      case ParamId::PARAM_LFO1_TO_OSC1:        return currentLfoState.lfo1_to_osc1;
      case ParamId::PARAM_LFO1_TO_OSC2:        return currentLfoState.lfo1_to_osc2;
      case ParamId::PARAM_LFO2_WAVEFORM:       return currentLfoState.lfo2_waveform;
      case ParamId::PARAM_LFO2_SPEED:          return currentLfoState.lfo2_speed;
      case ParamId::PARAM_LFO2_TO_OSC2:        return currentLfoState.lfo2_to_osc2;
      case ParamId::PARAM_LFO2_TO_OSC2_COARSE: return currentLfoState.lfo2_to_osc2_coarse;
      case ParamId::PARAM_LFO1_TO_DCO:         return currentLfoState.lfo1_to_dco;
      case ParamId::PARAM_LFO2_TO_PW:          return currentLfoState.lfo2_to_pw;
      case ParamId::PARAM_LFO1_TO_OSC3:        return lfo.lfo1_to_osc3;
      case ParamId::PARAM_LFO2_TO_OSC3:        return lfo.lfo2_to_osc3;
      case ParamId::PARAM_LFO2_TO_OSC3_COARSE: return lfo.lfo2_to_osc3_coarse;

      // DCO Architecture & Modulation
      case ParamId::PARAM_VOICE_MODE:          return osc.voice_mode;
      case ParamId::PARAM_VOICE_ALLOC_MODE:    return osc.voice_alloc_mode;
      case ParamId::PARAM_OSC1_INTERVAL:       return osc.osc1_interval;
      case ParamId::PARAM_OSC2_INTERVAL:       return osc.osc2_interval;
      case ParamId::PARAM_OSC3_INTERVAL:       return osc.osc3_interval;
      case ParamId::PARAM_OSC_PHASE_SYNC:      return osc.osc_phase_sync;
      case ParamId::PARAM_SYNC_MODE:           return osc.sync_mode;
      case ParamId::PARAM_SOFT_SYNC:           return osc.soft_sync;
      case ParamId::PARAM_OSC2_DETUNE_VAL:     return osc.osc2_detune;
      case ParamId::PARAM_OSC3_DETUNE_VAL:     return osc.osc3_detune;
      case ParamId::PARAM_UNISON_DETUNE:       return osc.unison_detune;
      case ParamId::PARAM_PORTAMENTO_TIME:     return osc.portamento_time;
      case ParamId::PARAM_PORTAMENTO_MODE:     return osc.portamento_mode;
      case ParamId::PARAM_SUBOSC_DIVIDE:       return osc.subosc_divide;
      
      default: return 0;
  }
}


// -----------------------------------------------------------------------------
// 3. PARAM_META STRING DELEGATION
// -----------------------------------------------------------------------------
void SCREEN_HOT(format_menu_value)(ParamId id, int32_t val, char* buf, size_t maxlen) {
  int32_t scaled = param_scale_display_value(id, val);

  switch(id) {
      // LFO Waveforms
      case ParamId::PARAM_LFO1_WAVEFORM:
      case ParamId::PARAM_LFO2_WAVEFORM:
      case ParamId::PARAM_LFO3_WAVEFORM:
          snprintf(buf, maxlen, "%s", param_lfo_waveform_name(scaled));
          break;

      // Bézier Curves (0..7)
      case ParamId::PARAM_ADSR1_ATTACK_CURVE:
      case ParamId::PARAM_ADSR1_DECAY_CURVE:
      case ParamId::PARAM_ADSR1_RELEASE_CURVE:
      case ParamId::PARAM_ADSR2_ATTACK_CURVE:
      case ParamId::PARAM_ADSR2_DECAY_CURVE:
      case ParamId::PARAM_ADSR2_RELEASE_CURVE:
      case ParamId::PARAM_ADSR3_ATTACK_CURVE:
      case ParamId::PARAM_ADSR3_DECAY_CURVE:
      case ParamId::PARAM_ADSR3_RELEASE_CURVE:
          snprintf(buf, maxlen, "%s", param_curve_name(scaled));
          break;

      // Envelope Modes (Normal, Centered, Inverted)
      case ParamId::PARAM_ADSR1_MODE:
      case ParamId::PARAM_ADSR2_MODE:
      case ParamId::PARAM_ADSR3_MODE:
          snprintf(buf, maxlen, "%s", param_env_mode_name(scaled));
          break;

      // Voice Architecture Enums
      case ParamId::PARAM_VOICE_MODE:
          snprintf(buf, maxlen, "%s", param_voice_mode_name(scaled));
          break;
      case ParamId::PARAM_VOICE_ALLOC_MODE:
          snprintf(buf, maxlen, "%s", param_voice_alloc_name(scaled));
          break;

      // Sync & Portamento Enums
      case ParamId::PARAM_SYNC_MODE:
          snprintf(buf, maxlen, "%s", param_sync_mode_name(scaled));
          break;
      case ParamId::PARAM_OSC_PHASE_SYNC:
          snprintf(buf, maxlen, "%s", param_phase_sync_mode_name(scaled));
          break;
      case ParamId::PARAM_SOFT_SYNC:
          snprintf(buf, maxlen, "%s", param_soft_sync_mode_name(scaled));
          break;
      case ParamId::PARAM_PORTAMENTO_MODE:
          snprintf(buf, maxlen, "%s", param_portamento_mode_name(scaled));
          break;

      // Bipolar Intervals & Detunes
      case ParamId::PARAM_OSC1_INTERVAL:
          snprintf(buf, maxlen, "%+ld OCT", scaled);
          break;
      case ParamId::PARAM_OSC2_INTERVAL:
      case ParamId::PARAM_OSC3_INTERVAL:
          snprintf(buf, maxlen, "%+ld SEMI", scaled);
          break;
      case ParamId::PARAM_OSC2_DETUNE_VAL:
      case ParamId::PARAM_OSC3_DETUNE_VAL:
      case ParamId::PARAM_ADSR3_TO_PWM:
          snprintf(buf, maxlen, "%+ld", scaled);
          break;

      // Toggles & Switches
      case ParamId::PARAM_ADSR1_RESTART:
      case ParamId::PARAM_ADSR2_RESTART:
      case ParamId::PARAM_ADSR3_ENABLED:
          snprintf(buf, maxlen, scaled ? " ON" : " OFF");
          break;

      // Calibration Triggers
      case ParamId::PARAM_CALIBRATION_FLAG:
      case ParamId::PARAM_MANUAL_CALIBRATION_FLAG:
          snprintf(buf, maxlen, " RUN ");
          break;

      // Dynamic OSC Routing & Dividers
      case ParamId::PARAM_ADSR3_TO_OSC_SELECT:
          snprintf(buf, maxlen, "%s", screen_adsr3_osc_select_label(screen_cal_topology(), scaled));
          break;
      case ParamId::PARAM_SUBOSC_DIVIDE:
          if(scaled == 0) snprintf(buf, maxlen, " OFF");
          else            snprintf(buf, maxlen, " /%ld", scaled);
          break;

      // Default Raw Numeric Display (LFO Speeds, LFO Pitch Routes, Detunes, Levels)
      default:
          snprintf(buf, maxlen, " %ld", scaled);
          break;
  }
}

// -----------------------------------------------------------------------------
// 4. UI SETUP (16 Rows with Auto-Scroll)
// -----------------------------------------------------------------------------
void setupMenues() {
  ui_FullMenuPanel = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(ui_FullMenuPanel);
  lv_obj_set_size(ui_FullMenuPanel, 480, 320);
  lv_obj_set_pos(ui_FullMenuPanel, 0, 0);
  lv_obj_set_style_bg_color(ui_FullMenuPanel, lv_color_hex(0x0C0C0C), 0);
  lv_obj_set_style_bg_opa(ui_FullMenuPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(ui_FullMenuPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);

  // Top Title
  ui_FullMenuTitle = lv_label_create(ui_FullMenuPanel);
  lv_obj_align(ui_FullMenuTitle, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_text_color(ui_FullMenuTitle, lv_color_hex(0xFF8800), 0);
  lv_obj_set_style_text_font(ui_FullMenuTitle, MENU_TITLE_FONT, 0);

  // List Container (Scrollable when items > 8)
  lv_obj_t* listCont = lv_obj_create(ui_FullMenuPanel);
  lv_obj_remove_style_all(listCont);
  lv_obj_set_size(listCont, 464, 278);
  lv_obj_align(listCont, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_set_style_bg_opa(listCont, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(listCont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(listCont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(listCont, 3, 0);
  lv_obj_set_scrollbar_mode(listCont, LV_SCROLLBAR_MODE_AUTO);

  // Pre-create up to 16 Rows
  for(int i = 0; i < 16; i++) {
    ui_MenuRows[i] = lv_obj_create(listCont);
    lv_obj_remove_style_all(ui_MenuRows[i]);
    lv_obj_set_size(ui_MenuRows[i], 456, 31);
    lv_obj_set_style_bg_color(ui_MenuRows[i], lv_color_hex(0x1B1B1B), 0);
    lv_obj_set_style_bg_opa(ui_MenuRows[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_MenuRows[i], 5, 0);
    lv_obj_clear_flag(ui_MenuRows[i], LV_OBJ_FLAG_SCROLLABLE);

    // Left Name
    ui_MenuRowLabels[i] = lv_label_create(ui_MenuRows[i]);
    lv_obj_align(ui_MenuRowLabels[i], LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_text_color(ui_MenuRowLabels[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ui_MenuRowLabels[i], MENU_ITEM_FONT, 0);

    // Right Value
    ui_MenuRowValues[i] = lv_label_create(ui_MenuRows[i]);
    lv_obj_align(ui_MenuRowValues[i], LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_color(ui_MenuRowValues[i], lv_color_hex(0x00E5A3), 0);
    lv_obj_set_style_text_font(ui_MenuRowValues[i], MENU_ITEM_FONT, 0);
  }
}

// --- MOD MATRIX GLOBALS ---
lv_obj_t* ui_ModMatrixPanel = nullptr;
lv_obj_t* ui_MM_Rows[8] = { nullptr };
lv_obj_t* ui_MM_SlotNum[8] = { nullptr };
lv_obj_t* ui_MM_Source[8] = { nullptr };
lv_obj_t* ui_MM_Dest[8] = { nullptr };
lv_obj_t* ui_MM_AmountVal[8] = { nullptr }; // <--- Brought back!
lv_obj_t* ui_MM_AmountBar[8] = { nullptr };

static uint8_t mmLastFocusedSlot = 255;

void setupModMatrix() {
  ui_ModMatrixPanel = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(ui_ModMatrixPanel);
  lv_obj_set_size(ui_ModMatrixPanel, 480, 320);
  lv_obj_set_style_bg_color(ui_ModMatrixPanel, lv_color_hex(0x0C0C0C), 0);
  lv_obj_set_style_bg_opa(ui_ModMatrixPanel, LV_OPA_COVER, 0);
  lv_obj_add_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t* title = lv_label_create(ui_ModMatrixPanel);
  lv_label_set_text(title, "MODULATION MATRIX");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFF8800), 0);
  lv_obj_set_style_text_font(title, MENU_TITLE_FONT, 0);

  // Headers (Shifted for better spacing)
  lv_obj_t* hSrc = lv_label_create(ui_ModMatrixPanel);
  lv_label_set_text(hSrc, "SOURCE");
  lv_obj_align(hSrc, LV_ALIGN_TOP_LEFT, 50, 36);
  lv_obj_set_style_text_color(hSrc, lv_color_hex(0x555555), 0);

  lv_obj_t* hDst = lv_label_create(ui_ModMatrixPanel);
  lv_label_set_text(hDst, "DESTINATION");
  lv_obj_align(hDst, LV_ALIGN_TOP_LEFT, 200, 36);
  lv_obj_set_style_text_color(hDst, lv_color_hex(0x555555), 0);

  lv_obj_t* hAmt = lv_label_create(ui_ModMatrixPanel);
  lv_label_set_text(hAmt, "AMOUNT");
  lv_obj_align(hAmt, LV_ALIGN_TOP_LEFT, 380, 36);
  lv_obj_set_style_text_color(hAmt, lv_color_hex(0x555555), 0);

  // 8 Tabular Rows
  for(int i = 0; i < 8; i++) {
    ui_MM_Rows[i] = lv_obj_create(ui_ModMatrixPanel);
    lv_obj_remove_style_all(ui_MM_Rows[i]);
    // Made rows slightly taller (30px) and spaced beautifully
    lv_obj_set_size(ui_MM_Rows[i], 460, 30);
    lv_obj_align(ui_MM_Rows[i], LV_ALIGN_TOP_MID, 0, 58 + (i * 31)); 
    lv_obj_set_style_bg_color(ui_MM_Rows[i], lv_color_hex(0x1B1B1B), 0);
    lv_obj_set_style_bg_opa(ui_MM_Rows[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_MM_Rows[i], 4, 0);
    lv_obj_clear_flag(ui_MM_Rows[i], LV_OBJ_FLAG_SCROLLABLE);

    // Slot Number
    ui_MM_SlotNum[i] = lv_label_create(ui_MM_Rows[i]);
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", i+1);
    lv_label_set_text(ui_MM_SlotNum[i], buf);
    lv_obj_align(ui_MM_SlotNum[i], LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(ui_MM_SlotNum[i], lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(ui_MM_SlotNum[i], MENU_ITEM_FONT, 0);

    // Source Text (Shifted to X=40, giving it 150px to breathe)
    ui_MM_Source[i] = lv_label_create(ui_MM_Rows[i]);
    lv_label_set_text(ui_MM_Source[i], "Off");
    lv_obj_align(ui_MM_Source[i], LV_ALIGN_LEFT_MID, 40, 0);
    lv_obj_set_style_text_font(ui_MM_Source[i], MENU_ITEM_FONT, 0);
    
    // Dest Text (Shifted to X=190, giving it 140px to breathe)
    ui_MM_Dest[i] = lv_label_create(ui_MM_Rows[i]);
    lv_label_set_text(ui_MM_Dest[i], "Pitch");
    lv_obj_align(ui_MM_Dest[i], LV_ALIGN_LEFT_MID, 190, 0);
    lv_obj_set_style_text_font(ui_MM_Dest[i], MENU_ITEM_FONT, 0);

    // Amount Value Text (Small font stacked ABOVE the bar)
    ui_MM_AmountVal[i] = lv_label_create(ui_MM_Rows[i]);
    lv_label_set_text(ui_MM_AmountVal[i], "0");
    lv_obj_align(ui_MM_AmountVal[i], LV_ALIGN_TOP_RIGHT, -10, 1);
    lv_obj_set_style_text_font(ui_MM_AmountVal[i], &raj_18, 0); // <--- Much smaller font

    // Amount Bipolar Bar Graph (Thinner, stacked BELOW the text)
    ui_MM_AmountBar[i] = lv_bar_create(ui_MM_Rows[i]);
    lv_obj_set_size(ui_MM_AmountBar[i], 110, 6); // Thinner & narrower
    lv_obj_align(ui_MM_AmountBar[i], LV_ALIGN_BOTTOM_RIGHT, -10, -4);
    lv_bar_set_range(ui_MM_AmountBar[i], -4096, 4096); 
    lv_bar_set_mode(ui_MM_AmountBar[i], LV_BAR_MODE_SYMMETRICAL);
    lv_obj_set_style_bg_color(ui_MM_AmountBar[i], lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_MM_AmountBar[i], lv_color_hex(0x00E5A3), LV_PART_INDICATOR);
  }
}

// -----------------------------------------------------------------------------
// 5. MAIN RENDER LOOP (Now with Cache Optimization for ALL menus)
// -----------------------------------------------------------------------------
void SCREEN_HOT(updateGenericFocus)(const Core1Snapshot &snap) {
  // 1. If NO menu is active, hide everything
  if (snap.activeMenuMode == 0) {
    if (!lv_obj_has_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);
    if (!lv_obj_has_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN);
    lastMenuMode = 255;
    return;
  }

  // 2. --- THE UISTATE ROUTER ---
  // Is it Mode 8? (Mod Matrix)
  if (snap.activeMenuMode == 8) {
    // Hide the standard list, show the tabular grid
    if (!lv_obj_has_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_has_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN);
    
    updateModMatrixView(snap); 
    lastMenuMode = 8;
    return; // <-- CRITICAL: We return here so the old menu code below does NOT run!
  }

  // 3. Otherwise, it's a standard menu list (Modes 1-7). Show standard, hide ModMatrix.
  if (!lv_obj_has_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN);
  if (lv_obj_has_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);


  // --- ORIGINAL MENU RENDERING LOGIC (Modes 1-7) ---
  
  // CACHE: Prevents LVGL from redrawing the text every frame!
  static int32_t cachedMenuValues[16] = {
    0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 
    0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
    0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
    0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF
  };

  static constexpr uint8_t TOTAL_MENUS = sizeof(screenMenus) / sizeof(screenMenus[0]);
  uint8_t modeIdx = (snap.activeMenuMode < TOTAL_MENUS) ? snap.activeMenuMode : 0;
  const MenuScreenDef& def = screenMenus[modeIdx];

  // Mode change
  if (lastMenuMode != snap.activeMenuMode) {
    lastMenuMode = snap.activeMenuMode;
    lv_label_set_text(ui_FullMenuTitle, def.title);

    for (int i = 0; i < 16; i++) {
      // Force a redraw of the text values when switching menus
      cachedMenuValues[i] = 0x7FFFFFFF; 

      // Clean all rows so no ghost highlight persists
      lv_obj_set_style_bg_color(ui_MenuRows[i], lv_color_hex(0x1B1B1B), 0);
      lv_obj_set_style_text_color(ui_MenuRowLabels[i], lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_text_color(ui_MenuRowValues[i], lv_color_hex(0x00E5A3), 0);

      if (i < def.count) {
        lv_label_set_text(ui_MenuRowLabels[i], def.items[i].label);
        lv_obj_remove_flag(ui_MenuRows[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ui_MenuRows[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    lastFocusedIndex = 255;
  }

  // Cursor focus & Auto-Scroll
  if (lastFocusedIndex != snap.navMenuIndex) {
    if (lastFocusedIndex < 16) {
      lv_obj_set_style_bg_color(ui_MenuRows[lastFocusedIndex], lv_color_hex(0x1B1B1B), 0);
      lv_obj_set_style_text_color(ui_MenuRowLabels[lastFocusedIndex], lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_text_color(ui_MenuRowValues[lastFocusedIndex], lv_color_hex(0x00E5A3), 0);
    }
    if (snap.navMenuIndex < 16 && snap.navMenuIndex < def.count) {
      lv_obj_set_style_bg_color(ui_MenuRows[snap.navMenuIndex], lv_color_hex(0x00FF88), 0);
      lv_obj_set_style_text_color(ui_MenuRowLabels[snap.navMenuIndex], lv_color_hex(0x000000), 0);
      lv_obj_set_style_text_color(ui_MenuRowValues[snap.navMenuIndex], lv_color_hex(0x000000), 0);
      lv_obj_scroll_to_view(ui_MenuRows[snap.navMenuIndex], LV_ANIM_OFF);
    }
    lastFocusedIndex = snap.navMenuIndex;
  }

// Live value update (Optimized with Cache)
char valBuf[32];
for (int i = 0; i < def.count; i++) {
  int32_t val = get_cached_param_value(def.items[i].id, currentOscState, currentMixState, currentLfoState);
  
  // ONLY format string and touch LVGL if the value actually changed!
  if (val != cachedMenuValues[i]) {
    cachedMenuValues[i] = val;
    format_menu_value(def.items[i].id, val, valBuf, sizeof(valBuf));
    lv_label_set_text(ui_MenuRowValues[i], valBuf);
  }
}
}



// Fast helper to update colors for ONLY the row that gained/lost focus
static void SCREEN_HOT(set_row_focus_style)(uint8_t row, bool isFocused) {
  if (row >= 8) return;
  
  // Row Background
  lv_obj_set_style_bg_color(ui_MM_Rows[row], isFocused ? lv_color_hex(0x00FF88) : lv_color_hex(0x1B1B1B), 0);

  // Text Colors (Standard White when unfocused, Solid Black when focused)
  lv_color_t textCol = isFocused ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
  lv_obj_set_style_text_color(ui_MM_SlotNum[row], isFocused ? lv_color_hex(0x000000) : lv_color_hex(0x555555), 0);
  lv_obj_set_style_text_color(ui_MM_Source[row], textCol, 0);
  lv_obj_set_style_text_color(ui_MM_Dest[row], textCol, 0);
  
  // Amount Color (Standard Green when unfocused, Solid Black when focused)
  lv_obj_set_style_text_color(ui_MM_AmountVal[row], isFocused ? lv_color_hex(0x000000) : lv_color_hex(0x00E5A3), 0);

  // Bar Colors (Always active, no greying out)
  lv_obj_set_style_bg_color(ui_MM_AmountBar[row], isFocused ? lv_color_hex(0x000000) : lv_color_hex(0x00E5A3), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ui_MM_AmountBar[row], isFocused ? lv_color_hex(0x00AA55) : lv_color_hex(0x333333), LV_PART_MAIN);
}

// Cache to prevent LVGL redrawing every frame
static uint8_t cachedNav = 255;
static uint8_t cachedSrc[8]   = {255, 255, 255, 255, 255, 255, 255, 255};
static uint8_t cachedDest[8]  = {255, 255, 255, 255, 255, 255, 255, 255};
static int16_t cachedDepth[8] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};



void SCREEN_HOT(updateModMatrixView)(const Core1Snapshot &snap) {
  bool forceFullRedraw = false;

  // If opening the panel, force a full refresh
  if (lv_obj_has_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_remove_flag(ui_ModMatrixPanel, LV_OBJ_FLAG_HIDDEN);
    forceFullRedraw = true;
  }

  // 1. FAST SCROLLING: Only touch the 2 rows that changed focus!
  if (forceFullRedraw || snap.navMenuIndex != cachedNav) {
    if (!forceFullRedraw && cachedNav < 8) {
      set_row_focus_style(cachedNav, false);
    }

    if (snap.navMenuIndex < 8) {
      set_row_focus_style(snap.navMenuIndex, true);
    }

    cachedNav = snap.navMenuIndex;
  }

  // 2. DATA UPDATES: Only run when a knob actually modifies a value!
  char amtBuf[12];
  for (int i = 0; i < 8; i++) {
    uint8_t curSrc = currentModState.slots[i].src;
    uint8_t curDst = currentModState.slots[i].dest;
    int16_t curDep = currentModState.slots[i].depth;

    bool srcChanged = forceFullRedraw || (cachedSrc[i] != curSrc);
    bool dstChanged = forceFullRedraw || (cachedDest[i] != curDst);
    bool depChanged = forceFullRedraw || (cachedDepth[i] != curDep);

    if (forceFullRedraw) {
      set_row_focus_style(i, (i == snap.navMenuIndex));
    }

    // Source label update
    if (srcChanged) {
      cachedSrc[i] = curSrc;
      lv_label_set_text(ui_MM_Source[i], param_mod_source_name(curSrc));
    }

    // Destination label update
    if (dstChanged) {
      cachedDest[i] = curDst;
      lv_label_set_text(ui_MM_Dest[i], param_mod_dest_name(curDst));
    }

    // Amount number & bar update (Always visible and formatted)
    if (depChanged) {
      cachedDepth[i] = curDep;
      snprintf(amtBuf, sizeof(amtBuf), "%+d", curDep);
      lv_label_set_text(ui_MM_AmountVal[i], amtBuf);
      lv_bar_set_value(ui_MM_AmountBar[i], curDep, LV_ANIM_OFF);
    }
  }
}