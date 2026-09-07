# AhaKey X1 case: a shipping vibecoding keyboard on Nexting Devices

**Summary.** The AhaKey X1 is a commercially shipping BLE console for
coding Agents — four keys, a physical approval lever, an LED strip, and an
OLED. This case integrates it with Nexting Devices in two modes: a native
firmware port for the AhaKey team, and a bridge dongle that works with any
stock X1 without a firmware change. The keyboard then pairs with the
Nexting App and reaches Claude Code and Codex sessions through the same
remote path as every other Nexting device — including sessions running on a
computer somewhere else.

| Layer               | Deliverable                                                            | Evidence level       |
| ------------------- | ---------------------------------------------------------------------- | -------------------- |
| Upstream protocol   | AhakeyAI/desktop `docs/ble-protocol.md`, pinned to `c1663032`          | Source auditable     |
| AhaKey codec        | `firmware/ahakey-x1/ahakey_proto.c/.h`                                 | Core tested          |
| Nexting mapping     | `firmware/ahakey-x1/nexting_ahakey_adapter.c/.h`                       | Core tested          |
| Device audio core   | `firmware/ahakey-x1/nexting_ahakey_audio.c/.h`                         | Core tested          |
| Compatibility port | `firmware/ahakey-x1/developer-kit/nexting_ahakey_port.c/.h`            | Core tested          |
| App descriptor      | `firmware/ahakey-x1/transport-descriptor.nexting-device.json`          | Schema/vector tested |
| Device Info         | `firmware/ahakey-x1/nexting-ahakey-device-info.template.json`          | Template             |
| CH582 microphone hook | AhaKey production source + physical unit                              | Pending board bring-up |
| Host boundary       | Host owns authorization, Agent routing, and the final answer action    | Public contract      |

# 案例：AhaKey X1 接入 Nexting Devices

AhaKey X1 是一把已经在售的 BLE vibecoding 键盘：4 个按键、一个物理审批
拨杆、一条 LED 灯带和一块 OLED。这个案例的目标不是重做它，而是保留原
厂全部能力，只增加一层可验证的 Nexting 设备协议映射。接入之后，键盘
配对 Nexting App，经同一条远程路径触达 Claude Code 与 Codex 会话——
包括运行在其他电脑上的会话。键盘不再只能坐在 Mac 旁边用。

## 上游工程与版本

- 桌面客户端与协议文档：[AhakeyAI/desktop](https://github.com/AhakeyAI/desktop)，
  Apache-2.0，协议文档 `docs/ble-protocol.md` pin 在 commit
  `c16630329cd0a53fcbcdd4d528015257a19449f6`。
- 上游协议已公开证明的事实：GATT 服务 `0x7340`（命令 `0x7343`、通知
  `0x7344`、大数据 `0x7341`）、帧格式 `AA BB cmd data CC DD`、标准
  Battery Service `0x180F`、IDE 状态同步命令 `0x90`（9 个 ClaudeState）、
  状态查询命令 `0x00`（含电量与拨杆 SwitchState）、OLED 160×80 RGB565、
  按键为可配置 HID 输出。
- AhaKey 官方 firmware 仓库当前公开的是 v1.1.0 HEX、CH582 烧录说明和发布
  记录，没有公开可构建的量产源码、原理图或 PCM callback。产品方确认设备
  具有录音能力，因此本案例实现完整的设备音频接口和状态机；最后一个
  ADC/I2S/PDM 符号只能在拿到量产源码与实物后绑定，现阶段不虚构接口名，
  也不宣称 Board verified。

## 能力映射

| X1 硬件               | Nexting 公共接口      | 案例行为                                   |
| --------------------- | --------------------- | ------------------------------------------ |
| 配置为 F18 的按键     | `approval/1` → Allow  | 回答当前待决请求                           |
| 配置为 F19 的按键     | `approval/1` → Deny   | 回答当前待决请求                           |
| 配置为 F13–F16 的按键 | `keys/1` 槽位 0–3     | keymap 启用后发带递增序列号的 `key_event`  |
| 物理拨杆              | `keys/1` 槽位 7       | keymap 启用后：离开静止位 press，回位 release |
| LED 灯带              | `status/1`            | Agent 状态 → AhaKey IDE 状态（`0x90`）     |
| OLED 160×80           | `text/1`              | 标题 + 正文                                |
| Battery `0x180F`      | Device Info           | `battery_service: true`                    |
| 设备本身麦克风        | `device-audio/1`      | PCM16/16 kHz → IMA ADPCM → BLE → App       |
| 待决审批（任意来源）  | LED 覆盖              | 待决期间强制 permission-request 状态       |

待决审批会用等待授权灯效覆盖灯带（审批通道与状态通道共用一条灯带，审
批优先）。其余时候灯带按 status/1 规则渲染编号最小的已占用槽位：活
动中的 Agent 显示工具执行，needs-input 显示等待授权，完成的 Agent
显示任务完成，错误显示通知告警，更高编号的槽位忽略，没有占用则熄
灭。按键与拨杆只在 Host keymap 声明并启用对应槽位后才发事件。拨杆
的语义保持在设备侧中立：它只报告物理位置，授权策略与自动批准的最
终决定权永远在 Host。

## 模式 A：Compatibility Tunnel 固件（推荐）

这条路径不要求 Nexting App 内置 AhaKey 解析器。App 从资源目录加载经过审
计的 JSON 描述符；AhaKey 的 UUID 与 opcode 只存在于描述符和本适配器。

在原有 `0x7343` write / `0x7344` notify 上增加四个直接前缀：A0 控制
下行、A1 控制上行、A2 音频上行、A3 流控下行。原厂 `AA BB ... CC DD`
配置帧原样交回原解析器，二者可共存。详细接线与入口见
[`developer-kit/README.md`](../../firmware/ahakey-x1/developer-kit/README.md)。

必须满足：bond + encryption、ATT payload 至少 64 字节、订阅后主动发
送带稳定 `device_id` 的 Device Info、生产 PCM hook 成功后才能声明
`device-audio/1`。Host 的 `audio_config` 只能准备缓冲区，绝不能启动麦
克风；只有实体语音键调用 `nexting_ahakey_port_voice_pressed()`。

## 模式 B：原生 Nexting GATT（可选）

把下面的公开文件编入 X1 固件，不要修改公共 SDK 的语义：

```text
devices/sdk/c/include/nexting_device.h
devices/sdk/c/src/nexting_device.c
devices/sdk/c/include/nexting_device_audio.h
devices/sdk/c/src/nexting_device_audio.c
devices/firmware/ahakey-x1/ahakey_proto.h
devices/firmware/ahakey-x1/ahakey_proto.c
devices/firmware/ahakey-x1/nexting_ahakey_adapter.h
devices/firmware/ahakey-x1/nexting_ahakey_adapter.c
devices/firmware/ahakey-x1/nexting_ahakey_audio.h
devices/firmware/ahakey-x1/nexting_ahakey_audio.c
```

1. 广播 Nexting 主服务 `6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0`，保
   留原厂 `0x7340` 服务——两者互不冲突，原厂配置工具照常可用。
2. 下行特征（`6EADC0DE-0002-…`）的写入喂给
   `nexting_ahakey_receive()`；`write_frame` 回调的内容通过上行特征
   （`6EADC0DE-0003-…`）notify 发出。
3. Device Info 特征（`6EADC0DE-0004-…`）从
   `nexting-ahakey-device-info.template.json` 出参，`fw` 与
   `device_id` 按实机填写。
4. 渲染回调直接驱动硬件：`render_led` 写灯带、`render_text` 画
   OLED、`render_approval` 显示待决摘要。这个模式里不经过 `0x7340`
   帧。
5. 按键中断调用 `nexting_ahakey_choose()` 或
   `nexting_ahakey_hid_key()`；拨杆边沿调用
   `nexting_ahakey_lever()`。
6. 适配器不接触 Agent 凭证、账号或云端地址；授权与最终审批动作始终
   在 Host 侧。不要把任何 Claude Code、Codex 或云配置写进固件。

## 模式 C：桥接 dongle（原厂固件零改动）

一块 BLE 小 dongle（XIAO nRF52840 参考板即可）坐在键盘和手机之间：

```text
AhaKey X1 ──BLE──> 桥接 dongle ──BLE──> Nexting App ──> 你的 Agent
 (HID + 0x7340)    （本套件）       （Nexting 服务）
```

1. 先用原厂工具配置一次键位：Key1 = F18、Key2 = F19、Key3–Key4 =
   F13–F14，并保存到 Flash。
2. dongle 对 X1 做 central：订阅 HID 输入报告收按键、周期发送状态查
   询（`0x00`）轮询拨杆、用 `ahakey_proto_build_update_state()` 写
   `0x90` 驱动 LED、用键描述帧（`0x73/0x75`）写 OLED 短文本。
3. dongle 对手机做 peripheral：运行 Nexting 服务，Device Info 声明
   使用本目录模板，`model` 保持 `ahakey-x1` 以便 App 识别形态。
4. 完整 RGB565 图片通道（`0x80–0x83`）已在 `ahakey_proto` 的通用帧
   构建器覆盖，图片排版属于桥接固件的板级实现，不属于协议核。

桥接模式默认不声明 `device-audio/1`。只有原厂 stock 固件把键盘真实
PCM 流开放给 dongle，并且通过同一套音频测试后，才可开启该能力；不允
许用手机麦克风伪装成 AhaKey 录音。

## 一次只验证一个行为

1. `npm run test:ahakey` 通过，包括上游文档公布的抓包帧。
2. 桥接或原生固件广播后，App 读到 Device Info，能力与本模板一致。
3. 发一个 `present`，确认 LED 进入等待授权、屏幕只显示当前请求。
4. 按 Allow 键，确认收到同一个 `id` 的 `answer`；Host 成功后发送
   `resolved`，设备清掉待决状态。
5. 确认 Host 的 keymap 声明槽位 0–3 与 7 后，拨动拨杆：槽位 7 的
   press/release 各到达一次；静止不动时无重复事件；未声明的槽位不发
   任何事件。
6. 单独测试超时与 BLE 断连：两者都必须清空审批与状态渲染，LED 熄
   灭，不能留下旧提示。
7. 用逻辑分析/抓包确认音频来自设备麦克风：实体键前没有采样，实体键
   后为 16 kHz mono PCM，每 320 样本编码一帧，A2 上行；拔掉/禁用手机
   麦克风权限仍能录音。
8. 人为停止 Host credit，确认 RAM 队列不超过 10 帧，500 ms 内取消；
   断连、拒绝、硬件错误和两分钟上限均停止麦克风并擦除 DMA/队列。

在没有这些逐项证据前，只能写「适配器与编解码 Core tested」，不能写
「X1 已 Board verified」或「桥接模式已量产」。措辞边界见
[conformance.md](../conformance.md)。

## 这个案例与 Nexting App 的关系

与所有第三方设备相同：X1（任一模式）必须在 App 内显式授权、可撤
销、标记为第三方设备，不能冒充 Nexting 生产硬件。公开 App 的第三方
开发者设备注册进度以 `docs/availability.json` 为准；注册未开放前，
验证走开发构建，不把内部进度写成公开承诺。

## 完成定义

- 上游协议文档固定到具体 commit；
- `npm run test:ahakey` 与全仓 `npm run check` 通过；
- 目标集成路径完成上面全部逐项验证并有记录；
- Device Info 只声明实际存在的能力（PCM hook 未验证时不声明
  `device-audio/1`；`voice/1` 仍仅表示 Host 手机/电脑麦克风）；
- 公开表述遵守 conformance 证据等级，不把 Core tested 写成整机兼容。
