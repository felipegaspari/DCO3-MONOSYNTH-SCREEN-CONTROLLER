#ifndef __DISPLAY_U8G2_H__
#define __DISPLAY_U8G2_H__

#include <U8g2lib.h>
#include "displayParams.h"
#include "screen_mode.h"
#include "screen_target.h"
#include "Serial.h"

// Hardware Pin assignments strictly on SPI1 for RP2040
#define OLED_SCK_PIN   14
#define OLED_MOSI_PIN  15
#define OLED_CS_PIN    16
#define OLED_DC_PIN    17

class U8G2_SSD1309_PICO_SPI1 : public U8G2 {
public:
  U8G2_SSD1309_PICO_SPI1(const u8g2_cb_t *rotation = U8G2_R0);
};

extern U8G2_SSD1309_PICO_SPI1 u8g2;

void init_u8g2();
void update_u8g2_core0();
void mark_u8g2_dirty();

#endif // __DISPLAY_U8G2_H__