# 调试串口：PB6/PB7 硬件重映射

当前项目将 **USART0 TX 映射到 PB6，RX 映射到 PB7**，使用侧边引脚接 USB 转串口模块。无需接 PA9，也无需实现 USB 虚拟串口。

标准 Longan Nano 的 PA9/PA10 串口通常位于 USB 相反一端的串口接口，而非两侧 GPIO 排针。实际板型和焊接情况可能不同；本工程现在使用 PB6/PB7，按自己板子 B6/B7 丝印连接即可。[Zephyr 板卡串口说明](https://docs.zephyrproject.org/latest/boards/sipeed/longan_nano/doc/index.html)

## 为什么选择硬件串口

| 方式 | 在当前项目中的情况 | 用途 |
|---|---|---|
| USART0 重映射到 PB6/PB7 | Zephyr 的引脚定义已支持，不与现用外设冲突 | 当前首选，用于详细日志 |
| USART1，TX PA2 / RX PA3 | PA2 连板载蓝灯，不能当作完全空闲引脚 | 可以另行设计，但本次不用 |
| USART2，TX PB10 / RX PB11 | PB10 已接 SI24 CE；使用它需要先修改无线模块接线 | 可以另行设计，但本次不用 |
| USB CDC ACM | 本工程 Zephyr 4.2.0 的 GD32VF103 SoC 设备树没有现成 USB 控制器节点；不能仅凭芯片支持 USB 就认定开一个选项即可使用 | 需要进一步确认控制器驱动和板级接入 |
| 屏幕 | 当前已显示 SI24 状态块，资源占用小 | 辅助观察，不便承载完整日志 |

引脚映射依据本地固定版本 HAL 头文件 `modules/hal/gigadevice/include/dt-bindings/pinctrl/gd32vf103c(b-8)xx-pinctrl.h`，外设占用依据板级设备树和本项目 overlay。

本次没有更换 USART 控制器，仍然是硬件 USART0；改变的是它连接的物理引脚。这能达到换到可接线串口的目的，同时保持 console/logging 的既有配置。

## 接线与运行

断电接线，使用 **3.3 V TTL 电平**的 USB 转串口模块：

| Longan Nano | USB 转串口 |
|---|---|
| PB6 / B6，TX | RX |
| PB7 / B7，RX | TX，可选，当前只需要输出日志 |
| GND | GND |

开发板由 USB 供电时，不连接串口模块 VCC。只看日志时接 PB6 与 GND 两根线即可；不要将 TX 接到 TX，也不要使用 RS-232 电平的串口。

```powershell
cd E:\dev\nrf_noise_measure
.\scripts\build.ps1 -Pristine

# 进入 DFU 后烧录，随后释放 BOOT0 并复位。
.\scripts\flash.ps1

# 查看实际串口号。
.\scripts\monitor.ps1
.\scripts\monitor.ps1 -Port COM5
```

串口参数保持 **115200 8N1，无流控**。COM5 要替换为自己的适配器端口。监视器打开后复位，即可看到：

```text
Console: USART0 remapped, TX=PB6, RX=PB7, 115200 8N1
```

SI24 模块接线保持原样：CE PB10、CSN PB12、SCK PB13、MISO PB14、MOSI PB15。仍需拔掉 microSD 卡。

## 重映射在 Zephyr 中如何表达

`app.overlay` 的配置是：

```dts
&pinctrl {
    console_uart0_remap: console_uart0_remap {
        group1 {
            pinmux = <USART0_TX_PB6_RMP>, <USART0_RX_PB7_RMP>;
        };
    };
};

&usart0 {
    current-speed = <115200>;
    pinctrl-0 = <&console_uart0_remap>;
    pinctrl-names = "default";
};
```

板卡的 `chosen { zephyr,console = &usart0; }` 无需修改，因为控制器仍是同一个。`pinctrl-0` 选择一组引脚配置，`RMP` 宏会携带 GD32 所需的硬件重映射信息，底层驱动在初始化时应用。

这不等于软件模拟串口，也不是任意 GPIO 都能配置成 UART；可选位置受芯片的复用/重映射规则限制。

PB6/PB7 也可用于 I²C0，本项目目前没有启用它。未来增加 I²C 外设时，应避免同时占用这两个引脚。

## 验证边界

原有 LED/LCD 已由用户实机确认，本次 USART0 重映射仍需用户烧录后用转串口实测。USB DFU 能下载固件，并不意味着应用固件已经提供 USB CDC ACM；DFU 与串口日志仍是两个独立功能。

2026-10-29：重映射后的设备树、源码编译及完整链接已在独立 `build-si24-check` 目录验证通过，使用的是可访问的 RISC-V GCC 14.2.0；日志为 `build-si24-check\uart-remap-build.log`。Flash 46,740 字节，RAM 6,120 字节。由于当前代理无法读取默认 Zephyr SDK 目录，默认 SDK 构建仍请按上述命令在本机执行。
