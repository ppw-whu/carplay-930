# 把 carplay 工程上传到 GitHub

本说明适用于首次发布。本次仅准备了文件，没有创建 GitHub 仓库或上传代码。使用浏览器就能完成，不需要安装 Git 或 GitHub Desktop。

## 1. 找到准备好的文件

本机工程目录内的 `release/carplay/` 是准备上传的文件夹；`release/carplay-github.zip` 是相同内容的压缩包。也可以把压缩包解压后，打开里面的 `carplay` 文件夹。

**进入 `carplay` 文件夹，上传里面的文件和子文件夹。** 不要只上传 ZIP，否则 GitHub 首页不能直接显示源码和 README；也不要把外层 `release` 一起上传。

需要能看到以下内容：

```text
carplay/
├── .gitignore
├── carplay.ino
├── CarConfig.h
├── ControlPage.h
├── MotionControl.h
├── MotorDriver.h
├── README.md
├── HTTP接口说明.md
├── docs/
│   ├── Arduino环境与上传.md
│   └── GitHub上传说明.md
├── motorTest/
│   └── motorTest.ino
└── tests/
    ├── motion_control_test.cpp
    ├── motor_driver_test.cpp
    ├── web_control_test.cjs
    └── arduino_stubs/
        ├── Arduino.h
        └── esp_arduino_version.h
```

上传包的 `PS3_HOST_MAC` 留空，使用手柄前要按环境说明填写自己的配对码；留空仍可编译和使用网页控制。本机原工程的配对码没有改动。热点名称及密码为公开示例值，实际交付时可以自行修改。没有打包 Arduino IDE、Arduino15 缓存、访问令牌或第三方库安装目录；环境和依赖按说明重新安装即可。

## 2. 创建仓库

1. 登录自己的 GitHub 账号，打开 [新建仓库页面](https://github.com/new?name=carplay&visibility=private)。链接预填名称 `carplay` 和私有可见性，请在页面上核对。
2. **Owner** 选择自己的账号；**Repository name** 填 `carplay`。同名仓库已存在时，可另取名称，但以后在 Arduino IDE 打开时仍要保证本地工程文件夹名为 `carplay`。
3. **Description** 可填写：`Maker-ESP32 四轮差速小车：PS3手柄、Wi-Fi网页控制及GPIO32电机调速`。
4. **Visibility** 建议先选择 **Private**，适合项目备份或定向交付；若希望任何人都能查看源码，再选择 **Public**。
5. 不要自动添加 README、.gitignore 或 License：前两项已在工程中，许可证可在确定授权方式后再选择。
6. 点击 **Create repository**。

以上对应 [GitHub 官方创建仓库说明](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository)。

## 3. 上传工程

1. 新建空仓库后，点击页面中的 **uploading an existing file**。若仓库已有文件，使用 **Add file → Upload files**。
2. 在资源管理器打开准备好的 `release/carplay/`，选中其中的全部文件和子文件夹，拖到上传区域。文件夹也要一起拖入，保持 `docs/`、`tests/`、`motorTest/` 的层级。
3. 检查待上传列表：应有根目录的 `carplay.ino`、4个 `.h`、`README.md`、`.gitignore`，以及文档和测试子目录。若看不到 `.gitignore`，开启资源管理器“显示隐藏的项目”后补选；不要把所有子目录里的文件摊平到同一层。
4. 提交说明填写：`Initial upload: ESP32 car firmware and setup guide`。
5. 新建的个人空仓库，按页面提示提交到默认分支，一般为 **main**；点击 **Commit changes** 完成。若仓库规则要求分支或 Pull Request，按页面提示走对应流程。

浏览器支持拖拽文件和文件夹上传。具体按钮与提交方式以 [GitHub 官方上传说明](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository) 及当前仓库页面为准。

## 4. 上传完成后检查

- 仓库首页直接显示 README，并能打开其中的环境配置与接口说明链接。
- 根目录能直接看到 `carplay.ino` 和4个 `.h`，没有多套一层 `release`。
- `docs/Arduino环境与上传.md`、`HTTP接口说明.md`、`tests/arduino_stubs/` 均存在。
- 发布的 `CarConfig.h` 中手柄配对码为空，需要使用的人自行填写。

可以再用 **Code → Download ZIP** 下载一份验证：解压后将工程文件夹从 `carplay-main` 改名为 `carplay`，按照 [Arduino 环境与上传说明](Arduino环境与上传.md) 安装依赖，用 Arduino IDE 打开 `carplay.ino` 编译。

## 5. 发给甲方

发送仓库首页链接，例如 `https://github.com/你的用户名/carplay`。对方打开首页即可阅读操作说明，环境文档用于配置 Arduino IDE，`HTTP接口说明.md` 用于接口对接。

私有仓库需要先在 **Settings → Collaborators** 中邀请对方的 GitHub 账号，对方接受邀请后才能访问。只有链接不足以访问私有仓库。也可以直接把 `carplay-github.zip` 发给对方作离线交付。

以后本地程序更新后，记得同步更新上传版本，并再次检查配对码等设备配置。不要仅更新 `.ino` 而漏掉同目录的 `.h`。
