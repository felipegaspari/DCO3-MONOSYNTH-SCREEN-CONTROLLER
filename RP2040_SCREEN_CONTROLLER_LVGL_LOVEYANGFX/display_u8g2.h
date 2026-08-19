#ifndef __DISPLAY_U8G2_H__
#define __DISPLAY_U8G2_H__

#include <U8g2lib.h>
#include "displayParams.h"
#include "screen_mode.h"
#include "screen_target.h"
#include "Serial.h"

#define OLED_SCK_PIN   14
#define OLED_MOSI_PIN  15
#define OLED_CS_PIN    16
#define OLED_DC_PIN    17

struct LocalGuiState {
  int32_t cutoff = 2048;
  int32_t resonance = 0;
  int32_t env2vcf = 0;
};
extern LocalGuiState guiState;

class U8G2_SSD1309_PICO_SPI1 : public U8G2 {
public:
  U8G2_SSD1309_PICO_SPI1(const u8g2_cb_t *rotation = U8G2_R0);
};

extern U8G2_SSD1309_PICO_SPI1 u8g2;

void init_u8g2();
void update_u8g2_core0();
void mark_u8g2_dirty();

// Dedicated Core 0 OLED Inspector Trigger (Immune to Core 1 resets)
void trigger_oled_inspector(InspectorType type);

#endif // __DISPLAY_U8G2_H__