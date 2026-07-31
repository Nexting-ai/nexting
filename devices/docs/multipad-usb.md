# MultiPad USB CDC 开发指南

这是一条给已经买到 ILX MultiPad 的开发者路径。它和 BLE 参考板是两种
不同的传输：MultiPad 先作为普通 USB HID 键盘工作，再通过 USB CDC ACM
通道接收配置和 Nexting JSON。它不直接连接 Claude Code、Codex 或 Nexting
云；Host/App 仍然负责授权、会话和最终动作。

如果这是你第一次把公开硬件接入 Nexting，请先读[第一个案例：ILX MultiPad](cases/multipad-first-case.md)。
本页是硬件路径和风险清单；案例页是从上游源码到新 HEX 的完整开发顺序。

## 先知道你手上的硬件是哪一版

先看[上游仓库](https://github.com/iLx11/multi-pad)与
[上游固件](https://github.com/iLx11/key-pad-application)，本目录只提供
Nexting 适配层，不复制上游 GPL 固件。

| 事实           | 当前证据                             | 对烧录的影响                  |
| -------------- | ------------------------------------ | ----------------------------- |
| MCU            | 上游工程的 STM32F103VET6 配置        | 使用 STM32 工具链，不使用 UF2 |
| 正常 USB       | HID + CDC ACM composite              | 普通 Type-C 不等于 bootloader |
| Module PCB     | 有串口模块、SERIAL 开关、RESET/BOOT  | 可按上游步骤进串口 bootloader |
| FPC PCB        | 移除串口和按键                       | 需要 SWD/J-Link               |
| App bootloader | 上游源码没有 DFU/IAP/应用 bootloader | 烧错后不能指望 USB 自救       |

上游工程的烧录配置是 `Config/stlink.cfg`：ST-Link 使用 SWD，目标是 STM32F1。
`leden.ioc` 明确把 `PA13` 配为 `SWDIO`、`PA14` 配为 `SWCLK`，但上游仓库没有
原理图或焊盘坐标文件，所以不能从仓库臆造物理焊盘顺序。看到焊盘后按丝印或
万用表确认 `3V3/GND/SWDIO/SWCLK`，不要把 Type-C CDC 当作烧录器。

拆机前不要猜显示屏、电池、序列号或开关位置。请先拔掉 Type-C，拆下四个
背面螺丝，拿起后盖时不要拉扯屏幕排线，并拍下 MCU、PCB 版本、BOOT/RESET
和 SERIAL 标记。本目录的
`firmware/multipad/nexting-multipad-device-info.template.json` 只声明了
上游源码能证明的按键/旋钮数量；未知字段保持缺省。

## 软件准备

```sh
git clone https://github.com/Nexting-ai/nexting.git
cd nexting/devices

# 先编译与运行不接硬件的 C99 适配器测试
npm run test:multipad

# 先看安全检查，不会写设备
sh firmware/multipad/tools/flash-multipad.sh --help
python3 firmware/multipad/tools/multipad-cdc-smoke.py --help
```

`nexting_multipad_adapter.c/.h` 是唯一需要移植到上游工程的薄层。它复用
`sdk/c` 的 framing、审批 TTL、回答锁定、1 秒重发、resolved 清理、状态全量
替换和断连清理；它不新增一套 JSON 解析器，也不携带 App/Agent 逻辑。

### 接到上游 USB 回调

把 adapter 源文件和 `sdk/c/src/nexting_device.c` 加入上游 CMake 工程，在
`USER/Usb/usb_user.c` 的 CDC 接收入口先保留老协议，再加这一段：

```c
if (nexting_multipad_accepts(&nexting_adapter, Buf, *Len)) {
    (void)nexting_multipad_receive(&nexting_adapter, Buf, *Len);
    return USBD_OK;
}
```

两个审批键调用 `nexting_multipad_choose()`；系统毫秒节拍调用
`nexting_multipad_tick()`；USB 断连调用 `nexting_multipad_disconnect()`。显示
和 USB 写入由你自己的板级回调实现。这样 HID 的键盘功能与原有 `AA BB xx`
配置命令仍然保留，Nexting 帧只在以 `{` 开始的 CDC 流上生效。

## 烧录前检查清单

1. **备份**：先让设备进入上游支持的串口 bootloader，再读取原始 flash；备份
   文件必须不存在，脚本不会覆盖旧备份。
2. **确认路径**：Module PCB 才能走串口；FPC PCB 直接停止，准备 SWD。
3. **校验固件**：使用与你的适配器构建对应的 Intel HEX，不能把 `.bin`、UF2
   或未知版本混用。
4. **预览 CDC**：设备仍能被系统识别为 `MultiPad_Device` 时，先运行：

   ```sh
   python3 firmware/multipad/tools/multipad-cdc-smoke.py /dev/cu.usbmodemXXXX
   ```

   这只发送上游已知的 `AA BB CC` 回显，不写 flash。只有适配器固件已经烧入
   后，才加 `--present` 发送 Nexting 测试帧。

5. **显式写入**：只有确认设备处于串口 bootloader、备份成功、端口和 HEX 都
   正确时才执行：

   ```sh
   sh firmware/multipad/tools/flash-multipad.sh write \
     --port /dev/cu.usbmodemXXXX \
     --hex /path/to/nexting-multipad.hex \
     --confirm --allow-write
   ```

脚本没有自动搜索端口，也不会替你按 RESET/BOOT；这两个动作必须依据拆机后
的 PCB 证据完成。这样可以避免把普通 CDC 误当作 bootloader。

## App 与 Host 边界

本路径完成的是**固件和公开 SDK 适配**。现有 Nexting App 的公开设备路径是
加密绑定的 BLE；它不会因为你烧入 USB CDC 就自动显示 MultiPad。要让某个
桌面 Host 或未来 App 使用 USB，需要该 Host 自己拥有 USB 权限、授权、端口
选择和断连策略，并通过同一套 `approval/1`、`status/1` 向量验证。不要把 USB
端口、Agent 会话 ID、云地址或凭证写进固件。

## 完成标准

- `npm run test:multipad` 通过；
- 上游工程按其原许可证独立构建；
- 原始 flash 有可恢复备份；
- CDC 回显通过，Nexting `present` 能得到设备 `answer`；
- 断连后审批和状态都清空；
- Device Info 只声明拆机后确实存在的能力。

在最后一项证据拿到前，不要在 GitHub README 或 App 中写“已支持 MultiPad”或
“可直接 Type-C 烧录”。
