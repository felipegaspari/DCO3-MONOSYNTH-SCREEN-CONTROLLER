#ifndef __MENUES_H__
#define __MENUES_H__

#include <lvgl.h>
#include "displayParams.h"
#include "sram_hot.h"
#include "_build_libs/DCO-PROTOCOL/params_def.h"

// Pair a text label with the actual parameter it controls
struct MenuItemDef {
    const char* label;
    ParamId id;
};

struct MenuScreenDef {
    const char* title;
    uint8_t count;
    MenuItemDef items[10];
};

extern const MenuScreenDef screenMenus[];

extern lv_obj_t* ui_FullMenuPanel;
extern lv_obj_t* ui_FullMenuTitle;
extern lv_obj_t* ui_MenuRows[16];
extern lv_obj_t* ui_MenuRowLabels[16];
extern lv_obj_t* ui_MenuRowValues[16];
extern lv_obj_t* ui_ModMatrixPanel;

void setupMenues();
void setupModMatrix();

void SCREEN_HOT(updateGenericFocus)(const Core1Snapshot &snap);
void SCREEN_HOT(updateModMatrixView)(const Core1Snapshot &snap);

// Exported shared UI state formatters (Used by both TFT and OLED)
int32_t get_cached_param_value(ParamId id, const PatchOscBlock& osc, const PatchMixBlock& mix, const PatchLfoBlock& lfo);
void format_menu_value(ParamId id, int32_t val, char* buf, size_t maxlen);
int32_t get_cached_param_value(ParamId id, const PatchOscBlock& osc, const PatchMixBlock& mix, const PatchLfoBlock& lfo);
void format_menu_value(ParamId id, int32_t val, char* buf, size_t maxlen);

#endif // __MENUES_H__