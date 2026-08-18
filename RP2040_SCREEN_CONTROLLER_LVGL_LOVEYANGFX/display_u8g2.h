#ifndef __DISPLAY_U8G2_H__
#define __DISPLAY_U8G2_H__

#include <U8g2lib.h>
#include "displayParams.h"
#include "screen_mode.h"
#include "screen_target.h"
#include "Serial.h"

// Hardware Pin assignments on SPI1 for Raspberry Pi Pico
#define OLED_SCK_PIN   14
#define OLED_MOSI_PIN  15
#define OLED_CS_PIN    16
#define OLED_DC_PIN    17
#define OLED_RST_PIN   U8X8_PIN_NONE

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;

void init_u8g2();
void update_u8g2_core0();
void mark_u8g2_dirty();

#endif // __DISPLAY_U8G2_H__