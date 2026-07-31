# MultiPad pin-function map

Last updated: 2026-07-31

This map records only literal evidence from the upstream STM32 source. A blank
or `pending` cell is intentional; do not fill it from memory or from a similar
board. The Nexting adapter itself is pin-agnostic and does not add a pin claim.

## Pin mappings visible in upstream source

| Pin | MCU function | Hardware connection | Function | Driver/source | Verification | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PB10 | GPIO output | matrix row 0 | key matrix row | upstream `KEY` module | ⏳ physical pending | source-level only |
| PB11 | GPIO output | matrix row 1 | key matrix row | upstream `KEY` module | ⏳ physical pending | source-level only |
| PE12 | GPIO input | matrix column 0 | key matrix column | upstream `KEY` module | ⏳ physical pending | source-level only |
| PE13 | GPIO input | matrix column 1 | key matrix column | upstream `KEY` module | ⏳ physical pending | source-level only |
| PE14 | GPIO input | matrix column 2 | key matrix column | upstream `KEY` module | ⏳ physical pending | source-level only |
| PE15 | GPIO input | matrix column 3 | key matrix column | upstream `KEY` module | ⏳ physical pending | source-level only |
| PA9 | USART1 TX | serial module path | bootloader/UART TX | upstream `USART1` init | ⏳ physical pending | only module PCB exposes this path |
| PA10 | USART1 RX | serial module path | bootloader/UART RX | upstream `USART1` init | ⏳ physical pending | only module PCB exposes this path |
| SWDIO | debug pad | MCU SWD header/pads | recovery/program data | ST-Link/J-Link | ⏳ physical pending | pad location unknown |
| SWCLK | debug pad | MCU SWD header/pads | recovery/program clock | ST-Link/J-Link | ⏳ physical pending | pad location unknown |

## Functions needing an enclosure photograph

| Function | Expected source evidence | Physical evidence required | Status |
| --- | --- | --- | --- |
| 8 key matrix | 2 rows × 4 columns | trace/connector and switch orientation | ⏳ |
| 3 rotary encoders | `encoder1`, `encoder2`, `encoder3` modules | exact A/B/SW pins and pull-ups | ⏳ |
| Displays | upstream OLED/LCD modules | module/FPC variant, bus pins, dimensions | ⏳ |
| USB HID + CDC | STM32 USB device stack | connector wiring and stable enumeration | ✅ host CDC echo only |
| BOOT/RESET | module PCB documentation | buttons and switch labels | ⏳ |
| Battery | no battery service found in upstream source | battery/charger IC and ADC trace | ⏳ / do not declare |

## Reverse index

| Goal | Pins/peripheral | Preconditions | Status |
| --- | --- | --- | --- |
| Preserve keyboard HID | USB device stack | device still enumerates | ✅ before flash |
| Read original flash | PA9/PA10 serial or SWD | confirmed PCB path | ⏳ |
| Nexting CDC frames | USB CDC RX/TX | adapter image + CDC callback hook | ⏳ |
| Approval keys | key matrix + encoder/button map | exact input wiring | ⏳ |
| Status rendering | displays/LEDs | exact display map | ⏳ |
