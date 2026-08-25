#ifndef __UI_VIEWS_H__
#define __UI_VIEWS_H__

#include <lvgl.h>
#include "screen_mode.h"
#include "displayParams.h"
#include "sram_hot.h"

extern ScreenMode currentMode;

void SCREEN_HOT(handleScreenModeChange)(const Core1Snapshot &snap);
void SCREEN_HOT(expireSilentMode)(const Core1Snapshot &snap);
void SCREEN_HOT(updateBottomMessageAndPresetUI)(ScreenMode mode, const Core1Snapshot &snap);
void SCREEN_HOT(updateLevelBars)(ScreenMode mode, const Core1Snapshot &snap);
void SCREEN_HOT(updateADSRBars)(const Core1Snapshot &snap);
void SCREEN_HOT(updateCalibrationUI)(ScreenMode mode, const Core1Snapshot &snap);
void SCREEN_HOT(updateInspectorLifecycle)(ScreenMode mode, const Core1Snapshot &snap);

#endif // __UI_VIEWS_H__