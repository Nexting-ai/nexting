# Pinclaw Firmware v1.0.0

基于 Zephyr RTOS (nRF Connect SDK 2.7.0) 的 Pinclaw 硬件固件。

## 硬件要求

- **主控**: Seeed XIAO nRF52840 Sense
- **按钮**: D4 (P0.04) → GND，active LOW
- **扬声器**: I2S (D1/D2/D3) + MAX98357A
- **电池**: 3.7V LiPo
- **麦克风**: 板载 PDM（XIAO Sense 自带）

## 快速烧录（不需要编译）

适用于已有 `pinclaw_v1.0.0.uf2` 文件的情况。

### 第一次烧录（需要升级 bootloader）

1. **升级 Bootloader**
   - 双击 Reset 按钮（快速双击），进入 bootloader 模式
   - 电脑上会出现 `XIAO-SENSE` USB 驱动器
   - 将 `bootloader0.9.0.uf2` 拷贝到驱动器
   - 等待设备自动重启

2. **烧录固件**
   - 再次双击 Reset 进入 bootloader
   - 将 `pinclaw_v1.0.0.uf2` 拷贝到 `XIAO-SENSE` 驱动器
   - 等待设备自动重启

### 后续烧录（已升级过 bootloader）

1. 拔 USB → 按住 Reset → 插 USB → 松开 → 双击 Reset
2. 将 UF2 文件拷贝到 `XIAO-SENSE` 驱动器

### macOS 命令行烧录

```bash
# 进入 bootloader 后：
python3 -c "
with open('pinclaw_v1.0.0.uf2', 'rb') as s:
    d = s.read()
with open('/Volumes/XIAO-SENSE/fw.uf2', 'wb') as f:
    f.write(d)
print(f'Wrote {len(d)} bytes')
"
```

## 从源码编译

### 前置条件

- Docker（推荐）
- 或 nRF Connect SDK 2.7.0 + Zephyr SDK 0.16.8

### Docker 编译（推荐）

```bash
cd firmware/pinclaw_zephyr

docker run --rm -v "$(pwd)/../..:/pinclaw" ghcr.io/zephyrproject-rtos/ci:v0.26.13 bash -c '
pip install west
export PATH="/root/.local/bin:$PATH"

# 初始化 SDK（首次需要，约 5 分钟）
mkdir -p /build/v2.7.0 && cd /build/v2.7.0
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.7.0
west update -o=--depth=1 -n
west zephyr-export

# 编译
west build -b xiao_ble_sense --pristine always /pinclaw/firmware/pinclaw_zephyr -- \
    -DNCS_TOOLCHAIN_VERSION="NONE" \
    -DCONF_FILE="prj.conf" \
    -DDTC_OVERLAY_FILE="/pinclaw/firmware/pinclaw_zephyr/overlay/xiao_ble_sense_devkitv2-adafruit.overlay" \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DPLATFORM=nrf52840

cp build/zephyr/zephyr.uf2 /pinclaw/firmware/pinclaw_zephyr/pinclaw_v1.0.0.uf2
'
```

## 功能

| 功能                | 状态 | 说明                         |
| ------------------- | ---- | ---------------------------- |
| BLE 连接            | ✅   | Pinclaw UUID, 自动广播       |
| iPhone hold-to-talk | ✅   | iPhone 发 0x01/0x00 控制录音 |
| 硬件按钮录音        | ✅   | 长按 D4 > 0.5s 开始录音      |
| 硬件按钮播放        | ✅   | 短按 D4 < 0.5s 触发 PLAY     |
| Opus 编码           | ✅   | 32kbps VBR, 16kHz mono       |
| 扬声器              | ✅   | I2S + MAX98357A              |
| 电池监测            | ✅   | 每 10s 上报电压              |
| SD 卡               | ❌   | SPI 通信待排查               |
| 加速度计            | ❌   | 无硬件                       |
| 震动马达            | ❌   | 无硬件                       |

## BLE 协议

### Service UUID

`12345678-1234-1234-1234-123456789ABC`

### Characteristics

| UUID 后缀 | 名称    | 属性              | 说明            |
| --------- | ------- | ----------------- | --------------- |
| `...9ABE` | Audio   | notify            | 音频数据 (Opus) |
| `...9ABD` | Text    | read/write/notify | 命令 + PLAY     |
| `...9ABF` | Speaker | write/notify      | 扬声器音频      |

### 命令协议 (写入 ABD)

| 字节   | 说明     |
| ------ | -------- |
| `0x01` | 开始录音 |
| `0x00` | 停止录音 |
| `0x40` | 关机     |

### 音频包格式 (ABE notify)

```
START: [0x01][0x14][0x00][0x00][0x00][0x00]
DATA:  [0x02][seqNo:2B BE][opus_data...]
END:   [0x03][totalFrames:4B BE]
```

### PLAY 命令 (ABD notify, 固件→iPhone)

```
[0x20]  — 触发 Interactive AI
```

## 按钮行为

| 操作          | 引脚 | 行为                        |
| ------------- | ---- | --------------------------- |
| 短按 (< 0.5s) | D4   | PLAY — 发 0x20 给 iPhone    |
| 长按 (≥ 0.5s) | D4   | 录音 — 发 START, 松开发 END |

## LED 状态

| 颜色         | 含义            |
| ------------ | --------------- |
| 蓝色常亮     | 已连接 BLE      |
| 红色常亮     | 未连接 / 录音中 |
| 绿色闪烁     | 充电中          |
| 红绿蓝依次闪 | 启动中          |
