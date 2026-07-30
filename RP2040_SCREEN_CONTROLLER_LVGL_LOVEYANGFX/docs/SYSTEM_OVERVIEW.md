# System overview (Screen Controller pointer)

This sketch folder is the **live DCO4 screen firmware**: RP2040 dual-core **LVGL** UI on a **LovyanGFX** panel, driven by UART from the **Input** controller — the only peer. The chain is DCO ↔ Input → Screen; the Screen has no direct link to the DCO, so DCO-sourced values (calibration gap `'x'` 154) reach it relayed by Input.

Canonical three-board overview:

**[`../../../DCO/docs/SYSTEM_OVERVIEW.md`](../../../DCO/docs/SYSTEM_OVERVIEW.md)**

Do not fork a second full system document here.

### This sketch’s UARTs (verified in firmware)

| Port | Pins (RX / TX) | Baud | Peer |
|------|----------------|------|------|
| `Serial` | USB | 1 000 000 | Debug |
| `Serial1` | GP13 / GP12 | 2 500 000 | Input controller — receive-only: RX GP13 from Input `Serial2` TX (GP4); TX GP12 is unconnected |

Display SPI/panel pins live in the external board header `lgfx_user/LGFX_RP2040_FELA.hpp` (not in this folder). LVGL logical size here: **480×320**.

Detail: [`HARDWARE.md`](HARDWARE.md), [`UI_AND_SERIAL.md`](UI_AND_SERIAL.md), [`REFERENCE_AI.md`](REFERENCE_AI.md).
