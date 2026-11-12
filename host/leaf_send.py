#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
leaf_send.py —— 给 LeafAI_INT8_E84 发送一张 128x128 RGB888 图片并打印识别结果

用法（先关掉串口助手，避免占用 COM 口）：
    pip install pyserial pillow
    python leaf_send.py --port COM7 --image apple.jpg

它会：
    1. 以 1000000 8N1 打开串口
    2. 发送 MSH 命令 leaf_stream 1
    3. 等到板子打印 LEAF_READY
    4. 按协议发送 LEAF + 20 字节头 + 49152 字节 RGB
    5. 读取板子返回的 JSON 并打印

如果板子已经停在 LEAF_READY（你先在串口里敲了 leaf_stream 1），
用 --no-command 直接发图。
"""

import argparse
import json
import struct
import sys
import time
import zlib

MAGIC = b"LEAF"
# 20 字节头：版本, 类型, 通道, 保留, 宽, 高, 序号, 负载长度, CRC32（全部小端）
HEADER = struct.Struct("<BBBBHHIII")

VERSION = 1
TYPE_IMAGE = 1
TYPE_RESULT = 2
TYPE_ACK = 3

IMAGE_SIZE = 128
IMAGE_BYTES = IMAGE_SIZE * IMAGE_SIZE * 3

CLASS_NAMES = ["healthy", "multiple_diseases", "rust", "scab"]


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def build_image_frame(payload, sequence=1):
    if len(payload) != IMAGE_BYTES:
        raise ValueError("payload must be %d bytes, got %d" % (IMAGE_BYTES, len(payload)))
    header = HEADER.pack(
        VERSION, TYPE_IMAGE, 3, 0,
        IMAGE_SIZE, IMAGE_SIZE,
        sequence, len(payload), crc32(payload),
    )
    return MAGIC + header + payload


def load_image(path):
    try:
        from PIL import Image
    except ImportError:
        sys.exit("缺少 Pillow，请先执行：pip install pillow")

    image = Image.open(path).convert("RGB")
    width, height = image.size
    side = min(width, height)
    left = (width - side) // 2
    top = (height - side) // 2
    image = image.crop((left, top, left + side, top + side))
    image = image.resize((IMAGE_SIZE, IMAGE_SIZE), Image.BILINEAR)
    return image.tobytes()


def read_frame(ser, timeout):
    """在字节流里找 LEAF 魔数，然后读 20 字节头和负载。"""
    deadline = time.time() + timeout
    window = b""
    while time.time() < deadline:
        chunk = ser.read(1)
        if not chunk:
            continue
        window = (window + chunk)[-4:]
        if window != MAGIC:
            continue

        header = ser.read(HEADER.size)
        if len(header) != HEADER.size:
            raise TimeoutError("收到魔数后头部不完整")

        version, frame_type, channels, _reserved, width, height, seq, length, expected_crc = HEADER.unpack(header)
        if length > 1024 * 1024:
            raise ValueError("负载长度异常：%d" % length)

        payload = ser.read(length)
        if len(payload) != length:
            raise TimeoutError("负载不完整：%d/%d" % (len(payload), length))

        actual_crc = crc32(payload)
        if actual_crc != expected_crc:
            print("[警告] CRC 不匹配：收到 %08X，计算 %08X" % (expected_crc, actual_crc), file=sys.stderr)

        return {
            "version": version,
            "type": frame_type,
            "channels": channels,
            "width": width,
            "height": height,
            "seq": seq,
            "payload": payload,
        }
    raise TimeoutError("等待板子回包超时")


def wait_for_text(ser, needle, timeout):
    """读串口日志，直到出现 needle；返回读到的文本。"""
    deadline = time.time() + timeout
    buffer = b""
    while time.time() < deadline:
        chunk = ser.read(1)
        if not chunk:
            continue
        buffer += chunk
        if needle in buffer:
            return buffer.decode("utf-8", "replace")
        if len(buffer) > 8192:
            buffer = buffer[-1024:]
    return buffer.decode("utf-8", "replace")


def print_result(frame):
    payload = frame["payload"].decode("utf-8", "replace")
    try:
        result = json.loads(payload)
    except json.JSONDecodeError:
        print("板子返回（不是合法 JSON）：")
        print(payload)
        return

    if result.get("status") != "ok":
        print("板子返回错误：", payload)
        return

    class_id = int(result.get("class_id", -1))
    name = result.get("class_name") or (CLASS_NAMES[class_id] if 0 <= class_id < 4 else "?")
    confidence = result.get("confidence_milli", 0) / 10.0
    inference_ms = result.get("inference_ms", 0)
    probs = result.get("probabilities_milli", [])
    temperature = result.get("temperature_centi", -1)
    humidity = result.get("humidity_centi", -1)

    print("=" * 52)
    print("类别      : %s (class_id=%d)" % (name, class_id))
    print("置信度    : %.1f%%" % confidence)
    if len(probs) == 4:
        print("四类概率  : " + "  ".join(
            "%s=%.1f%%" % (CLASS_NAMES[i], probs[i] / 10.0) for i in range(4)))
    print("推理耗时  : %d ms" % inference_ms)
    if temperature >= 0:
        print("温度      : %.2f C" % (temperature / 100.0))
    else:
        print("温度      : 传感器无效")
    if humidity >= 0:
        print("湿度      : %.2f %%" % (humidity / 100.0))
    else:
        print("湿度      : 传感器无效")
    print("环境告警  : %s" % ("是" if result.get("environment_alarm") else "否"))
    print("=" * 52)
    print("原始 JSON :", payload)


def selftest():
    payload = bytes((i * 7) & 0xFF for i in range(IMAGE_BYTES))
    frame = build_image_frame(payload, 42)
    assert len(frame) == 24 + IMAGE_BYTES, len(frame)

    class FakeSerial:
        def __init__(self, data):
            self.data = data
            self.pos = 0

        def read(self, size=1):
            out = self.data[self.pos:self.pos + size]
            self.pos += len(out)
            return out

    # 先塞一些日志，再塞一帧结果，验证解析器能在噪声中找到魔数
    result = b'{"status":"ok","seq":42,"class_id":2,"class_name":"rust","confidence_milli":932,"probabilities_milli":[12,3,932,53],"inference_ms":41,"temperature_centi":2530,"humidity_centi":5610,"environment_alarm":false}'
    response = b"leaf: IMAI_compute end rc=0\r\n" + MAGIC + HEADER.pack(
        VERSION, TYPE_RESULT, 0, 0, 0, 0, 42, len(result), crc32(result)
    ) + result

    parsed = read_frame(FakeSerial(response), timeout=5)
    assert parsed["type"] == TYPE_RESULT
    assert parsed["payload"] == result
    print_result(parsed)
    print("自检通过：帧长度、魔数查找、头部解析、CRC 校验都正常。")


def main():
    parser = argparse.ArgumentParser(description="向 LeafAI_INT8_E84 发送图片并读取识别结果")
    parser.add_argument("--port", help="串口号，例如 COM7")
    parser.add_argument("--image", help="图片路径，支持 jpg/png/bmp")
    parser.add_argument("--baud", type=int, default=1000000, help="波特率，默认 1000000")
    parser.add_argument("--seq", type=int, default=1, help="帧序号，默认 1")
    parser.add_argument("--timeout", type=float, default=30.0, help="等回包秒数，默认 30")
    parser.add_argument("--no-command", action="store_true",
                        help="不再发送 leaf_stream 命令（板子已在等待收图时用）")
    parser.add_argument("--verbose", action="store_true", help="打印板子发来的文本日志")
    parser.add_argument("--selftest", action="store_true", help="不接硬件，自检协议代码")
    args = parser.parse_args()

    if args.selftest:
        selftest()
        return 0

    if not args.port or not args.image:
        parser.error("需要 --port 和 --image（或使用 --selftest）")

    try:
        import serial
    except ImportError:
        sys.exit("缺少 pyserial，请先执行：pip install pyserial")

    payload = load_image(args.image)

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as exc:
        sys.exit("打开串口失败：%s" % exc)

    try:
        ser.reset_input_buffer()

        if not args.no_command:
            print("发送命令：leaf_stream 1")
            ser.write(b"leaf_stream 1\r\n")
            ser.flush()
            text = wait_for_text(ser, b"LEAF_READY", 10)
            if args.verbose:
                print(text, end="")
            if b"LEAF_READY" not in text.encode("utf-8", "replace"):
                print("没有等到 LEAF_READY，板子最后输出：")
                print(text)
                print("请检查：波特率 1000000、板子是否在 msh、是否已烧 LeafAI 固件。")
                return 1
        else:
            print("直接发图（板子应已在 LEAF_READY 状态）")

        frame = build_image_frame(payload, args.seq)
        print("发送图片：%d 字节（128x128 RGB888），CRC=%08X" % (len(payload), crc32(payload)))
        ser.write(frame)
        ser.flush()

        result = read_frame(ser, args.timeout)
        if result["type"] == TYPE_RESULT:
            print_result(result)
        elif result["type"] == TYPE_ACK:
            print("收到 ACK：", result["payload"])
        else:
            print("收到未知帧类型 %d：%r" % (result["type"], result["payload"]))

        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
