#ifndef __DISPLAY_PARAMS_H__
#define __DISPLAY_PARAMS_H__

#include "params_def.h"
#include "screen_mode.h"
#include "screen_target.h"
#include <lvgl.h>

// Bits of levelBarFlag: pending level-bar widget updates.
static constexpr uint8_t LEVEL_BAR_OSC1 = 0x01;
static constexpr uint8_t LEVEL_BAR_OSC2 = 0x02;
static constexpr uint8_t LEVEL_BAR_SUB = 0x04;

// Contextual Inspector Categories
enum class InspectorType : uint8_t {
  None = 0,
  ADSR1,
  ADSR2,
  ADSR3,
  Filter,
  Oscillators,
  LFO1,
  LFO2,
  LFO3,
  VoiceEngine
};

// Single Atomic Snapshot Structure for Lock-Free Core 1 Rendering
struct Core1Snapshot {
  bool hasSignal;
  uint8_t signalMode;

  bool hasPresetScroll;
  uint8_t presetNum;
  char presetName[17];
  bool hasPresetChar;
  uint8_t presetChar;

  bool hasParamChange;
  const char* paramName;
  int32_t paramValue;
  uint8_t paramNumber;

  // Inspector Shell State
  InspectorType inspectorType;
  bool hasInspectorChange;
  uint32_t inspectorActivityTime;

  uint8_t levelBars;
  uint8_t osc1Level, osc2Level, subLevel;

  bool hasADSR1, hasADSR2;
  uint16_t a1a, a1d, a1s, a1r;
  uint16_t a2a, a2d, a2s, a2r;

  uint8_t calMenuIndex;
  int8_t calOffset;
  uint16_t calAmp440;
  uint16_t calPwCenter;
  uint8_t calOscN;
  uint8_t calStage;
  int32_t calGap;
  CalTopology calTopology;
};

// Inspector Shared State
extern uint16_t inspectorTimeoutMillis;
extern volatile InspectorType activeInspector;
extern volatile uint32_t inspectorLastActivityMillis;
extern volatile bool inspectorActiveFlag;

// Existing Externs
extern uint16_t paramHideTimeMillis;
extern bool paramChangeTimerFlag;
extern uint32_t paramChangeLastMillis;

extern volatile uint8_t calibrationMenuIndex;
extern volatile int8_t offset;
extern volatile uint16_t ampComp440Display;
extern volatile uint16_t calPwCenterDisplay;
extern volatile uint8_t manualCalibrationOSCN;
extern volatile uint8_t manualCalibrationStage;
extern volatile int32_t calibrationGap;

extern volatile uint8_t OSC1Level;
extern volatile uint8_t OSC2Level;
extern volatile uint8_t OSC3Level;
extern volatile uint8_t SUBLevel;

extern volatile uint16_t ADSR1Attack;
extern volatile uint16_t ADSR1Decay;
extern volatile uint16_t ADSR1Sustain;
extern volatile uint16_t ADSR1Release;

extern volatile uint16_t ADSR2Attack;
extern volatile uint16_t ADSR2Decay;
extern volatile uint16_t ADSR2Sustain;
extern volatile uint16_t ADSR2Release;

extern lv_obj_t* ui_calGapTrack;

// Lock-Free Drawing Shells
void draw_param_1(const char* name, int32_t value);
void draw_preset_scroll_1(ScreenMode mode, uint8_t num, const char* name, uint8_t charPos);
void drawManualCalibration(const Core1Snapshot& snap);
void setDisplayParam();
void applyNavParam(uint8_t id, int32_t value);
InspectorType get_param_inspector_type(ParamId id);

extern lv_obj_t* ui_calGapTrack;

#endif  // __DISPLAY_PARAMS_H__
