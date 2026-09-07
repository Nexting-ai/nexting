# AhaKey production-firmware port

This directory is the narrow interface the AhaKey firmware team implements.
It does not require the Nexting App to understand AhaKey commands and it does
not put Agent credentials or routing into the keyboard.

## What is already implemented

`nexting_ahakey_port.c` owns the compatibility tunnel, Device Info handshake,
physical-key audio lifecycle, IMA ADPCM encoding, fragmentation, eight-credit
flow control, a ten-frame RAM queue, 500 ms stall cancellation, the two-minute
limit, and disconnect/error wiping. The portable implementation and hostile
edges run in host CTest.

The tunnel coexists with the stock `0x7340` service:

- writes beginning `A0` or `A3` on `0x7343` are Nexting control/flow chunks;
- notifications beginning `A1` or `A2` on `0x7344` are Nexting control/audio;
- existing `AA BB ... CC DD` traffic is forwarded unchanged to the stock
  parser.

This direct one-byte prefix is a firmware extension. Do not wrap Nexting
payloads in the stock `AA BB ... CC DD` configuration frame.

## Board hooks to wire

Fill `nexting_ahakey_port_ops_t` with the existing firmware primitives:

1. `notify_7344` — enqueue one notification and return false if the controller
   cannot accept it.
2. `vendor_7343_write` — call the existing stock command parser.
3. `microphone_start` — configure the device microphone for signed PCM16,
   16 kHz, mono, and exact 320-sample callbacks. It must not expose pre-roll.
4. `microphone_stop` — stop cleanly when the user submits.
5. `microphone_cancel` — stop immediately and wipe DMA/ring buffers.
6. `render_audio_state` — visibly render ready, receiving, transcribing,
   submitted, and error.

Call:

```c
nexting_ahakey_port_on_7343_write(&port, value, length, bonded, encrypted);
nexting_ahakey_port_publish_device_info(&port, bonded, encrypted);
nexting_ahakey_port_voice_pressed(&port);
nexting_ahakey_port_voice_released(&port);
nexting_ahakey_port_on_pcm(&port, pcm_320);
nexting_ahakey_port_tick(&port);
```

The BLE connection must negotiate an ATT payload of at least 64 bytes.
For the supplied descriptor use MTU 247 / ATT payload 244 when the controller
supports it, and initialize the port with `att_payload_bytes = 244`.

## Required production substitutions

Generate Device Info from
`../nexting-ahakey-device-info.template.json`. Replace both the firmware
version and the per-device UUID, then serialize it as one compact JSON line
without a trailing newline (the port adds the JSONL terminator). The template
placeholders intentionally fail Host validation. Include `device-audio/1` only
after the real microphone hook passes the physical checklist.

The public AhaKey firmware repository at commit
`7e950321f8a4c2f860a3f4d67de85ef32bf42d7c` publishes release HEX files and
CH582 flashing instructions, but not the buildable v1.1.0 production source or
a public PCM callback. Therefore this repository can provide and test the
complete port boundary, but cannot name the final CH582 ADC/I2S/PDM symbol or
claim board verification until AhaKey supplies that private source/hook and a
device is measured.
