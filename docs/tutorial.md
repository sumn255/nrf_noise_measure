# 用 LED、串口和 LCD 理解 Zephyr

这个教程保留最初 LED、串口和 LCD 验证阶段的讲解，部分示例对应早期 `main.c`。当前主程序已升级为扫描器，最新调度、分块显示和按钮消抖见 [扫描器教程](scanner.md)。下面先用三个能够直接观察的外设，把“配置如何变成程序、程序如何找到硬件、屏幕如何收到像素”串起来。

## 1. 从应用到固件

Zephyr 是运行在单片机上的实时操作系统及其配套驱动、库和构建系统。它与应用一起链接为一个固件；不是先安装一个系统，再上传脚本。

本项目可以理解为这条构建链：

```text
应用 C 源码 ──────────────┐
prj.conf → Kconfig 配置 ───┤
板卡设备树 → 硬件描述 ──────┼→ CMake → Ninja → RISC-V GCC → ELF / BIN
Zephyr 内核与所选驱动 ──────┘
```

`CMakeLists.txt` 中的 `find_package(Zephyr ...)` 接入 Zephyr 构建流程；`target_sources(app ...)` 把应用文件加入构建。CMake 负责生成构建规则，Ninja 按这些规则决定哪些文件需要重新编译，SDK 内的 RISC-V 编译器负责生成 GD32VF103 能执行的机器码。

因此 IDF 的 CMake、Ninja 可以复用，它们运行在 Windows 上；ESP-IDF 的目标编译器和驱动配置却不能直接视作 Longan Nano 的工具链。

SDK 中请求的工具链名称是 `riscv64-zephyr-elf`，但它通过目标架构参数也能生成 RV32 代码；这个名称不意味着把本板当成 64 位处理器。GD32VF103 是 32 位 RISC-V。

## 2. west 与 Python 虚拟环境解决不同问题

`.venv` 隔离 Python 包。这里装 west、pyserial 和 Zephyr 构建脚本需要的依赖，不影响现有 IDF 虚拟环境。

west 则管理多仓库工作区：一个 Zephyr 工程可能同时需要内核仓库和芯片 HAL 等模块。`west init` 建立工作区，`west update` 按 manifest 下载对应版本的模块；`west build` 和 `west flash` 是构建与烧录入口。

本工程的 `env.ps1` 直接把 `.venv\Scripts` 加入当前进程 PATH，并设置 `PYTHONNOUSERSITE`。这与运行虚拟环境的激活脚本有相同的核心目的：让 `python` 和 `west` 来自本工程。它没有改提示符，也没有提供 `deactivate` 函数；关闭这个终端即可丢弃环境变量变化。

Zephyr v4.2.0 被固定在 setup 脚本中，模块版本由这一版本自己的 manifest 决定；这比每次下载不断变化的主分支更适合学习。当前没有生成完整 Python 依赖锁文件，首次成功安装后可用 `python -m pip freeze` 记录精确环境。

## 3. Devicetree：硬件连接和属性

应用代码没有直接写“PC13 是红灯、哪个 SPI 接 LCD”，而是从设备树读取：

```c
static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct device *const panel =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
```

`alias` 是便于应用引用的硬件别名，`led0` 对应板卡定义的 LED。`chosen` 表示板卡为某种角色选定的设备，`zephyr,display` 就是默认屏幕。

`gpio_dt_spec` 包含 GPIO 控制器、引脚编号和有效电平等信息。Longan Nano 的 LED 为低电平有效；`gpio_pin_set_dt(&led, 1)` 表示逻辑上的“点亮”，GPIO API 会结合设备树自动转换为物理低电平。不要再手工把它反转一次。

设备树通常包含：

- SoC 的 `.dtsi`：芯片有哪些外设、寄存器和中断。
- 板卡的 `.dts` / `.dtsi`：哪些外设引出、屏幕接在哪、频率和偏移是多少。
- 应用的 overlay：对某个应用追加或覆盖硬件配置。

这一阶段没有创建 overlay，因为板级配置已经提供 LED、串口和 LCD。以后接 nRF24 时，再添加应用 overlay 描述 SPI 从设备与 CE 引脚。

这些宏在**构建时**展开，不会在启动时解析一份设备树文本。成功构建后看 `build\zephyr\zephyr.dts`，能看到最终合并的硬件配置；生成头文件通常位于 `build\zephyr\include\generated\zephyr\devicetree_generated.h`。

## 4. Kconfig：固件包含哪些功能

`prj.conf` 中的配置例如：

```ini
CONFIG_GPIO=y
CONFIG_SPI=y
CONFIG_DISPLAY=y
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y
```

这些选项决定构建哪些子系统、驱动及功能。它们与设备树配合工作：设备树说明“屏幕存在且接在这里”，Kconfig 允许“把显示和相应总线驱动编译进来”。启用了设备树节点，并不意味着任何功能配置下都会生成对应设备。

最终配置不只来自 `prj.conf`，还包括板卡默认值、依赖关系和自动选择。看 `build\zephyr\.config` 才能知道最后采用的值。`menuconfig` 可以探索选项，稳定需要的改动要记录回 `prj.conf`，避免只留在临时构建目录。

## 5. 设备存在与设备就绪

`DEVICE_DT_GET(...)` 获取编译期生成的设备对象；`device_is_ready(...)` 检查驱动启动初始化是否成功。两者并不相同。

如果出现类似 `__device_dts_ord_*` 的链接错误，通常要检查设备树节点状态及对应驱动是否启用。若能够链接，但运行时 `device_is_ready()` 为假，则继续检查初始化失败的错误信息、总线及依赖设备。

程序分别检查 LED 和屏幕。屏幕初始化失败时仍然保留 LED 和串口输出，方便定位失败发生在哪一层。

## 6. main 运行在线程里

Zephyr 完成内核与设备初始化后，在主线程调用 `main()`。因此这个例子不需要自己创建线程，就可以使用：

```c
while (true) {
    /* 更新 LED 与右下角方块。 */
    k_sleep(K_MSEC(500));
}
```

`k_sleep()` 让主线程暂时不运行，CPU 可以运行其他就绪线程或进入空闲状态。它不是精密的硬件定时器：这个程序两次更新的间隔是约 500 ms 加上本轮绘制、日志等执行时间。

以后射频采样需要微秒级等待时，可以考虑短时间 `k_busy_wait()`；忙等会占用 CPU，应限制在需要的时序片段中。先完成单线程扫描，再根据实际需求拆分显示线程，容易理解和调试。

## 7. 日志与串口

`printk()` 用于直接打印启动标记；`LOG_INF()`、`LOG_ERR()` 属于日志系统，提供模块名、等级等上下文。

当前 `CONFIG_LOG_MODE_IMMEDIATE=y` 让日志在调用处处理，减少异步日志线程和缓冲带来的学习成本。代价是输出会消耗当前线程时间。对一秒一条的心跳足够；以后不要在每个 200 μs 射频窗口里打印日志，否则采样行为会被日志显著改变。

日志后台使用 USART0，应用的 `app.overlay` 已把引脚从板级默认 PA9/PA10 重映射到 PB6/PB7，并设置 115200 波特率。USB DFU 是固件下载协议，UART 是运行日志接口；两者在这块板上走不同的连接路径。重映射和其他串口选项见 [串口调试说明](serial-debug.md)。

## 8. 色块怎样变成像素

屏幕使用 RGB565：16 位中，红色占 5 位、绿色占 6 位、蓝色占 5 位，例如：

| 颜色 | RGB565 |
|---|---|
| 红 | `0xF800` |
| 绿 | `0x07E0` |
| 蓝 | `0x001F` |
| 白 | `0xFFFF` |
| 黑 | `0x0000` |

本工程按驱动的 `current_pixel_format` 判断是否交换红蓝分量。并且使用 `sys_put_be16()` 输出像素字节：Zephyr 4.2 的 Display API 明确规定多字节像素按大端排列。GD32VF103 是小端机器，直接把 `uint16_t[]` 的内存送出去可能得到错误颜色。

字节序是“这两个字节谁先发”，RGB/BGR 是“红蓝分量放在哪几个位”，它们是两个不同的问题。

`display_buffer_descriptor` 描述发送缓冲：

- `width` / `height`：本次矩形的尺寸，单位是像素。
- `pitch`：缓冲中相邻两行起点间隔，单位也是像素。
- `buf_size`：缓冲总字节数。

本工程一次发送一行：宽 160、高 1、pitch 160、buf_size 320。LCD 控制器有自己的显存，主控把一行写进去以后，就可以复用这 320 字节来生成下一行，不需要保留 25,600 字节的全屏缓冲。

白色边框用来发现屏幕偏移或裁切，八条色带检查颜色顺序，右下角方块检查程序和总线是否持续工作。Display API 返回成功只证明驱动没有报告传输错误，不能证明屏幕物理上正确显示，因此还需要目视验收。

## 9. 把构建目录当作学习工具

成功编译后，按这个顺序看文件：

1. `zephyr.dts`：实际启用了哪些设备，显示节点的尺寸、偏移及 SPI 接口是什么。
2. `.config`：哪些驱动和功能最终生效，主线程栈多大。
3. `zephyr.map`：Flash 和 RAM 被哪些代码、数据占用。
4. `ram_report` / `rom_report`：按模块汇总资源占用。

`CONFIG_MAIN_STACK_SIZE=2048` 设置主线程栈预算，`row` 是静态缓冲，不占这 2 KiB 线程栈。`CONFIG_HEAP_MEM_POOL_SIZE=0` 表示不为内核通用堆预留内存，不意味着整个固件 RAM 占用为零。

## 10. 建议的三个练习

完成 README 中的实机验收后，可以依次修改：

1. 把心跳改成 250 ms，观察串口间隔也会随循环计数变化。思考“每两轮打印”为什么不等于固定一秒。
2. 调整 `colors[]` 顺序，理解显示坐标与颜色打包；保留边框，验证没有裁切。
3. 从最终 `zephyr.dts` 找到默认 LED 的引脚与有效电平，再将 `DT_ALIAS(led0)` 换成板卡存在的 `led1`，观察另一种颜色。

接下来再添加 nRF24：先在 overlay 描述连接并验证寄存器读回，再编写独立接收窗口的采样代码，最后复用现有分行绘制方法显示柱状图和瀑布图。

参考：[设备树介绍](https://docs.zephyrproject.org/latest/build/dts/intro.html)、[Kconfig 配置](https://docs.zephyrproject.org/latest/build/kconfig/index.html)、[线程](https://docs.zephyrproject.org/latest/kernel/services/threads/index.html)、[v4.2.0 Display API](https://github.com/zephyrproject-rtos/zephyr/blob/v4.2.0/include/zephyr/drivers/display.h)。
