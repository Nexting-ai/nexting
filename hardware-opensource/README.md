# Nexting PIN · 3D Print Edition

This directory is the reproducible, open-hardware path for the Nexting PIN
3D Print Edition. The enclosure follows the open Omi hardware lineage, but
the geometry, button feel, clip behavior, and Nexting firmware are our own.
It is not official Omi hardware and it does not use Omi firmware.

## Use the current file set

Start with these files. The older design folders are historical iterations and
are kept for comparison; they are not the default build.

| What you need | Path | Notes |
| --- | --- | --- |
| Enclosure | `enclosure/current-version/base.stl`<br>`cover.stl`<br>`button.stl` | The three-piece enclosure set used for the current build. |
| Firmware | `firmware/pinclaw_zephyr/pinclaw_v2.2.4.uf2` | For the matching Seeed XIAO nRF52840 Sense path only. |
| Schematic | `docs/SCH_pinclaw_v1.0_diy.pdf` | Wiring, power, and pin reference. |
| Firmware notes | `firmware/pinclaw_zephyr/README.md` | Build and board-specific notes. |

## Build the product path

1. Print `base.stl`, `cover.stl`, and `button.stl` from
   [`enclosure/current-version/`](enclosure/current-version/).
2. Assemble the XIAO nRF52840 Sense, audio amplifier, speaker, battery,
   button, and LED according to the schematic. Do not wire from a product
   photo alone.
3. Double-tap the XIAO Reset button. When the expected UF2 volume appears,
   drag `pinclaw_v2.2.4.uf2` onto it. If no volume appears, stop and identify
   the board before flashing.
4. Reconnect power, verify recording and playback, then pair through the
   current Nexting App onboarding.

The shipping path is a voice product:

```text
PTT button ↔ XIAO firmware ↔ BLE audio ↔ Nexting App ↔ cloud Agent
```

This is separate from the Nexting Devices SDK. BLE audio is not proof that a
device implements `voice/1`, `status/1`, or another SDK Profile.

## SDK adapter boundary

To make this hardware a public Nexting Devices SDK device, add and verify a
small adapter that translates physical state into the public contract:

| Hardware | SDK target | First proof |
| --- | --- | --- |
| Hold PTT | `voice/1` | Hold, release, duplicate, expiry, and disconnect vectors. |
| Status LED | `status/1` | One bounded state round-trip and disconnect clearing. |
| Battery report | Device Info / battery service | Confirm the real BLE advertisement. |
| No display | Do not declare `text/1` | Keep text in the trusted Host or App. |

The device never receives Agent credentials. The trusted Nexting Host owns
authorization and maps the declared physical intent to an Agent session over
encrypted BLE GATT.

## Public source and lineage

- [Nexting Devices SDK](../devices/)
- [Current enclosure files](enclosure/current-version/)
- [Firmware directory](firmware/pinclaw_zephyr/)
- [Schematic](docs/SCH_pinclaw_v1.0_diy.pdf)
- [Omi open-source project](https://github.com/BasedHardware/omi)

Keep this README, the current file set, and the Nexting developer case study
aligned. Do not copy private App or cloud implementation details into this
directory.

## License

MIT. Build it, modify it, and share your improvements. Check the license of
any third-party component or upstream Omi material separately.
