#include "menues.h"
#include <stdio.h>
#include <stdlib.h>
#include "_build_libs/DCO-PROTOCOL/param_meta.h"
#include "screen_target.h"

// -----------------------------------------------------------------------------
// FONT CONFIGURATION (Automatic fallback to larger fonts if available)
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

// -----------------------------------------------------------------------------
// 1. MENU DEFINITIONS
// -----------------------------------------------------------------------------
const MenuScreenDef screenMenus[] = {
    {"", 0, {}}, // Mode 0: NORMAL (Closed)
    {"ENVELOPE MENU", 1, {{"Legacy", ParamId::PARAM_SINE_STATUS}}}, // Mode 1
    
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
    {"ADSR 3 (PITCH/PWM) SETTINGS", 8, {
        {"Attack Curve",      ParamId::PARAM_ADSR3_ATTACK_CURVE},
        {"Decay Curve",       ParamId::PARAM_ADSR3_DECAY_CURVE},
        {"Release Curve",     ParamId::PARAM_ADSR3_RELEASE_CURVE},
        {"Env Enable",        ParamId::PARAM_ADSR3_ENABLED},
        {"Envelope Mode",     ParamId::PARAM_ADSR3_MODE},
        {"Oscillator Select", ParamId::PARAM_ADSR3_TO_OSC_SELECT},
        {"Mod Depth -> PWM",  ParamId::PARAM_ADSR3_TO_PWM},
        {"Mod Depth -> Pitch",ParamId::PARAM_ADSR3_TO_DETUNE1}
    }},
    
    {"SYSTEM SETTINGS", 1, {{"Placeholder", ParamId::PARAM_SINE_STATUS}}}, // Mode 5

    // Mode 6: DCO MENU
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

    // Mode 7: DCO MOD MENU
#if PROJECT_INSTRUMENT == 3
    {"DCO MODULATION", 6, {
        {"OSC 2 Detune",      ParamId::PARAM_OSC2_DETUNE_VAL},
        {"OSC 3 Detune",      ParamId::PARAM_OSC3_DETUNE_VAL},
        {"Unison Detune",     ParamId::PARAM_UNISON_DETUNE},
        {"Portamento Time",   ParamId::PARAM_PORTAMENTO_TIME},
        {"Portamento Mode",   ParamId::PARAM_PORTAMENTO_MODE},
        {"Subosc Divide",     ParamId::PARAM_SUBOSC_DIVIDE}
    }},
#else
    {"DCO MODULATION", 4, {
        {"OSC 2 Detune",      ParamId::PARAM_OSC2_DETUNE_VAL},
        {"Unison Detune",     ParamId::PARAM_UNISON_DETUNE},
        {"Portamento Time",   ParamId::PARAM_PORTAMENTO_TIME},
        {"Portamento Mode",   ParamId::PARAM_PORTAMENTO_MODE}
    }},
#endif

    // Mode 8: MOD MATRIX
    {"MODULATION MATRIX", 1, {
        {"Slot 0 Source",     ParamId::PARAM_MOD_SLOT0_SOURCE}
    }}
};

lv_obj_t* ui_FullMenuPanel = nullptr;
lv_obj_t* ui_FullMenuTitle = nullptr;
lv_obj_t* ui_MenuRows[10] = { nullptr };
lv_obj_t* ui_MenuRowLabels[10] = { nullptr };
lv_obj_t* ui_MenuRowValues[10] = { nullptr };

static uint8_t lastFocusedIndex = 255;
static uint8_t lastMenuMode = 255;

// -----------------------------------------------------------------------------
// 2. LIVE CACHE LOOKUP
// -----------------------------------------------------------------------------
static int32_t get_cached_param_value(ParamId id) {
    switch(id) {
        // ADSR 1
        case ParamId::PARAM_ADSR1_ATTACK_CURVE:  return currentMixState.adsr1_attack_curve;
        case ParamId::PARAM_ADSR1_DECAY_CURVE:   return currentMixState.adsr1_decay_curve;
        case ParamId::PARAM_ADSR1_RELEASE_CURVE: return currentMixState.adsr1_release_curve;
        case ParamId::PARAM_ADSR1_RESTART:    return (currentMixState.misc_flags & (1<<1)) ? 1 : 0;
        case ParamId::PARAM_ADSR1_MODE:          return currentMixState.adsr1_mode;
        
        // ADSR 2
        case ParamId::PARAM_ADSR2_ATTACK_CURVE:  return currentMixState.adsr2_attack_curve;
        case ParamId::PARAM_ADSR2_DECAY_CURVE:   return currentMixState.adsr2_decay_curve;
        case ParamId::PARAM_ADSR2_RELEASE_CURVE: return currentMixState.adsr2_release_curve;
        case ParamId::PARAM_ADSR2_RESTART:    return (currentMixState.misc_flags & (1<<2)) ? 1 : 0;
        case ParamId::PARAM_ADSR2_MODE:          return currentMixState.adsr2_mode;
        
        // ADSR 3
        case ParamId::PARAM_ADSR3_ATTACK_CURVE:  return currentMixState.adsr3_attack_curve;
        case ParamId::PARAM_ADSR3_DECAY_CURVE:   return currentMixState.adsr3_decay_curve;
        case ParamId::PARAM_ADSR3_RELEASE_CURVE: return currentMixState.adsr3_release_curve;
        case ParamId::PARAM_ADSR3_ENABLED:       return (currentMixState.misc_flags & (1<<3)) ? 1 : 0;
        case ParamId::PARAM_ADSR3_MODE:          return currentLfoState.adsr3_mode;
        case ParamId::PARAM_ADSR3_TO_OSC_SELECT: return currentLfoState.adsr3_to_osc_select;
        case ParamId::PARAM_ADSR3_TO_PWM:        return currentLfoState.adsr3_to_pwm;
        case ParamId::PARAM_ADSR3_TO_DETUNE1:    return currentLfoState.adsr3_to_detune1;

        // DCO Architecture & Mod
        case ParamId::PARAM_VOICE_MODE:          return currentOscState.voice_mode;
        case ParamId::PARAM_VOICE_ALLOC_MODE:    return currentOscState.voice_alloc_mode;
        case ParamId::PARAM_OSC1_INTERVAL:       return currentOscState.osc1_interval;
        case ParamId::PARAM_OSC2_INTERVAL:       return currentOscState.osc2_interval;
        case ParamId::PARAM_OSC3_INTERVAL:       return currentOscState.osc3_interval;
        case ParamId::PARAM_OSC_PHASE_SYNC:      return currentOscState.osc_phase_sync;
        case ParamId::PARAM_SYNC_MODE:           return currentOscState.sync_mode;
        case ParamId::PARAM_SOFT_SYNC:           return currentOscState.soft_sync;
        case ParamId::PARAM_OSC2_DETUNE_VAL:     return currentOscState.osc2_detune;
        case ParamId::PARAM_OSC3_DETUNE_VAL:     return currentOscState.osc3_detune;
        case ParamId::PARAM_UNISON_DETUNE:       return currentOscState.unison_detune;
        case ParamId::PARAM_PORTAMENTO_TIME:     return currentOscState.portamento_time;
        case ParamId::PARAM_PORTAMENTO_MODE:     return currentOscState.portamento_mode;
        case ParamId::PARAM_SUBOSC_DIVIDE:       return currentOscState.subosc_divide;

        default: return 0;
    }
}

// -----------------------------------------------------------------------------
// 3. PARAM_META STRING DELEGATION
// -----------------------------------------------------------------------------
static void format_menu_value(ParamId id, int32_t val, char* buf, size_t maxlen) {
    int32_t scaled = param_scale_display_value(id, val);

    switch(id) {
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

        // Dynamic OSC Routing & Dividers
        case ParamId::PARAM_ADSR3_TO_OSC_SELECT:
            snprintf(buf, maxlen, "%s", screen_adsr3_osc_select_label(screen_cal_topology(), scaled));
            break;
        case ParamId::PARAM_SUBOSC_DIVIDE:
            if(scaled == 0) snprintf(buf, maxlen, " OFF");
            else            snprintf(buf, maxlen, " /%ld", scaled);
            break;

        // Default Raw Numeric Display
        default:
            snprintf(buf, maxlen, " %ld", scaled);
            break;
    }
}

// -----------------------------------------------------------------------------
// 4. UI SETUP (Optimized for 8 slots in 480x320)
// -----------------------------------------------------------------------------
void setupMenues() {
  // 1. Full-Screen Backdrop (480x320)
  ui_FullMenuPanel = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(ui_FullMenuPanel);
  lv_obj_set_size(ui_FullMenuPanel, 480, 320);
  lv_obj_set_pos(ui_FullMenuPanel, 0, 0);
  lv_obj_set_style_bg_color(ui_FullMenuPanel, lv_color_hex(0x0C0C0C), 0);
  lv_obj_set_style_bg_opa(ui_FullMenuPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(ui_FullMenuPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);

  // 2. Header Title Bar (Larger Font, top aligned)
  ui_FullMenuTitle = lv_label_create(ui_FullMenuPanel);
  lv_obj_align(ui_FullMenuTitle, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_text_color(ui_FullMenuTitle, lv_color_hex(0xFF8800), 0);
  lv_obj_set_style_text_font(ui_FullMenuTitle, MENU_TITLE_FONT, 0);

  // 3. List Container (464x278, fills entire vertical area)
  lv_obj_t* listCont = lv_obj_create(ui_FullMenuPanel);
  lv_obj_remove_style_all(listCont);
  lv_obj_set_size(listCont, 464, 278);
  lv_obj_align(listCont, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_set_style_bg_opa(listCont, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(listCont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(listCont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(listCont, 3, 0); // Tight 3px gap
  lv_obj_clear_flag(listCont, LV_OBJ_FLAG_SCROLLABLE);

  // 4. Pre-create 10 Full-Width Menu Rows (456px wide x 31px high)
  for(int i = 0; i < 10; i++) {
    ui_MenuRows[i] = lv_obj_create(listCont);
    lv_obj_remove_style_all(ui_MenuRows[i]);
    lv_obj_set_size(ui_MenuRows[i], 456, 31);
    lv_obj_set_style_bg_color(ui_MenuRows[i], lv_color_hex(0x1B1B1B), 0);
    lv_obj_set_style_bg_opa(ui_MenuRows[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_MenuRows[i], 5, 0);
    lv_obj_clear_flag(ui_MenuRows[i], LV_OBJ_FLAG_SCROLLABLE);

    // Left Setting Name
    ui_MenuRowLabels[i] = lv_label_create(ui_MenuRows[i]);
    lv_obj_align(ui_MenuRowLabels[i], LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_text_color(ui_MenuRowLabels[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ui_MenuRowLabels[i], MENU_ITEM_FONT, 0);

    // Right Live Value
    ui_MenuRowValues[i] = lv_label_create(ui_MenuRows[i]);
    lv_obj_align(ui_MenuRowValues[i], LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_color(ui_MenuRowValues[i], lv_color_hex(0x00E5A3), 0);
    lv_obj_set_style_text_font(ui_MenuRowValues[i], MENU_ITEM_FONT, 0);
  }
}

// -----------------------------------------------------------------------------
// 5. MAIN RENDER LOOP
// -----------------------------------------------------------------------------
void SCREEN_HOT(updateGenericFocus)(const Core1Snapshot &snap) {
  if (snap.activeMenuMode == 0) {
    if (!lv_obj_has_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);
      lastMenuMode = 255;
    }
    return;
  }

  if (lv_obj_has_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_remove_flag(ui_FullMenuPanel, LV_OBJ_FLAG_HIDDEN);
  }

  static constexpr uint8_t TOTAL_MENUS = sizeof(screenMenus) / sizeof(screenMenus[0]);
  uint8_t modeIdx = (snap.activeMenuMode < TOTAL_MENUS) ? snap.activeMenuMode : 0;
  const MenuScreenDef& def = screenMenus[modeIdx];

  // Mode change: update titles and hide/show rows
  if (lastMenuMode != snap.activeMenuMode) {
    lastMenuMode = snap.activeMenuMode;
    lv_label_set_text(ui_FullMenuTitle, def.title);

    for (int i = 0; i < 10; i++) {
      if (i < def.count) {
        lv_label_set_text(ui_MenuRowLabels[i], def.items[i].label);
        lv_obj_remove_flag(ui_MenuRows[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ui_MenuRows[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    lastFocusedIndex = 255;
  }

  // Cursor focus update (Active row turns neon green with black text)
  if (lastFocusedIndex != snap.navMenuIndex) {
    if (lastFocusedIndex < 10) {
      lv_obj_set_style_bg_color(ui_MenuRows[lastFocusedIndex], lv_color_hex(0x1B1B1B), 0);
      lv_obj_set_style_text_color(ui_MenuRowLabels[lastFocusedIndex], lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_text_color(ui_MenuRowValues[lastFocusedIndex], lv_color_hex(0x00E5A3), 0);
    }
    if (snap.navMenuIndex < 10 && snap.navMenuIndex < def.count) {
      lv_obj_set_style_bg_color(ui_MenuRows[snap.navMenuIndex], lv_color_hex(0x00FF88), 0);
      lv_obj_set_style_text_color(ui_MenuRowLabels[snap.navMenuIndex], lv_color_hex(0x000000), 0);
      lv_obj_set_style_text_color(ui_MenuRowValues[snap.navMenuIndex], lv_color_hex(0x000000), 0);
      lv_obj_scroll_to_view(ui_MenuRows[snap.navMenuIndex], LV_ANIM_OFF);
    }
    lastFocusedIndex = snap.navMenuIndex;
  }

  // Live value update
  char valBuf[32];
  for (int i = 0; i < def.count; i++) {
    int32_t val = get_cached_param_value(def.items[i].id);
    format_menu_value(def.items[i].id, val, valBuf, sizeof(valBuf));
    lv_label_set_text(ui_MenuRowValues[i], valBuf);
  }
}