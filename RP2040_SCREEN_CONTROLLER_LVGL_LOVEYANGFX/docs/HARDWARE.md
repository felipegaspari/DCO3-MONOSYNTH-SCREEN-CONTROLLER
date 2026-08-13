# Screen — hardware notes

Scoped to **`RP2040_SCREEN_CONTROLLER_LVGL_LOVEYANGFX`**.

---

## UART (authoritative: main `.ino`)

| Port | RX | TX | Baud | Peer |
|------|----|----|------|------|
| USB `Serial` | — | — | 1 000 000 | Debug |
| `Serial1` | 13 | 12 | 2 500 000 | Input — only peer link, receive-only (RX 13 from Input `Serial2` TX GP4; TX 12 unconnected) |

`Serial1`: IRQ (`setPollingMode(false)`), FIFO 512. Brought up in Core0 `setup()` then `init_screen_serial()`. TX pin 12 is configured in firmware but has no conductor on the board — the Input controller never reads from the Screen. Framing default RAW; `#define SERIAL_FRAMING_COBS` in `Serial.h` must match Input/DCO.

Live GPIOs: SPI 0/2/3, DC 18, RST 19, CS 22, UART RX 13 / TX 12. **GP23/24 unused** by the panel.

| `DCO_MCU_BOARD` | GP23 | GP24 |
|-----------------|------|------|
| WeAct RP2040 | Onboard KEY. Press returns UI to preset-scroll | Unused |
| Pico / Pico 2 | `SMPS_PS_PIN` OUT HIGH (RT6150 PWM) | VBUS sense — not driven |

---

## Display

| Item | Value |
|------|--------|
| Driver stack | **LovyanGFX** (`LGFX tft`) |
| Board config | Sketch `#include "LGFX_RP2040_FELA.hpp"` (keep it here — not under LovyanGFX) |
| LVGL resolution | 480 × 320 |
| Rotation | `tft.setRotation(3)` |
| Flush | Partial buffer (~1/10 screen); `my_disp_flush` → `pushPixels` |
| Touch | None — display-only. No LVGL input device is registered (the previous no-op touchpad indev/callback was removed). |

**Do not use `tft_setup.h` for pin truth** — it is a leftover TFT_eSPI / ILI9341 config and is **not included** by the live sketch.

---

## Unused / legacy in this folder

| Path | Status |
|------|--------|
| `fela_U8g2/` | Vendored U8g2 — **not referenced** by live sources |
| `src/felanew_U8g2/` | Second U8g2 copy — **unused** |
| `tft_setup.h` | TFT_eSPI leftovers |
| `tusb_config.h.legacy` | Old TinyUSB MIDI-only config (`CFG_TUD_CDC 0`). Renamed so it cannot shadow Pico SDK USB Serial. |

`ui.ino` (a fully commented-out SquareLine/TFT_eSPI template) has been removed from the sketch folder.

---

## External libraries (required to build)

| Library | Role |
|---------|------|
| `lvgl` | UI framework |
| `LovyanGFX` | Panel/SPI draw |
| SquareLine export (`ui.h`) | Screens/widgets (`ui_Main`, `ui_MANUALCALIBRATION`, bars, labels, …) |
