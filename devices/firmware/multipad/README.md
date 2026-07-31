# ILX MultiPad USB CDC adapter

This directory is the public, source-first integration layer for the open-source
[ILX MultiPad](https://github.com/iLx11/multi-pad). It is intentionally **not**
a copy of the vendor repository and it is not Nexting production firmware.

## What is verified before opening the enclosure

The upstream application is an STM32F103VET6 composite USB device:

- USB HID (`STM32 HID CUSTOM`) for the normal keyboard surface;
- USB CDC ACM (`STM32 CDC ACM0`) for the configuration channel;
- 8 matrix keys (2 rows × 4 columns) and 3 rotary encoders in the upstream
  source;
- no DFU, IAP, or application-side bootloader path in the upstream source;
- upstream reference commit `78c1ee533a7f513e9f390741c4f5eed1e0aa91b3`;
- upstream license: GPL-3.0.

The purchased unit still needs one physical check before a write: whether it is
the **module PCB** (USB serial switch plus BOOT/RESET buttons) or the **FPC
PCB** (SWD/J-Link is required). Do not assume the external Type-C connector is
a bootloader. The included `nexting-multipad-device-info.template.json` keeps
unknown display, serial, and battery fields absent until that check is done.

## What this adapter does

`nexting_multipad_adapter.c` reuses the public portable C99 SDK. It adds no
private App or Agent code. The board port supplies five things:

1. a monotonic millisecond clock;
2. a USB CDC write function;
3. approval rendering (the two small displays, LEDs, or keys);
4. status rendering (up to the number of displays/indicators actually wired);
5. a call from the CDC receive callback.

The adapter accepts newline-terminated Nexting JSON frames and leaves the
upstream `AA BB xx` legacy commands untouched. It owns stream framing, approval
expiry, answer retry, choice locking, resolved cleanup, status replacement, and
disconnect cleanup by delegating to `sdk/c`. It never receives a credential or
an Agent session identifier.

Build the host-side contract test without an STM32 toolchain:

```sh
cmake -S firmware/multipad -B firmware/multipad/.build
cmake --build firmware/multipad/.build --parallel
ctest --test-dir firmware/multipad/.build --output-on-failure
```

## Integrating with the upstream firmware

Do not replace `USER/Usb/usb_user.c` wholesale. Add the adapter to the upstream
project and route only frames for which
`nexting_multipad_accepts(&adapter, Buf, *Len)` is true. Keep the existing
`AA BB CC`, `AA BB AA`, `AA BB DD`, `AA BB EE`, and `AA BB FF` branches for the
vendor configuration UI. A minimal receive hook is:

```c
if (nexting_multipad_accepts(&nexting_adapter, Buf, *Len)) {
    (void)nexting_multipad_receive(&nexting_adapter, Buf, *Len);
    return USBD_OK;
}
```

Call `nexting_multipad_choose(&nexting_adapter, NEXTING_DEVICE_CHOICE_ALLOW)`
or `..._DENY` from the two approval keys, call
`nexting_multipad_tick()` from the existing millisecond loop, and call
`nexting_multipad_disconnect()` from the USB disconnect callback. The board
callbacks should render only the state they receive; they must not implement a
second JSON parser.

The adapter is a protocol binding, not App enrollment. The public Nexting App
BLE path does not claim USB CDC enrollment. A Host that owns the USB permission
and authorization policy can use this binding over a local CDC port; a future
App USB transport must be released separately with its own Host tests.

## Flash preparation (do not write yet)

The checked-in upstream artifact is `leden.hex` from the commit above. Before
writing anything:

1. open the back and photograph the PCB, MCU marking, and BOOT/RESET/SERIAL
   switch labels;
2. save the original firmware with the vendor's supported tool;
3. confirm that the unit has the module serial path, or attach an ST-Link/J-Link
   to the SWD pads;
4. run the dry checks in `tools/flash-multipad.sh --help` and the CDC smoke
   check in `tools/multipad-cdc-smoke.py`;
5. only then use `--confirm --allow-write` with an explicit artifact path.

There is no recovery promise over ordinary USB HID/CDC. If the board variant
does not expose the serial bootloader, stop and use SWD. A failed application
flash can otherwise leave the keyboard working as HID but unable to accept the
next write.

## License and boundary

The adapter and tests in this directory follow the Nexting Devices repository
license. The upstream MultiPad application remains GPL-3.0 and is not vendored
here. This directory contains no Nexting App, cloud, Agent bridge, production
firmware, signing key, or manufacturing file.
