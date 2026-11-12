# LeafAI INT8 E84：RT-Thread Studio 使用说明

## 工程位置

RT-Thread Studio 工作空间：

```text
D:\RT-ThreadStudio\workspace
```

工程名：`LeafAI_INT8_E84`

工程使用 PSoC E84 Edgi-Talk Board Support Package 1.4.0：

```text
D:\RT-ThreadStudio\repo\Local\Board_Support_Packages\Infineon\PSOC_E84-EDGI-TALK\1.4.0
```

## 打开与编译

1. 启动 RT-Thread Studio，选择上述工作空间。
2. 在左侧 Project Explorer 打开 `LeafAI_INT8_E84`。
3. 等待 C/C++ 索引完成。
4. 点击工具栏的 Build（锤子图标），或在工程上右键选择 `Build Project`。

编译产物：

```text
rt-thread.elf
rtthread.hex
rtthread.map
```

工程默认使用 Studio 已安装的 GNU Arm Toolchain 13.3。实际工具链路径记录在 `rtconfig.py` 中；如果以后升级 Studio 工具链，可修改该文件中的 `EXEC_PATH`。

## 烧录与调试

1. 使用 USB 连接板载 KitProg3。
2. 关闭占用 COM 口的串口助手。
3. 打开 `Run > Debug Configurations`。
4. 选择 `LeafAI_INT8_E84.OpenOCD.Debug`。
5. 点击 `Debug`。

该启动配置会通过 OpenOCD 写入 QSPI Flash，然后连接 M55 调试目标。首次烧录或 KitProg3 偶发无法暂停目标时，可拔插 USB 后重试一次。

## 串口

默认串口参数：

```text
波特率：1000000
数据位：8
校验：无
停止位：1
流控：无
```

端口以设备管理器中实际显示的 COM 号为准。M55 应用启动后会把控制台切换到 `1000000` 波特率，与现有图片上传 GUI 保持一致。串口终端可以观察到 `LEAF_READY`、检测类别、置信度和耗时。

## 模型与图片测试

模型和应用代码位于：

```text
applications\leaf_ai\
```

电脑端拖拽上传界面位于原先独立交付目录，不在本嵌入式工程内。测试时先通过该界面发送图片，再在板载屏幕或串口查看结果。

## 关键源码

- `applications\leaf_ai\leaf_app.c`：图片接收、推理调度与协议处理
- `applications\leaf_ai\leaf_ui.c`：板载显示界面
- `applications\leaf_ai\portable_leaf_ops.cpp`：ExecuTorch 模型算子
- `applications\leaf_ai\leaf_app_config.h`：模型尺寸及推理参数
- `applications\lvgl_port`：LVGL 的 RT-Thread 移植与配置
- `board`：板级初始化
- `packages`：AHT10/AHT20 温湿度等软件包

## 常见问题

- 编译时提示 SDK 路径不存在：确认 1.4.0 BSP 已下载，并检查 `SConstruct` 顶部的 `SDK_BSP_ROOT`。
- 调试时提示无法连接：确认没有其他 OpenOCD、调试器或串口程序占用 KitProg3。
- 串口没有输出：确认使用的是 M55 应用对应的串口，并检查波特率是否为 `1000000`。
- 修改板级 Kconfig 后：执行 Studio 的 RT-Thread Settings 保存，再清理并重新编译，避免旧对象与 `.config` 不一致。
