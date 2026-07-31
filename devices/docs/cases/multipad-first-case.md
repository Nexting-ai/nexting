# 第一个案例：ILX MultiPad 接入 Nexting Devices

ILX MultiPad 是 Nexting Devices 的第一个完整硬件案例。它不是一块
Nexting 量产板，而是一块已经开源、已经能作为键盘使用的 STM32F103VET6
USB HID + CDC 设备。这个案例的目标是说明：如何保留原作者的键盘功能，
只增加一层可验证的 Nexting 设备协议适配。

## 这个案例到底交付什么

| 层             | 案例交付物                                                           | 证据等级           |
| -------------- | -------------------------------------------------------------------- | ------------------ |
| 上游固件       | `iLx11/multi-pad`，固定到 `78c1ee533a7f513e9f390741c4f5eed1e0aa91b3` | 源码可审计         |
| Nexting 设备核 | `sdk/c` 的固定缓冲 C99 协议、流式拼帧和审批状态机                    | Core tested        |
| MultiPad 适配  | `firmware/multipad/nexting_multipad_adapter.c/.h`                    | CMake 合约测试通过 |
| 兼容入口       | 原有 `AA BB xx` 保留，只有换行 JSON 帧进入 Nexting 适配器            | 主机测试通过       |
| 烧录路径       | Module 版走串口路径（仅当看到 SERIAL/BOOT/RESET）；FPC 版走 SWD      | 由拆机证据决定     |
| Host 边界      | Host 负责授权、Agent 路由和最终回答；固件不保存凭证                  | 公开契约           |

当前公开 SDK 不提供一个可以误称为“已经适配所有版本”的 HEX。上游工程中
的 `leden.hex` 是原版固件（原作者固件）；接入 Nexting 适配器后必须重新构建并单独保存
新的 HEX。不能把原固件误当作 Nexting 固件烧进去。

## 上游工程与版本

使用下面两个公开仓库，不要把上位机仓库当成下位机固件：

- 下位机固件：[iLx11/multi-pad](https://github.com/iLx11/multi-pad)
- 上位机：[iLx11/key-pad-application](https://github.com/iLx11/key-pad-application)

本案例只接入下位机仓库。上游工程本身已经证明了这些事实：

- `STM32F103VET6`、Cortex-M3；
- USB HID + USB CDC composite；
- 2 × 4 矩阵按键和 3 个旋钮模块；
- `PA13 = SWDIO`、`PA14 = SWCLK`；
- `PA9/PA10 = USART1_TX/RX`；
- 工程没有 DFU、IAP 或应用侧 USB bootloader；
- 上游许可证为 GPL-3.0。

烧录配置在上游的 `Config/stlink.cfg`，不是 Nexting App 的配置。普通 Type-C
枚举出来的 CDC 端口只能证明应用正在运行，不能证明它已经进入 bootloader。
Type-C 不能直接烧录，除非先确认对应的 bootloader 路径。

## 第一个可运行映射

这个案例只承诺一个小而明确的映射，不把未来能力提前写成已支持：

| MultiPad 控件     | Nexting 公共接口                   | 案例行为                                        |
| ----------------- | ---------------------------------- | ----------------------------------------------- |
| 第一个矩阵键      | `approval/1` → Allow               | 批准当前唯一待决请求                            |
| 第二个矩阵键      | `approval/1` → Deny                | 拒绝当前唯一待决请求                            |
| 现有 CDC 接收回调 | `nexting_multipad_receive()`       | 接收 `present`、`resolved`、`status`            |
| 现有 CDC 发送函数 | `nexting_multipad_write_frame()`   | 回传 `answer`                                   |
| 板上屏幕/指示器   | `render_approval`、`render_status` | 只渲染回调收到的状态                            |
| 其他按键和旋钮    | 保留原厂行为                       | 等 `keys/1`、`rotary/1` 等 profile 发布后再映射 |

适配器不会解析 Agent 会话，也不会把 Claude Code、Codex、账户或云地址写入
设备。显示摘要、倒计时和状态的具体排版属于板级回调，不属于协议核。

## 从上游源码到案例固件

### 1. 固定上游源码并先构建原版

```sh
git clone https://github.com/iLx11/multi-pad.git
cd multi-pad
git checkout 78c1ee533a7f513e9f390741c4f5eed1e0aa91b3

# 上游工程要求 arm-none-eabi-gcc、CMake 和 STM32 工具链
cmake -S . -B build
cmake --build build
sha256sum build/leden.hex
```

先保存这个原版产物和哈希。它只用于恢复原厂功能，不能标记为 Nexting 固件。

### 2. 加入公开 Nexting 源码

从 `Nexting-ai/nexting/devices/` 取下面的公开文件，复制到上游工程的独立
目录（例如 `USER/Nexting/`），不要修改公共 SDK 的语义：

```text
devices/sdk/c/include/nexting_device.h
devices/sdk/c/src/nexting_device.c
devices/firmware/multipad/nexting_multipad_adapter.h
devices/firmware/multipad/nexting_multipad_adapter.c
```

把这四个文件加入上游 CMake target，并把 `sdk/c/include` 加入 include path。
上游 `USER/Usb/usb_user.c` 的 CDC 回调先判断 Nexting 帧，再落回原有分支：

```c
if (nexting_multipad_accepts(&nexting_adapter, Buf, *Len)) {
    (void)nexting_multipad_receive(&nexting_adapter, Buf, *Len);
    return USBD_OK;
}

/* 原有 AA BB CC / AA BB AA / AA BB DD / AA BB EE / AA BB FF 分支继续保留。 */
```

初始化时提供四个板级回调：单调时钟、CDC 发送、审批渲染、状态渲染。主循环
调用 `nexting_multipad_tick()`；USB 断连调用
`nexting_multipad_disconnect()`；前两个审批键分别调用
`nexting_multipad_choose(...ALLOW)` 和
`nexting_multipad_choose(...DENY)`。回调失败时不要伪造成功 UI。

### 3. 构建、备份、烧录

重新构建后，对新的 `leden.hex` 计算哈希，并把原版备份、Nexting 版本和日志
放在不同文件名下。根据硬件版本选择路径：

| 版本       | 写入方式                   | 关键条件                                   |
| ---------- | -------------------------- | ------------------------------------------ |
| Module PCB | 上游串口路径（待实板确认） | 必须看到 SERIAL、BOOT、RESET 路径          |
| FPC PCB    | ST-Link/J-Link SWD         | `3V3`、`GND`、`SWDIO(PA13)`、`SWCLK(PA14)` |

在确认 bootloader 之后，才可以使用本目录的
`firmware/multipad/tools/flash-multipad.sh`。脚本要求显式的端口、HEX、
`--confirm --allow-write`，且不会覆盖原始备份。

### 4. 一次只验证一个行为

1. 先用 `multipad-cdc-smoke.py` 验证原有 `AA BB CC` 回显。
2. 发送一个 `present`，确认屏幕或指示器只显示当前请求。
3. 按 Allow，确认收到同一个 `id` 的 `answer`。
4. 确认 Host 成功后发送 `resolved`，设备清掉待决状态。
5. 单独测试超时和 USB 断连；两者都必须清空审批和状态，不能留下旧提示。

在没有这些逐项证据前，只能写“适配器测试通过”，不能写“MultiPad 真板已
兼容”或“Type-C 可直接烧录”。

## 这个案例与 Nexting App 的关系

MultiPad USB 是公开的开发者案例，不是现有 BLE App 自动支持的 USB 变体。
Host 必须自己拥有 USB 权限、端口选择、授权状态和 Agent 最终回答操作；
设备只接收有界的公共消息。未来如果 Nexting App 开放 USB transport，需要
另行发布 Host 支持、授权测试和断连策略，不能由这份固件文档推断出来。

## 完成定义

- 上游工程能在固定 commit 上复现构建；
- 原版 HEX 已备份并有哈希；
- Nexting HEX 由明确的适配源码生成并有哈希；
- `npm run test:multipad`、C99 合约测试和 CDC smoke 通过；
- `present → answer → resolved`、超时、断连分别有记录；
- `Device Info` 只声明实际存在的按键、旋钮、显示和电量能力；
- GitHub README 只链接本案例，不把实验性 USB 路径写成量产承诺。
