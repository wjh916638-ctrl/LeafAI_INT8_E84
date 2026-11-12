# LeafAI_INT8_E84

基于 **Infineon PSoC™ Edge E84** 的叶片病害智能识别系统。电脑端摄像头拍照，通过 USB 虚拟串口把 128×128 RGB 图像发给开发板，板端在 Cortex-M55 上用 **ExecuTorch + INT8 MobileNetV2** 完成本地推理，结果以 JSON 回传，并同步显示在板载 LCD 上；同时通过 I2C 采集环境温湿度一并展示。

---

## 1. 功能特性

- **四类叶片识别**：`healthy`（健康）、`multiple_diseases`（多种病害）、`rust`（锈病）、`scab`（黑星病）
- **端侧推理**：INT8 量化模型，离线运行，不依赖云端
- **双核协同**：CM33 负责启动与系统初始化，释放 CM55；CM55 运行 RT-Thread 应用
- **自定义串口协议**：魔数 + 20 字节帧头 + RGB888 负载 + CRC32，带命令/应答握手
- **双通道展示**：串口 JSON + 板载 LCD（LVGL 9.2）
- **环境监测**：AHT20 温湿度（I2C1），随识别结果一起返回
- **上位机**：Python 摄像头界面，支持拍照识别和本地图片发送

---

## 2. 硬件与软件环境

| 项目 | 要求 |
|---|---|
| 开发板 | Infineon PSoC Edge E84（PSE846GPS2DBZC4A） |
| 调试器 | 板载 KitProg3（CMSIS-DAP，SWD） |
| IDE | RT-Thread Studio（含 RT-Thread 5.0.2） |
| BSP | `PSOC_E84-EDGI-TALK` **1.4.0** |
| 工具链 | GNU Arm Embedded **13.3**（`arm-none-eabi-gcc`） |
| 上位机 | Windows 10/11 + Python 3.10+（pyserial / pillow / opencv-python / tkinter） |

> 说明：工程不是“克隆即可编译”的独立仓库。`rt-thread/`、`libraries/`、`libs/`、`tools/` 四个目录是指向本机 BSP 的**目录联接**，有意不纳入版本库，克隆后需按第 3 节生成。

---

## 3. 快速开始

### 3.1 安装依赖

1. 安装 RT-Thread Studio；
2. 在 **SDK Manager** 中安装：
   - `Board Support Packages → Infineon → PSOC_E84-EDGI-TALK 1.4.0`
   - `Toolchain Support Packages → ARM → GNU Tools for ARM Embedded Processors 13.3`
3. 连接开发板，确认设备管理器出现 `KitProg3 ... (COMx)`。

### 3.2 获取代码并建立 BSP 链接

```powershell
git clone https://github.com/<your-name>/LeafAI_INT8_E84.git
cd LeafAI_INT8_E84

# 根据本机 BSP 安装位置修改 mklinks.bat 第 3 行的 BSP 路径，然后运行
mklinks.bat
```

`mklinks.bat` 会创建四个目录联接：

```text
rt-thread   -> BSP 的 rt-thread
libraries   -> BSP 的 libraries
libs        -> BSP 的 libs
tools       -> BSP 的 tools
```

### 3.3 导入并编译

1. Studio：**文件 → 导入 → 现有项目到工作空间**，选择本目录；
2. 右键工程 → **构建项目**（首次会全量编译，约 1～3 分钟）；
3. 产物：

```text
Debug/rtthread.elf
Debug/rtthread.hex
Debug/rtthread.map
```

### 3.4 烧录

**方式一：Studio 下载**

确认调试器为 `KitProg3`、接口 `SWD`，点“下载程序”。

**方式二：命令行 OpenOCD**（不依赖 Studio 配置）

```powershell
$OCD  = 'D:\RT-ThreadStudio\repo\Extract\Debugger_Support_Packages\Infineon\OpenOCD-Infineon\2.0.0\bin\openocd.exe'
$ROOT = 'D:\RT-ThreadStudio\repo\Extract\Debugger_Support_Packages\Infineon\OpenOCD-Infineon\2.0.0'
$HEX  = 'D:/RT-ThreadStudio/workspace/LeafAI_INT8_E84/Debug/rtthread.hex'

& $OCD -s "$ROOT\scripts" -s "$ROOT\flm\cypress\cat1d" -f "interface/kitprog3.cfg" -f "target/infineon/pse84xgxs2.cfg" `
  -c "set QSPI_FLASHLOADER {$ROOT/flm/cypress/cat1d/PSE84_SMIF.FLM}" -c "transport select swd" `
  -c "cat1d.cm55 configure -rtos auto" -c "init" -c "reset halt" `
  -c "flash write_image erase {$HEX};" -c "targets cat1d.cm33" -c "reset run" -c "shutdown"
```

> 需要 OpenOCD-Infineon **2.0.0**（1.0.0 没有 `target/infineon/pse84xgxs2.cfg`）。

`prebuilt/rtthread.hex` 是已验证的固件，可直接烧录，用于快速验证硬件。

---

## 4. 串口使用

| 参数 | 值 |
|---|---|
| 波特率 | **1 000 000** |
| 数据位 | 8 |
| 校验 | 无 |
| 停止位 | 1 |
| 流控 | 无 |

复位后可见 RT-Thread 启动日志与 `msh >` 提示符。

### 4.1 命令

```text
leaf_status      # 查看温湿度与识别状态
leaf_stream 1    # 进入收图模式，等待一帧 128×128 图像
```

`leaf_status` 输出示例：

```text
LEAF_STATUS sensor_valid=1 temp_centi=2759 humidity_centi=3842 result_valid=0 ...
```

`temp_centi` 单位为 0.01 ℃，`humidity_centi` 单位为 0.01 %。

`leaf_stream 1` 输出：

```text
leaf: initializing model
LEAF_READY 1 49152
```

随后 15 秒内等待图像，超时打印 `leaf: image header timeout` 并返回命令行。

### 4.2 上位机发图

```powershell
pip install pyserial pillow
python host/leaf_send.py --port COM7 --image samples/healthy_1.png
```

识别结果示例：

```text
类别      : healthy (class_id=0)
置信度    : 90.5%
四类概率  : healthy=90.5%  multiple_diseases=1.4%  rust=0.3%  scab=7.8%
推理耗时  : 5567 ms
温度      : 28.55 C
湿度      : 32.05 %
```

摄像头界面：`python host/leaf_camera_gui.py`（需 `opencv-python`），支持实时预览、拍照识别、选择本地图片。

---

## 5. 通信协议

### 5.1 帧格式（PC → 开发板）

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 4 | 魔数 | `"LEAF"` |
| 4 | 1 | 版本 | 1 |
| 5 | 1 | 类型 | 1=图像，2=结果，3=ACK |
| 6 | 1 | 通道数 | 3（RGB） |
| 7 | 1 | 保留 | 0 |
| 8 | 2 | 宽度 | 128（小端） |
| 10 | 2 | 高度 | 128（小端） |
| 12 | 4 | 序号 | 匹配请求与响应 |
| 16 | 4 | 负载长度 | 49152 |
| 20 | 4 | CRC32 | 负载校验 |
| 24 | 49152 | 像素 | RGB888，逐行存储 |

整帧 49176 字节。板端回包使用同样格式，类型为 2，负载为 UTF-8 JSON。

### 5.2 返回 JSON 字段

| 字段 | 含义 |
|---|---|
| `status` | `ok` / 错误原因 |
| `class_id` / `class_name` | 类别编号与名称 |
| `confidence_milli` | 置信度（千分之一） |
| `probabilities_milli` | 四类概率数组（千分之一） |
| `inference_ms` | 推理耗时（ms） |
| `temperature_centi` / `humidity_centi` | 温度/湿度（百分之一） |
| `environment_alarm` | 温湿度是否超阈值 |

---

## 6. 目录结构

```text
LeafAI_INT8_E84/
├─ applications/
│  ├─ main.c                 # 应用入口：切换控制台波特率、启动 UI
│  ├─ leaf_ai/               # 业务核心：协议、推理、UI、模型
│  │  ├─ leaf_app.c          # 收图、CRC、预处理、推理调度、命令行
│  │  ├─ leaf_ui.c           # LVGL 界面
│  │  ├─ model/              # INT8 模型（model.c / model.h）
│  │  ├─ cmsis_nn_v7/        # CMSIS-NN 算子源码
│  │  ├─ u55_driver/         # Ethos-U55 驱动副本
│  │  └─ executorch_runtime/ # ExecuTorch 包装层
│  └─ lvgl_port/             # LVGL 与 RT-Thread 适配
├─ board/                    # 板级初始化与链接脚本
│  └─ linker_scripts/link.ld # 内存布局 + 算子对象 INPUT
├─ middleware/               # ExecuTorch 运行时、算子库、ML 中间件
├─ packages/                 # AHT10、CPU usage
├─ prebuilt/                 # 已验证固件（hex/elf/map）
├─ host/                     # 上位机脚本（send / camera GUI）
├─ samples/                  # 示例叶片图片
└─ README.md
```

> 仓库不包含 `Debug/`、`build/` 以及 BSP 目录联接（`rt-thread/`、`libraries/`、`libs/`、`tools/`）。

---

## 7. 可移植性说明

### 7.1 链接脚本

`board/linker_scripts/link.ld` 中的 20 个 `INPUT()` 使用**相对路径**：

```ld
INPUT("../middleware/ml-executorch/.../linkobj/RegisterCodegenUnboxedKernelsEverything.cpp.obj")
```

相对的是链接工作目录 `Debug/`，因此工程放在任何路径下都能正常链接。

> 该相对路径针对 **Studio 构建**（链接器在 `Debug/` 下运行）。若改用 SCons 在工程根目录构建，请把 `INPUT()` 改成绝对路径，或在构建脚本中把工作目录切到 `Debug/`。 

> 这些对象负责 ExecuTorch 算子的静态注册。若被删除或链接时未包含，模型加载会报
> `Missing operator: cortex_m::...` / `IMAI_init failed (-2)`。

### 7.2 构建脚本

`SConstruct` / `rtconfig.py` 会自动探测 BSP 与工具链，也支持环境变量覆盖：

| 变量 | 作用 |
|---|---|
| `LEAF_BSP_ROOT` | 指定 `PSOC_E84-EDGI-TALK` BSP 根目录 |
| `RTT_EXEC_PATH` | 指定 GNU Arm 工具链 `bin` 目录 |
| `CMSIS_DSP_INCLUDE` | 指定 CMSIS-DSP 头文件目录 |

若使用 SCons 构建，请从工程根目录执行，并确保上述变量或默认路径可用。

---

## 8. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| 编译报 `Cannot found RT-Thread root directory` | 未运行 `mklinks.bat` 或 BSP 路径不对 | 检查 BSP 1.4.0 与四个目录联接 |
| `arm-none-eabi-gcc: not found` | 工具链未安装/未识别 | 安装 13.3，或设置 `RTT_EXEC_PATH` |
| 下载报 `Can't find target/infineon/pse84xgxs2.cfg` | 用了 OpenOCD 1.0.0 | 改用 2.0.0 |
| 串口无输出 | 波特率不对 | 用 1 000 000 8N1，按复位 |
| `IMAI_init failed (-2)` | 固件缺算子对象 | 确认 `link.ld` 的 `INPUT()` 生效 |
| 图像超时 | 上位机未发图 | 用 `leaf_send.py` 或 GUI |
| 推理约 5.5 s | 当前走 CPU（CMSIS-NN）路径 | 需 Vela 重新导出以启用 Ethos-U55 |

---

## 9. 已知限制

- 推理运行在 CM55 的 CPU 路径，单帧约 5.5 s；Ethos-U55 尚未启用；
- 模型为四分类 INT8 MobileNetV2，输入 160×160，训练数据与现场图像存在域差异；
- `prebuilt/` 中的 ELF/HEX/MAP 为特定版本产物，修改源码后需重新编译。

---

## 10. 许可

本项目仅用于课程学习与实验演示；引用的 RT-Thread、LVGL、ExecuTorch、Infineon BSP 等遵循其各自许可证。
