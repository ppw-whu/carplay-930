# Arduino IDE 环境配置与程序上传

这份说明用于在另一台电脑上重新编译、上传本工程。先按以下步骤安装环境，再打开 `carplay.ino`；不需要把原电脑的 Arduino 缓存或个人配置文件复制过去。

## 1. 已验证的环境

以下为 2026-09-30 从本机安装信息核对的版本，并非“必须使用最新版”：

| 项目 | 已验证值 | 说明 |
| --- | --- | --- |
| 开发板 | emakefun Maker-ESP32，经典 ESP32 | 本项目选择 ESP32 Dev Module；不选 ESP32-S3/C3/S2 |
| Arduino IDE | 2.3.10 | IDE 安装目录的 `resources/app/package.json` |
| Arduino CLI | 1.5.1 | IDE 自带 CLI 的 `version` 输出；仅命令行使用者需要了解 |
| ESP32 开发板包 | `esp32:esp32` **3.3.2-cn** | `arduino-cli core list` 核对；中国镜像版本 |
| PS3 手柄库 | 厂家 `emakefun_esp32_ps3` **2.0.1** | `library.properties` 显示名称为 **PS3 Controller Host** |
| Flash / 程序分区 | **4MB / Huge APP** | 程序区上限 3,145,728 字节 |
| 串口监视器 | **115200 baud** | 与程序 `Serial.begin(115200)` 一致 |

项目使用的 `WiFi.h`、`WebServer.h`、Arduino 与 FreeRTOS 由 ESP32 开发板包提供，无需另外安装同名库。厂家资料推荐 3.3.2 / 3.3.2-cn；其已验证范围为 2.0.17～3.3.6，明确指出 3.3.7 存在手柄连接兼容问题。新版本不等于此项目已经验证过的版本。[厂家版本说明](https://docs.emakefun.com/#/zh-cn/peripheral/bluetooth_gamepad_ps3/bluetooth_gamepad_ps3)

## 2. 下载完整工程

1. 在 GitHub 仓库页面点击 **Code → Download ZIP**，解压到一个普通目录。
2. GitHub 解压出的目录通常带 `-main` 后缀。将**直接包含 `carplay.ino` 的目录改名为 `carplay`**，再打开里面的 `carplay.ino`。Arduino 要求主 `.ino` 文件名与工程目录名一致。
3. 若拿到的是随项目准备的 `carplay-github.zip`，解压后使用其中已命名为 `carplay` 的工程目录。以下五个文件应在同一个目录，不要只下载 `.ino`：

```text
carplay/
  carplay.ino
  CarConfig.h
  ControlPage.h
  MotionControl.h
  MotorDriver.h
  README.md
  HTTP接口说明.md
  docs/
  tests/
  motorTest/
    motorTest.ino
```

`motorTest` 是单独的原始电机测试工程；日常上传打开顶层 `carplay.ino`。`tests` 是电脑上运行的开发测试，无需加入 Arduino IDE 标签页。

## 3. 安装 Arduino IDE 和 ESP32 开发板包

1. 从 [Arduino 官方下载页](https://www.arduino.cc/en/software/) 安装 Arduino IDE 2。已有 IDE 2 可继续使用；本工程记录的验证版本为 2.3.10。
2. 在 Windows 中打开 **文件 → 首选项 → 附加开发板管理器网址**。中国网络环境可填入本项目实际使用的官方镜像地址：

```text
https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_index_cn.json
```

若使用国际源，则选以下地址并安装 **3.3.2**；为了复现本项目的验证环境，优先使用上面的镜像和 **3.3.2-cn**。不需要为了本项目同时配置两套源。

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

3. 打开左侧 **开发板管理器**，搜索 `esp32`，找到 Espressif Systems 提供的 `esp32`，在版本下拉框选择 **3.3.2-cn**，安装并等待完成。
4. 若镜像下载失败，先检查网络与网址，不要改装 `Arduino ESP32 Boards` 来替代；本工程用的是 Espressif 的包。中国镜像带 `-cn` 的版本需要手动选取，官方文档不建议通过自动更新切换到非镜像包。[Espressif 安装说明](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)

## 4. 安装厂家 PS3 库

1. 打开 **库管理器**，搜索 `PS3 Controller Host`。如果之前安装了另一个旧版同名库，先移除旧版，避免编译器选错库。
2. 下载厂家提供的 [emakefun_esp32_ps3 2.0.1 ZIP](https://raw.githubusercontent.com/emakefun/emakefun-docsify/main/docs/zh-cn/peripheral/bluetooth_gamepad_ps3/emakefun_esp32_ps3-2.0.1.zip)。
3. Arduino IDE 选择 **项目 / Sketch → 导入库 / Include Library → 添加 .ZIP 库 / Add .ZIP Library**，选刚下载的 ZIP。
4. 成功后应能在 **文件 → 示例 → PS3 Controller Host** 找到示例。厂家的库内部显示名仍是 `PS3 Controller Host`，要以 **2.0.1 版本及下载来源**区分。

如果编译提示 `Multiple libraries were found for "Ps3Controller.h"`，看输出中实际使用了哪一个目录，移除或移出 Arduino 的 `libraries` 目录中的重复旧库，再重新编译。只在文件夹名称后加 `_old` 可能仍被 Arduino 扫描，不适合作为禁用方式。不要从库管理器另装旧库覆盖厂家版本。[厂家安装步骤](https://docs.emakefun.com/#/zh-cn/peripheral/bluetooth_gamepad_ps3/bluetooth_gamepad_ps3)

## 5. Arduino IDE 工具菜单

打开 `carplay.ino`，先选择 **工具 → 开发板 → esp32 → ESP32 Dev Module**。随后核对下表。除分区以外，表中的默认项来自本机 3.3.2-cn 的 `board details`；它们不是对 IDE 当前界面选中状态的猜测。

| 菜单 | 设置 |
| --- | --- |
| Board / 开发板 | **ESP32 Dev Module** |
| Port / 端口 | 插入该板后新增的实际串口，例如 COM5；每台电脑可能不同 |
| Upload Speed / 上传速度 | 默认 **921600**；上传不稳定时改 **115200** 后重试 |
| CPU Frequency | **240MHz (WiFi/BT)** |
| Flash Frequency | **80MHz** |
| Flash Mode | **QIO** |
| Flash Size | **4MB (32Mb)** |
| Partition Scheme / 分区方案 | **Huge APP (3MB No OTA/1MB SPIFFS)**，必须手动选 |
| Core Debug Level | **None** |
| PSRAM | **Disabled** |
| Arduino Runs On | **Core 1** |
| Events Run On | **Core 1** |
| Erase All Flash Before Sketch Upload | **Disabled** |
| JTAG Adapter | **Disabled** |
| Zigbee Mode | **Disabled** |

**上传速度和串口监视器波特率是两件事**：921600 是烧录传输速度，查看本程序日志固定选 115200。更换开发板或分区后重新编译。命令行的 FQBN 设置不会替你修改 IDE 的下拉菜单。

## 6. 填写小车配置

打开 IDE 中的 `CarConfig.h` 标签页，按自己的设备填写：

供上传 GitHub 的发布包已将 `PS3_HOST_MAC` 留空，避免把原车手柄配对码一起发布。此时网页可以使用，手柄不会连接；要使用手柄，先填自己的配对码再上传固件。本地原工作工程保留原有配对设置。默认热点名与密码是公开示例值，可按自己的小车修改。

```cpp
constexpr char WIFI_SSID[] = "Carplay-ESP32";
constexpr char WIFI_PASSWORD[] = "carplay123";
constexpr char PS3_HOST_MAC[] = "02:00:01:23:45:67"; // 示例，换成自己的手柄配对码
```

- Wi-Fi 参数指**小车创建的热点**，不需要填家中路由器的账号；热点密码长度为 8～63 个字符。
- `PS3_HOST_MAC` 填厂家手柄背面的**目标主机配对码**，不是随意找的电脑/手机/手柄蓝牙地址。留空 `""` 可关闭蓝牙，仅使用网页。
- 保存并重新上传后，配置才会在小车上生效。
- 本工程默认车轮速度 80%；□ 每按一次增加20%，100%后回到20%。GPIO32 新增电机的三档独立配置，不随车轮档位变化。

连接厂家这款手柄时，先断开手柄 USB 线，给已上传程序的小车供电，再按手柄中间 PS 键。配对后摇杆回中，松开肩键再按 **L1+R1** 才进入遥控；单纯连接成功不会立即让车运动。该流程针对厂家资料中的预设配对码手柄，普通 PS3 手柄不能假定背面也有相同配对码方案。[厂家配对说明](https://docs.emakefun.com/#/zh-cn/peripheral/bluetooth_gamepad_ps3/bluetooth_gamepad_ps3)

## 7. 编译、上传与首次连接

1. 用支持数据传输的 USB 线连接电脑和开发板，选择对应串口。先将车轮架空；电机接线和供电要求见 [README](../README.md)。
2. 点击左上角 **√ 验证**。本机已填写 PS3 配对码的完整功能版本在上述环境中编译占用约 **1.66MB**，Huge APP 程序区为 3MB；发布包配对码留空，编译大小可能不同，版本或其他配置变化也会影响精确数字。
3. 点击 **→ 上传**，等待显示上传完成。若其他串口工具占用了端口，先关闭它们。普通上传会替换板上的原程序。
4. 打开 **串口监视器**，选 **115200**。若没看到启动信息，按一下板上的复位键。正常日志包含 `Connect to car Wi-Fi:` 和 `Car API: http://192.168.4.1`。`PS3 ready` 表示蓝牙主机已初始化，不等于手柄已连上。
5. 手机或电脑连接 **Carplay-ESP32**，输入配置的热点密码，在浏览器地址栏打开 **http://192.168.4.1**。提示“无互联网”是正常现象，请保持连接。
6. 点击网页 **激活网页控制**；若正在手柄遥控，按钮为 **切换到网页控制**。先用20%档位检查前后方向及左右原地旋转，再增加档位。
7. 测试 **全部停车**、PS3 的 **×**、网页与手柄接管以及断连停车。完整按键与接口见 [README](../README.md) 和 [HTTP 接口说明](../HTTP接口说明.md)。

当前提交的编译、电脑自动测试和浏览器模拟已通过；这不代表已替用户完成烧录或实车验证。供电、轮子方向、MOSFET 驱动及无线连接仍需在实车检查。

## 8. 常见问题

| 现象 | 检查方法 |
| --- | --- |
| `Sketch too big`、`text section exceeds available space`，上限 1,310,720 字节 | 仍是默认分区。改为 **Huge APP**、Flash 4MB，再编译；正确程序区上限为 3,145,728 字节 |
| `Ps3Controller.h: No such file or directory` | 厂家 ZIP 库未正确安装，按第4节处理 |
| PS3 库中出现编译错误，或蓝牙一直连不上 | 核对 ESP32 3.3.2-cn、厂家 PS3 2.0.1，检查重复库和手柄配对码，不要选择 S3/C3/S2 |
| 工具菜单没有端口或插拔后无新端口 | 换数据线/USB口，在设备管理器检查 USB 串口设备；驱动须按板上实际 USB 转串口芯片从厂家安装，不假定所有板子使用同一驱动 |
| 上传停在 `Connecting...` | 核对端口、关闭占用串口的软件、降低上传速度；若板上有 BOOT 键，可在连接阶段按住，开始写入后松开，必要时配合复位 |
| 串口乱码 | 串口监视器选115200，并确认打开的是该板串口 |
| 手机搜不到热点 | 看115200串口中的热点初始化结果；确认上传成功并已复位、热点名称正确，检查供电；本工程创建2.4GHz热点 |
| 已连热点但网页打不开 | 地址栏输入完整 `http://192.168.4.1`，确认手机没有自动离开“无互联网”热点；地址以串口输出为准 |
| 网页旧按钮/旧样式 | 重新加载页面，必要时关闭页面后重新打开；网页随固件一起上传 |
| 网页或手柄已连接但推杆不动 | 先激活控制模式；手柄回中再按 L1+R1，网页点激活；检查电机供电和 Motor and IO Switch |

## 9. 可选：Arduino CLI 复现

以下命令假设已经安装 `arduino-cli` 并加入 PATH，当前目录是包含 `carplay.ino` 的 `carplay`。IDE 用户可以跳过本节。独立 CLI 使用自己的配置；如需共享已有配置，用 `--config-file` 指向**自己电脑**上的文件，不要复制别人的个人配置。

首次配置镜像和开发板包：

```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_index_cn.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.2-cn
```

如果 CLI 已配置过，则不必再次初始化或重复添加网址。PS3 库仍按第4节安装到该 CLI 使用的 sketchbook 的 `libraries` 目录；可用 `arduino-cli config dump` 查看其 `directories.user`，用 `arduino-cli lib list` 核对是否识别到2.0.1。

检查与编译：

```sh
arduino-cli version
arduino-cli core list
arduino-cli lib list
arduino-cli board details --fqbn esp32:esp32:esp32
arduino-cli compile --fqbn esp32:esp32:esp32:FlashSize=4M,PartitionScheme=huge_app .
```

确定串口后上传（`COM5` 只是示例，替换为 `board list` 中自己的端口）：

```sh
arduino-cli board list
arduino-cli upload --port COM5 --fqbn esp32:esp32:esp32:FlashSize=4M,PartitionScheme=huge_app .
arduino-cli monitor --port COM5 --config baudrate=115200
```

## 10. 可选：电脑上的开发测试

这些测试不会烧录或驱动小车；日常使用仅需完成前面的 IDE 安装和上传。网页测试需要支持 `node:` 内置模块的 Node.js，不需要 `npm install`：

```sh
node tests/web_control_test.cjs
```

C++ 测试需要电脑上的 C++11 编译器。以下是 GCC/Clang 风格命令，Windows 和类 Unix 终端都可使用这种命令格式（编译器须已加入 PATH）：

```sh
g++ -std=c++11 -O2 -Wall -Wextra -Werror -pedantic tests/motion_control_test.cpp -o motion_control_test.exe
./motion_control_test.exe
g++ -std=c++11 -O2 -Wall -Wextra -Werror -pedantic -Itests/arduino_stubs -DTEST_ESP32_MAJOR=3 tests/motor_driver_test.cpp -o motor_driver_test.exe
./motor_driver_test.exe
g++ -std=c++11 -O2 -Wall -Wextra -Werror -pedantic -Itests/arduino_stubs -DTEST_ESP32_MAJOR=2 tests/motor_driver_test.cpp -o motor_driver_test.exe
./motor_driver_test.exe
```

后两条分别检查驱动对 Arduino-ESP32 3.x / 2.x LEDC API 的适配，使用模拟 GPIO；它们不代表已完成对应全部开发板版本的固件或硬件兼容测试。
