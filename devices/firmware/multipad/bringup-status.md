# MultiPad bring-up status

Last updated: 2026-07-31

This is the pre-flash record for the purchased ILX MultiPad. It deliberately
separates source/build evidence from physical-board evidence. No firmware write
has been performed.

## Current state

| Phase | Step                            | Status                          | Evidence                                                                               |
| ----- | ------------------------------- | ------------------------------- | -------------------------------------------------------------------------------------- |
| 0     | Upstream source audit           | ✅ complete                     | STM32F103VET6, USB HID + CDC, commit `78c1ee533a7f513e9f390741c4f5eed1e0aa91b3`        |
| 0     | Portable adapter build          | ✅ complete                     | `npm run test:multipad`, CMake + ctest pass                                            |
| 0     | Host CDC application path       | ✅ complete                     | Connected `MultiPad_Device` CDC endpoint echoed `AA BB CC` byte-for-byte on 2026-07-31 |
| 0     | PCB variant identification      | ⏳ blocked on enclosure opening | Module vs FPC is not visible from the outside                                          |
| 0     | Original flash backup           | ⏳ not started                  | Must identify the boot path first                                                      |
| 1     | Serial bootloader write         | ⏳ not started                  | Only possible if the module serial path is present                                     |
| 1     | SWD recovery/write              | ⏳ not started                  | Required fallback for FPC or failed serial path                                        |
| 2     | Nexting present/answer/resolved | ⏳ not started                  | Requires adapter firmware on the exact board                                           |
| 2     | Status rendering                | ⏳ not started                  | Display/indicator wiring must be photographed and mapped                               |

## Bring-up order after opening

1. Photograph the MCU, PCB revision, screens, SERIAL switch, BOOT and RESET.
2. Update [`pin-function-map.md`](pin-function-map.md) from the photograph and
   the upstream source; record unknown pins rather than guessing.
3. Back up the original flash and record the tool output/hash.
4. Verify the USB CDC echo only; do not combine this with a write.
5. Build and write the adapter image through the confirmed path.
6. Verify one `present → answer → resolved` exchange, then expiry and disconnect
   cleanup, one scenario at a time.
7. Update this file and the pin map immediately after each observation.

## Explicit blockers

- A normal HID/CDC USB connection is not proof of a bootloader.
- The public Nexting App BLE enrollment path does not imply USB enrollment.
- Battery, exact display dimensions, serial number, and SWD pad locations are
  unknown until the enclosure is opened.
