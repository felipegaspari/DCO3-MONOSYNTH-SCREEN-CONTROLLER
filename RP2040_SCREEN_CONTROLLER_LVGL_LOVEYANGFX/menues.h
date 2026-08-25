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
extern lv_obj_t* ui_MenuRows[10];
extern lv_obj_t* ui_MenuRowLabels[10];
extern lv_obj_t* ui_MenuRowValues[10]; // NEW: Right-side inline values

void setupMenues();
void SCREEN_HOT(updateGenericFocus)(const Core1Snapshot &snap);


#endif // __MENUES_H__