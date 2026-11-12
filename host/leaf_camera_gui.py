#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LeafAI 摄像头识别界面（PC 端）

作用：
    打开电脑摄像头预览 -> 点“拍照识别” -> 把当前画面缩成 128x128 RGB888
    -> 通过串口按 LEAF 协议发给 PSoC E84 开发板 -> 显示板子返回的识别结果。

用法：
    python leaf_camera_gui.py
    （依赖 pyserial / pillow / opencv-python，已装在 D:\\deepseek_work\\pylibs）
"""

import json
import os
import queue
import struct
import sys
import threading
import time
import zlib

import cv2
import numpy as np
from PIL import Image, ImageTk
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

MAGIC = b"LEAF"
HEADER = struct.Struct("<BBBBHHIII")
VERSION = 1
TYPE_IMAGE = 1
TYPE_RESULT = 2
IMAGE_SIZE = 128
IMAGE_BYTES = IMAGE_SIZE * IMAGE_SIZE * 3
BAUD = 1000000
CLASS_NAMES = ["healthy", "multiple_diseases", "rust", "scab"]
CLASS_ZH = {
    "healthy": "健康",
    "multiple_diseases": "多种病害",
    "rust": "锈病",
    "scab": "黑星病",
}


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def to_payload(frame_bgr):
    """BGR 图像 -> 居中裁剪正方形 -> 128x128 RGB888 bytes"""
    h, w = frame_bgr.shape[:2]
    side = min(h, w)
    top = (h - side) // 2
    left = (w - side) // 2
    crop = frame_bgr[top:top + side, left:left + side]
    resized = cv2.resize(crop, (IMAGE_SIZE, IMAGE_SIZE), interpolation=cv2.INTER_AREA)
    rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
    return rgb.tobytes()


def build_frame(payload, seq=1):
    header = HEADER.pack(VERSION, TYPE_IMAGE, 3, 0, IMAGE_SIZE, IMAGE_SIZE,
                         seq, len(payload), crc32(payload))
    return MAGIC + header + payload


def read_exact(ser, n, timeout=10.0):
    buf = b""
    deadline = time.time() + timeout
    while len(buf) < n and time.time() < deadline:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf += chunk
        else:
            time.sleep(0.01)
    return buf


def read_frame(ser, timeout=30.0):
    deadline = time.time() + timeout
    window = b""
    while time.time() < deadline:
        chunk = ser.read(1)
        if not chunk:
            continue
        window = (window + chunk)[-4:]
        if window != MAGIC:
            continue
        header = read_exact(ser, HEADER.size, 5)
        if len(header) != HEADER.size:
            raise TimeoutError("结果帧头不完整")
        version, ftype, ch, _r, w, h, seq, length, crc = HEADER.unpack(header)
        if length > 1024 * 1024:
            raise ValueError("结果长度异常")
        payload = read_exact(ser, length, timeout)
        if len(payload) != length:
            raise TimeoutError("结果负载不完整")
        if crc32(payload) != crc:
            raise ValueError("结果 CRC 校验失败")
        return ftype, payload
    raise TimeoutError("等待板子回包超时")


class LeafCameraApp:
    def __init__(self, root):
        self.root = root
        root.title("LeafAI 叶片病害识别 - PSoC E84")
        root.geometry("980x560")
        root.minsize(900, 520)

        self.cap = None
        self.last_frame = None
        self.busy = False
        self.result_queue = queue.Queue()
        self.photo = None

        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="就绪：点“拍照识别”把当前画面发给开发板")
        self.result_title = tk.StringVar(value="—")
        self.result_detail = tk.StringVar(value="尚未识别")
        self.sensor_var = tk.StringVar(value="温度 / 湿度：—")

        self._build_ui()
        self._refresh_ports()
        self._open_camera()
        self.root.after(30, self._update_preview)
        self.root.after(120, self._poll_results)
        root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ---------- UI ----------
    def _build_ui(self):
        left = ttk.Frame(self.root, padding=10)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.video = tk.Label(left, text="正在打开摄像头…", bg="#202020", fg="white")
        self.video.pack(fill=tk.BOTH, expand=True)

        bar = ttk.Frame(left)
        bar.pack(fill=tk.X, pady=(8, 0))

        ttk.Label(bar, text="串口").pack(side=tk.LEFT)
        self.port_box = ttk.Combobox(bar, textvariable=self.port_var, width=10, state="readonly")
        self.port_box.pack(side=tk.LEFT, padx=(4, 8))
        ttk.Button(bar, text="刷新", width=6, command=self._refresh_ports).pack(side=tk.LEFT)

        self.capture_btn = ttk.Button(bar, text="拍照识别", command=self.capture_and_send)
        self.capture_btn.pack(side=tk.RIGHT)
        ttk.Button(bar, text="选择图片…", command=self.choose_image).pack(side=tk.RIGHT, padx=6)

        right = ttk.Frame(self.root, padding=10, width=300)
        right.pack(side=tk.RIGHT, fill=tk.Y)
        right.pack_propagate(False)

        ttk.Label(right, text="识别结果", font=("Microsoft YaHei UI", 12, "bold")).pack(anchor="w")
        ttk.Label(right, textvariable=self.result_title, font=("Microsoft YaHei UI", 20, "bold"),
                  foreground="#1a7f37").pack(anchor="w", pady=(8, 2))
        ttk.Label(right, textvariable=self.result_detail, justify="left",
                  wraplength=270).pack(anchor="w", pady=(0, 10))
        ttk.Separator(right).pack(fill=tk.X, pady=6)
        ttk.Label(right, textvariable=self.sensor_var, justify="left",
                  wraplength=270).pack(anchor="w")
        ttk.Separator(right).pack(fill=tk.X, pady=6)

        ttk.Label(right, textvariable=self.status_var, justify="left",
                  wraplength=270, foreground="#555").pack(anchor="w", pady=(4, 0))

    # ---------- camera ----------
    def _open_camera(self):
        for idx in (0, 1, 2):
            cap = cv2.VideoCapture(idx, cv2.CAP_DSHOW)
            if cap is not None and cap.isOpened():
                self.cap = cap
                self.status_var.set("摄像头已打开。")
                return
            if cap is not None:
                cap.release()
        self.status_var.set("没找到摄像头，可以用“选择图片…”直接发一张图。")

    def _update_preview(self):
        if self.cap is not None:
            ok, frame = self.cap.read()
            if ok and frame is not None:
                self.last_frame = frame
                self._show_frame(frame)
        self.root.after(30, self._update_preview)

    def _show_frame(self, frame_bgr):
        h, w = frame_bgr.shape[:2]
        scale = min(640 / w, 460 / h, 1.0)
        show = cv2.resize(frame_bgr, (int(w * scale), int(h * scale)))
        img = Image.fromarray(cv2.cvtColor(show, cv2.COLOR_BGR2RGB))
        self.photo = ImageTk.PhotoImage(img)
        self.video.configure(image=self.photo, text="")

    # ---------- actions ----------
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if not ports:
            ports = ["COM7"]
        self.port_box["values"] = ports
        if self.port_var.get() not in ports:
            self.port_var.set("COM7" if "COM7" in ports else ports[0])

    def capture_and_send(self):
        if self.busy:
            return
        if self.last_frame is None:
            messagebox.showwarning("没有画面", "摄像头没有画面，请稍等或改用“选择图片…”")
            return
        self._send_payload(to_payload(self.last_frame))

    def choose_image(self):
        if self.busy:
            return
        path = filedialog.askopenfilename(
            title="选择一张叶片图片",
            filetypes=[("图片", "*.jpg *.jpeg *.png *.bmp"), ("所有文件", "*.*")])
        if not path:
            return
        data = open(path, "rb").read()
        img = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_COLOR)
        if img is None:
            messagebox.showerror("读图失败", "这个文件不是图片或格式不支持。")
            return
        self._show_frame(img)
        self._send_payload(to_payload(img))

    def _send_payload(self, payload):
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("缺少串口", "请先选择串口。")
            return
        self.busy = True
        self.capture_btn.configure(state="disabled")
        self.status_var.set("正在发送图片并等待板子返回…")
        self.result_title.set("…")
        self.result_detail.set("识别中")
        threading.Thread(target=self._worker, args=(port, payload), daemon=True).start()

    def _worker(self, port, payload):
        try:
            ser = serial.Serial(port, BAUD, timeout=0.2)
        except Exception as exc:
            self.result_queue.put(("error", "打开串口失败：%s\n（MobaXterm 等串口工具是否还占着 %s？）" % (exc, port)))
            return
        try:
            time.sleep(0.2)
            ser.reset_input_buffer()
            ser.write(b"leaf_stream 1\r\n")
            ser.flush()

            buf = b""
            deadline = time.time() + 10
            seen = False
            while time.time() < deadline:
                chunk = ser.read(256)
                if chunk:
                    buf += chunk
                    if b"LEAF_READY" in buf:
                        seen = True
                        break
                else:
                    time.sleep(0.02)
            if not seen:
                tail = buf.decode("utf-8", "replace")[-400:]
                if "IMAI_init failed" in tail:
                    self.result_queue.put(("error", "板子模型初始化失败（固件缺算子）。\n" + tail))
                else:
                    self.result_queue.put(("error", "没有等到 LEAF_READY，板子最后输出：\n" + tail))
                return

            ser.write(build_frame(payload, 1))
            ser.flush()

            ftype, data = read_frame(ser, 30)
            if ftype != TYPE_RESULT:
                self.result_queue.put(("error", "收到未知帧类型 %d" % ftype))
                return
            self.result_queue.put(("ok", data.decode("utf-8", "replace")))
        except Exception as exc:
            self.result_queue.put(("error", "通信失败：%s" % exc))
        finally:
            try:
                ser.close()
            except Exception:
                pass

    def _poll_results(self):
        try:
            while True:
                kind, text = self.result_queue.get_nowait()
                self.busy = False
                self.capture_btn.configure(state="normal")
                if kind == "error":
                    self.status_var.set(text)
                    self.result_title.set("失败")
                    self.result_detail.set(text[:300])
                else:
                    self._show_result(text)
        except queue.Empty:
            pass
        self.root.after(120, self._poll_results)

    def _show_result(self, payload):
        try:
            r = json.loads(payload)
        except json.JSONDecodeError:
            self.result_detail.set(payload[:400])
            return
        if r.get("status") != "ok":
            self.status_var.set("板子返回错误：" + payload[:200])
            return
        name = r.get("class_name") or CLASS_NAMES[int(r.get("class_id", 0))]
        zh = CLASS_ZH.get(name, "")
        probs = r.get("probabilities_milli", [])
        lines = []
        if len(probs) == 4:
            lines.append("  ".join("%s %.1f%%" % (CLASS_NAMES[i], probs[i] / 10.0) for i in range(4)))
        lines.append("置信度 %.1f%%    推理 %d ms" % (r.get("confidence_milli", 0) / 10.0, r.get("inference_ms", 0)))
        self.result_title.set("%s（%s）" % (name, zh))
        self.result_detail.set("\n".join(lines))
        t = r.get("temperature_centi", -1)
        h = r.get("humidity_centi", -1)
        if t >= 0 and h >= 0:
            self.sensor_var.set("温度：%.2f ℃\n湿度：%.2f %%" % (t / 100.0, h / 100.0))
        else:
            self.sensor_var.set("温度 / 湿度：传感器无效")
        self.status_var.set("识别完成。可以继续拍照。")

    def _on_close(self):
        if self.cap is not None:
            self.cap.release()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = LeafCameraApp(root)
    root.mainloop()
