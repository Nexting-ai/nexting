#!/usr/bin/env python3
"""
Pinclaw Hardware Automated Test Suite
======================================
Tests all BLE-accessible hardware functions:
  1. BLE scan & connect
  2. Device Info (firmware/hardware version via DIS)
  3. Battery level (BAS)
  4. Heartbeat (interval, counter, battery voltage)
  5. Button press/release detection
  6. Microphone recording (START→DATA→END, packet integrity, audio level)
  7. Speaker write path
  8. Reconnection stability (disconnect + reconnect × 3)

Usage:
  python3 pinclaw_test.py              # Run all tests
  python3 pinclaw_test.py --no-button  # Skip button test (needs physical press)
  python3 pinclaw_test.py --quick      # Skip reconnect + button tests

Requirements:
  pip3 install bleak
"""

import asyncio
import argparse
import struct
import sys
import time
from dataclasses import dataclass, field

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("ERROR: bleak not installed. Run: pip3 install bleak")
    sys.exit(1)

# ── BLE UUIDs ──

# Audio Service
AUDIO_SERVICE_UUID = "12345678-1234-1234-1234-123456789abc"
AUDIO_DATA_UUID    = "12345678-1234-1234-1234-123456789abe"  # notify: audio packets
AUDIO_CMD_UUID     = "12345678-1234-1234-1234-123456789abd"  # write: commands
AUDIO_HB_UUID      = "12345678-1234-1234-1234-123456789abf"  # notify: heartbeat

# Button Service
BUTTON_SERVICE_UUID = "23ba7924-0000-1000-7450-346eac492e92"
BUTTON_CHAR_UUID    = "23ba7925-0000-1000-7450-346eac492e92"

# Speaker Service
SPEAKER_SERVICE_UUID = "cab1ab95-2ea5-4f4d-bb56-874b72cfc984"
SPEAKER_CHAR_UUID    = "cab1ab96-2ea5-4f4d-bb56-874b72cfc984"

# Standard BLE Services
DIS_SERVICE_UUID     = "0000180a-0000-1000-8000-00805f9b34fb"
FW_REVISION_UUID     = "00002a26-0000-1000-8000-00805f9b34fb"
HW_REVISION_UUID     = "00002a27-0000-1000-8000-00805f9b34fb"
BAS_SERVICE_UUID     = "0000180f-0000-1000-8000-00805f9b34fb"
BATTERY_LEVEL_UUID   = "00002a19-0000-1000-8000-00805f9b34fb"

# Commands
CMD_START_REC = bytes([0x01])
CMD_STOP_REC  = bytes([0x00])
CMD_PLAY      = bytes([0x20])

# Packet types
PKT_START     = 0x01
PKT_DATA      = 0x02
PKT_END       = 0x03
PKT_HEARTBEAT = 0x04

# Button states
BTN_PRESS   = 4
BTN_RELEASE = 5

# ── Colors ──
GREEN  = "\033[0;32m"
RED    = "\033[0;31m"
YELLOW = "\033[1;33m"
BLUE   = "\033[0;34m"
NC     = "\033[0m"


@dataclass
class TestResults:
    passed: int = 0
    failed: int = 0
    warnings: int = 0
    details: list = field(default_factory=list)

    def ok(self, msg):
        self.passed += 1
        self.details.append(("PASS", msg))
        print(f"  {GREEN}✓ PASS{NC}: {msg}")

    def fail(self, msg):
        self.failed += 1
        self.details.append(("FAIL", msg))
        print(f"  {RED}✗ FAIL{NC}: {msg}")

    def warn(self, msg):
        self.warnings += 1
        self.details.append(("WARN", msg))
        print(f"  {YELLOW}⚠ WARN{NC}: {msg}")

    def info(self, msg):
        print(f"  {BLUE}→{NC} {msg}")


results = TestResults()


async def test_scan():
    """Test 1: BLE scan — find Pinclaw device"""
    print(f"\n{'━'*40}")
    print(f"Test 1: BLE 扫描")
    print(f"{'━'*40}")

    results.info("扫描 Pinclaw 设备 (10s)...")
    devices = await BleakScanner.discover(
        timeout=10.0,
        return_adv=True,
        service_uuids=[AUDIO_SERVICE_UUID],
    )

    if not devices:
        results.fail("未发现 Pinclaw 设备")
        return None

    addr, (device, adv) = next(iter(devices.items()))
    name = device.name or "Unknown"
    results.ok(f"发现设备: {name} [{addr}] RSSI={adv.rssi} dBm")

    if adv.rssi < -80:
        results.warn(f"信号较弱 (RSSI={adv.rssi})，建议靠近设备")

    return device


async def test_device_info(client: BleakClient):
    """Test 2: Read Device Information Service"""
    print(f"\n{'━'*40}")
    print(f"Test 2: 设备信息 (DIS)")
    print(f"{'━'*40}")

    # Firmware version
    try:
        fw_bytes = await client.read_gatt_char(FW_REVISION_UUID)
        fw_ver = fw_bytes.decode("utf-8").strip('\x00').strip()
        if fw_ver:
            results.ok(f"固件版本: {fw_ver}")
            if fw_ver != "2.1.7":
                results.warn(f"期望 2.1.7，实际 {fw_ver}")
        else:
            results.fail("固件版本为空")
    except Exception as e:
        results.fail(f"读取固件版本失败: {e}")

    # Hardware version
    try:
        hw_bytes = await client.read_gatt_char(HW_REVISION_UUID)
        hw_ver = hw_bytes.decode("utf-8").strip('\x00').strip()
        if hw_ver:
            results.ok(f"硬件版本: {hw_ver}")
        else:
            results.warn("硬件版本为空")
    except Exception as e:
        results.warn(f"读取硬件版本失败: {e}")


async def test_battery(client: BleakClient):
    """Test 3: Read Battery Service"""
    print(f"\n{'━'*40}")
    print(f"Test 3: 电池电量 (BAS)")
    print(f"{'━'*40}")

    try:
        batt_bytes = await client.read_gatt_char(BATTERY_LEVEL_UUID)
        batt_pct = batt_bytes[0]
        results.ok(f"电池电量: {batt_pct}%")

        if batt_pct < 10:
            results.warn(f"电量过低 ({batt_pct}%)，建议充电后测试")
        elif batt_pct == 0:
            results.fail("电池电量为 0%，可能电池未连接")
    except Exception as e:
        results.fail(f"读取电池电量失败: {e}")


async def test_heartbeat(client: BleakClient):
    """Test 4: Heartbeat — verify interval, counter, battery voltage"""
    print(f"\n{'━'*40}")
    print(f"Test 4: 心跳检测")
    print(f"{'━'*40}")

    heartbeats = []
    hb_event = asyncio.Event()

    def on_heartbeat(sender, data):
        ts = time.time()
        if len(data) >= 6 and data[0] == PKT_HEARTBEAT:
            counter = (data[1] << 8) | data[2]
            flags = data[3]
            batt_mv = (data[4] << 8) | data[5]
            heartbeats.append({
                "time": ts,
                "counter": counter,
                "flags": flags,
                "batt_mv": batt_mv,
            })
            recording = bool(flags & 0x01)
            charging = bool(flags & 0x02)
            results.info(
                f"心跳 #{counter}: 电压={batt_mv}mV "
                f"录音={'是' if recording else '否'} "
                f"充电={'是' if charging else '否'}"
            )
            if len(heartbeats) >= 2:
                hb_event.set()

    results.info("订阅心跳通知，等待 2 个心跳包 (最多 25s)...")
    try:
        await client.start_notify(AUDIO_HB_UUID, on_heartbeat)
    except Exception as e:
        results.fail(f"订阅心跳失败: {e}")
        return

    try:
        await asyncio.wait_for(hb_event.wait(), timeout=25.0)
    except asyncio.TimeoutError:
        if len(heartbeats) == 1:
            results.warn("只收到 1 个心跳，无法验证间隔")
        elif len(heartbeats) == 0:
            results.fail("25 秒内未收到心跳包")
            return

    await client.stop_notify(AUDIO_HB_UUID)

    if len(heartbeats) >= 2:
        interval = heartbeats[1]["time"] - heartbeats[0]["time"]
        results.ok(f"心跳间隔: {interval:.1f}s（期望 ~10s）")
        if not (5 < interval < 20):
            results.warn(f"心跳间隔异常: {interval:.1f}s")

        c1, c2 = heartbeats[0]["counter"], heartbeats[1]["counter"]
        if c2 == c1 + 1:
            results.ok(f"心跳计数器递增正常: {c1} → {c2}")
        else:
            results.warn(f"心跳计数器不连续: {c1} → {c2}")

    if heartbeats:
        batt_mv = heartbeats[-1]["batt_mv"]
        if 2500 < batt_mv < 4300:
            results.ok(f"电池电压正常: {batt_mv}mV")
        elif batt_mv == 0:
            results.warn("电池电压为 0mV，可能未接电池")
        else:
            results.warn(f"电池电压异常: {batt_mv}mV（正常范围 2500-4200）")


async def test_button(client: BleakClient):
    """Test 5: Button press/release detection"""
    print(f"\n{'━'*40}")
    print(f"Test 5: 按钮检测")
    print(f"{'━'*40}")

    button_events = []
    btn_event = asyncio.Event()

    def on_button(sender, data):
        if len(data) >= 1:
            state = data[0]
            state_name = {BTN_PRESS: "PRESS", BTN_RELEASE: "RELEASE"}.get(state, f"UNKNOWN({state})")
            button_events.append(state)
            results.info(f"按钮事件: {state_name}")
            # Wait for press + release pair
            if len(button_events) >= 2:
                btn_event.set()

    try:
        await client.start_notify(BUTTON_CHAR_UUID, on_button)
    except Exception as e:
        results.fail(f"订阅按钮通知失败: {e}")
        return

    print(f"\n  >>> 请按一下设备按钮（按下然后松开）<<<")
    print(f"  >>> 等待 15 秒... <<<\n")

    try:
        await asyncio.wait_for(btn_event.wait(), timeout=15.0)
    except asyncio.TimeoutError:
        pass

    await client.stop_notify(BUTTON_CHAR_UUID)

    if len(button_events) == 0:
        results.fail("未收到任何按钮事件")
    elif len(button_events) == 1:
        results.warn(f"只收到 1 个事件（期望 PRESS + RELEASE 一对）")
    else:
        has_press = BTN_PRESS in button_events
        has_release = BTN_RELEASE in button_events
        if has_press and has_release:
            results.ok("按钮 PRESS + RELEASE 事件正常")
        else:
            results.fail(f"按钮事件不完整: {button_events}")


async def test_microphone(client: BleakClient):
    """Test 6: Microphone recording — trigger via BLE, validate audio packets"""
    print(f"\n{'━'*40}")
    print(f"Test 6: 麦克风录音测试")
    print(f"{'━'*40}")

    audio_packets = []
    got_start = asyncio.Event()
    got_end = asyncio.Event()
    seq_numbers = []

    def on_audio(sender, data):
        if not data:
            return
        pkt_type = data[0]

        if pkt_type == PKT_START:
            codec = data[1] if len(data) > 1 else 0
            codec_name = {0x14: "Opus", 0x03: "ADPCM"}.get(codec, f"0x{codec:02X}")
            results.info(f"START 包: codec={codec_name}")
            audio_packets.append(("START", data))
            got_start.set()

        elif pkt_type == PKT_DATA:
            if len(data) >= 3:
                seq = (data[1] << 8) | data[2]
                payload_size = len(data) - 3
                seq_numbers.append(seq)
                audio_packets.append(("DATA", data))
                if len(audio_packets) % 20 == 0:
                    results.info(f"已收 {len(audio_packets)} 包, 最新 seq={seq}, 负载={payload_size}B")

        elif pkt_type == PKT_END:
            if len(data) >= 5:
                total = struct.unpack(">I", data[1:5])[0]
            else:
                total = 0
            results.info(f"END 包: 总帧数={total}")
            audio_packets.append(("END", data))
            got_end.set()

    try:
        await client.start_notify(AUDIO_DATA_UUID, on_audio)
    except Exception as e:
        results.fail(f"订阅音频通知失败: {e}")
        return

    # Trigger recording via BLE command
    results.info("发送 START_REC 命令...")
    try:
        await client.write_gatt_char(AUDIO_CMD_UUID, CMD_START_REC, response=True)
    except Exception as e:
        results.fail(f"发送录音命令失败: {e}")
        await client.stop_notify(AUDIO_DATA_UUID)
        return

    # Wait for START packet
    try:
        await asyncio.wait_for(got_start.wait(), timeout=3.0)
    except asyncio.TimeoutError:
        results.fail("3 秒内未收到 START 包")
        await client.stop_notify(AUDIO_DATA_UUID)
        return

    results.ok("收到 START 包")

    # Record for 3 seconds
    results.info("录音 3 秒...")
    await asyncio.sleep(3.0)

    # Stop recording
    results.info("发送 STOP_REC 命令...")
    try:
        await client.write_gatt_char(AUDIO_CMD_UUID, CMD_STOP_REC, response=True)
    except Exception as e:
        results.warn(f"发送停止命令失败: {e}")

    # Wait for END packet
    try:
        await asyncio.wait_for(got_end.wait(), timeout=3.0)
    except asyncio.TimeoutError:
        results.warn("未收到 END 包（可能固件未响应停止命令）")

    await client.stop_notify(AUDIO_DATA_UUID)

    # ── Validate results ──

    data_count = sum(1 for t, _ in audio_packets if t == "DATA")
    has_start = any(t == "START" for t, _ in audio_packets)
    has_end = any(t == "END" for t, _ in audio_packets)

    # 1. Packet sequence: START → DATA × N → END
    if has_start and has_end and data_count > 0:
        results.ok(f"录音包序列完整: START → {data_count} DATA → END")
    elif has_start and data_count > 0 and not has_end:
        results.warn(f"缺少 END 包（收到 {data_count} 个 DATA 包）")
    else:
        results.fail(f"录音包序列异常: START={has_start} DATA={data_count} END={has_end}")

    # 2. Sequence number continuity
    if len(seq_numbers) >= 2:
        gaps = []
        for i in range(1, len(seq_numbers)):
            expected = seq_numbers[i - 1] + 1
            if seq_numbers[i] != expected:
                gaps.append((seq_numbers[i - 1], seq_numbers[i]))

        if not gaps:
            results.ok(f"序列号连续: 0 → {seq_numbers[-1]}，无丢包")
        else:
            results.fail(f"序列号有 {len(gaps)} 处间断: {gaps[:5]}")

    # 3. Data packet sizes (Opus frames should be > 10 bytes)
    data_sizes = [len(d) - 3 for t, d in audio_packets if t == "DATA"]
    if data_sizes:
        avg_size = sum(data_sizes) / len(data_sizes)
        min_size = min(data_sizes)
        max_size = max(data_sizes)
        results.info(f"音频负载: 平均={avg_size:.0f}B, 最小={min_size}B, 最大={max_size}B")

        if avg_size > 10:
            results.ok(f"音频数据非空（平均 {avg_size:.0f}B/帧）— 麦克风工作正常")
        else:
            results.fail(f"音频数据过小（平均 {avg_size:.0f}B）— 麦克风可能有问题")

        # Check for silent/constant data (all same size = suspicious for silence)
        if min_size == max_size and len(data_sizes) > 10:
            results.warn("所有帧大小完全相同，可能是静音数据")
    else:
        results.fail("没有收到任何音频数据帧")

    # 4. Frame rate (should be ~50 fps for 20ms Opus frames)
    if data_count > 0 and has_start:
        expected_fps = data_count / 3.0  # 3 seconds recording
        results.info(f"帧率: ~{expected_fps:.0f} fps（期望 ~50 fps）")
        if 30 < expected_fps < 80:
            results.ok(f"帧率正常: {expected_fps:.0f} fps")
        else:
            results.warn(f"帧率异常: {expected_fps:.0f} fps")


async def test_speaker(client: BleakClient):
    """Test 7: Speaker write path"""
    print(f"\n{'━'*40}")
    print(f"Test 7: 扬声器写入通路")
    print(f"{'━'*40}")

    # Try writing a small haptic buzz command to speaker characteristic
    try:
        # Write a short haptic command (duration in ms, 16-bit LE)
        haptic_cmd = struct.pack("<H", 100)  # 100ms buzz
        await client.write_gatt_char(SPEAKER_CHAR_UUID, haptic_cmd, response=False)
        results.ok("扬声器 characteristic 写入成功（100ms 振动指令）")
        results.info("如果听到短促蜂鸣/振动，说明扬声器硬件正常")
    except Exception as e:
        results.warn(f"扬声器写入失败: {e}（可能扬声器硬件不可用）")


async def test_mtu(client: BleakClient):
    """Check MTU size"""
    mtu = client.mtu_size
    if mtu >= 185:
        results.ok(f"MTU = {mtu}（足够传输音频包）")
    else:
        results.warn(f"MTU = {mtu}（较小，可能影响音频传输效率）")


async def test_reconnect(client: BleakClient, device):
    """Test 8: Disconnect and reconnect stability"""
    print(f"\n{'━'*40}")
    print(f"Test 8: 重连稳定性 (3 次)")
    print(f"{'━'*40}")

    success = 0
    for i in range(3):
        results.info(f"第 {i+1}/3 次: 断开...")
        try:
            await client.disconnect()
            await asyncio.sleep(2)

            results.info(f"第 {i+1}/3 次: 重连...")
            await client.connect()
            if client.is_connected:
                success += 1
                results.info(f"第 {i+1}/3 次: 连接成功")
            else:
                results.info(f"第 {i+1}/3 次: 连接失败")
            await asyncio.sleep(1)
        except Exception as e:
            results.info(f"第 {i+1}/3 次: 异常 — {e}")

    if success == 3:
        results.ok("3 次重连全部成功")
    elif success > 0:
        results.warn(f"重连 {success}/3 次成功")
    else:
        results.fail("3 次重连全部失败")


async def main():
    parser = argparse.ArgumentParser(description="Pinclaw Hardware Test Suite")
    parser.add_argument("--no-button", action="store_true", help="跳过按钮测试（需要手动按）")
    parser.add_argument("--quick", action="store_true", help="快速模式：跳过按钮和重连测试")
    parser.add_argument("--device-num", type=int, help="设备编号（仅显示用）")
    args = parser.parse_args()

    print("")
    print("=" * 48)
    print("  Pinclaw 硬件自动化测试")
    print(f"  {time.strftime('%Y-%m-%d %H:%M')}")
    if args.device_num:
        print(f"  设备: #{args.device_num}")
    print("=" * 48)

    # Test 1: Scan
    device = await test_scan()
    if device is None:
        print_summary(args.device_num)
        return

    # Connect
    print(f"\n{BLUE}→{NC} 连接设备...")
    try:
        client = BleakClient(device)
        await client.connect()
    except Exception as e:
        results.fail(f"连接失败: {e}")
        print_summary(args.device_num)
        return

    if not client.is_connected:
        results.fail("连接后状态异常")
        print_summary(args.device_num)
        return

    results.ok(f"BLE 连接成功, MTU={client.mtu_size}")

    try:
        # Test 2: Device Info
        await test_device_info(client)

        # Test 3: Battery
        await test_battery(client)

        # MTU check
        await test_mtu(client)

        # Test 4: Heartbeat
        await test_heartbeat(client)

        # Test 5: Button (optional)
        if not args.no_button and not args.quick:
            await test_button(client)
        else:
            print(f"\n{'━'*40}")
            print(f"Test 5: 按钮检测 — 已跳过")
            print(f"{'━'*40}")

        # Test 6: Microphone
        await test_microphone(client)

        # Test 7: Speaker
        await test_speaker(client)

        # Test 8: Reconnect (optional)
        if not args.quick:
            await test_reconnect(client, device)
        else:
            print(f"\n{'━'*40}")
            print(f"Test 8: 重连稳定性 — 已跳过")
            print(f"{'━'*40}")

    except Exception as e:
        results.fail(f"测试过程异常: {e}")
    finally:
        if client.is_connected:
            await client.disconnect()

    print_summary(args.device_num)


def print_summary(device_num=None):
    print("")
    print("=" * 48)
    if device_num:
        print(f"  设备 #{device_num} 测试结果")
    else:
        print(f"  测试结果")
    print("=" * 48)
    print(f"  {GREEN}通过: {results.passed}{NC}")
    if results.warnings:
        print(f"  {YELLOW}警告: {results.warnings}{NC}")
    if results.failed:
        print(f"  {RED}失败: {results.failed}{NC}")

    print("")
    if results.failed == 0:
        print(f"  {GREEN}设备可以发货{NC}")
    else:
        print(f"  {RED}有 {results.failed} 项失败，请检查后再发货{NC}")
    print("")

    sys.exit(results.failed)


if __name__ == "__main__":
    asyncio.run(main())
