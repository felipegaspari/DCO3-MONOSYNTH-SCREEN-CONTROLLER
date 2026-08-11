#ifndef __DISPLAY_PARAMS_H__
#define __DISPLAY_PARAMS_H__

#include "params_def.h"
#include "screen_mode.h"
#include "screen_target.h"

// Bits of levelBarFlag: pending level-bar widget updates.
static constexpr uint8_t LEVEL_BAR_OSC1 = 0x01;
static constexpr uint8_t LEVEL_BAR_OSC2 = 0x02;
static constexpr uint8_t LEVEL_BAR_SUB  = 0x04;

// Definitions live in displayParams.ino (extern pattern like Serial.h/Serial.ino).
extern uint16_t paramHideTimeMillis;
extern bool     paramChangeTimerFlag;

extern volatile int8_t   offset;
extern volatile uint8_t  manualCalibrationOSCN;
extern volatile uint8_t  manualCalibrationStage;
extern volatile int32_t  calibrationGap;

extern volatile uint8_t  OSC1Level;
extern volatile uint8_t  OSC2Level;
extern volatile uint8_t  OSC3Level;
extern volatile uint8_t  SUBLevel;

extern volatile uint16_t ADSR1Attack;
extern volatile uint16_t ADSR1Decay;
extern volatile uint16_t ADSR1Sustain;
extern volatile uint16_t ADSR1Release;

extern volatile uint16_t ADSR2Attack;
extern volatile uint16_t ADSR2Decay;
extern volatile uint16_t ADSR2Sustain;
extern volatile uint16_t ADSR2Release;

// displayParams.ino
void draw_param_1();
void draw_preset_scroll_1(ScreenMode mode);
void drawManualCalibration();
void setDisplayParam();
void applyNavParam(uint8_t id, int32_t value);

#endif
