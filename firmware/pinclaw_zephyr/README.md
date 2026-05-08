# Pinclaw Firmware

基于 Zephyr RTOS (nRF Connect SDK 2.7.0) 的 Pinclaw 硬件固件。

## 硬件

| 组件   | 型号                      | 说明                                              |
| ------ | ------------------------- | ------------------------------------------------- |
| 主控   | Seeed XIAO nRF52840 Sense | 21×17.5mm，集成 BLE 5.0 + PDM 麦克风 + IMU + 充电 |
| 按钮   | 轻触开关 → D4 (P0.04)     | 按下接地，内部上拉                                |
| 扬声器 | I2S + PAM8302A            | SCK=A3, LRCK=A2, SDOUT=A1                         |
| 电池   | 3.7V LiPo 200mAh          | 板载充电管理 (BQ25101)                            |
| 麦克风 | 板载 PDM                  | XIAO Sense 自带                                   |

## 烧录

### 方法一：自动化（推荐）

```bash
cd firmware
./test-device.sh 10          # 烧录 + 写入设备编号 + 串口验证 + BLE 测试
./test-device.sh 10 --quick   # 烧录 + 验证（跳过按钮和重连测试）
./test-device.sh 10 --no-flash # 已烧录过，只跑测试
```

脚本会自动：

1. 等你双击 Reset 进入 bootloader
2. 复制 uf2 固件到设备
3. 通过串口写入设备编号（BLE 名称变为 "Pinclaw 010"）
4. 验证启动日志（固件版本、按钮初始值、LED、蓝牙）
5. 运行 BLE 全面测试（设备信息、电池、心跳、麦克风、扬声器、重连）

依赖：`pip3 install bleak`

### 方法二：手动

1. **双击 Reset** — 快速双击设备上的 Reset 按钮，橙色 LED 脉冲闪烁
2. 电脑出现 `XIAO-SENSE` USB 驱动器
3. 复制固件文件：
   ```bash
   cp pinclaw_v2.1.9.uf2 /Volumes/XIAO-SENSE/fw.uf2
   ```
4. 设备自动重启

### 首次烧录（新板子）

新板子需要先升级 bootloader：

1. 双击 Reset 进入 bootloader
2. `cp bootloader0.9.0.uf2 /Volumes/XIAO-SENSE/`
3. 等待重启，再次双击 Reset
4. 烧录固件（同上）

## 编译

### 环境准备

需要以下工具，按顺序安装：

#### 1. nRF Connect SDK 2.7.0

```bash
# 安装 west（Zephyr 的构建管理工具）
pip3 install west

# 初始化 NCS 到 ~/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.7.0 ~/ncs
cd ~/ncs
west update
```

安装完成后目录结构：

```
~/ncs/
├── zephyr/          # Zephyr RTOS 内核
├── nrf/             # Nordic SDK
├── modules/         # 第三方模块
└── ...
```

#### 2. ARM GCC 工具链

macOS 上推荐通过 Seeed Arduino 包获取（已验证兼容）：

1. 安装 [Arduino IDE](https://www.arduino.cc/en/software)
2. Arduino IDE → 偏好设置 → 附加开发板管理器网址，添加：
   ```
   https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
   ```
3. 工具 → 开发板管理器 → 搜索 "Seeed nRF52" → 安装

安装后工具链位于：

```
~/Library/Arduino15/packages/Seeeduino/tools/arm-none-eabi-gcc/9-2019q4
```

或者直接下载 [ARM GNU Toolchain](https://developer.arm.com/downloads/-/gnu-rm)（9-2019-q4-major 版本）。

#### 3. CMake（重要：必须是 3.x 版本）

Zephyr / NCS 2.7.0 **不兼容 CMake 4.x**（picolibc 的 `try_compile` 和 `--` 分隔符行为在 CMake 4 中已更改）。

检查当前版本：

```bash
cmake --version
```

如果是 4.x，需要降级：

```bash
# 方法一：通过 pip 安装 CMake 3.x（推荐，不影响系统 cmake）
pip3 install cmake==3.31.6

# 编译前确保 pip 的 cmake 优先
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
cmake --version  # 应显示 3.31.6
```

如果 Python 版本不是 3.9，替换为你的实际版本（`python3 --version` 查看）。

#### 4. 其他依赖

```bash
# Ninja 构建工具
brew install ninja

# dtc（devicetree 编译器）— 可选，缺少时会警告但不影响编译
brew install dtc
```

### 编译命令

```bash
# 设置环境变量
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=~/Library/Arduino15/packages/Seeeduino/tools/arm-none-eabi-gcc/9-2019q4
export ZEPHYR_BASE=~/ncs/zephyr

# 如果 cmake 是 4.x，需要切到 pip 安装的 3.x
export PATH="$HOME/Library/Python/3.9/bin:$PATH"

cd firmware/pinclaw_zephyr

# 完整重新编译（--pristine 清除旧缓存）
west build -b xiao_ble/nrf52840/sense . \
  -DDTC_OVERLAY_FILE="$(pwd)/overlay/xiao_ble_sense_devkitv2-adafruit.overlay" \
  --pristine

# 增量编译（只重编改过的文件，更快）
west build
```

产物：`build/zephyr/zephyr.uf2`

```bash
cp build/zephyr/zephyr.uf2 pinclaw_v<版本号>.uf2
```

### 编译问题排查

| 错误                                  | 原因                  | 解决                                   |
| ------------------------------------- | --------------------- | -------------------------------------- |
| `picolibc.cmake` / `try_compile` 失败 | CMake 4.x 不兼容      | 降级到 CMake 3.x（见上方）             |
| `west: unknown command "build"`       | 不在 NCS workspace 内 | 设置 `export ZEPHYR_BASE=~/ncs/zephyr` |
| `Could NOT find Dtc`                  | 缺少 dtc              | `brew install dtc`（警告可忽略）       |
| `GNUARMEMB_TOOLCHAIN_PATH` 相关       | 工具链路径错误        | 确认 `arm-none-eabi-gcc` 在该路径下    |

### 版本号约定

修改 `prj.conf` 中的 `CONFIG_BT_DIS_FW_REV_STR` 后再编译。

## 测试

### 自动化测试脚本

```bash
# 只跑 BLE 测试（不烧录）
python3 firmware/pinclaw_test.py --device-num 10

# 完整测试（烧录 + 串口 + BLE）
./firmware/test-device.sh 10
```

### 串口监控

```bash
# 查看实时日志
cat /dev/cu.usbmodem*

# 或用 screen
screen /dev/cu.usbmodem3101 115200
```

### 测试覆盖项

**串口层（开机自动验证）：**

| 项目          | 判定标准                            |
| ------------- | ----------------------------------- |
| 固件版本      | 日志匹配 `Firmware revision: x.x.x` |
| 硬件型号      | 日志匹配 `Model: Pinclaw Clip`      |
| D4 按钮初始值 | 值为 1（未按下）。为 0 说明按钮短路 |
| TWIM1 修复    | 日志显示 `TWIM1 disconnected`       |
| LED 初始化    | 日志显示 `LEDs started`             |
| I2S 扬声器    | 日志显示 `[speaker] OK`             |
| 无错误刷屏    | 不出现 `Failed to broadcast`        |

**BLE 层（pinclaw_test.py）：**

| 项目           | 判定标准                                   |
| -------------- | ------------------------------------------ |
| BLE 扫描       | 能发现设备，RSSI > -80 dBm                 |
| 连接 + MTU     | 连接成功，MTU ≥ 185                        |
| 固件版本 (DIS) | 读 BLE DIS 特性确认版本                    |
| 电池 (BAS)     | 电量 > 0%，电压 2500-4200 mV               |
| 心跳           | 间隔 ~10s，计数器递增                      |
| 麦克风         | START→DATA×N→END，序列号连续，帧大小 > 10B |
| 扬声器         | 写入 haptic 指令不报错                     |
| 按钮           | PRESS + RELEASE 事件成对出现               |
| 重连           | 断开再连 ×3 全部成功                       |

## 功能

| 功能                | 状态 | 说明                                |
| ------------------- | ---- | ----------------------------------- |
| BLE 连接            | ✅   | 自动广播，支持多连接（最多 3）      |
| 硬件按钮录音        | ✅   | 长按 D4 > 0.5s 录音，松开停止       |
| 硬件按钮播放        | ✅   | 短按 D4 < 0.5s 触发 PLAY            |
| iPhone hold-to-talk | ✅   | App 发 0x01/0x00 控制录音           |
| Opus 编码           | ✅   | 16kHz mono                          |
| 电池监测            | ✅   | BAS + 心跳包上报电压                |
| 心跳保活            | ✅   | 10s 间隔，独立线程                  |
| 设备编号            | ✅   | NVS 存储，BLE 名称 "Pinclaw XXX"    |
| 扬声器              | ⚠️   | I2S 初始化正常，PAM8302A 引脚待适配 |
| SD 卡               | ❌   | 未启用                              |
| 加速度计            | ❌   | 未启用                              |

## BLE 协议

### Services

| 服务        | UUID                                   | 用途           |
| ----------- | -------------------------------------- | -------------- |
| Audio       | `12345678-1234-1234-1234-123456789ABC` | 录音/命令/心跳 |
| Button      | `23BA7924-0000-1000-7450-346EAC492E92` | 按钮事件       |
| Speaker     | `CAB1AB95-2EA5-4F4D-BB56-874B72CFC984` | 扬声器/触觉    |
| Battery     | `180F`（标准 BAS）                     | 电池百分比     |
| Device Info | `180A`（标准 DIS）                     | 固件/硬件版本  |

### Audio Characteristics

| UUID 后缀 | 属性              | 说明                         |
| --------- | ----------------- | ---------------------------- |
| `...9ABE` | notify            | 音频数据包（START/DATA/END） |
| `...9ABD` | read/write/notify | 命令（录音/停止/关机）       |
| `...9ABF` | notify            | 心跳包                       |

### 命令（写入 ...9ABD）

| 字节   | 说明             |
| ------ | ---------------- |
| `0x01` | 开始录音         |
| `0x00` | 停止录音         |
| `0x20` | 播放（短按模拟） |
| `0x40` | 关机             |

### 音频包格式（...9ABE notify）

```
START: [0x01][codec:1B][0x00 ×4]     codec: 0x14=Opus
DATA:  [0x02][seqNo:2B BE][opus...]
END:   [0x03][totalFrames:4B BE]
```

### 心跳包格式（...9ABF notify，每 10s）

```
[0x04][counter:2B BE][flags:1B][battery_mV:2B BE]

flags:
  bit 0 = 录音中
  bit 1 = USB 充电中
```

### 按钮事件（Button characteristic notify）

```
[state:1B][0x00]

state:
  4 = PRESS
  5 = RELEASE
```

## 按钮行为

| 操作        | 效果                                |
| ----------- | ----------------------------------- |
| 长按 ≥ 0.5s | 红灯亮，开始录音 → 松开停止，红灯灭 |
| 短按 < 0.5s | 发 PLAY 命令（0x20）给 App          |

## LED 状态

| 颜色         | 含义       |
| ------------ | ---------- |
| 红绿蓝依次闪 | 启动中     |
| 蓝色常亮     | BLE 已连接 |
| 红色常亮     | 录音中     |
| 绿色闪烁     | 充电中     |
