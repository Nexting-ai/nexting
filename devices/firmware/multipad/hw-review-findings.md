# MultiPad hardware review findings

Last updated: 2026-07-31

## Findings

| ID    | Finding                                                                                                                      | Impact                                                        | Next action                                        |
| ----- | ---------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- | -------------------------------------------------- |
| MP-01 | The outside photo does not identify Module PCB vs FPC PCB.                                                                   | Type-C serial flashing cannot be selected safely.             | Open the enclosure and photograph the PCB labels.  |
| MP-02 | Upstream source contains USB HID + CDC application code but no DFU/IAP path.                                                 | Ordinary CDC enumeration cannot be used as recovery evidence. | Use the module serial boot sequence or SWD/J-Link. |
| MP-03 | Upstream source exposes matrix and encoder modules, but the purchased unit's exact display/connector revision is unverified. | Device Info must not claim battery/display details yet.       | Map traces and connectors after opening.           |
| MP-04 | The live device echoed `AA BB CC` over CDC before any write.                                                                 | Confirms the application CDC endpoint, not a bootloader.      | Keep this as the baseline regression check.        |

No electrical anomaly has been observed because the enclosure has not been
opened. Any mismatch with the source or schematic must be added here before
continuing.
