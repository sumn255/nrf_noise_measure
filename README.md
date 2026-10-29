# Longan Nano：SI24 2.4 GHz 活动扫描器

本工程连续扫描 **2400～2525 MHz**，LCD 显示绿色检测活动度柱、黄色峰值保持及白色频率光标。按钮接 **PB8 → 按钮 → GND**，每按一次向右移动 1 MHz，到末尾回绕。使用方法和原理见 [扫描器教程](docs/scanner.md)，射频接线见 [SI24R1 驱动教程](docs/si24r1.md)。

目标固定为 `longan_nano/gd32vf103`，Zephyr 固定为 **v4.2.0**，SDK 固定为 **0.17.2**。这是便于复现实验的版本基线，不表示当前最新版本。默认面向普通版 128 KiB Flash / 32 KiB RAM 的 Longan Nano。

## 先看当前完成状态

LED、原有 LCD 测试画面、SI24 初始化和单频检测已由用户实机确认。新扫描动画和 PB8 按钮已完成编译链接及逻辑测试，尚待上板验收。SI24 接线请按 [接线表](docs/si24r1.md) 操作，**拔掉 microSD 卡**。

完整编译链接及桌面逻辑测试已验证。日常开发按下方步骤安装固定版本 Zephyr 和 SDK；关键概念和代码阅读顺序见 [中文教程](docs/tutorial.md)。

仓库只包含应用源码、设备树配置、构建脚本、测试和教程。Zephyr 及依赖模块由 `setup.ps1` 下载，SDK、Python 虚拟环境和构建产物均不纳入版本控制。

## 本机初始化

使用普通 Windows PowerShell，避免在已经执行过 ESP-IDF export 的终端中开始。初始化脚本会复用 IDF 的主机工具，并自动导入本项目环境。

```powershell
cd E:\dev\nrf_noise_measure

# 仅对当前 PowerShell 进程生效；遇到脚本执行策略限制时使用。
Set-ExecutionPolicy -Scope Process Bypass

# 首次需要联网，可能下载较多依赖。
.\scripts\install-wget.ps1
.\scripts\setup.ps1
.\scripts\doctor.ps1
.\scripts\build.ps1
```

初始化会执行：创建/复用 `.venv` → 安装 west、pyserial → 下载固定版本 Zephyr 和模块 → 安装该版本的 Python 基础依赖 → 安装 RISC-V SDK。任一步失败会停止并显示错误；修复连接问题后可以重新执行。

SDK 0.17.2 的 Windows 安装器还要求真正的 `wget.exe`，PowerShell 的 `wget` 别名不能代替。`install-wget.ps1` 将 x64 Wget 1.25.0 安装到项目 `.tools\bin`，使用 [Scoop 清单](https://github.com/ScoopInstaller/Main/blob/master/bucket/wget.json)公布的 SHA256 验证 ZIP；校验失败会停止，不解压或执行下载内容。当前工具受网络限制，脚本已检查语法但尚未实际完成下载验证。若 winget 的旧版包出现哈希不匹配，不要跳过校验。

项目没有修改系统 PATH，也没有执行 IDF 的 `export.ps1`。CMake、Ninja、dfu-util 是通用主机工具，可直接复用；ESP 的交叉编译器和 OpenOCD 不作为本板工具链使用。

目录布局：

```text
nrf_noise_measure/
├── CMakeLists.txt             应用的构建入口
├── prj.conf                   Zephyr 功能配置
├── src/main.c                 扫描、按钮、显示和 LED 调度
├── src/spectrum*.c            滚动统计、动画、峰值与分块绘制
├── src/button_filter.c        按钮消抖
├── src/si24r1.c               SI24 接收检测驱动
├── scripts/                   初始化、环境、构建、烧录、串口脚本
├── docs/                      中文教程和环境记录
├── .venv/                     项目独立 Python 环境，已忽略
├── .zephyr-workspace/         west 工作区，初始化时下载，已忽略
├── .tools/                    本项目 SDK 安装目录，已忽略
└── build/                     编译产物，已忽略
```

如果首次初始化时找不到 CMake 或 Ninja，脚本会把对应 Python 分发包装进 `.venv`，而不是安装到系统 Python。SDK 解压依赖 7-Zip，本机已有并由脚本加入当前进程 PATH。

若不想自动下载 SDK，可以使用 `setup.ps1 -SkipSdk`，然后设置一个已安装的兼容 SDK：

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = 'D:\your-tools\zephyr-sdk-0.17.2'
.\scripts\build.ps1
```

项目内 `.tools\zephyr-sdk-0.17.2` 存在时，环境脚本优先使用它。SDK 安装命令也可能复用机器上已有的同版本 SDK；若该 SDK 位于其他目录，按上面设置变量。Windows 用户驱动安装不能放进 Python 虚拟环境。

## 烧录与串口接线

使用 USB 数据线给开发板供电和 DFU 下载。串口日志走单独的 USB 转串口模块，使用 **3.3 V 电平**：

| Longan Nano | USB 转串口 |
|---|---|
| PB6 / B6（USART0 TX 重映射） | RX |
| PB7 / B7（USART0 RX 重映射） | TX，可选，本工程只输出日志 |
| GND | GND |

开发板已通过 USB 供电时，串口模块的 VCC 不接。板载 USB 在本工程中没有配置为 USB CDC 串口，不能把它直接当成日志串口。

当前已通过 `app.overlay` 将 USART0 从板级默认的 PA9/PA10 重映射至 PB6/PB7，无需接 PA9。详见 [串口选择与调试说明](docs/serial-debug.md)。

进入 DFU：按住 BOOT0 → 按下并释放 RESET → 释放 BOOT0。

```powershell
. .\scripts\env.ps1
dfu-util --list

# 确认开发板已经在 DFU 模式，且 build 已成功。
.\scripts\flash.ps1
```

烧录后释放 BOOT0，按 RESET 启动应用。`flash.ps1` 会写入应用 Flash；需要在重新进入 DFU 后才能再次下载。

若设备管理器能看到 DFU 设备，而 dfu-util 无法打开，可能需要给这一个 DFU 设备安装兼容 libusb 的驱动，例如 WinUSB。可用 [Zadig](https://zadig.akeo.ie/) 手动选择实际开发板的 DFU 设备，不要替换串口适配器的驱动。需要系统权限的驱动步骤由你在本机操作。

查看串口：

```powershell
# 列出端口。
.\scripts\monitor.ps1

# 将 COM5 改成实际 USB 转串口模块的端口。
.\scripts\monitor.ps1 -Port COM5
```

参数为 115200、8 数据位、无校验、1 停止位、无流控。按 `Ctrl+]` 退出；打开监视器后按 RESET，可以看到启动日志。

## 预期现象与验收

以下是预期结果，不是当前已经观测到的实机结果。

- 红色 LED 每 500 ms 切换一次，即亮灭完整周期约 1 秒。
- 串口启动时输出 `SI24 2.4 GHz activity scanner`，随后每秒报告选中频率、命中率、峰值及扫描/显示速度。
- LCD 先显示 `WARMUP`，每频点集齐 16 个样本后显示活动度柱。
- 绿色实时柱、黄色峰值标记，峰值保持约 1 秒再缓慢下落。
- 白色光标起始于 2412 MHz；PB8 按钮短按向右移动 1 MHz，按住 500 ms 后每 100 ms 连续右移，松开停止。
- 右上绿色方块表示 SI24 正常；出错时显示 `SI24 ERR` 并冻结测量图形。

示意日志：

```text
SI24 2.4 GHz activity scanner | Zephyr 4.2.0
... Console: USART0 TX=PB6 RX=PB7, 115200 8N1; button=PB8 to GND
... Scanning 2400..2525 MHz (126 channels), 16 windows/bin, 200 us/window
... 2412MHz hits=4/16 valid=1 activity=25% peak=50% sweeps/s=... fps=... err=0 LCD=ok KEY=ok
```

活动度是检测命中率的相对指标，不是幅值或 dBm。像素格式可为 RGB565 或 BGR565，应用适配两者。日志 `LCD=ok` 只表示驱动没有报告错误，仍需目视确认。扫描、动画与按钮的关键概念及验收步骤见 [扫描器教程](docs/scanner.md)。

## 日常开发

所有任务脚本会自动导入项目环境。若需要手工执行 `python`、`west` 等命令，先 dot-source：

```powershell
cd E:\dev\nrf_noise_measure
. .\scripts\env.ps1
python --version
west --version

.\scripts\build.ps1            # 通常使用增量编译
.\scripts\build.ps1 -Pristine  # 配置或板卡变更后强制重新配置和编译
```

手工执行 west 时，要进入它的工作区。应用源码虽然在工作区外，仍然可以按绝对路径构建：

```powershell
Push-Location .zephyr-workspace
west build -b longan_nano/gd32vf103 -d ..\build ..
west build -d ..\build -t menuconfig
west build -d ..\build -t ram_report
west build -d ..\build -t rom_report
Pop-Location
```

构建成功后，主要产物为 `build\zephyr\zephyr.elf`、`zephyr.bin` 和 `zephyr.map`。编译数据库通常在 `build\compile_commands.json`，可供 VS Code C/C++ 或 clangd 使用。

## 常见问题

| 现象 | 排查方法 |
|---|---|
| pip 报 WinError 10013，GitHub 连接被拒绝 | 在你自己的终端重跑 setup；检查实际网络、代理和安全软件策略。当前代理网络限制不能通过工程脚本修复 |
| pip 使用了不合适的镜像 | 激活项目环境后查看 `python -m pip config debug`；可临时设置 `$env:PIP_INDEX_URL='https://pypi.org/simple'` 再重试，前提是该源可访问 |
| west 命令不存在 | setup 尚未成功；先看 pip 的第一条错误，而不是继续 build |
| 提示没有 west workspace | 通过本项目 build 脚本运行，或手工进入 `.zephyr-workspace` |
| 未找到 SDK | setup 是否完成；检查 `ZEPHYR_SDK_INSTALL_DIR` 指向 SDK 根目录，而不是其 `bin` 子目录 |
| dtc / gperf 未找到 | 本机尚无这两个工具。dtc 在部分流程是可选检查；gperf 是否需要取决于功能配置。若构建报错要求它们，按固定版本文档安装并加入 PATH，可放到 `.tools\bin` |
| 长路径错误 | 使用当前较短路径；Windows 的长路径策略与 Git 长路径设置需要根据实际报错在本机配置 |
| dfu-util 找不到设备 | USB 数据线、BOOT0/RESET 顺序、DFU 驱动；进入 DFU 后重跑 `dfu-util --list` |
| 串口没有日志 | 这不是 USB CDC 日志；检查 PB6 → 转串口 RX、共地、COM 端口和 115200；打开串口后复位，并确认烧入了重映射后的固件 |
| LED 工作，屏幕黑屏 | 先看串口中的 LCD 状态和错误码；检查排线、板子是否与标准 Longan Nano 一致 |
| 色带红蓝反了或边框被裁切 | 核对实际显示屏版本、驱动报告格式和设备树中的 `rgb-is-inverted`、偏移、方向；不要先靠交换随机字节试错 |

## 官方参考

- [Longan Nano 板卡说明](https://docs.zephyrproject.org/latest/boards/sipeed/longan_nano/doc/index.html)
- [Zephyr v4.2.0 源码](https://github.com/zephyrproject-rtos/zephyr/tree/v4.2.0)
- [该版本推荐 SDK 版本](https://github.com/zephyrproject-rtos/zephyr/blob/v4.2.0/SDK_VERSION)
- [该版本 Display API 与像素字节序](https://github.com/zephyrproject-rtos/zephyr/blob/v4.2.0/include/zephyr/drivers/display.h)
- [Windows 环境入门](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)

`latest` 链接用于找入口；具体接口、依赖和板级配置应以本工程固定的 v4.2.0 源码为准。
