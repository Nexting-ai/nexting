# AhaKey X1 integration kit

[简体中文](#简体中文)

Portable glue that turns the AhaKey X1 vibecoding keyboard into a Nexting
device. The kit speaks both sides of the conversation:

- `ahakey_proto.h/.c` — a C99 codec for the published AhaKey X1 BLE
  configuration protocol (GATT service `0x7340`, frame layout
  `AA BB cmd data CC DD`), pinned to AhakeyAI/desktop commit
  `c16630329cd0a53fcbcdd4d528015257a19449f6` (`docs/ble-protocol.md`).
- `nexting_ahakey_adapter.h/.c` — the mapping between the portable Nexting
  C99 state machine (`sdk/c`) and the X1 hardware concepts: approval keys,
  the physical lever, the LED strip, and the OLED.
- `nexting_ahakey_audio.h/.c` — physical-only microphone capture, IMA ADPCM,
  bounded RAM, credits, fragmentation, timeout, and wipe-on-failure.
- `developer-kit/nexting_ahakey_port.h/.c` — the production CH582-facing hook
  boundary over the existing `0x7343`/`0x7344` characteristics.
- `transport-descriptor.nexting-device.json` — the data-only App descriptor;
  all AhaKey UUIDs and opcodes stay here and in firmware, outside App core.
- `nexting-ahakey-device-info.template.json` — the Device Info declaration
  the integrated device publishes to the Host.

The case document [docs/cases/ahakey-x1-case.md](../../docs/cases/ahakey-x1-case.md)
walks through both integration modes end to end.

## Capability mapping

| X1 hardware                     | Nexting interface        | Mapping                                             |
| ------------------------------- | ------------------------ | --------------------------------------------------- |
| Key configured as F18           | `approval/1` → Allow     | Answers the pending request                         |
| Key configured as F19           | `approval/1` → Deny      | Answers the pending request                         |
| Keys configured as F13–F16      | `keys/1` slots 0–3       | `key_event` press with a rising sequence            |
| Physical lever                  | `keys/1` slot 7          | Press when it leaves rest, release when it returns  |
| LED strip                       | `status/1`               | Agent state → AhaKey IDE state (`0x90`)             |
| OLED 160×80                     | `text/1`                 | Title + content for the display channel             |
| Battery service `0x180F`        | Device Info              | `battery_service: true`                             |
| On-device microphone            | `device-audio/1`         | PCM16 → IMA ADPCM → encrypted BLE → Nexting App    |
| Approval pending (any source)   | LED override             | Forces the permission-request state while pending   |

A pending approval overrides the strip with the waiting-for-approval light.
Otherwise the strip renders the lowest-numbered occupied status slot and
ignores the rest, per the status/1 rule: an active agent shows the
tool-running state, needs-input shows waiting-for-approval, a completed
agent shows task-completed, an error shows the notification state, and
nothing occupied turns the strip off. Keys and the lever emit events only
for slots the Host keymap has declared and enabled.

## Integration modes

- **Preferred — Compatibility Tunnel firmware:** link the developer kit into
  X1 firmware. It preserves the original `0x7340` service and adds four fixed
  data opcodes. The App loads the generic descriptor; no AhaKey parser enters
  the App.
- **Alternative — native Nexting GATT:** expose the standard Nexting service
  and audio characteristic directly. The same audio state machine is reused.
- **Stock-firmware bridge:** remains useful for keys, lever, LED, and OLED,
  but cannot claim device audio unless stock firmware exposes the keyboard's
  real PCM stream to the bridge.

All modes use the same adapter and the same Device Info identity; the
end-to-end wiring, callbacks, and verification steps are in
[docs/cases/ahakey-x1-case.md](../../docs/cases/ahakey-x1-case.md). The
adapter never sees Agent credentials, accounts, or cloud addresses;
authorization and the final approval action stay with the Host.

## Build and test

```sh
npm run test:ahakey
```

This builds the codec and adapter with `-Wall -Wextra -Wpedantic -Werror`
and runs the host-side CTest suites, including the published wire captures
from the AhaKey protocol document.

## Evidence wording

The codec, audio state machine, tunnel, and adapter are **Core tested** (host
CTest). That label says
nothing about radio behavior; real-keyboard and real-phone verification
follows the checklist in the case document before any stronger claim. See
[docs/conformance.md](../../docs/conformance.md) for the allowed wording.

## 简体中文

把 AhaKey X1 vibecoding 键盘接入 Nexting 的移植套件。套件同时讲两种语言：

- `ahakey_proto.h/.c` — AhaKey X1 公开 BLE 配置协议（GATT 服务 `0x7340`，
  帧格式 `AA BB cmd data CC DD`）的 C99 编解码，协议文档 pin 在
  AhakeyAI/desktop 的 `c16630329cd0a53fcbcdd4d528015257a19449f6`。
- `nexting_ahakey_adapter.h/.c` — Nexting C99 协议状态机（`sdk/c`）与
  X1 硬件概念之间的映射：审批按键、物理拨杆、LED 灯条和 OLED。
- `nexting_ahakey_audio.h/.c` 与 `developer-kit/` — 把设备本身麦克风的
  PCM16 编码成 IMA ADPCM，经加密 BLE 上传；包含固定内存、信用流控、
  分片、超时、断连擦除，以及 CH582 生产固件需要实现的窄接口。
- `nexting-ahakey-device-info.template.json` — 设备向 Host 发布的
  Device Info 声明模板。

推荐模式是把 Compatibility Tunnel 编进 X1 固件：保留原厂 `0x7340`
服务，只增加 A0–A3 四个固定数据 opcode。Nexting App 只加载声明式
描述符，不理解 AhaKey 私有命令。原生 Nexting GATT 也可复用同一音频
状态机。stock 固件桥接模式可支持按键、拨杆、灯和屏幕，但只有 stock
固件真正开放设备 PCM 时才能声明设备音频。所有模式共用同一个适配器
与 Device Info 身份；端到端接
线、回调与验证步骤见案例文档
[docs/cases/ahakey-x1-case.md](../../docs/cases/ahakey-x1-case.md)。
适配器不接触 Agent 凭证、账号或云端地址；授权与最终审批动作始终在
Host 侧。
编解码、音频状态机、Tunnel 与适配器当前为 **Core tested**（主机侧
CTest 通过）；真键盘、
真手机验证按案例文档清单推进，措辞边界见
[docs/conformance.md](../../docs/conformance.md)。
